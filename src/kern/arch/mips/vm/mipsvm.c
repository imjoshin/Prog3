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
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc_array.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <stat.h>

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 *
 * NOTE: it's been found over the years that students often begin on
 * the VM assignment by copying dumbvm.c and trying to improve it.
 * This is not recommended. dumbvm is (more or less intentionally) not
 * a good design reference. The first recommendation would be: do not
 * look at dumbvm at all. The second recommendation would be: if you
 * do, be sure to review it from the perspective of comparing it to
 * what a VM system is supposed to do, and understanding what corners
 * it's cutting (there are many) and why, and more importantly, how.
 */

/* under dumbvm, always have 72k of user stack */
/* (this must be > 64K so argument blocks of size ARG_MAX will fit) */
#define DUMBVM_STACKPAGES    18


static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
static struct lock* cm_lock;

void
vm_bootstrap(void)
{
	if(vm_bootstrap_done) return;

	if(DEBUGP) kprintf("VM_BOOTSTRAP: called\n");

	cm_lock = lock_create("cm_lock");
	
	paddr_t firstaddr, lastaddr;
	unsigned int page_num, i, steal, fromStealMem;


	(void)page_num; (void) i; (void) steal;
	(void)lastaddr; (void) firstaddr;
 
	lastaddr = ram_getsize();  


	steal = sizeof(struct coremap);
	steal += (lastaddr / PAGE_SIZE) * sizeof(struct coremap_entry);
	steal = (steal + (PAGE_SIZE - 1)) / PAGE_SIZE;
	if(DEBUGP) kprintf("VM_BOOTSTRAP: steal: %d\n", steal);

	page_num = lastaddr / PAGE_SIZE;

	// pages should be a kernel virtual address !!
	fromStealMem = PADDR_TO_KVADDR(ram_stealmem(steal));
	coremap = (struct coremap*) fromStealMem;
	firstaddr = ram_getfirstfree();

	if(DEBUGP) kprintf("VM_BOOTSTRAP: first: %08x, last: %08x, coremap: %08x, entries: %08x\n", firstaddr, lastaddr, (unsigned int)coremap, (unsigned int)&coremap->entries);
	if(DEBUGP) kprintf("VM_BOOTSTRAP: entries points at : %08x fromStealMem: %08x\n", (unsigned int) (coremap + sizeof(coremap)), fromStealMem);
	if(DEBUGP) kprintf("VM_BOOTSTRAP: page_num: %d, PAGE_SIZE: %d\n", page_num, PAGE_SIZE);

	coremap->entries = (struct coremap_entry*) (coremap + sizeof(coremap));
	
	firstaddr += PAGE_SIZE - (firstaddr % PAGE_SIZE);
	for(i = 0; i < page_num; i++){
		//kprintf("%d (%08x): 1\n", i, (unsigned int) &coremap->entries[i]);
		coremap->entries[i].st = FREE;
		coremap->entries[i].as = NULL;
		coremap->entries[i].pa = ((PAGE_SIZE * i) + firstaddr);
		coremap->entries[i].va = -1;
	}

	vm_bootstrap_done = 1;
	if(DEBUGP) kprintf("VM_BOOTSTRAP: created %d pages\n", page_num);
	max_pages = page_num;
	coremap->size = page_num;

}

/*
 * Check if we're in a context that can sleep. While most of the
 * operations in dumbvm don't in fact sleep, in a real VM system many
 * of them would. In those, assert that sleeping is ok. This helps
 * avoid the situation where syscall-layer code that works ok with
 * dumbvm starts blowing up during the VM assignment.
 */
static
void
dumbvm_can_sleep(void)
{
	if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}

static
paddr_t
getppages(unsigned long npages)
{
	if(DEBUGP) kprintf("GETPPAGES\n");
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
	return addr;
}

