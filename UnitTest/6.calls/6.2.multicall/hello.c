#include "foo.h"

int main(int argc, char* argv[]) {
  if(argc)
    foo0(argc + 1);
  else
    foo1(argc + 1);
	return 0;
}
