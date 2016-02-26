#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <synch.h>
#include <thread.h>
#include <current.h>
#include <clock.h>
#include <test.h>

#define NAMESTRING "some-silly-name"

static struct lock* makelock() {
	struct lock* lock;

	lock = lock_create(NAMESTRING);
	if (lock == NULL) {
		panic("asst1: whoops: lock_create failed\n");
	}
	return lock;
}



static void ok(void) {
	kprintf("Test passed; now cleaning up.\n");
}



static bool spinlock_not_held(struct spinlock *splk) {
	return splk->splk_holder == NULL;
}


/* =============
   =   LOCKS   =
   =============
*/


#define NAMESTRING "some-silly-name"

////////////////////////////////////////////////////////////
// support code

static unsigned waiters_running = 0;
static struct spinlock waiters_lock = SPINLOCK_INITIALIZER;


/*
 * A thread that just waits on a lock.
 */
static
void
waiter(void *vlock, unsigned long junk)
{
	struct lock *lock = vlock;
	(void)junk;

	lock_acquire(lock);

	spinlock_acquire(&waiters_lock);
	KASSERT(waiters_running > 0);
	waiters_running--;
	spinlock_release(&waiters_lock);
}

/*
 * Set up a waiter.
 */
static
void
makewaiter(struct lock *lock)
{
	int result;

	spinlock_acquire(&waiters_lock);
	waiters_running++;
	spinlock_release(&waiters_lock);

	result = thread_fork("lockunit waiter", NULL, waiter, lock, 0);
	if (result) {
		panic("lockunit: thread_fork failed\n");
	}
	kprintf("Sleeping for waiter to run\n");
	clocksleep(1);
}

////////////////////////////////////////////////////////////
// tests

/*
 * 1. After a successful lock_create:
 *     - lk_name compares equal to the passed-in name
 *     - lk_name is not the same pointer as the passed-in name
 *     - lock_wchan is not null
 *     - lock_lock is not held and has no owner
 *     - lock_count is the passed-in count
 */
int
locku1(int nargs, char **args)
{
	struct lock *lock;
	const char *name = NAMESTRING;

	(void)nargs; (void)args;

	lock = lock_create(name);
	if (lock == NULL) {
		panic("locku1: whoops: lock_create failed\n");
	}
	KASSERT(!strcmp(lock->lk_name, name));
	KASSERT(lock->lk_name != name);
	KASSERT(lock->lock_wchan != NULL);
	KASSERT(spinlock_not_held(&lock->lock_lock));
	KASSERT(lock->lock_count == 1);

	ok();
	/* clean up */
	lock_destroy(lock);
	return 0;
}

/*
 * 5. A lock can be successfully initialized with a count of at
 * least 0xf0000000.
 */
int
locku5(int nargs, char **args)
{
	struct lock *lock;

	(void)nargs; (void)args;

	lock = lock_create(NAMESTRING);
	if (lock == NULL) {
		/* This might not be an innocuous malloc shortage. */
		panic("locku5: lock_create failed\n");
	}
	KASSERT(lock->lock_count == 1);

	/* Clean up. */
	ok();
	lock_destroy(lock);
	return 0;
}

/*
 * 6. Passing a lock with a waiting thread to lock_destroy asserts
 * (in the wchan code).
 */
int
locku6(int nargs, char **args)
{
	struct lock *lock;

	(void)nargs; (void)args;

	lock = makelock();
	makewaiter(lock);
	kprintf("This should assert that the wchan's threadlist is empty\n");
	lock_destroy(lock);
	panic("locku6: wchan_destroy with waiters succeeded\n");
	return 0;
}

/*
 * 7. Calling V on a lock does not block the caller, regardless
 * of the lock count.
 */
int
locku7(int nargs, char **args)
{
	struct lock *lock;
	struct spinlock lk;

	(void)nargs; (void)args;

	lock = makelock();

	/*
	 * Check for blocking by taking a spinlock; if we block while
	 * holding a spinlock, wchan_sleep will assert.
	 */
	spinlock_init(&lk);
	spinlock_acquire(&lk);

	/* try with count 0, count 1, and count 2, just for completeness */
	lock_release(lock);
	lock_release(lock);
	lock_release(lock);

	/* Clean up. */
	ok();
	spinlock_release(&lk);
	spinlock_cleanup(&lk);
	lock_destroy(lock);
	return 0;
}

/*
 * 8/9. After calling V on a lock with no threads waiting:
 *    - lk_name is unchanged
 *    - lock_wchan is unchanged
 *    - lock_lock is (still) unheld and has no owner
 *    - lock_count is increased by one
 *
 * This is true even if we are in an interrupt handler.
 */
