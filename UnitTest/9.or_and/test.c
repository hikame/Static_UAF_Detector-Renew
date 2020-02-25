#include "stdlib.h"
#include  <stdio.h>
#include <string.h>
#include <stdbool.h>

void foo(int arg) {
  char* bf = malloc(10);
  if(bf == NULL)
    return;
  bool cond1 = (arg > 100);
  // current not support % here, therefore arg%2 will generate a symbolic value
  // bool cond2 = (arg % 2 == 0); 
  bool cond2 = (arg < 50); 
  if(cond1 || cond2){
    free(bf);
  }
  printf("%p", bf);   // sink when arg == 1 or 101
  return;
}

int main(int argc, char** argv) {
  if(argc)
    foo(75);
  else if(argc + 1)
    foo(1);  
  else
    foo(101);  
}
