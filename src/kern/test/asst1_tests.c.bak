#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <synch.h>
#include <thread.h>
#include <current.h>
#include <clock.h>
#include <test.h>

//Jon Faron, Riley Hirn, Josh Johnson

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

	spinlock_acquire(&waiters_lock);
	waiters_running++;
	spinlock_release(&waiters_lock);

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
	waiters_running = 0;
	kprintf("Waiter should print when signalled.\n");
	struct cv* cv = makecv();
	cvGlobalLock = makelock();

	makecvwaiter(cv);
	clocksleep(1);

	lock_acquire(cvGlobalLock);
	cv_signal(cv, cvGlobalLock);
	lock_release(cvGlobalLock);

	while(waiters_running > 0);
	lock_destroy(cvGlobalLock);
	ok();
	return 0;
}

//Verify that cv's will wait until signalled, and only one is released at a time
int cvu2(){
	waiters_running = 0;
	struct cv* cv = makecv();
	cvGlobalLock = makelock();

	makecvwaiter(cv);
	makecvwaiter(cv);
	makecvwaiter(cv);
	while(waiters_running < 3);

	lock_acquire(cvGlobalLock);
	kprintf("First signal.\n");
	cv_signal(cv, cvGlobalLock);
	lock_release(cvGlobalLock);
	clocksleep(1);

	lock_acquire(cvGlobalLock);
	kprintf("Second signal.\n");
	cv_signal(cv, cvGlobalLock);
	lock_release(cvGlobalLock);
	clocksleep(1);

	lock_acquire(cvGlobalLock);
	kprintf("Third signal.\n");
	cv_signal(cv, cvGlobalLock);
	lock_release(cvGlobalLock);
	clocksleep(1);

	while(waiters_running > 0);
	lock_destroy(cvGlobalLock);
	ok();
	return 0;
}

//cv_broadcast releases all waiters
int cvu3(){
	waiters_running = 0;
	struct cv* cv = makecv();
	cvGlobalLock = makelock();

	makecvwaiter(cv);
	makecvwaiter(cv);
	makecvwaiter(cv);
	while(waiters_running < 3);

	lock_acquire(cvGlobalLock);
	kprintf("Three CV's should be signalled.\n");
	cv_broadcast(cv, cvGlobalLock);
	lock_release(cvGlobalLock);
	
	while(waiters_running > 0);
	lock_destroy(cvGlobalLock);
	ok();
	return 0;
}

//Verify that signals only release currently waiting cv's
int cvu4(){
	waiters_running = 0;
	struct cv* cv = makecv();
	cvGlobalLock = makelock();

	lock_acquire(cvGlobalLock);
	cv_signal(cv, cvGlobalLock);
	lock_release(cvGlobalLock);

	makecvwaiter(cv);

	while(waiters_running < 1);
	kprintf("There should be a delay before the waiter completes.\n");
	clocksleep(5);

	lock_acquire(cvGlobalLock);
	cv_signal(cv, cvGlobalLock);
	lock_release(cvGlobalLock);
	
	while(waiters_running > 0);
	lock_destroy(cvGlobalLock);
	ok();
	return 0;
}




/* ===============
   =   RWLOCKS   =
   ===============
*/

#define RWNAMESTRING "RWLOCK-TESTS"

static struct rwlock* makerwlock() {
	struct rwlock* rwlock;

	rwlock = rwlock_create(RWNAMESTRING);
	if (rwlock == NULL) {
		panic("asst1: whoops: rwlock_create failed\n");
	}
	return rwlock;
}


//A thread that just waits on a lock.
static void readwaiter(struct rwlock* rwlock) {
	rwlock_acquire(rwlock, READ);

	spinlock_acquire(&waiters_lock);
	kprintf("READER: %d processes currently running.\n", waiters_running);
	spinlock_release(&waiters_lock);
	clocksleep(5);
	spinlock_acquire(&waiters_lock);
	KASSERT(waiters_running > 0);
	waiters_running--;
	spinlock_release(&waiters_lock);

	kprintf("Read Waiter complete.\n");
	
	rwlock_release(rwlock, READ);
}

//A thread that just waits on a lock.
static void writewaiter(struct rwlock* rwlock) {
	rwlock_acquire(rwlock, WRITE);

	spinlock_acquire(&waiters_lock);
	kprintf("WRITER: %d processes currently running.\n", waiters_running);
	spinlock_release(&waiters_lock);
	clocksleep(5);
	spinlock_acquire(&waiters_lock);
	KASSERT(waiters_running > 0);
	waiters_running--;
	spinlock_release(&waiters_lock);

	kprintf("Write Waiter complete.\n");
	
	rwlock_release(rwlock, WRITE);
}

