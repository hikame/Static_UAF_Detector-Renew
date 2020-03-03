#include <stdlib.h>
#include  <stdio.h>

void foo2(int arg) {
  char* bf = malloc(10);
  if(bf == NULL)
    return;
  char* array[10];
  array[0] = bf;
  free(array[arg]);
  if(bf)
    free(bf);  // sink, maybe false positive
  return;
}

int main(int argc, char** argv) {
  foo2(argc + 1);
}


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

char teststr(const char *file){
    return file[0];
}

// int main(int argc, char** argv) {
//   if(argc)
//     foo0(argc + 1);
//   else if(argc + 1)
//     foo1(argc + 1);
//   else
//     foo2(argc + 1);
//   teststr("abc");
// }
