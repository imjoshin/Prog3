/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/syscall.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <kern/fcntl.h>
#include <copyinout.h>
#include <vfs.h>
#include <stat.h>
#include <proc.h>
#include <proc_array.h>
#include <addrspace.h>
#include <../arch/mips/include/trapframe.h>
#include <limits.h>


/*
 * System call dispatcher.
 *
 * A pointer to the trapframe created during exception entry (in
 * exception-*.S) is passed in.
 *
 * The calling conventions for syscalls are as follows: Like ordinary
 * function calls, the first 4 32-bit arguments are passed in the 4
 * argument registers a0-a3. 64-bit arguments are passed in *aligned*
 * pairs of registers, that is, either a0/a1 or a2/a3. This means that
 * if the first argument is 32-bit and the second is 64-bit, a1 is
 * unused.
 *
 * This much is the same as the calling conventions for ordinary
 * function calls. In addition, the system call number is passed in
 * the v0 register.
 *
 * On successful return, the return value is passed back in the v0
 * register, or v0 and v1 if 64-bit. This is also like an ordinary
 * function call, and additionally the a3 register is also set to 0 to
 * indicate success.
 *
 * On an error return, the error code is passed back in the v0
 * register, and the a3 register is set to 1 to indicate failure.
 * (Userlevel code takes care of storing the error code in errno and
 * returning the value -1 from the actual userlevel syscall function.
 * See src/user/lib/libc/arch/mips/syscalls-mips.S and related files.)
 *
 * Upon syscall return the program counter stored in the trapframe
 * must be incremented by one instruction; otherwise the exception
 * return code will restart the "syscall" instruction and the system
 * call will repeat forever.
 *
 * If you run out of registers (which happens quickly with 64-bit
 * values) further arguments must be fetched from the user-level
 * stack, starting at sp+16 to skip over the slots for the
 * registerized values, with copyin().
 */


//used for new child process
struct childinfo {
	struct trapframe* tf;
	struct fdesc** fdtable;
};

void
syscall(struct trapframe *tf)
{
	int callno;
	int32_t retval;
	int err = 0;

	KASSERT(curthread != NULL);
	KASSERT(curthread->t_curspl == 0);
	KASSERT(curthread->t_iplhigh_count == 0);

	callno = tf->tf_v0;

	/*
	 * Initialize retval to 0. Many of the system calls don't
	 * really return a value, just 0 for success and -1 on
	 * error. Since retval is the value returned on success,
	 * initialize it to 0 by default; thus it's not necessary to
	 * deal with it except for calls that return other values,
	 * like write.
	 */

	retval = 0;
	kprintf("START OF SYSCALL: %d\n", callno);

	switch (callno) {
	    case SYS_reboot:
		err = sys_reboot(tf->tf_a0);
		break;

	    case SYS___time:
		err = sys___time((userptr_t)tf->tf_a0,
				 (userptr_t)tf->tf_a1);
		break;

	    case SYS_open:
			err = sys_open((userptr_t)tf->tf_a0, tf->tf_a1, tf->tf_a2, &retval);
			
			break;
	    case SYS_read:
			err = sys_read(tf->tf_a0, (userptr_t)tf->tf_a1, tf->tf_a2, &retval);
			break;
	    case SYS_write:
			//kprintf("\n\n%d %d %d %d\n\n", tf->tf_a0, tf->tf_a1, tf->tf_a2, tf->tf_a3);
			err = sys_write(tf->tf_a0, (userptr_t)tf->tf_a1, tf->tf_a2, &retval);
			break;
	    case SYS_close:
			err = sys_close(tf->tf_a0);
			break;
	    case SYS_getpid:
			err = sys_getpid(&retval);
			break;
	    case SYS_fork:
			kprintf("call fork\n");
			err = sys_fork(tf, &retval);
			break;
	    case SYS_execv:
			err = sys_execv((userptr_t)tf->tf_a0, (userptr_t)tf->tf_a1);
			break;
	    case SYS_waitpid:
			err = sys_waitpid(tf->tf_a0, (int*) &tf->tf_a1, (int) tf->tf_a2, &retval);
			break;
	    case SYS__exit:
			sys__exit(tf->tf_a0);
			break;

	    default:
		kprintf("Unknown syscall %d\n", callno);
		err = ENOSYS;
		break;
	}


	if (err) {
		/*
		 * Return the error code. This gets converted at
		 * userlevel to a return value of -1 and the error
		 * code in errno.
		 */
		kprintf("ERROR in syscall error check: %d\n", callno);
		tf->tf_v0 = err;
		tf->tf_a3 = 1;      /* signal an error */
	}
	else {
		/* Success. */
		tf->tf_v0 = retval;
		tf->tf_a3 = 0;      /* signal no error */
	}

	/*
	 * Now, advance the program counter, to avoid restarting
	 * the syscall over and over again.
	 */

	tf->tf_epc += 4;
	kprintf("END OF SYSCALL\n");

	/* Make sure the syscall code didn't forget to lower spl */
	KASSERT(curthread->t_curspl == 0);
	/* ...or leak any spinlocks */
	KASSERT(curthread->t_iplhigh_count == 0);
}





