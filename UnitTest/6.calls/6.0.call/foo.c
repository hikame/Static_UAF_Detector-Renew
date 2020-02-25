#include "foo.h"
#include <stdlib.h>


extern int foo3();

char* foo2(char* buf){
  free(buf);
  int i = foo3();
  if(i)
    return buf;
  else
    return NULL;
  
}

void foo(int argc) {
  char *buf;
  buf = malloc(10);
  if(buf == NULL)
    return;
  char* ret = foo2(buf);
  free(buf); // sink here
}
