#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <synch.h>
#include <thread.h>
#include <current.h>
#include <clock.h>
#include <test.h>


static unsigned waiters_running = 0;
static struct spinlock waiters_lock = SPINLOCK_INITIALIZER;

static void ok(void) {
	kprintf("Test complete; now cleaning up.\n");
}
/*
static bool spinlock_not_held(struct spinlock *splk) {
	return splk->splk_holder == NULL;
}
*/

/* =============
   =   LOCKS   =
   =============
*/

#define NAMESTRING "LOCK-TESTS"

static struct lock* makelock() {
	struct lock* lock;

	lock = lock_create(NAMESTRING);
	if (lock == NULL) {
		panic("asst1: whoops: lock_create failed\n");
	}
	return lock;
}

//A thread that just waits on a lock.
static void waiter(struct lock* lock) {
	lock_acquire(lock);

	spinlock_acquire(&waiters_lock);
	KASSERT(waiters_running > 0);
	waiters_running--;
	spinlock_release(&waiters_lock);

	lock_release(lock);
}


// Set up a waiter
static void makewaiter(struct lock* lock) {
	int result;

	spinlock_acquire(&waiters_lock);
	waiters_running++;
	spinlock_release(&waiters_lock);

	result = thread_fork("lock waiter", NULL, (void*) waiter, lock, 0);
	if (result) {
		panic("lockunit: thread_fork failed\n");
	}
	kprintf("Sleeping for waiter to run\n");
	clocksleep(1);
}


//Lock can be acquired and released with no issues
int locku1(){

	struct lock* lock = makelock();

	lock_acquire(lock);
	lock_release(lock);
	
	ok();
	lock_destroy(lock);
	return 0;
}

//Lock will be waited on if one process is using it
int locku2(){

	struct lock* lock = makelock();
	lock_acquire(lock);

	makewaiter(lock);	

	KASSERT(waiters_running == 1);

	lock_release(lock);

	ok();
	lock_destroy(lock);
	return 0;
}

//Lock can be acquired and released multiple times
int locku3(){
	kprintf("This will hang if the test fails.\n");

	struct lock* lock = makelock();

	lock_acquire(lock);
	lock_release(lock);

	lock_acquire(lock);
	lock_release(lock);
	
	ok();
	lock_destroy(lock);
	return 0;
}

//Unowned lock cannot be released
int locku4(){
	kprintf("This should cause a KASSERT failure.\n");

	struct lock* lock = makelock();

	lock_release(lock);

	ok();
	lock_destroy(lock);
	return 0;
}






/* ==========
   =   CV   =
   ==========
*/

#define CVNAMESTRING "CV-TESTS"
struct lock* cvGlobalLock;

//A thread that just waits on a lock.
static void cvwaiter(struct cv* cv) {
	lock_acquire(cvGlobalLock);
	cv_wait(cv, cvGlobalLock);

	spinlock_acquire(&waiters_lock);
	KASSERT(waiters_running > 0);
	waiters_running--;
	spinlock_release(&waiters_lock);

	lock_release(cvGlobalLock);

	kprintf("CV Waiter complete.\n");
}


// Set up a waiter
static void makecvwaiter(struct cv* cv) {
	int result;

	spinlock_acquire(&waiters_lock);
	waiters_running++;
	spinlock_release(&waiters_lock);

	result = thread_fork("cv waiter", NULL, (void*) cvwaiter, cv, 0);
	if (result) {
		panic("cvunit: thread_fork failed\n");
	}
	kprintf("Sleeping for cvwaiter to run\n");
	clocksleep(1);
}

static struct cv* makecv() {
	struct cv* cv;

	cv = cv_create(CVNAMESTRING);
	if (cv == NULL) {
		panic("asst1: whoops: cv_create failed\n");
	}
	return cv;
}



//Verify that you can signal a cv
int cvu1(){
	kprintf("Waiter should print when signalled.\n");
	struct cv* cv = makecv();
	cvGlobalLock = makelock();

	lock_acquire(cvGlobalLock);
	makecvwaiter(cv);
	cv_signal(cv, cvGlobalLock);
	lock_release(cvGlobalLock);

	lock_destroy(cvGlobalLock);
	ok();
	return 0;
}