// Allocate/free some kernel-space virtual pages 
vaddr_t
alloc_kpages(unsigned npages)
{
	unsigned int i, j;
	paddr_t pa = 0;


	if(vm_bootstrap_done){
		if(DEBUGP) kprintf("ALLOC_KPAGES: acquire lock\n");
		if(DEBUGP) kprintf("ALLOC_KPAGES: coremap size = %d\n", max_pages);
		lock_acquire(cm_lock);
		if(DEBUGP) kprintf("ALLOC_KPAGES: lock acquired\n");
		if(DEBUGP) kprintf("ALLOC_KPAGES: coremap->size: %d\n", coremap->size);

		for(i = 0; i < coremap->size; i++){
			for(j = 0; j < npages; j++){
				//kprintf("ALLOC_KPAGES: state[%d] = %d\n", i+j, coremap->entries[i + j].st);
				if(coremap->entries[i + j].st != FREE){
					//kprintf("ALLOC_KPAGES: not free\n");
					i += j;
					break;
				}
			}
			if(j == npages){
				break;
			}
		}
		
		if(DEBUGP) kprintf("ALLOC_KPAGES: finished for\n");

		if (i == coremap->size) { // || coremap->entries[i + j].pa == 0) {
			if(DEBUGP) kprintf("ALLOC_KPAGES: return 0 => i = size: %d\n", i == coremap->size);
			lock_release(cm_lock);
			return 0;
		}

		pa = PADDR_TO_KVADDR(coremap->entries[i].pa);

		for(j = 0; j < npages; j++){
			page_alloc(i + j);
		}
	
		if(DEBUGP) kprintf("ALLOC_KPAGES: release lock\n");
		lock_release(cm_lock);

	}else{
		dumbvm_can_sleep();
		pa = getppages(npages);
		if (pa==0) {
			return 0;
		}
		if(DEBUGP) kprintf("\tELSE: %08x\n", (unsigned)PADDR_TO_KVADDR(pa));
		return PADDR_TO_KVADDR(pa);
	}

	if(DEBUGP) kprintf("ALLOC_KPAGES: returning %08x\n", pa);
	return pa;
	
}

//alloc_kpages helper
paddr_t page_alloc(int pnum){
	KASSERT(coremap->entries[pnum].st == FREE);
	
	if(DEBUGP) kprintf("PAGE_ALLOC: starting\n");

	coremap->entries[pnum].as = curthread->t_proc->p_addrspace;
	coremap->entries[pnum].st = DIRTY;

	if(DEBUGP) kprintf("PAGE_ALLOC: returning %08x\n", PADDR_TO_KVADDR(coremap->entries[pnum].pa));
	return PADDR_TO_KVADDR(coremap->entries[pnum].pa);

}

void
free_kpages(vaddr_t addr)
{
	unsigned int i;
	int zero = 0;
	struct tlbshootdown	ts;

	if(DEBUGP) kprintf("FREE_KPAGES: acquire lock\n");
	lock_acquire(cm_lock);

	for(i = 0; i < coremap->size; i++){
		if(coremap->entries[i].va == addr){
			coremap->entries[i].st = FREE;

			if(coremap->entries[i].as != NULL){
				coremap->entries[i].as = NULL;	
	
				if((ts.ts_placeholder = tlb_probe(coremap->entries[i].va, zero)) >= 0)
					vm_tlbshootdown(&ts);
			}

			break;
		}
	}

	if(DEBUGP) kprintf("FREE_KPAGES: release lock\n");
	lock_release(cm_lock);
}

