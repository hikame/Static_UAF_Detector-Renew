#include "foo.h"


char test[] = "abc";
char tmp(const char *file){
    return file[1];
}

int main(int argc, char** argv) {
    tmp(test);
    if(argc == 0)
        foo0(argc);
    else if(argc == 1)
        foo1(argc);
    else if(argc == 2)
        foo2(argc);
    else if(argc == 3)
        foo3(argc);
    else
        foo4(argc);
    return 0;
}
