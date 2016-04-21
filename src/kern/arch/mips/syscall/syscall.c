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
#define DEBUGP 0

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
#include <synch.h>
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

	//kprintf("SYSTEMCALL: %d\n", callno);

	retval = 0;
	//if(DEBUGP) kprintf("START OF SYSCALL: %d\n", callno);

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
			//if(DEBUGP) kprintf("\n\n%d %d %d %d\n\n", tf->tf_a0, tf->tf_a1, tf->tf_a2, tf->tf_a3);
			err = sys_write(tf->tf_a0, (userptr_t)tf->tf_a1, tf->tf_a2, &retval);
			break;
	    case SYS_close:
			err = sys_close(tf->tf_a0);
			break;
	    case SYS_getpid:
			err = sys_getpid(&retval);
			break;
	    case SYS_fork:
			if(DEBUGP) kprintf("call fork\n");
			err = sys_fork(tf, &retval);
			break;
	    case SYS_execv:
			err = sys_execv((userptr_t)tf->tf_a0, (char**)tf->tf_a1);
			break;
	    case SYS_waitpid:
			err = sys_waitpid(tf->tf_a0, (int*) &tf->tf_a1, (int) tf->tf_a2, &retval);
			break;
	    case SYS_sbrk:
			err = sys_sbrk(tf->tf_a0, &retval);
			break;
	    case SYS__exit:
			sys__exit(tf->tf_a0);
			break;

	    default:
		if(DEBUGP) kprintf("Unknown syscall %d\n", callno);
		err = ENOSYS;
		break;
	}


	if (err) {
		/*
		 * Return the error code. This gets converted at
		 * userlevel to a return value of -1 and the error
		 * code in errno.
		 */
		if(DEBUGP) kprintf("ERROR in syscall error check: %d\n", callno);
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
	//if(DEBUGP) kprintf("END OF SYSCALL\n");

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
	if(filename == NULL){
		return ENODEV;
	}
	if(flagmask != O_RDONLY && flagmask != O_WRONLY && flagmask != O_RDWR){
		return EINVAL;
	}

	name = kmalloc(sizeof(char) * PATH_MAX);
	if(name == NULL) {
		return ENOMEM;
	}
	statbuf = kmalloc(sizeof(struct stat));
	if(statbuf == NULL) {
		kfree(name);
		return ENOMEM;
	}

	newfdesc = (struct fdesc*) kmalloc(sizeof(struct fdesc));
	if(newfdesc == NULL) {
		kfree(name);
		kfree(statbuf);
		return ENOMEM;
	}

	//find file descriptor
	for(fd = 0; fd < OPEN_MAX; fd++){
		if((void*) curthread->t_fdtable[fd] == NULL) break;
	}
	curthread->t_fdtable[fd] = newfdesc;

	//protect file name and open file
	ret = copyinstr((const_userptr_t) filename, name, PATH_MAX, &len);
	//if(DEBUGP) kprintf("Name before: %s, Name after: %s, len: %d\n", filename, name, len);
	if(ret < 0){
		kfree(name);
		kfree(statbuf);
		kfree(newfdesc);
		return EIO;
	}

	ret = vfs_open(name, flags, mode, &vn);
	if(ret){
		kfree(name);
		kfree(statbuf);
		kfree(newfdesc);
		if(DEBUGP) kprintf("vfs_open failed\n");
		return ret;
	}

	//set variables in file table
	curthread->t_fdtable[fd]->fname = name;
	curthread->t_fdtable[fd]->flags = flags;
	curthread->t_fdtable[fd]->vn = vn;
	curthread->t_fdtable[fd]->fdlock = lock_create(name);
	curthread->t_fdtable[fd]->refcount = 1;

	if(curthread->t_fdtable[fd]->vn == NULL){
		if(DEBUGP) kprintf("vn is null in open after setting fdtable\n");
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
	if(DEBUGP) kprintf("retval: %d\n", *retval);
	return 0;
}

ssize_t sys_read(int fd, void* buf, size_t size, int32_t* retval){
	int ret;
	struct uio* read;

	//check valid arguments
	if(curthread->t_fdtable[fd] == NULL || buf == NULL || size == 0){
		return EBADF;
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
		return ret;
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
	//if(DEBUGP) kprintf("SYS_WRITE: in sys_write\n");

	//check valid arguments
	if(buf == NULL){
		//kprintf("buff is null in write\n");
		return EFAULT;
	}
	if(curthread->t_fdtable[fd] == NULL){
		//kprintf("curthread->t_fdtable[fd] is null in write\n");
		return EBADF;
	}
	//if(size == 0){
	//	if(DEBUGP) kprintf("size is 0 in write\n");
	//	return -1;
	//}
	//if(DEBUGP) kprintf("SYS_WRITE: get lock\n");
	if(DEBUGP) kprintf("before lock_aqcuire\n");
	lock_acquire(curthread->t_fdtable[fd]->fdlock);

	if(DEBUGP) kprintf("kmalloc write\n");
	//if(DEBUGP) kprintf("SYS_WRITE: kmalloc for write\n");
	write = kmalloc(sizeof(struct uio));
	//set uio variables
	//if(DEBUGP) kprintf("SYS_WRITE: set write variables\n");


	if(DEBUGP) kprintf("malloc iov\n");
	write->uio_iov = kmalloc(sizeof(struct iovec));

	write->uio_iov->iov_ubase = (void*) buf;
  	write->uio_iov->iov_len = size;
  	write->uio_offset = curthread->t_fdtable[fd]->offset;
  	write->uio_resid = size;
  	write->uio_segflg = UIO_USERSPACE;
 	write->uio_rw = UIO_WRITE;
  	write->uio_space = curthread->t_proc->p_addrspace; //WE THINK?!?!

	//read
	if(DEBUGP) kprintf("VOP_Write\n");
	ret = VOP_WRITE(curthread->t_fdtable[fd]->vn, write);
	if(ret < 0){
		kfree(write->uio_iov);
		kfree(write);
		return ret;
	}

	//update offset and set return to how many bytes read
	curthread->t_fdtable[fd]->offset = write->uio_offset;
	*retval = size - write->uio_resid;
	kfree(write->uio_iov);
	kfree(write);
	lock_release(curthread->t_fdtable[fd]->fdlock);

	return 0;
}

int sys_close(int fd){
	if(curthread->t_fdtable[fd] == NULL){
		return EBADF;
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
		if(DEBUGP) kprintf("freed fd \n");
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
	int i;

	

	if(DEBUGP) kprintf("entered enter_forked_process\n");
	info = (struct childinfo*) cinfo;

	if(DEBUGP) kprintf("set child tf values\n");
	info->tf->tf_v0 = 0;
	info->tf->tf_a3 = 0;
	info->tf->tf_epc += 4;

	if(DEBUGP) kprintf("copy fdtable\n");
	for(i = 0; i < OPEN_MAX; i++){
		curthread->t_fdtable[i] = info->fdtable[i];
		if(curthread->t_fdtable[i] != NULL)
			curthread->t_fdtable[i]->refcount++;
	}

	struct trapframe newtf;
	memcpy(&newtf, info->tf, sizeof(struct trapframe));	

	if(DEBUGP) kprintf("call mips_usermode\n");
	mips_usermode(&newtf);

	kfree(info->tf);
	kfree(info);
}


pid_t sys_fork(struct trapframe *tf, int32_t* retval){

	struct childinfo* info;
	struct trapframe* newtf;
	struct addrspace* newas;
	struct proc* newproc;
	int i, j;

	if(DEBUGP) kprintf("start sys_fork!!!!!!!!!!!!!!!!!: %d\n", sizeof(struct proc));
	newproc = (struct proc*) kmalloc(sizeof(struct proc));

	lock_acquire(proc_Lock);

	if(DEBUGP) kprintf("num threads: %d\n", newproc->p_numthreads);

	//make copy of tf
	if(DEBUGP) kprintf("make newtf\n");
	newtf = (struct trapframe*) kmalloc(sizeof(struct trapframe));
	if(DEBUGP) kprintf("memcpy newtf\n");
	memcpy(newtf, tf, sizeof(struct trapframe));

	//make copy of as
	if(DEBUGP) kprintf("call as_copy\n");
	as_copy(curthread->t_proc->p_addrspace, &newas);
	if(DEBUGP) kprintf("end as_copy\n");

	//make new process
	newproc->p_name = (char*) kmalloc(sizeof(char));
	newproc->p_name[0] = '\0';

	for(i = 1; i < __PID_MAX; i++){
		if(proc_Array[i] == NULL) break;
	}
	if(i == __PID_MAX) {
		return ENPROC;
	}
	proc_Array[i] = newproc;
	*retval = i;
	newproc->p_id = i;
	newproc->p_numthreads = 1;
	newproc->p_addrspace = newas;
	newproc->p_cwd = curthread->t_proc->p_cwd;

	newproc->p_parent = curthread->t_proc->p_id;


	newproc->p_parsem = kmalloc(sizeof(struct semaphore));
	newproc->p_childsem = kmalloc(sizeof(struct semaphore));

	newproc->p_parsem = sem_create("parent", 0);
	newproc->p_childsem = sem_create("child", 0);
	
	memset(newproc->p_children, -1, __PID_MAX);

	for(j = 0; j < __PID_MAX; j++){
		if(curthread->t_proc->p_children[j] == -1){
			curthread->t_proc->p_children[j] = newproc->p_id;
			break;
		}
	}

	if(j == __PID_MAX){
		return ENPROC;
	}


	


	info = (struct childinfo*) kmalloc(sizeof(struct childinfo));
	info->tf = newtf;
	info->fdtable = curthread->t_fdtable;

	lock_release(proc_Lock);

	if(DEBUGP) kprintf("call thread_fork\n");
	thread_fork("childproc", newproc, enter_forked_process, info, 0);
	
	//kfree(newproc->p_name);
	//kfree(newproc);
	

	return 0;

}

pid_t sys_getpid(int32_t* retval){
	*retval = curthread->t_proc->p_id;
	return 0;
}


int sys_execv(userptr_t prog, char** args){
	char* progname;
	char** progargs;
	size_t size;
	int ret, argc, i;
	struct vnode *v;
	vaddr_t startptr, stackptr;
	userptr_t* argptr;

	//struct addrspace *as;
	//struct vnode *v;
	//vaddr_t entrypoint, stackptr;

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

	if(DEBUGP) kprintf("EXECV: name = %s\n", progname);

	for(argc = 0; (void*) args[argc] != NULL; argc++);

	progargs = (char**) kmalloc(sizeof(char*) * argc);
	for(i = 0; i < argc; i++){
		progargs[i] = (char*) kmalloc(sizeof(char) * strlen(args[i]) + 1);
		ret = copyinstr((const_userptr_t) args[i], progargs[i], sizeof(char) * strlen(args[i]) + 1, &size);
		
		if(DEBUGP) kprintf("EXECV: read args[%d] = %s\n", i, progargs[i]);
		//TODO error check
	}	

	if(curthread->t_proc->p_addrspace != NULL){
		if(DEBUGP) kprintf("EXECV: destroy as\n");
		as_destroy(curthread->t_proc->p_addrspace);
	}

	if(DEBUGP) kprintf("EXECV: vfs_open\n");
	ret = vfs_open((char*) progname, O_RDONLY, 0, &v);
	if(ret){
		return ret;
	}

	//create and activate new as
	if(DEBUGP) kprintf("EXECV: create as\n");
	curthread->t_proc->p_addrspace = as_create();
	if(DEBUGP) kprintf("EXECV: activate as\n");
	as_activate();
	
	if(DEBUGP) kprintf("EXECV: load_elf\n");
	ret = load_elf(v, &startptr);
	

	vfs_close(v);
	ret = as_define_stack(curthread->t_proc->p_addrspace, &stackptr);


	if(DEBUGP) kprintf("EXECV: move stackptr\n");
	stackptr -= sizeof(char*) * (argc + 1);
	argptr = (userptr_t*) stackptr;
	copyout(progargs, (userptr_t) argptr, sizeof(char*) * (argc + 1));

	for(i = 0; i < argc; i++){
		if(DEBUGP) kprintf("EXECV: set argptr[%d]\n", i);
		stackptr -= sizeof(char) * (strlen(progargs[i]) + 1);
		argptr[i] = (userptr_t) stackptr;
		copyout(progargs[i], (userptr_t) argptr[i], sizeof(char) * (strlen(progargs[i]) + 1));
	}

	argptr[argc] = 0;
	stackptr -= stackptr % 8;

	if(DEBUGP) kprintf("EXECV: enter new process\n");
	enter_new_process(argc, (userptr_t) argptr, NULL, stackptr, startptr);

	(void) progargs;
	return 0;
}

pid_t sys_waitpid(pid_t pid, int *returncode, int flags, int32_t* retval){
	(void) flags;
	(void) pid;
	(void) returncode;
	(void) retval;

	if(proc_Array[pid] == NULL){
		return -1;
	}
	if(DEBUGP) kprintf("WAITPID: waiting for parent\n");
	P(proc_Array[pid]->p_childsem);
	if(DEBUGP) kprintf("WAITPID: done waiting for parent\n");
	*returncode = proc_Array[pid]->p_exitcode;
	*retval = pid;

	/*
	if(proc_Array[pid] == NULL){
		return -1;
	}

	V(proc_Array[pid]->p_waitsem);

	*retval = pid;
	*returncode = proc_Array[pid]->p_exitcode;
	*/
	return 0;
}

void sys__exit(int code){
	int i = 0;
	curthread->t_proc->p_exitcode = code;
	
	V(curthread->t_proc->p_childsem);

	P(curthread->t_proc->p_parsem);

	proc_Array[curthread->t_proc->p_id] = NULL;

	for(i = 0; curthread->t_proc->p_children[i] != -1; i++){
		V(proc_Array[curthread->t_proc->p_children[i]]->p_parsem);	
	}

	kfree(curthread->t_proc->p_parsem);
	kfree(curthread->t_proc->p_childsem);
	kfree(curthread->t_proc->p_name);
	spinlock_cleanup(&curthread->t_proc->p_lock);
	as_destroy(curthread->t_proc->p_addrspace);

	thread_exit();

	kprintf("should not get here!!!!!!!\n");

	while(1);

}

int sys_sbrk(int inc, int* retval) {
		inc += 4 - (inc % 4);
		if(curproc->p_addrspace->as_heapend + inc > curproc->p_addrspace->as_stackpbase) {
			*retval = -1;
			return ENOMEM;
		}

		if(curproc->p_addrspace->as_heapend + inc < curproc->p_addrspace->as_heapstart) {
			*retval = -1;
			return EINVAL;
		}

		*retval = curproc->p_addrspace->as_heapend;
		curproc->p_addrspace->as_heapend += inc;
		return 0;
}












