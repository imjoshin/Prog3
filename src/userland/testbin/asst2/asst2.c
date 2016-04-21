#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <test.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

//Jon Faron, Riley Hirn, Josh Johnson


int main(int nargs , char ** args){
	(void)nargs;
	(void) args;

	int size = 4096;
	int BIGSTUFFWOOO[size];
	printf("Made int array of size %d.\n", size);

	for(int i = 0; i < size; i++){
		BIGSTUFFWOOO[i] = 1;
	}
	printf("Wrote to %d every value in int array.\n", BIGSTUFFWOOO[0]);

	return 0;
}

