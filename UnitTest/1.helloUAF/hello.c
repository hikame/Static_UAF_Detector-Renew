#include <stdio.h>
#include <stdlib.h>

void foo(int argc) {
    char* buf = malloc(10);
    if(buf == NULL)
        return;
    buf[0] = 100;
    free(buf);
    if(buf != NULL)
        buf[0];  // sink here
}

int main(int argc, char** argv) {
    foo(argc);
    return 0;
}
