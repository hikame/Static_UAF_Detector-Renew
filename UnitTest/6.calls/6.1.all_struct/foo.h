#include <stdio.h>

struct TestStr{
  int i;
  char* fld;
  //long test; 
};

void foo0(int argc);
char* foo1(struct TestStr tsarg);
char* foo2(struct TestStr* tsarg);
