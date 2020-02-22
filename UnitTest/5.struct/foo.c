#include "foo.h"
#include <stdlib.h>

struct InnerStr{
  char buf[10];
};

struct TestStr{
  int i;
  struct InnerStr is;
  char* fld;
};

int gloInt = 555;

struct TestStr gts = {.i = 666, .fld = NULL};

void foo(int argc) {
  char *buf;
  struct TestStr *tsp;
  buf = malloc(sizeof(struct TestStr));
  if(buf == NULL)
    return;
  tsp = buf;
  
  gts.fld = buf;
  free(tsp);
  printf("This is a sink instruction: %p.\n", gts.fld);  // sink here
}
