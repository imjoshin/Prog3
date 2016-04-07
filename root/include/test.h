
#ifndef _TEST_H_
#define _TEST_H_

/*
 * Declarations for test code and other miscellaneous high-level
 * functions.
 */

#ifndef DEBUG
#define DEBUG
#endif

/* io tests */
int io1(void);
int io2(void);
int io3(void);
int io4(void);

int forktest(void);
int waittest(void);
int execvtest(void);

#endif /* _TEST_H_ */
