#include "foo.h"
#include <stdlib.h>

char* gBuf;

void foo1(){
  if(gBuf)
    free(gBuf);
}

void foo0() {
  char* buf = malloc(10);
  if(buf)
    gBuf = buf;
  printf("Buf is %s\n", gBuf);  // sink on second call 
}
