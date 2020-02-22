#include "stdlib.h"
#include  <stdio.h>
#include <string.h>
#include <stdbool.h>

void foo0(int arg) {
  char* bf = malloc(10);
  if(bf == NULL)
    return;
  char* array[10];
  array[0] = bf;
  
  free(array[0]);
  free(bf);  // sink
  return;
}

void foo1(int arg) {
  char* bf = malloc(10);
  if(bf == NULL)
    return;
  char* array[10];
  array[arg] = bf;
  
  free(array[arg]);
  free(bf);  // sink
  return;
}

void foo2(int arg) {
  char* bf = malloc(10);
  if(bf == NULL)
    return;
  char* array[10];
  array[0] = bf;
  
  free(array[arg]);
  free(bf);  // sink, maybe false positive
  return;
}

char teststr(const char *file){
    return file[0];
}

int main(int argc, char** argv) {
  if(argc)
    foo0(argc + 1);
  else if(argc + 1)
    foo1(argc + 1);
  else
    foo2(argc + 1);
  teststr("abc");
}
