#include "foo.h"
#include <stdlib.h>

char* foo2(char* buf){
  free(buf);
  return buf;
}

void foo(int argc) {
  char *buf;
  buf = malloc(10);
  if(buf == NULL)
    return;
  char* ret = foo2(buf);
  free(buf); // sink here
}