//Verify that cv's will wait until signalled, and only one is released at a time
int cvu2(){
	struct cv* cv = makecv();
	cvGlobalLock = makelock();

	makecvwaiter(cv);
	makecvwaiter(cv);
	makecvwaiter(cv);

	lock_acquire(cvGlobalLock);
	kprintf("First signal.\n");
	cv_signal(cv, cvGlobalLock);
	clocksleep(1);
	kprintf("Second signal.\n");
	cv_signal(cv, cvGlobalLock);
	clocksleep(1);
	kprintf("Third signal.\n");
	cv_signal(cv, cvGlobalLock);
	clocksleep(1);
	lock_release(cvGlobalLock);

	lock_destroy(cvGlobalLock);
	ok();
	return 0;
}

//cv_broadcast releases all waiters
int cvu3(){
	struct cv* cv = makecv();
	cvGlobalLock = makelock();

	makecvwaiter(cv);
	makecvwaiter(cv);
	makecvwaiter(cv);

	kprintf("Three CV's should be signalled.\n");
	
	cv_broadcast(cv, cvGlobalLock);
	
	lock_destroy(cvGlobalLock);
	ok();
	return 0;
}

//Verify that signals only release currently waiting cv's
int cvu4(){
	int i;
	struct cv* cv = makecv();
	cvGlobalLock = makelock();

	cv_signal(cv, cvGlobalLock);
	makecvwaiter(cv);
	kprintf("There should be a delay before the waiter completes.\n");
	for(i = 0; i < 1000000; i++);
	cv_signal(cv, cvGlobalLock);
	
	lock_destroy(cvGlobalLock);
	ok();
	return 0;
}




/* ===============
   =   RWLOCKS   =
   ===============
*/

#define RWNAMESTRING "RWLOCK-TESTS"
/*
static struct rwlock* makerwlock() {
	struct rwlock* rwlock;

	rwlock = rwlock_create(NAMESTRING);
	if (rwlock == NULL) {
		panic("asst1: whoops: lock_create failed\n");
	}
	return rwlock;
}
*/






























int asst1_tests(int nargs , char ** args){
  int err = 0;
	int ret;
  int i;
	int (*locktests[4])();
	//int (*rwlocktests[9])();
	int (*cvtests[4])();

	locktests[0] = locku1;
	locktests[1] = locku2;
	locktests[2] = locku3;
	locktests[3] = locku4;

	cvtests[0] = cvu1;
	cvtests[1] = cvu2;
	cvtests[2] = cvu3;
	cvtests[3] = cvu4;

/*
	rwlocktests[0] = rwlocku1;
	rwlocktests[1] = rwlocku2;
	rwlocktests[2] = rwlocku3;
	rwlocktests[3] = rwlocku4;
	rwlocktests[6] = rwlocku7;
	rwlocktests[7] = rwlocku8;
	rwlocktests[8] = rwlocku9;
*/


  kprintf("Args %d\n",nargs);
  for (i=0;i<nargs;i++)kprintf("Arg[%d] <%s>\n",i,args[i]);
	

	kprintf("-- LOCK TESTS --\n");

	for(i = 0; i < 4; i++){
		kprintf("Starting lock test %d\n", i + 1);
		ret = locktests[i]();
		err += ret;
		kprintf("Test %d complete with status %d\n", i + 1, ret);
	}

	kprintf("-- CV TESTS --\n");

	for(i = 0; i < 4; i++){
		kprintf("Starting cv test %d\n", i + 1);
		ret = cvtests[i]();
		err += ret;
		kprintf("Test %d complete with status %d\n", i + 1, ret);
	}
/*
	kprintf("-- RWLOCK TESTS --\n");

	for(i = 0; i < 9; i++){
		kprintf("Starting rwlock test %d\n", i + 1);
		ret = tests[i]();
		err += ret;
		kprintf("Test %d complete with status %d\n", i + 1, ret);
	}

*/

  kprintf("Tests complete with status %d\n",err);
  kprintf("UNIT tests complete\n");
  kprintf("Number of errors: %d\n", err);
  return(err);
}