static
void
do_locku89(bool interrupthandler)
{
	struct lock *lock;
	struct wchan *wchan;
	const char *name;

	lock = makelock();

	/* check preconditions */
	name = lock->lk_name;
	wchan = lock->lock_wchan;
	KASSERT(!strcmp(name, NAMESTRING));
	KASSERT(spinlock_not_held(&lock->lock_lock));

	/*
	 * The right way to this is to set up an actual interrupt,
	 * e.g. an interprocessor interrupt, and hook onto it to run
	 * the lock_release() in the actual interrupt handler. However, that
	 * requires a good bit of infrastructure that we don't
	 * have. Instead we'll fake it by explicitly setting
	 * curthread->t_in_interrupt.
	 */
	if (interrupthandler) {
		KASSERT(curthread->t_in_interrupt == false);
		curthread->t_in_interrupt = true;
	}

	lock_release(lock);

	if (interrupthandler) {
		KASSERT(curthread->t_in_interrupt == true);
		curthread->t_in_interrupt = false;
	}

	/* check postconditions */
	KASSERT(name == lock->lk_name);
	KASSERT(!strcmp(name, NAMESTRING));
	KASSERT(wchan == lock->lock_wchan);
	KASSERT(spinlock_not_held(&lock->lock_lock));
	KASSERT(lock->lock_count == 1);

	/* clean up */
	ok();
	lock_destroy(lock);
}

int
locku8(int nargs, char **args)
{
	(void)nargs; (void)args;

	do_locku89(false /*interrupthandler*/);
	return 0;
}

int
locku9(int nargs, char **args)
{
	(void)nargs; (void)args;

	do_locku89(true /*interrupthandler*/);
	return 0;
}

/*
 * 10/11. After calling V on a lock with one thread waiting, and giving
 * it time to run:
 *    - lk_name is unchanged
 *    - lock_wchan is unchanged
 *    - lock_lock is (still) unheld and has no owner
 *    - lock_count is still 0
 *    - the other thread does in fact run
 *
 * This is true even if we are in an interrupt handler.
 */
static
int
do_locku1011(bool interrupthandler)
{
	struct lock *lock;
	struct wchan *wchan;
	const char *name;

	lock = makelock();
	makewaiter(lock);

	/* check preconditions */
	name = lock->lk_name;
	wchan = lock->lock_wchan;
	KASSERT(!strcmp(name, NAMESTRING));
	KASSERT(spinlock_not_held(&lock->lock_lock));
	spinlock_acquire(&waiters_lock);
	KASSERT(waiters_running == 1);
	spinlock_release(&waiters_lock);

	/* see above */
	if (interrupthandler) {
		KASSERT(curthread->t_in_interrupt == false);
		curthread->t_in_interrupt = true;
	}

	lock_release(lock);

	if (interrupthandler) {
		KASSERT(curthread->t_in_interrupt == true);
		curthread->t_in_interrupt = false;
	}

	/* give the waiter time to exit */
	clocksleep(1);

	/* check postconditions */
	KASSERT(name == lock->lk_name);
	KASSERT(!strcmp(name, NAMESTRING));
	KASSERT(wchan == lock->lock_wchan);
	KASSERT(spinlock_not_held(&lock->lock_lock));
	KASSERT(lock->lock_count == 0);
	spinlock_acquire(&waiters_lock);
	KASSERT(waiters_running == 0);
	spinlock_release(&waiters_lock);

	/* clean up */
	ok();
	lock_destroy(lock);
	return 0;

}

int
locku10(int nargs, char **args)
{
	(void)nargs; (void)args;

	do_locku1011(false /*interrupthandler*/);
	return 0;
}

int
locku11(int nargs, char **args)
{
	(void)nargs; (void)args;

	do_locku1011(true /*interrupthandler*/);
	return 0;
}


/*
 * 12/13. After calling V on a lock with two threads waiting, and
 * giving it time to run:
 *    - lk_name is unchanged
 *    - lock_wchan is unchanged
 *    - lock_lock is (still) unheld and has no owner
 *    - lock_count is still 0
 *    - one of the other threads does in fact run
 *    - the other one does not
 */
