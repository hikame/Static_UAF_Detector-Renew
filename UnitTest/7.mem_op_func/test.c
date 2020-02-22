#include "stdlib.h"
#include  <stdio.h>
#include <string.h>

struct InnerStr{
  char* ibuf;
  int justtest[100];
};

struct TestStr{
  int i;
  struct InnerStr is;
  long test;
};

void foo0_memset(argc) {
  char* a = malloc(10);
  if(a == NULL)
    return;
  char* b = a;
  char* buf[10];
  buf[0] = a;
  free(a);
  if(argc + 1){ // symboliz argc
    memset(buf, 0, sizeof(char*) * 10);
    memset(&b, 0, sizeof(char*));
  }
  if(argc + 2) // symboliz argc
    printf("b=%s\n", buf[0]); // sink without go through memset
  else
    printf("b=%s\n", b);      // sink without go through memset
  return;
}

void foo1_memcpy(void *arg) {
  char* a = malloc(10);
  if(a == NULL)
    return;
  struct TestStr ts1, ts2;
  ts1.is.ibuf = a;

  memcpy(&ts2, &ts1, sizeof(struct TestStr));
  free(a);
  printf("b=%s\n", ts2.is.ibuf);  // sink here
  return;
}


void foo0_memcpy(int argc) {
  char* a = malloc(10);
  if(a == NULL)
    return;

  char* b;
  memcpy(&b, &a, sizeof(char**));

  char* buf[10];
  char* buf2[10];
  buf[0] = a;

  memcpy(buf2, buf, sizeof(char*) * 10);
  free(buf[0]);
  if(argc)
    printf("b=%s\n", buf2[1]); // no sink
  else if(argc + 1)
    printf("b=%s\n", buf2[0]); // sink
  else if(argc + 2)
    printf("b=%s\n", buf2[argc]); // sink too (may have false positive)
  else
    printf("b=%s\n", b); // sink
  return;
}

void foo_memmove(int argc) {
  char* a = malloc(10);
  char* buf[100];
  buf[5] = a;
  free(a);

  if(argc - 1)  // symbolize the argc
    memmove(buf, &buf[5], sizeof(char*) * 6);
  else 
    memmove(buf, &buf[argc], sizeof(char*) * 6); // cannot handle this, analyzer will make no change on records of elemetn in buf
  if(argc + 1)  // re-symbolize the argc
    printf("b0=%p\n", buf[0]);  // sink here if from 81, true posotive
  else
    printf("b5=%p\n", buf[5]);  // sink here if from 83, may be false posotive
  return;
}

int main(int argc, char** argv) {
  if(argc)
    foo_memmove(argc + 1);
  else if(argc + 1)
    foo0_memcpy(argc + 1);   
  else if(argc + 2)
    foo1_memcpy(argc + 1);
  else if(argc + 3)
    foo0_memset(argc + 1);
}
