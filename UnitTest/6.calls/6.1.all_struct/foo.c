#include "foo.h"
#include <stdlib.h>



void foo0(int argc) {
  char *buf;
  buf = malloc(10);
  if(buf == NULL)
    return;
  struct TestStr str;
  str.fld = buf;
  char* ret = NULL;
  if(argc)
    ret = foo1(str);
  else
    ret = foo2(&str);  
  free(ret); // sink from two path
}

char* foo1(struct TestStr tsarg){
  free(tsarg.fld);
  return tsarg.fld;
}

char* foo2(struct TestStr* tsarg){
  free(tsarg->fld);
  return tsarg->fld;
}