static
void
locku1213(bool interrupthandler)
{
	struct lock *lock;
	struct wchan *wchan;
	const char *name;

	lock = makelock();
	makewaiter(lock);
	makewaiter(lock);

	/* check preconditions */
	name = lock->lk_name;
	wchan = lock->lock_wchan;
	KASSERT(!strcmp(name, NAMESTRING));
	wchan = lock->lock_wchan;
	KASSERT(spinlock_not_held(&lock->lock_lock));
	spinlock_acquire(&waiters_lock);
	KASSERT(waiters_running == 2);
	spinlock_release(&waiters_lock);

	/* see above */
	if (interrupthandler) {
		KASSERT(curthread->t_in_interrupt == false);
		curthread->t_in_interrupt = true;
	}

	lock_release(lock);

	if (interrupthandler) {
		KASSERT(curthread->t_in_interrupt == true);
		curthread->t_in_interrupt = false;
	}

	/* give the waiter time to exit */
	clocksleep(1);

	/* check postconditions */
	KASSERT(name == lock->lk_name);
	KASSERT(!strcmp(name, NAMESTRING));
	KASSERT(wchan == lock->lock_wchan);
	KASSERT(spinlock_not_held(&lock->lock_lock));
	KASSERT(lock->lock_count == 0);
	spinlock_acquire(&waiters_lock);
	KASSERT(waiters_running == 1);
	spinlock_release(&waiters_lock);

	/* clean up */
	ok();
	lock_release(lock);
	clocksleep(1);
	spinlock_acquire(&waiters_lock);
	KASSERT(waiters_running == 0);
	spinlock_release(&waiters_lock);
	lock_destroy(lock);
}

int
locku12(int nargs, char **args)
{
	(void)nargs; (void)args;

	locku1213(false /*interrupthandler*/);
	return 0;
}

int
locku13(int nargs, char **args)
{
	(void)nargs; (void)args;

	locku1213(true /*interrupthandler*/);
	return 0;
}

/*
 * 14. Calling V on a lock whose count is the maximum allowed value
 * asserts.
 */
int
locku14(int nargs, char **args)
{
	struct lock *lock;

	(void)nargs; (void)args;

	kprintf("This should assert that lock_count is > 0.\n");
	lock = makelock();

	/*
	 * The maximum value is (unsigned)-1. Get this by decrementing
	 * from 0.
	 */
	lock->lock_count--;
	lock_release(lock);
	KASSERT(lock->lock_count == 0);
	panic("locku14: V tolerated count wraparound\n");
	return 0;
}

/*
 * 15. Calling V on a null lock asserts.
 */
int
locku15(int nargs, char **args)
{
	(void)nargs; (void)args;

	kprintf("This should assert that the lock isn't null.\n");
	lock_release(NULL);
	panic("locku15: V tolerated null lock\n");
	return 0;
}

/*
 * 16. Calling P on a lock with count > 0 does not block the caller.
 */
int
locku16(int nargs, char **args)
{
	struct lock *lock;
	struct spinlock lk;

	(void)nargs; (void)args;

	lock = makelock();

	/* As above, check for improper blocking by taking a spinlock. */
	spinlock_init(&lk);
	spinlock_acquire(&lk);

	lock_acquire(lock);

	ok();
	spinlock_release(&lk);
	spinlock_cleanup(&lk);
	lock_destroy(lock);
	return 0;
}

/*
 * 17. Calling P on a lock with count == 0 does block the caller.
 */

static struct thread *locku17_thread;

static
void
locku17_sub(void *lockv, unsigned long junk)
{
	struct lock *lock = lockv;

	(void)junk;

	locku17_thread = curthread;

	/* precondition */
	KASSERT(lock->lock_count == 0);

	lock_acquire(lock);
}

int
locku17(int nargs, char **args)
{
	struct lock *lock;
	int result;

	(void)nargs; (void)args;

	locku17_thread = NULL;

	lock = makelock();
	result = thread_fork("locku17_sub", NULL, locku17_sub, lock, 0);
	if (result) {
		panic("locku17: whoops: thread_fork failed\n");
	}
	kprintf("Waiting for subthread...\n");
	clocksleep(1);

	/* The subthread should be blocked. */
	KASSERT(locku17_thread != NULL);
	KASSERT(locku17_thread->t_state == S_SLEEP);

	/* Clean up. */
	ok();
	lock_release(lock);
	clocksleep(1);
	lock_destroy(lock);
	locku17_thread = NULL;
	return 0;
}

/*
 * 18. After calling P on a lock with count > 0:
 *    - lk_name is unchanged
 *    - lock_wchan is unchanged
 *    - lock_lock is unheld and has no owner
 *    - lock_count is one less
 */
