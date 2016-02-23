#include <types.h>
#include <lib.h>
#include <test.h>

static int utest1(){
  kprintf("Start test 1.  Expect no output.\n");
  return(0);
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
  return(err);
}

