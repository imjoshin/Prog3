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

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than runprogram() does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <proc_array.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname)
{
	struct addrspace *as;
	struct vnode* v;
	struct vnode* vin;
	struct vnode* vout;
	struct vnode* verr;
	struct fdesc* fin;
	struct fdesc* fout;
	struct fdesc* ferr;
	char sin[6] = "STDIN";
	char sout[6] = "STDOUT";
	char serr[6] = "STDERR";
	char in[5] = "con:";
	char out[5] = "con:";
	char err[5] = "con:";
	vaddr_t entrypoint, stackptr;
	int result, pid;

	if(proc_Lock == NULL){
		proc_Lock = lock_create("proc_Lock");
	}
	lock_acquire(proc_Lock);

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
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
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	//set up stdin stdout and stderr

	fin = (struct fdesc*) kmalloc(sizeof(struct fdesc));
	vfs_open(in, O_RDONLY, 0664, &vin);
	curthread->t_fdtable[0] = fin;

	//set variables in file table
	curthread->t_fdtable[0]->fname = sin;
	curthread->t_fdtable[0]->flags = O_RDONLY;
	curthread->t_fdtable[0]->vn = vin;
	curthread->t_fdtable[0]->fdlock = lock_create(sin);
	curthread->t_fdtable[0]->refcount = 1;
	if(curthread->t_fdtable[0]->vn == NULL){
		kprintf("vin is null\n");
	}

	fout = (struct fdesc*) kmalloc(sizeof(struct fdesc));
	vfs_open(out, O_WRONLY, 0664, &vout);
	curthread->t_fdtable[1] = fout;
	if(vout == NULL){
		//kprintf("vout1 is null\n");
	}

	//set variables in file table
	curthread->t_fdtable[1]->fname = sout;
	curthread->t_fdtable[1]->flags = O_WRONLY;
	curthread->t_fdtable[1]->vn = vout;
	curthread->t_fdtable[1]->fdlock = lock_create(sout);
	curthread->t_fdtable[1]->refcount = 1;
	if(curthread->t_fdtable[1]->vn == NULL){
		kprintf("vout is null\n");
	}


	ferr = (struct fdesc*) kmalloc(sizeof(struct fdesc));
	vfs_open(err, O_WRONLY, 0664, &verr);
	curthread->t_fdtable[2] = ferr;
	if(verr == NULL){
		//kprintf("verr1 is null\n");
	}

	//set variables in file table
	curthread->t_fdtable[2]->fname = serr;
	curthread->t_fdtable[2]->flags = O_WRONLY;
	curthread->t_fdtable[2]->vn = verr;
	curthread->t_fdtable[2]->fdlock = lock_create(serr);
	curthread->t_fdtable[2]->refcount = 1;
	if(curthread->t_fdtable[2]->vn == NULL){
		//kprintf("verr is null\n");
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}


	memset(proc_Array, 0, (sizeof(struct proc*) * __PID_MAX));
	//proc_Array[1] = curthread->t_proc;
	//curthread->t_proc->p_id = 1;
	//kprintf("something\n");
	
	//get and set pid
	for(pid = 1; pid < __PID_MAX; pid++){
		if(proc_Array[pid] == NULL) break;
	}
	kprintf("runprogram pid to be %d\n", pid);

	curthread->t_proc->p_id = pid;
	proc_Array[pid] = curthread->t_proc;
	kprintf("set runprogram pid\n");
	
	lock_release(proc_Lock);
	
	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  NULL /*userspace addr of environment*/,
			  stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