int sys_open(userptr_t filename, int flags, mode_t mode, int32_t* retval){
	int fd, ret, flagmask;
	size_t len;
	char* name;
	struct vnode *vn;
	struct fdesc* newfdesc;
	struct stat *statbuf; //used for file properties

	flagmask = O_CREAT | O_EXCL | O_TRUNC | O_APPEND;
	flagmask = flags & !flagmask; //remove extra flags

	//check valid arguments, no more than one flag is set, one must be set
	if(filename == NULL || (flagmask != O_RDONLY && flagmask != O_WRONLY && flagmask != O_RDWR)){
		return -7;
	}

	name = kmalloc(sizeof(char) * PATH_MAX);
	if(name == NULL) {
		return -2;
	}
	statbuf = kmalloc(sizeof(struct stat));
	if(statbuf == NULL) {
		kfree(name);
		return -2;
	}

	newfdesc = (struct fdesc*) kmalloc(sizeof(struct fdesc));
	if(newfdesc == NULL) {
		kfree(name);
		kfree(statbuf);
		return -2;
	}

	//find file descriptor
	for(fd = 0; fd < OPEN_MAX; fd++){
		if((void*) curthread->t_fdtable[fd] == NULL) break;
	}
	curthread->t_fdtable[fd] = newfdesc;

	//protect file name and open file
	ret = copyinstr((const_userptr_t) filename, name, PATH_MAX, &len);
	//kprintf("Name before: %s, Name after: %s, len: %d\n", filename, name, len);
	if(ret < 0){
		kfree(name);
		kfree(statbuf);
		kfree(newfdesc);
		return -3;
	}

	ret = vfs_open(name, flags, mode, &vn);
	if(vn == NULL){
		kprintf("vn is null in open\n");
	}
	if(ret){
		kfree(name);
		kfree(statbuf);
		kfree(newfdesc);
		kprintf("vfs_open failed\n");
		return ret;
	}

	//set variables in file table
	curthread->t_fdtable[fd]->fname = name;
	curthread->t_fdtable[fd]->flags = flags;
	curthread->t_fdtable[fd]->vn = vn;
	curthread->t_fdtable[fd]->fdlock = lock_create(name);
	curthread->t_fdtable[fd]->refcount = 1;

	if(curthread->t_fdtable[fd]->vn == NULL){
		kprintf("vn is null in open after setting fdtable\n");
	}

	//if append, set to file size
	if((flags & O_APPEND) != 0){
		//get vn statistics and set to current size
		VOP_STAT(vn, statbuf);
		curthread->t_fdtable[fd]->offset = statbuf->st_size;
	}else{
		curthread->t_fdtable[fd]->offset = 0;
	}

	kfree(statbuf);
	
	*retval = fd;
	kprintf("retval: %d\n", *retval);
	return 0;
}

