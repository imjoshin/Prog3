#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <test.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

//Jon Faron, Riley Hirn, Josh Johnson


/* ===============
   =   FILE IO   =
   ===============
*/



#define NAMESTRING "FILEIO-TESTS"



int io1(){
	//char buf[100];
	printf("type something: ");	
	printf("type something2: ");	
	//read(0, buf, 100);
	//write(1, "\nread ", 6);
	//write(1, buf, 100);
	//write(1, " from stdin\n", 13);
	//printf("write to stdout Test passed!\n");
	write(1, "write to stderr Test passed!\n", 30);
	return 0;
}

int io2() {
	int fd;
	char buf[100];

	fd = open("OurFile", O_RDWR | O_CREAT);
	printf("fd after open: ");
	buf[0] = (char) (fd + 'a');
	buf[1] = '\n';
	write(1, buf, 2);
	write(fd, "Hello File!", 13);
	close(fd);
	fd = open("OurFile", O_RDWR);
	printf("fd before: %d\n", fd);
	read(fd, buf, 100);
	printf("fd: %d\nbuf length: %d\n", fd, strlen(buf));
	printf("read [%s] from file\n", buf);
	return 0;
}

int io3() {
	int pid;
	printf("fork #1\n");
	pid = fork();
	printf("fork #2\n");
	pid = fork();
	printf("fork #3\n");
	pid = fork();
	printf("pid: %d\n", pid);
	return 0;
}

int io4() {
	
	return 0;
}

int forktest() {
	int i;
	fork();
	write(1, "Starting\n", 10);
	i = fork();
	printf("fork returned: %d\n", i);
	printf("my pid is: %d\n", getpid());

	return 0;
}

int waittest() {
	int i, pid;
	i = 0;
	pid = fork();
	printf("fork returned %d\n", pid);
	if(pid == 0){
		//child 
		printf("CHILD: start counting\n");
		while(i < 10000000) i++;
		printf("CHILD: done counting\n");
		//exit(1);
	}else{
		//parent
		printf("PARENT: waiting on pid %d\n", pid);
		//waitpid(pid, &i, 0);
		//printf("PARENT: done waiting - returned %d\n", i);
	}
	return 0;
}
/*
//Files can be opened and closed. Lowest descriptor is returned.
int io1(){
	int ret, t1, t2;
	t1 = open("t1", O_RDONLY | O_CREAT);
	printf("t1 = %d\n", t1);
	t2 = open("t2", O_WRONLY | O_CREAT);
	printf("t2 = %d\n", t2);

	ret = close(t1);
	printf("Close t1\n");
	ret = close(t2);
	printf("Close t2\n");

	t1 = open("t1", O_RDONLY);
	printf("t1 = %d\n", t1);
	ret = close(t1);

	printf("Close t1\n");
	return ret;
}

int io2(){
	int ret, t1;
	char buf1[30] = "Hello World";
	char buf2[30];

	t1 = open("t1", O_WRONLY | O_CREAT);
	printf("open t1 = %d\n", t1);

	printf("Writing to t1\n");
	write(t1, buf1, 30);

	ret = close(t1);
	printf("Close t1\n");

	t1 = open("t1", O_RDONLY);
	printf("open t1 = %d\n", t1);
	read(t1, buf2, 30);
	ret = close(t1);
	printf("Close t1\n");
	
	printf("read: %s\n", buf2);
	
	return ret;
}

*/





int main(int nargs , char ** args){
	waittest();
	(void)nargs;
	(void) args;
	
/*
	(void) nargs;
	(void) args;
	int i;
	int err = 0;
	int (*iotests[410])();
	
	iotests[0] = io1;
	iotests[1] = io2;
	iotests[2] = io3;
	iotests[3] = io4;

	for(i = 3; i < 4; i++) {
		err += iotests[i]();
	}
*/	
/*
  int err = 0;
	int ret;
  int i;
	int (*iotests[4])();
	
	iotests[0] = io1;
	//iotests[1] = io2;
	//iotests[2] = io3;
	//iotests[3] = io4;


  printf("Args %d\n",nargs);
  for (i=0;i<nargs;i++) printf("Arg[%d] <%s>\n",i,args[i]);
	
	
	printf("\n\n-- IO TESTS --\n\n");

	for(i = 0; i < 1; i++){
		printf("Starting io test %d\n", i + 1);
		ret = iotests[i]();
		err += ret;
		printf("Test %d complete with status %d\n\n", i + 1, ret);
	}


  printf("\n---------------------------\n");
  printf("Tests complete with status %d\n",err);
  printf("UNIT tests complete\n");
  printf("Number of errors: %d\n", err);
	*/
	return 0;
}