// Set up a waiter
static void rwmakewaiter(struct rwlock* rwlock, int mode) {
	int result;

	spinlock_acquire(&waiters_lock);
	waiters_running++;
	spinlock_release(&waiters_lock);

	if(mode == READ){
		result = thread_fork("read lock waiter", NULL, (void*) readwaiter, rwlock, 0);
		if (result) {
			panic("rwlockunit: thread_fork failed\n");
		}
	}else if(mode == WRITE){
		result = thread_fork("write lock waiter", NULL, (void*) writewaiter, rwlock, 0);
		if (result) {
			panic("rwlockunit: thread_fork failed\n");
		}
	}
	clocksleep(1);
}


//Two reads should be able to hold lock at the same time
int rwlocku1(){
	waiters_running = 0;
	struct rwlock* rwlock = makerwlock();

	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, READ);

	while(waiters_running > 0);
	
	ok();
	rwlock_destroy(rwlock);
	return 0;
}

//Two writers should wait for the first to finish
int rwlocku2(){
	waiters_running = 0;
	struct rwlock* rwlock = makerwlock();

	rwmakewaiter(rwlock, WRITE);
	rwmakewaiter(rwlock, WRITE);

	while(waiters_running > 0);
	
	ok();
	rwlock_destroy(rwlock);
	return 0;
}
//writers should wait for readers to finish
int rwlocku3(){
	waiters_running = 0;
	struct rwlock* rwlock = makerwlock();

	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, WRITE);

	while(waiters_running > 0);
	
	ok();
	rwlock_destroy(rwlock);
	return 0;
}
//readers should wait for writers to finish
int rwlocku4(){
	waiters_running = 0;
	struct rwlock* rwlock = makerwlock();

	rwmakewaiter(rwlock, WRITE);
	rwmakewaiter(rwlock, READ);

	while(waiters_running > 0);
	
	ok();
	rwlock_destroy(rwlock);
	return 0;
}
//writers should wait for all readers to finish
int rwlocku5(){
	waiters_running = 0;
	struct rwlock* rwlock = makerwlock();

	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, WRITE);

	while(waiters_running > 0);
	
	ok();
	rwlock_destroy(rwlock);
	return 0;
}
//readers and writers should all wait to prevent starvation
int rwlocku6(){
	waiters_running = 0;
	struct rwlock* rwlock = makerwlock();

	rwmakewaiter(rwlock, WRITE);
	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, WRITE);
	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, WRITE);
	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, READ);
	rwmakewaiter(rwlock, READ);

	while(waiters_running > 0);
	
	ok();
	rwlock_destroy(rwlock);
	return 0;
}

























int asst1_tests(int nargs , char ** args){
  int err = 0;
	int ret;
  int i;
	int (*locktests[4])();
	int (*rwlocktests[6])();
	int (*cvtests[4])();
	
	locktests[0] = locku1;
	locktests[1] = locku2;
	locktests[2] = locku3;
	locktests[3] = locku4;

	cvtests[0] = cvu1;
	cvtests[1] = cvu2;
	cvtests[2] = cvu3;
	cvtests[3] = cvu4;
	
	rwlocktests[0] = rwlocku1;
	rwlocktests[1] = rwlocku2;
	rwlocktests[2] = rwlocku3;
	rwlocktests[3] = rwlocku4;
	rwlocktests[4] = rwlocku5;
	rwlocktests[5] = rwlocku6;



  kprintf("Args %d\n",nargs);
  for (i=0;i<nargs;i++)kprintf("Arg[%d] <%s>\n",i,args[i]);
	
	
	kprintf("\n\n-- LOCK TESTS --\n\n");

	for(i = 0; i < 4; i++){
		kprintf("Starting lock test %d\n", i + 1);
		ret = locktests[i]();
		err += ret;
		kprintf("Test %d complete with status %d\n\n", i + 1, ret);
	}

	kprintf("\n\n-- CV TESTS --\n\n");

	for(i = 0; i < 4; i++){
		kprintf("Starting cv test %d\n", i + 1);
		ret = cvtests[i]();
		err += ret;
		kprintf("Test %d complete with status %d\n\n", i + 1, ret);
	}
	
	kprintf("\n\n-- RWLOCK TESTS --\n\n");

	for(i = 0; i < 6; i++){
		kprintf("Starting rwlock test %d\n", i + 1);
		ret = rwlocktests[i]();
		err += ret;
		kprintf("Test %d complete with status %d\n\n", i + 1, ret);
	}


  kprintf("\n---------------------------\n");
  kprintf("Tests complete with status %d\n",err);
  kprintf("UNIT tests complete\n");
  kprintf("Number of errors: %d\n", err);
  return(err);
}