ssize_t sys_read(int fd, void* buf, size_t size, int32_t* retval){
	int ret;
	struct uio* read;

	//check valid arguments
	if(curthread->t_fdtable[fd] == NULL || buf == NULL || size == 0){
		return -1;
	}
	lock_acquire(curthread->t_fdtable[fd]->fdlock);

	read = kmalloc(sizeof(struct uio));

	//set uio variables
	read->uio_iov->iov_ubase = buf;
  	read->uio_iov->iov_len = size;
  	read->uio_offset = curthread->t_fdtable[fd]->offset;
  	read->uio_resid = size;
  	read->uio_segflg = UIO_USERSPACE;
 	read->uio_rw = UIO_READ;
  	read->uio_space = curthread->t_proc->p_addrspace; //WE THINK?!?!

	//read
	ret = VOP_READ(curthread->t_fdtable[fd]->vn, read);
	if(ret < 0){
		kfree(read);
		return -1;
	}

	//update offset and set return to how many bytes read
	curthread->t_fdtable[fd]->offset = read->uio_offset;

	*retval = size - read->uio_resid;
	kfree(read);
	lock_release(curthread->t_fdtable[fd]->fdlock);

	return 0;
}

int sys_write(int fd, void* buf, size_t size, int32_t* retval){
	int ret;
	struct uio* write;

	kprintf("SYS_WRITE: in sys_write\n");

	//check valid arguments
	if(buf == NULL){
		kprintf("buff is null in write\n");
		return -1;
	}
	if(curthread->t_fdtable[fd] == NULL){
		kprintf("curthread->t_fdtable[fd] is null in write\n");
		return -1;
	}
	//if(size == 0){
	//	kprintf("size is 0 in write\n");
	//	return -1;
	//}
	kprintf("SYS_WRITE: get lock\n");
	lock_acquire(curthread->t_fdtable[fd]->fdlock);
	kprintf("SYS_WRITE: kmalloc for write\n");
	write = kmalloc(sizeof(struct uio));
	//set uio variables
	kprintf("SYS_WRITE: set write variables\n");
	write->uio_iov->iov_ubase = buf;
  	write->uio_iov->iov_len = size;
  	write->uio_offset = curthread->t_fdtable[fd]->offset;
  	write->uio_resid = size;
  	write->uio_segflg = UIO_USERSPACE;
 	write->uio_rw = UIO_WRITE;
  	write->uio_space = curthread->t_proc->p_addrspace; //WE THINK?!?!

	//read
	ret = VOP_WRITE(curthread->t_fdtable[fd]->vn, write);
	if(ret < 0){
		kfree(write);
		return -1;
	}

	//update offset and set return to how many bytes read
	curthread->t_fdtable[fd]->offset = write->uio_offset;
	*retval = size - write->uio_resid;
	kfree(write);
	lock_release(curthread->t_fdtable[fd]->fdlock);

	return 0;
}

int sys_close(int fd){
	if(curthread->t_fdtable[fd] == NULL){
		return -1;
	}

	//acquire lock
	lock_acquire(curthread->t_fdtable[fd]->fdlock);


	//decrease reference counter
	curthread->t_fdtable[fd]->refcount--;
	
	//if no more references, close fd
	if(curthread->t_fdtable[fd]->refcount == 0){
		vfs_close(curthread->t_fdtable[fd]->vn);
		kfree(curthread->t_fdtable[fd]->fname);
		lock_destroy(curthread->t_fdtable[fd]->fdlock);
		kfree(curthread->t_fdtable[fd]);
		kprintf("freed fd \n");
		curthread->t_fdtable[fd] = NULL;
	}else{
		lock_release(curthread->t_fdtable[fd]->fdlock);
	}

	return 0;
}


