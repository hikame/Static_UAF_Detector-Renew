#include <stdlib.h>
#include  <stdio.h>

typedef struct bignum_pool_item {
    int val;
    /* Linked-list admin */
    struct bignum_pool_item *prev, *next;
} BN_POOL_ITEM;

typedef struct bignum_pool {
    /* Linked-list admin */
    BN_POOL_ITEM *head, *current, *tail;
    /* Stack depth and allocation size */
    unsigned used, size;
} BN_POOL;

void foo(BN_POOL *p){
    while (p->head) {
        p->current = p->head->next;
        free(p->head);
        p->head = p->current;
    }
}

BN_POOL fake_bp;
int main(int argc, char** argv) {
  foo(&fake_bp);
}
