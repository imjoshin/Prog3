#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <synch.h>
#include <thread.h>
#include <current.h>
#include <clock.h>
#include <test.h>
#include <syscall.h>
#include <kern/fcntl.h>

//Jon Faron, Riley Hirn, Josh Johnson

int asst1_tests(int nargs , char ** args){
	(void) nargs;
	(void) args;
	kprintf("Testy test test");
	return 0;
}
