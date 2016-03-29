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


/* ===============
   =   FILE IO   =
   ===============
*/

#define NAMESTRING "FILEIO-TESTS"

//Files can be opened and closed. Lowest descriptor is returned.
int io1(){
	int ret, t1, t2;
	t1 = open("t1", O_RDONLY | O_CREAT);
	kprintf("t1 = %d\n", t1);
	t2 = open("t2", O_WRONLY | O_CREAT);
	kprintf("t2 = %d\n", t2);

	ret = close(t1);
	kprintf("Close t1\n");
	ret = close(t2);
	kprintf("Close t2\n");

	t1 = open("t1", O_RDONLY);
	kprintf("t1 = %d\n", t1);
	ret = close(t1);

	kprintf("Close t1\n");
	return ret;
}

int io2(){
	int ret, t1;
	char buf1[30] = "Hello World";
	char buf2[30];

	t1 = open("t1", O_WRONLY | O_CREAT);
	kprintf("open t1 = %d\n", t1);

	kprintf("Writing to t1\n");
	write(t1, buf1, 30);

	ret = close(t1);
	kprintf("Close t1\n");

	t1 = open("t1", O_RDONLY);
	kprintf("open t1 = %d\n", t1);
	read(t1, buf2, 30);
	ret = close(t1);
	kprintf("Close t1\n");
	
	kprintf("read: %s\n", buf2);
	
	return ret;
}





















int asst1_tests(int nargs , char ** args){
  int err = 0;
	int ret;
  int i;
	int (*iotests[4])();
	
	iotests[0] = io1;
	iotests[1] = io2;
	//iotests[2] = io3;
	//iotests[3] = io4;


  kprintf("Args %d\n",nargs);
  for (i=0;i<nargs;i++)kprintf("Arg[%d] <%s>\n",i,args[i]);
	
	
	kprintf("\n\n-- IO TESTS --\n\n");

	for(i = 0; i < 2; i++){
		kprintf("Starting io test %d\n", i + 1);
		ret = iotests[i]();
		err += ret;
		kprintf("Test %d complete with status %d\n\n", i + 1, ret);
	}


  kprintf("\n---------------------------\n");
  kprintf("Tests complete with status %d\n",err);
  kprintf("UNIT tests complete\n");
  kprintf("Number of errors: %d\n", err);
  return(err);
}