void
vm_tlbshootdown_all(void)
{
	struct tlbshootdown ts;
	for(int i = 0; i < 64; i++) {
		ts.ts_placeholder = i;
		vm_tlbshootdown(&ts);
	}
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	unsigned int elo, ehi;
	tlb_read(&ehi, &elo, ts->ts_placeholder);
	elo &= ~TLBLO_VALID;
	tlb_write(ehi, elo, ts->ts_placeholder);
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i; 
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	if(DEBUGP)kprintf("VM_FAULT: entered (PID = %d) fault address: %08x\n", curthread->t_proc->p_id, faultaddress);

	faultaddress &= PAGE_FRAME;

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		panic("dumbvm: got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		kprintf("VM_FAULT: curproc == NULL\n");
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		
		ehi = faultaddress;
		//kprintf("something else\n");
		for(i = 0; (unsigned) i < coremap->size; i++) {
			//kprintf("VM_FAULT: va: %08x fault: %08x\n", coremap->entries[i].pa, (faultaddress & UPPERTWENTYM));
			if(coremap->entries[i].va == (faultaddress & UPPERTWENTYM)) {
				//this is the right entry
				
				elo = coremap->entries[i].pa | TLBLO_DIRTY | TLBLO_VALID;
				tlb_random(ehi, elo);
				//kprintf("something\n");
				return 0;
			}
		}
		kprintf("VM_FAULT: as == NULL\n");
		return EFAULT;
	}else{
		/* Assert that the address space has been set up properly. */
		KASSERT(as->as_vbase1 != 0);
		KASSERT(as->as_pbase1 != 0);
		KASSERT(as->as_npages1 != 0);
		KASSERT(as->as_vbase2 != 0);
		KASSERT(as->as_pbase2 != 0);
		KASSERT(as->as_npages2 != 0);
		KASSERT(as->as_stackpbase != 0);
		KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
		KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
		KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
		KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
		KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

		vbase1 = as->as_vbase1;
		vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
		vbase2 = as->as_vbase2;
		vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
		stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
		stacktop = USERSTACK;

		if (faultaddress >= vbase1 && faultaddress < vtop1) {
			if(DEBUGP) kprintf("VM_FAULT: base1 %08x %08x %08x \n", faultaddress, vbase1, as->as_pbase1);
			paddr = (faultaddress - vbase1) + as->as_pbase1;
		}
		else if (faultaddress >= vbase2 && faultaddress < vtop2) {
			if(DEBUGP) kprintf("VM_FAULT: base2\n");
			paddr = (faultaddress - vbase2) + as->as_pbase2;
		}
		else if (faultaddress >= stackbase && faultaddress < stacktop) {
			if(DEBUGP) kprintf("VM_FAULT: stack\n");
			paddr = (faultaddress - stackbase) + as->as_stackpbase;
		}
		else {
			return EFAULT;
		}

		/* make sure it's page-aligned */
		KASSERT((paddr & PAGE_FRAME) == paddr);
	}
	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	if(DEBUGP) kprintf("VM_FAULT: finding page table entry\n");
	//see if the page is in the page table
	for(i = 0; i < (int)PTABLESIZE; i++) {
		if(as->ptable[i].va == faultaddress) {
			if(DEBUGP) kprintf("VM_FAULT: vaddr in ptable\n");
			//make sure the page is in the coremap
			for(unsigned int j = 0; j < coremap->size; j++) {
				if(coremap->entries[j].as == as && coremap->entries[j].va == faultaddress) {
					if(DEBUGP) kprintf("VM_FAULT: found in coremap, adding to tlb\n");
					ehi = faultaddress;
					elo = as->ptable[i].pa | TLBLO_DIRTY | TLBLO_VALID;
					tlb_random(ehi, elo);
					splx(spl);
					return 0;
				}
			}
			if(DEBUGP) kprintf("need to swap it back in\n");
			//TODO: swap in page and update tlb
			//make as dirty

		}
	}
	/*if(DEBUGP)*/ if(DEBUGP) kprintf("VM_FAULT: finding new table entry\n");


	//find a place to put the new page
	for(i = 0; i < (int) PTABLESIZE; i++) {
		if(!as->ptable[i].valid) {
			if(DEBUGP) kprintf("VM_FAULT: found invalid entry: %d\n", i);
			as->ptable[i].valid = 1;
			as->ptable[i].va = faultaddress;

			if(DEBUGP) kprintf("VM_FAULT: starting for\n");
			//find an empty coremap entry
			for(unsigned int j = 1; j < coremap->size; j++) {
				if(coremap->entries[j].st == FREE) {
					if(DEBUGP) kprintf("VM_FAULT: found free entry - j: %d\n", j);
					coremap->entries[j].as = curproc->p_addrspace;
					coremap->entries[j].st = DIRTY;
					coremap->entries[j].va = faultaddress;
					as->ptable[i].pa = coremap->entries[j].pa;

					ehi = faultaddress;
					elo = as->ptable[i].pa | TLBLO_DIRTY | TLBLO_VALID;
					if(DEBUGP) kprintf("VM_FAULT: setting TLB - hi: %08x pa: %08x paddr: %08x\n", ehi, as->ptable[i].pa, paddr);
					tlb_random(ehi, elo);
					splx(spl);
					return 0;
				}
			}

			//find a coremap entry for a different proc
			for(unsigned int j = 0; j < coremap->size; j++) {
				if(coremap->entries[j].as != as) {
					//TODO: swap out this frame
				

					coremap->entries[j].as = curproc->p_addrspace;
					coremap->entries[j].st = DIRTY;
					coremap->entries[j].va = faultaddress;
					as->ptable[i].pa = coremap->entries[j].pa;

					ehi = faultaddress;
					elo = as->ptable[i].pa | TLBLO_DIRTY | TLBLO_VALID;
					tlb_random(ehi, elo);
					splx(spl);
					return 0;
				}
			}
		}
	}


	kprintf("VA_FAULT: page table ran out of entries\n");


        for (i=0; i<NUM_TLB; i++) {
                tlb_read(&ehi, &elo, i);
                if (elo & TLBLO_VALID) {
                        continue;
                }
                ehi = faultaddress;
                elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
                if(DEBUGP) kprintf("dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
                DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
                tlb_write(ehi, elo, i);
                splx(spl);
                return 0;
        }
        kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
        splx(spl);
        return EFAULT;

}

struct addrspace *
as_create(void)
{
	if(DEBUGP) kprintf("AS_CREATE: starting\n");
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		kprintf("AS_CREATE: as==NULL\n"); 
		return NULL;
	}

	//kprintf("DUMBVM: set creation variables;\n");
	as->as_vbase1 = 0;
	as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_pbase2 = 0;
	as->as_npages2 = 0;
	as->as_stackpbase = 0;

	for(unsigned int i = 0; i < PTABLESIZE; i++) {
		as->ptable[i].va = 0xDEADBEEF;
		as->ptable[i].pa = 0xDEADBEEF;
		as->ptable[i].valid = 0;
	}

	return as;

/*
	//kprintf("DUMBVM: malloc - size: %d\n", sizeof(struct addrspace));
	kprintf("AS_CREATE: entered\n");
	struct addrspace *as = kmalloc(sizeof(struct addrspace));

	if (as==NULL) {
		kprintf("AS_CREATE: as == NULL\n");
		return NULL;
	}

	//TODO figure out addresses
	as->as_heapstart = 0;
	as->as_heapend = 0;
	as->as_stackpbase = USERSTACK;

	as->pages = kmalloc(sizeof(int) * max_pages);
	memset(as->pages, 0, sizeof(int) * max_pages);

	return as;
*/
}

