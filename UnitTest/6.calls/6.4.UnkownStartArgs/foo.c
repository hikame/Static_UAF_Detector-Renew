#include <stdlib.h>

struct evp_pkey_ctx_st;
struct evp_pkey_st;

typedef struct evp_pkey_ctx_st EVP_PKEY_CTX;
typedef struct evp_pkey_st EVP_PKEY;

struct evp_pkey_st {
    void *lock;
};

struct evp_pkey_ctx_st {
    EVP_PKEY *pkey;
    EVP_PKEY *peerkey;
} /* EVP_PKEY_CTX */ ;

void EVP_PKEY_free(EVP_PKEY *x){
    free(x->lock);
    // free(x);
}

void EVP_PKEY_CTX_free(EVP_PKEY_CTX *ctx)
{
    if (ctx == NULL)
        return;
    EVP_PKEY_free(ctx->pkey);
    EVP_PKEY_free(ctx->peerkey);   // should report nothing
}

int main(){
  EVP_PKEY_CTX_free(NULL);
}