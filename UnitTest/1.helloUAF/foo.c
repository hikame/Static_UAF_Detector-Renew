#include "foo.h"
#include <stdlib.h>

void foo(int argc) {
    char* buf = malloc(10);
    if(buf == NULL)
        return;
    buf[0] = 100;
    free(buf);
    // printf("Buf is %s\n", buf);  // sink here
    buf[0];  // sink here
}