/*
 * Enter user mode for a newly forked process.
 *
 * This function is provided as a reminder. You need to write
 * both it and the code that calls it.
 *
 * Thus, you can trash it and do things another way if you prefer.
 */
void enter_forked_process(void* cinfo, unsigned long x){
	(void) x;
	struct childinfo* info;

	

	kprintf("entered enter_forked_process\n");
	info = (struct childinfo*) cinfo;

	kprintf("set child tf values\n");
	info->tf->tf_v0 = 0;
	info->tf->tf_a3 = 0;
	info->tf->tf_epc += 4;

	kprintf("copy fdtable\n");
	memcpy(curthread->t_fdtable, info->fdtable, sizeof(struct fdesc*));

	

	kprintf("call mips_usermode\n");
	mips_usermode(info->tf);
}


pid_t sys_fork(struct trapframe *tf, int32_t* retval){
	(void) tf;
	(void) retval;
	return 0;
/*
	struct childinfo* info;
	struct trapframe* newtf;
	struct addrspace* newas;
	struct proc* newproc;
	kprintf("start sys_fork: %d\n", sizeof(struct proc));
	newproc = (struct proc*) kmalloc(sizeof(struct proc));
	memcpy(newproc, curthread->t_proc, sizeof(struct proc));

	kprintf("num threads: %d\n", newproc->p_numthreads);

	//make copy of tf
	kprintf("make newtf\n");
	newtf = (struct trapframe*) kmalloc(sizeof(struct trapframe));
	kprintf("memcpy newtf\n");
	memcpy(newtf, tf, sizeof(struct trapframe));

	//make copy of as
	kprintf("call as_copy\n");
	as_copy(curthread->t_proc->p_addrspace, &newas);
	kprintf("end as_copy\n");

	//make new process
	kprintf("make newproc\n");
	//newproc = (struct proc*) kmalloc(sizeof(struct proc));
	kprintf("memcpy newproc\n");
	memcpy(newproc, curthread->t_proc, sizeof(struct proc));

	info = (struct childinfo*) kmalloc(sizeof(struct childinfo));
	info->tf = newtf;
	info->fdtable = curthread->t_fdtable;

	kprintf("call thread_fork\n");
	thread_fork("childproc", newproc, enter_forked_process, tf, 0);
	(void) retval;

	return 0;
*/
}

pid_t sys_getpid(int32_t* retval){
	*retval = curthread->t_proc->p_id;
	return 0;
}

