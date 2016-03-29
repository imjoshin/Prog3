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
void
syscall(struct trapframe *tf)
{
	int callno;
	int32_t retval;
	int err;

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

	switch (callno) {
	    case SYS_reboot:
		err = sys_reboot(tf->tf_a0);
		break;

	    case SYS___time:
		err = sys___time((userptr_t)tf->tf_a0,
				 (userptr_t)tf->tf_a1);
		break;

	    case SYS_open:
			
			break;
	    case SYS_read:
			
			break;
	    case SYS_write:
			
			break;
	    case SYS_close:
			
			break;
	    case SYS_getpid:
			
			break;
	    case SYS_fork:
			
			break;
	    case SYS_execv:
			
			break;
	    case SYS_waitpid:
			
			break;
	    case SYS__exit:
			
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

	/* Make sure the syscall code didn't forget to lower spl */
	KASSERT(curthread->t_curspl == 0);
	/* ...or leak any spinlocks */
	KASSERT(curthread->t_iplhigh_count == 0);
}

/*
 * Enter user mode for a newly forked process.
 *
 * This function is provided as a reminder. You need to write
 * both it and the code that calls it.
 *
 * Thus, you can trash it and do things another way if you prefer.
 */
void enter_forked_process(struct trapframe *tf){
	(void)tf;
}


int open(const char* filename, int flags){
	int fd, retval, flagmask;
	size_t len;
	char* name;
	struct vnode *vn;
	struct fdesc* newfdesc;
	struct stat *statbuf; //used for file properties

	flagmask = O_CREAT | O_EXCL | O_TRUNC | O_APPEND;
	flagmask = flags & !flagmask; //remove extra flags

	//check valid arguments, no more than one flag is set, one must be set
	if(filename == NULL || (flagmask != O_RDONLY && flagmask != O_WRONLY && flagmask != O_RDWR)){
		return -1;
	}

	name = kmalloc(sizeof(char) * PATH_MAX);
	statbuf = kmalloc(sizeof(struct stat));

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
	retval = copyinstr((const_userptr_t) filename, name, PATH_MAX, &len);

	kprintf("Name before: %s, Name after: %s, len: %d\n", filename, name, len);
	if(retval < 0){
		kfree(name);
		kfree(statbuf);
		kfree(newfdesc);
		return -3;
	}

	retval = vfs_open(name, flags, 0, &vn);
	if(retval){
		kfree(name);
		kfree(statbuf);
		kfree(newfdesc);
		return 1000+retval;
	}

	//set variables in file table
	curthread->t_fdtable[fd]->fname = name;
	curthread->t_fdtable[fd]->flags = flags;
	curthread->t_fdtable[fd]->vn = vn;
	curthread->t_fdtable[fd]->fdlock = lock_create(name);
	curthread->t_fdtable[fd]->refcount = 1;

	//if append, set to file size
	if((flags & O_APPEND) != 0){
		//get vn statistics and set to current size
		VOP_STAT(vn, statbuf);
		curthread->t_fdtable[fd]->offset = statbuf->st_size;
	}else{
		curthread->t_fdtable[fd]->offset = 0;
	}

	kprintf("open refcount = %d\n", curthread->t_fdtable[fd]->refcount);

	kfree(statbuf);
	
	return fd;
}

ssize_t read(int fd, void* buf, size_t size){
	int retval;
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
	retval = VOP_READ(curthread->t_fdtable[fd]->vn, read);
	if(retval < 0){
		return -1;
	}

	//update offset and set return to how many bytes read
	curthread->t_fdtable[fd]->offset = read->uio_offset;
	lock_release(curthread->t_fdtable[fd]->fdlock);
	return size - read->uio_resid;
}

int write(int fd, void* buf, size_t size){
	int retval;
	struct uio* write;

	//check valid arguments
	if(curthread->t_fdtable[fd] == NULL || buf == NULL || size == 0){
		return -1;
	}
	lock_acquire(curthread->t_fdtable[fd]->fdlock);

	write = kmalloc(sizeof(struct uio));

	//set uio variables
	write->uio_iov->iov_ubase = buf;
  	write->uio_iov->iov_len = size;
  	write->uio_offset = curthread->t_fdtable[fd]->offset;
  	write->uio_resid = size;
  	write->uio_segflg = UIO_USERSPACE;
 	write->uio_rw = UIO_WRITE;
  	write->uio_space = curthread->t_proc->p_addrspace; //WE THINK?!?!

	//read
	retval = VOP_WRITE(curthread->t_fdtable[fd]->vn, write);
	if(retval < 0){
		return -1;
	}

	//update offset and set return to how many bytes read
	curthread->t_fdtable[fd]->offset = write->uio_offset;
	lock_release(curthread->t_fdtable[fd]->fdlock);
	return size - write->uio_resid;
}

int close(int fd){
	if(curthread->t_fdtable[fd] == NULL){
		return -1;
	}

	//acquire lock
	lock_acquire(curthread->t_fdtable[fd]->fdlock);

	kprintf("close refcount = %d\n", curthread->t_fdtable[fd]->refcount);

	//decrease reference counter
	curthread->t_fdtable[fd]->refcount--;
	
	//if no more references, close fd
	if(curthread->t_fdtable[fd]->refcount == 0){
		vfs_close(curthread->t_fdtable[fd]->vn);
		kfree(curthread->t_fdtable[fd]);
		kprintf("freed fd \n");
		curthread->t_fdtable[fd] = NULL;
	}else{
		lock_release(curthread->t_fdtable[fd]->fdlock);
	}

	return 0;
}

pid_t getpid(){
	return curthread->t_proc->p_id;
}

pid_t fork(){
	return 0;
}

int execv(const char *prog, char *const *args){
	(void) prog;
	(void) args;
	return 0;
}

pid_t waitpid(pid_t pid, int *returncode, int flags){
	(void) pid;
	(void) returncode;
	(void) flags;
	return 0;
}

void _exit(int code){
	(void) code;
	while(1);
}
