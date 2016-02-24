#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <synch.h>
#include <thread.h>
#include <current.h>
#include <clock.h>
#include <test.h>

void ok(void) {
	kprintf("Test passed; now cleaning up.\n");
}

/* makelock */
struct lock* makelock() {
    struct lock* lock;
    lock = lock_create(NAMESTRING);
	if (lock == NULL) {
		panic("locktest: whoops: lock_create failed\n");
	}
	return lock;
}


/*
 * 1. After a successful lock_create:
 *     - lock_name compares equal to the passed-in name
 *     - lock_name is not the same pointer as the passed-in name
 *     - lock_wchan is not null
 *     - lock_lock is not held and has no owner
 *     - lock_count is 1
 */
static int utest1(){
	struct lock* lock;
	const char *name = NAMESTRING;

	(void)nargs; (void)args;

	lock = lock_create(name);
	if (lock == NULL) {
		panic("locktest: whoops: lock_create failed\n");
	}
	KASSERT(!strcmp(lock->lock_name, name));
	KASSERT(lock->lock_name != name);
	KASSERT(lock->lock_wchan != NULL);
	KASSERT(spinlock_not_held(&lock->lock_lock));
	KASSERT(lock->lock_count == 1);

	ok();
	/* clean up */
	lock_destroy(lock);
	return 0;
}



static int utest2(){
  kprintf("Start test 2.  Expect no output.\n");
  return(0);
}

int asst1_tests(int nargs , char ** args){

  int err;
  int i;
  kprintf("Args %d\n",nargs);
  for (i=0;i<nargs;i++)kprintf("Arg[%d] <%s>\n",i,args[i]);
  kprintf("Calling UNIT tests.\n");
  err=0;
  err+=utest1();
  kprintf("Test2 complete with status %d\n",err);
  err+=utest2();
  kprintf("Tests complete with status %d\n",err);
  kprintf("UNIT tests complete\n");
  kprintf("Number of errors: %d\n", err);
  return(err);
}

