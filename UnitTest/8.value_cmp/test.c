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


void CRYPTO_free(void *str, const char *file, int line);
static void (*free_impl)(void *, const char *, int)
    = CRYPTO_free;

void CRYPTO_free(void *str, const char *file, int line)
{
    if (free_impl != NULL && free_impl != &CRYPTO_free) {
        free_impl(str, file, line);
        return;
    }

    free(str);
}

void foo3(){
  char* buf = malloc(10);
  if(buf == NULL) 
    return;
  CRYPTO_free(buf, NULL, 0);
  printf("%s", buf);  // sink here
}

int main(int argc, char** argv) {
  if(argc)
    foo0_0(argc + 1);
  else if(argc + 1)
    foo0_1(argc + 1);
  else
    foo3();
}