void
as_destroy(struct addrspace *as)
{
	if(DEBUGP) kprintf("AS_DESTROY: starting\n");
	
	dumbvm_can_sleep();
	kfree(as);
}

void
as_activate(void)
{
	//kprintf("AS_ACTIVATE: starting\n");
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();
	vm_tlbshootdown_all();
	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	
	if(DEBUGP) kprintf("AS_DEACTIVATE: starting\n");
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	if(DEBUGP) kprintf("AS_DEFINE_REGION: starting\n");
	size_t npages;

	dumbvm_can_sleep();

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return ENOSYS;
}

static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	if(DEBUGP) kprintf("AS_ZERO_REGION %08x %08x\n", (unsigned)PADDR_TO_KVADDR(paddr), (unsigned)&npages);
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
	if(DEBUGP) kprintf("AS_ZERO_REGION done\n");
}

int
as_prepare_load(struct addrspace *as)
{
	if(DEBUGP) kprintf("AS_PREP_LOAD: starting\n");
	KASSERT(as->as_pbase1 == 0);
	KASSERT(as->as_pbase2 == 0);
	KASSERT(as->as_stackpbase == 0);

	dumbvm_can_sleep();

	as->as_pbase1 = KVADDR_TO_PADDR(alloc_kpages(as->as_npages1));
	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}
	if(DEBUGP) kprintf("AS_PREP_LOAD: base1: %08x\n", (unsigned) as->as_pbase1);
	as->as_pbase2 = KVADDR_TO_PADDR(alloc_kpages(as->as_npages2));
	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}

	if(DEBUGP) kprintf("AS_PREP_LOAD: stackbase\n");
	as->as_stackpbase = KVADDR_TO_PADDR(alloc_kpages(DUMBVM_STACKPAGES));
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}

	if(DEBUGP) kprintf("AS_PREP_LOAD: zero 1 %08x %08x %08x\n", (unsigned)as, (unsigned)&as->as_pbase1, (unsigned)&as->as_npages1);
	as_zero_region(as->as_pbase1, as->as_npages1);
	if(DEBUGP) kprintf("AS_PREP_LOAD: zero 2\n");
	as_zero_region(as->as_pbase2, as->as_npages2);
	if(DEBUGP) kprintf("AS_PREP_LOAD: zero 3\n");
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);
	if(DEBUGP) kprintf("AS_PREP_LOAD: zero 4\n");

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	if(DEBUGP) kprintf("AS_COMPLETE_LOAD: starting\n");
	dumbvm_can_sleep();
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	if(DEBUGP) kprintf("AS_DEFINE_STACK: starting\n");
	KASSERT(as->as_stackpbase != 0);

	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	if(DEBUGP) kprintf("AS_COPY: starting\n");
	struct addrspace *new;

	dumbvm_can_sleep();

	//kprintf("DUMBVM: as_create();\n");
	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	//kprintf("DUMBVM: copy vars;\n");
	//TODO change vars
	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	//kprintf("DUMBVM: as_prepare_load(new);\n");
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	KASSERT(new->as_pbase1 != 0);
	KASSERT(new->as_pbase2 != 0);
	KASSERT(new->as_stackpbase != 0);

	//kprintf("DUMBVM: move pbase1;\n");
	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	//kprintf("DUMBVM: move pbase2;\n");
	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	//kprintf("DUMBVM: move stackpbase;\n");
	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);

	*ret = new;
	return 0;
}
