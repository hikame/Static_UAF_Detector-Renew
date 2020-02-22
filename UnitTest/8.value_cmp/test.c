#include "stdlib.h"
#include  <stdio.h>
#include <string.h>

struct TestStr{
  int i;
  char* buf;
  long test;
};

void foo2(struct TestStr* ts) {
  switch(ts->i){
    case 0:
      return;
    case 1:
      return free(ts->buf);
    default:
      break;
  }
  return;
}

char* foo1(int arg){
  switch(arg){
    case 0:
      return NULL;
    case 1:
      return malloc(10);
    default:
      break;
  }
  return NULL;
}

void foo0_1(int arg) {
  char* bf = foo1(arg);
  if(bf == NULL)
    return;
  struct TestStr ts;
  ts.i = arg + 1;
  ts.buf = bf;
  foo2(&ts);
  free(bf);  // false positive
  return;
}

void foo0_0(int arg) {
  char* bf = foo1(arg);
  if(bf == NULL)
    return;
  struct TestStr ts;
  ts.i = arg;
  ts.buf = bf;
  foo2(&ts);
  free(bf);  // sink here
  return;
}

int main(int argc, char** argv) {
  if(argc)
    foo0_0(argc + 1);
  else
    foo0_1(argc + 1);
}
