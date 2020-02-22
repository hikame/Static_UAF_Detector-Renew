#include "foo.h"
#include <stdlib.h>

char* foo2(){
  char buf;
  return &buf;
}

void foo(int argc) {
  char* ret = foo2();
  printf("%p", ret); // sink here
}
