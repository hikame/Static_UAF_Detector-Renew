#include <stdlib.h>
#include  <stdio.h>
#include <string.h>
#include <stdbool.h>

struct buf_mem_st {
    size_t length;              /* current number of bytes */
    char *data;
    size_t max;                 /* size of buffer */
    unsigned long flags;
};
typedef struct buf_mem_st BUF_MEM;
struct ssl_st {
    int init_num; 
    BUF_MEM *init_buf;
    void *init_msg; 
};
typedef struct ssl_st SSL;

extern ssl_read_bytes(unsigned char *buf);

void foo0(int argc, SSL* s){
  char* buf = malloc(10);
  if(buf == NULL)
    return;
  s->init_buf->data = buf;
  // s->init_msg = s->init_buf->data + (argc + 1);
  s->init_msg = s->init_buf->data + 1;
  free(s->init_buf->data);
  unsigned char *p = s->init_msg;
  ssl_read_bytes(&p[s->init_num]);  // sink
}

extern unsigned long Rand();
#define SSL3_HM_HEADER_LENGTH 4
void fake_tls_get_message_header(SSL *s, int *mt){
  // unsigned char *p;
  // p = (unsigned char *)s->init_buf->data;
  if (Rand()) {
      s->init_msg = s->init_buf->data;
  } else {
      s->init_msg = s->init_buf->data + SSL3_HM_HEADER_LENGTH;
  }
  return;
}

void foo1(SSL* s){
  int mt;
  fake_tls_get_message_header(s, &mt);
  free(s->init_buf->data);
  char* tmp = (char*) s->init_msg;
  char a = tmp[0];  // sink
}

int main(argc){
  foo0(0, NULL);
  foo1(NULL);
  return 0;
}
