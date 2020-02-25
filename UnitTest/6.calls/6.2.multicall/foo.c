#include "foo.h"
#include <stdlib.h>

struct TestStr{
  int i;
  char* fld;
};

void fake_seq_open(struct TestStr* ts){
  char* buf = malloc(10);
  if(buf)
     ts->fld = buf;
  char* fbuf = ts->fld;
  if(fbuf)
    free(fbuf);  // sink here on the second call
}

void foo0(int argc) {
  struct TestStr ts;
  ts.fld = NULL;  // otherwise, it will not be treated as null by kmdriver
  fake_seq_open(&ts);
  fake_seq_open(&ts);
}

void fake_seq_open2(int off, struct TestStr* ts){
  if(off != ts->i)
    free(ts->fld);  // no sink here, because of the path condition limitation
}


void foo2(int* off, struct TestStr* ts){
  if(*off == ts->i)
    fake_seq_open2(*off, ts);
}

void foo1(int argc) {
  int offset;
  char* buf = malloc(10);
  if(buf == NULL)
    return;
  struct TestStr ts;
  ts.i = 0;
  ts.fld = buf;
  foo2(&offset, &ts);
  foo2(&offset, &ts);  // no sink here
}