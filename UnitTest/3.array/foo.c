#include "foo.h"
#include <stdlib.h>

struct InnerStr{
  char* buf;
  char ib[10];
};

void foo0(int arg) {
    arg++;  // make it symbolized
    struct InnerStr is[10];
    char* buf = malloc(10);
    if(buf == NULL)
      return;
    is[2].buf = buf;
    free(buf);
    if(arg > 10)
      printf("Buf is %s\n", is[2].buf);  // sink
    else
      printf("Buf is %s\n", is[0].buf);  // not sink
}

void foo1(int arg) {
    arg++;  // make it symbolized
    long buf[10];
    buf[0] = malloc(10);
    free(buf[0]);
    if(arg > 10)
      printf("Buf is %s\n", buf[1]);  // not sink
    else
      printf("Buf is %s\n", buf[0]);  // sink here
}

void foo2(int arg) {
    arg++;  // make it symbolized
    char* bufarray[10];
    char* buf = malloc(10);
    bufarray[2] = buf;
    free(buf);
    if(arg == 3)
      printf("Buf is %s\n", bufarray[arg]);  // not sink
    else if(arg == 2)
      printf("Buf is %s\n", bufarray[arg]);  // sink
    else
      // false positive!
      // because when handling getelement instruction, 
      // we do not consider not equel situation
      printf("Buf is %s\n", bufarray[arg]);  
}

void foo3(int arg) {
    arg++;  // make it symbolized
    struct InnerStr is[10];
    char* buf = malloc(10);
    is[2].buf = buf;
    free(buf);
    // if(arg == 2)
    // not reported. false negative!
    // It's because that not supporing symbolic index in an array of StructType
    printf("Buf is %s\n", is[arg].buf);  
}

void foo4(int arg) {
    struct InnerStr* is = malloc(sizeof(struct InnerStr));
    if(is == NULL)
      return;
    char* buf = is->ib;
    free(is);
    printf("Buf is %s\n", buf);    // sink
}