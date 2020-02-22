// hello.c
#include "foo.h"

int main(int argc, char* argv[]) {
  if(argc)
    foo0(argc);
  else
    foo1(argc);
	return 0;
}
