// foo.c
#include "foo.h"

void foo0(int argc) {
  struct TestStr ts;
  ts.fld = NULL;
  if(argc == 2){
    ts.fld = malloc(10);
    char *buf = ts.fld;
    if(buf)
      free(buf);
  }
  argc = argc + 1;  // this will symbolize the record of argc
  if(argc == 2)
    printf("ts.fld[0] is %c.\n", *ts.fld);  // report on this line will be a false positive
  else
    printf("ts.fld[0] is %c.\n", *ts.fld);  // sink
}

void foo1(int argc) {
  char* buf = malloc(10);
  if(buf == NULL)
    return;
  free(buf);
  // if "-bbAnalyzeLimit 2" is set, code in this block should be analyzed twice
  // if "-bbAnalyzeLimit 1" is set, code in this block should be analyzed twice
  // and for the 2nd time, i will be treated as symblic value
  for(int i = 0; i < 5; i++){
    if(i > 3)
      printf("this is a true sink: %c.\n", buf);  //sink
  }
}