int sys_execv(userptr_t prog, userptr_t args){
	
	char* progname;
	char** progargs;
	size_t size;
	int ret, i, len, argc;

	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;

	
	//set program name
	progname = (char*) kmalloc(sizeof(char) * PATH_MAX);
	ret = copyinstr((const_userptr_t) prog, progname, PATH_MAX, &size);
	if(ret){
		kfree(progname);
		return EFAULT;
	}
	if(size == 1){
		kfree(progname);
		return EINVAL;
	}

	//set program arguments
	progargs = (char**) kmalloc(sizeof(char**));
	ret = copyin((const_userptr_t) args, progargs, sizeof(char**));
	if(ret){
		kfree(progname);
		kfree(progargs);
		return EFAULT;
	}

	progargs[0] = (char*) kmalloc(sizeof(char) * PATH_MAX);
	ret = copyinstr((const_userptr_t) &(args[0]), progargs[0], PATH_MAX, &size);
	if(ret){
		kfree(progname);
		kfree(progargs);
		return EFAULT;
	}
	argc = 0;
	while(progargs[argc] != NULL){
		argc++;
		progargs[argc] = (char*) kmalloc(sizeof(char) * PATH_MAX);
		ret = copyinstr((const_userptr_t) &(args[argc]), progargs[argc], PATH_MAX, &size);
		if(ret){
			kfree(progname);
			kfree(progargs);
			return EFAULT;
		}
	}


	/* Open the file. */
	ret = vfs_open(progname, O_RDONLY, 0, &v);
	if (ret) {
		return ret;
	}

	/* We should be a new process. */
	KASSERT(proc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	proc_setas(as);
	as_activate();

	/* Load the executable. */
	ret = load_elf(v, &entrypoint);
	if (ret) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return ret;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	ret = as_define_stack(as, &stackptr);
	if (ret) {
		/* p_addrspace will go away when curproc is destroyed */
		return ret;
	}

	for(i = 0; i < argc; i++){
		len = strlen(progargs[i]) + 1;
		if(len % 4 != 0) {
			len += len % 4;
		}
		copyoutstr(progargs[i], (userptr_t)&stackptr, len, (unsigned int*) &ret);
		stackptr -= len;
	}

	/* Warp to user mode. */
	enter_new_process(argc, (userptr_t) &stackptr,
			  NULL /*userspace addr of environment*/,
			  stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;










	return 0;
}

pid_t sys_waitpid(pid_t pid, int *returncode, int flags, int32_t* retval){
	(void) pid;
	(void) returncode;
	(void) flags;
	(void) retval;
	return 0;
}

void sys__exit(int code){
	(void) code;
	while(1);
}

/*
static void child_setup(void* data1, unsigned long data2){
	(void) data2;
	struct childinfo* info = data1;

	kprintf("activate child as\n");
	as_activate();

	kprintf("set child tf values\n");
	info->tf->tf_v0 = 0;
	info->tf->tf_a3 = 0;
	info->tf->tf_epc += 4;

	kfree(info->tf);
	kfree(info);

	

	//curthread->tf
}

pid_t sys_fork(struct trapframe *tf, int32_t* retval){
	struct trapframe* newtf;
	struct addrspace* newas;
	struct proc *proc;
	struct childinfo* info;

	//if(proc_Locker == NULL){
	//	lock_create(proc_Locker);
	//}
	//lock_acquire(proc_Locker);

	//copy tf
	kprintf("make tf\n");
	newtf = (struct trapframe*) kmalloc(sizeof(struct trapframe));
	if(newtf == NULL) {
		return -1;
	}
	memcpy(tf, newtf, sizeof(struct trapframe));

	//copy as
	kprintf("make as\n");
	//newas = (struct addrspace*) kmalloc(sizeof(struct addrspace));
	//newas = as_create();
	//if(newas == NULL) {
	//	return -1;
	//}
	kprintf("before as copy\n");
	if(curthread->t_proc->p_addrspace == NULL){
		kprintf("as is NULL\n");
	}
	//---------------------------------------------------
	as_copy(curthread->t_proc->p_addrspace, &newas);
	//memcpy(&newas, curthread->t_proc->p_addrspace, sizeof(struct addrspace));
	kprintf("after as copy\n");
	
	//new proc
	kprintf("make proc\n");
	proc = kmalloc(sizeof(struct proc));
	if (proc == NULL) {
		return -1;
	}
	
	kprintf("set proc name\n");
	proc->p_name = kstrdup("childproc");
	if (proc->p_name == NULL) {
		kfree(proc);
		return -1;
	}

	proc->p_numthreads = 0;
	kprintf("make proc spinlock\n");
	spinlock_init(&proc->p_lock);

	kprintf("assign proc as\n");
	proc->p_addrspace = newas;
	curthread->t_proc->p_cwd = proc->p_cwd;
	//memcpy(curthread->t_proc->p_cwd, proc->p_cwd, sizeof(struct vnode*));

	kprintf("create info\n");
	info = (struct childinfo*) kmalloc(sizeof(struct childinfo*));
	if(info == NULL) {
		return -1;
	}

	info->tf = newtf;
	info->fdtable = curthread->t_fdtable;

	kprintf("fork child\n");
	*retval = thread_fork("child", proc, child_setup, info, 0);


	//lock_release(proc_Locker);

	return 0;
}
*/
