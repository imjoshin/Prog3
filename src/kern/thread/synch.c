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
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <unistd.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
        struct semaphore *sem;

        sem = kmalloc(sizeof(*sem));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(*lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }

	lock->lock_wchan = wchan_create(lock->lk_name);
	if (lock->lock_wchan == NULL) {
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}

	spinlock_init(&lock->lock_lock);
        lock->lock_count = 1;

        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

        // add stuff here as needed
    	spinlock_cleanup(&lock->lock_lock);
	    wchan_destroy(lock->lock_wchan);

        kfree(lock->lk_name);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
        KASSERT(lock != NULL);
        KASSERT(curthread->t_in_interrupt == false);

	/* Use the lock spinlock to protect the wchan as well. */
	spinlock_acquire(&lock->lock_lock);
        while (lock->lock_count == 0) {
		    wchan_sleep(lock->lock_wchan, &lock->lock_lock);
        }
        KASSERT(lock->lock_count > 0);
        lock->lock_count--;
		lock->lock_holder = curcpu->c_self;
	spinlock_release(&lock->lock_lock);
}

void
lock_release(struct lock *lock)
{
        KASSERT(lock != NULL);
        KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&lock->lock_lock);

        lock->lock_count++;
        KASSERT(lock->lock_count > 0);
	wchan_wakeone(lock->lock_wchan, &lock->lock_lock);

	spinlock_release(&lock->lock_lock);
}

bool
lock_do_i_hold(struct lock *lock)
{
        if(!CURCPU_EXISTS()){
			return true;
		}

		return (lock->lock_holder == curcpu->c_self);
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(*cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }

    cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

	spinlock_init(&cv->cv_lock);
        cv->cv_count = 0;

        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

    spinlock_cleanup(&cv->cv_lock);
	wchan_destroy(cv->cv_wchan);
        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
        if(lock_do_i_hold(lock)) {
            lock_release(lock);

            spinlock_acquire(&cv->cv_lock);
            cv->cv_count++;
            spinlock_release(&lock->lock_lock);

		    wchan_sleep(cv->cv_wchan, &cv->cv_lock);

            spinlock_acquire(&cv->cv_lock);
            cv->cv_count--;
            spinlock_release(&lock->lock_lock);
            lock_acquire(lock);
        }
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
    if(lock_do_i_hold(lock)) {
       lock_release(lock);
       wchan_wakeone(cv->cv_wchan, &cv->cv_lock);
       lock_acquire(lock);
    }
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
    unsigned int i;
    if(lock_do_i_hold(lock)) {
       lock_release(lock);

        spinlock_acquire(&cv->cv_lock);
        for(i = 0; i < cv->cv_count; i++) {
            wchan_wakeone(cv->cv_wchan, &cv->cv_lock);
        }
        spinlock_release(&lock->lock_lock);

        lock_acquire(lock);
    }
}


struct rwlock* rwlock_create(const char *name) {
        struct rwlock* rwlock;

        rwlock = kmalloc(sizeof(*rwlock));
        if (rwlock == NULL) {
                return NULL;
        }

        rwlock->rwlk_name = kstrdup(name);
        if (rwlock->rwlk_name == NULL) {
                kfree(rwlock);
                return NULL;
        }

	rwlock->rwlock_wchan = wchan_create(rwlock->rwlk_name);
	if (rwlock->rwlock_wchan == NULL) {
		kfree(rwlock->rwlk_name);
		kfree(rwlock);
		return NULL;
	}

	    spinlock_init(&rwlock->rwlock_lock);
        rwlock->reader_count =0;
        rwlock->writer_in = false;
        rwlock->writer_waiting = 0;
        rwlock->writer_pid = -1;
        memset(rwlock->reader_pids, 0xff, sizeof(int) * 1000);
        return rwlock;
}

void rwlock_destroy(struct rwlock *rwlock)
{
        KASSERT(rwlock != NULL);

        // add stuff here as needed
    	spinlock_cleanup(&rwlock->rwlock_lock);
	    wchan_destroy(rwlock->rwlock_wchan);

        kfree(rwlock->rwlk_name);
        kfree(rwlock);
}
void rwlock_acquire(struct rwlock* rwlock, int mode) {
    int i;
    if(mode == READ) {
        spinlock_acquire(&rwlock->rwlock_lock);
        while(rwlock->writer_in || (rwlock->writer_waiting != 0 && rwlock->reader_count != 0)) {
            wchan_sleep(rwlock->rwlock_wchan, &rwlock->rwlock_lock);
        }
        rwlock->reader_count++;
        for(i = 0; rwlock->reader_pids[i] != -1; i++) {}
        rwlock->reader_pids[i] = getpid();
        spinlock_release(&rwlock->rwlock_lock);
    } else if(mode == WRITE) {
        spinlock_acquire(&rwlock->rwlock_lock);
        rwlock->writer_waiting++;
        while(rwlock->reader_count != 0 || rwlock->writer_in) {
            wchan_sleep(rwlock->rwlock_wchan, &rwlock->rwlock_lock);
        }
        rwlock->writer_in = true;
        rwlock->writer_waiting--;
        rwlock->writer_pid = getpid();
        spinlock_release(&rwlock->rwlock_lock);
    } else {
        kprintf("Improper mode!!!!!\n");
    }
}
void rwlock_release(struct rwlock* rwlock, int mode) {
    bool match = false;
    int i;
    KASSERT(rwlock != NULL);
    KASSERT(rwlock_do_i_hold(rwlock));

	spinlock_acquire(&rwlock->rwlock_lock);
    if(mode == READ) {
        for (i = 0; i < 1000; i++) {
            if(rwlock->reader_pids[i] == getpid()) {
                match = true;
                break;
            }
        }
        KASSERT(match);
        rwlock->reader_pids[i] = -1;
        rwlock->reader_count--;
    } else if(mode == WRITE) {
        KASSERT(rwlock->writer_pid == getpid());
        rwlock->writer_pid = -1;
        rwlock->writer_in = false;
    } else {
        kprintf("Improper mode!!!!!\n");
    }

	wchan_wakeone(rwlock->rwlock_wchan, &rwlock->rwlock_lock);
	spinlock_release(&rwlock->rwlock_lock);
}

bool rwlock_do_i_hold(struct rwlock* rwlock) {
    (void)rwlock;
    return true;
}