int
locku18(int nargs, char **args)
{
	struct lock *lock;
	struct wchan *wchan;
	const char *name;

	(void)nargs; (void)args;

	lock = makelock();

	/* preconditions */
	name = lock->lk_name;
	KASSERT(!strcmp(name, NAMESTRING));
	wchan = lock->lock_wchan;
	KASSERT(spinlock_not_held(&lock->lock_lock));
	KASSERT(lock->lock_count == 1);

	lock_acquire(lock);
	
	/* postconditions */
	KASSERT(name == lock->lk_name);
	KASSERT(!strcmp(name, NAMESTRING));
	KASSERT(wchan == lock->lock_wchan);
	KASSERT(spinlock_not_held(&lock->lock_lock));
	KASSERT(lock->lock_count == 0);

	return 0;
}

/*
 * 19. After calling P on a lock with count == 0 and another
 * thread uses V exactly once to cause a wakeup:
 *    - lk_name is unchanged
 *    - lock_wchan is unchanged
 *    - lock_lock is unheld and has no owner
 *    - lock_count is still 0
 */

static
void
locku19_sub(void *lockv,  unsigned long junk)
{
	struct lock *lock = lockv;

	(void)junk;

	kprintf("locku19: waiting for parent to sleep\n");
	clocksleep(1);
	/*
	 * We could assert here that the parent *is* sleeping; but for
	 * that we'd need its thread pointer and it's not worth the
	 * trouble.
	 */
	lock_release(lock);
}

int
locku19(int nargs, char **args)
{
	struct lock *lock;
	struct wchan *wchan;
	const char *name;
	int result;

	(void)nargs; (void)args;

	lock = makelock();
	result = thread_fork("locku19_sub", NULL, locku19_sub, lock, 0);
	if (result) {
		panic("locku19: whoops: thread_fork failed\n");
	}

	/* preconditions */
	name = lock->lk_name;
	KASSERT(!strcmp(name, NAMESTRING));
	wchan = lock->lock_wchan;
	KASSERT(spinlock_not_held(&lock->lock_lock));
	KASSERT(lock->lock_count == 0);

	lock_acquire(lock);

	/* postconditions */
	KASSERT(name == lock->lk_name);
	KASSERT(!strcmp(name, NAMESTRING));
	KASSERT(wchan == lock->lock_wchan);
	KASSERT(spinlock_not_held(&lock->lock_lock));
	KASSERT(lock->lock_count == 0);

	return 0;
}

/*
 * 20/21. Calling P in an interrupt handler asserts, regardless of the
 * count.
 */
int
locku20(int nargs, char **args)
{
	struct lock *lock;

	(void)nargs; (void)args;

	kprintf("This should assert that we aren't in an interrupt\n");

	lock = makelock();
	/* as above */
	curthread->t_in_interrupt = true;
	lock_acquire(lock);
	panic("locku20: P tolerated being in an interrupt handler\n");
	return 0;
}

int
locku21(int nargs, char **args)
{
	struct lock *lock;

	(void)nargs; (void)args;

	kprintf("This should assert that we aren't in an interrupt\n");

	lock = makelock();
	/* as above */
	curthread->t_in_interrupt = true;
	lock_acquire(lock);
	panic("locku21: P tolerated being in an interrupt handler\n");
	return 0;
}

/*
 * 22. Calling P on a null lock asserts.
 */
int
locku22(int nargs, char **args)
{
	(void)nargs; (void)args;

	kprintf("This should assert that the lock isn't null.\n");
	lock_acquire(NULL);
	panic("locku22: P tolerated null lock\n");
	return 0;
}














































int asst1_tests(int nargs , char ** args){
  int err;
	int ret;
  int i;
	int (*tests[22])();

	tests[0] = locku1;
	tests[1] = locku5;
	tests[2] = locku5;
	tests[3] = locku6;
	tests[4] = locku7;
	tests[5] = locku8;
	tests[6] = locku9;
	tests[7] = locku10;
	tests[8] = locku11;
	tests[9] = locku12;

  kprintf("Args %d\n",nargs);
  for (i=0;i<nargs;i++)kprintf("Arg[%d] <%s>\n",i,args[i]);
	
	for(i = 0; i < 10; i++){
		if(i == 1 || i == 3) continue;
		kprintf("Starting test %d\n", i + 1);
		ret = tests[i]();
		err += ret;
		kprintf("Test %d complete with status %d\n", i + 1, ret);
	}

  kprintf("Tests complete with status %d\n",err);
  kprintf("UNIT tests complete\n");
  kprintf("Number of errors: %d\n", err);
  return(err);
}

