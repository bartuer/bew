#include "ngx-queue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct obj_s obj;
struct obj_s{
  obj*  next;
  obj*  prev;
  int   v;
};

int main(int argc, char *argv[])
{
  obj buf[10];
  for ( int i = 0; i < 10; ++i ) {
    buf[i].v = i;
  }

  obj head;
  obj* q = &head;
  ngx_queue_init(q);
  assert(ngx_queue_empty(q));

  for ( int i = 0; i < 7; ++i ) {
    ngx_queue_insert_tail(q, &buf[i]);
  }
  obj* p;
  ngx_queue_foreach(q, p){
    printf("p->v: %d\n",p->v);
  }

  ngx_queue_split(q, &buf[3], ngx_queue_last(q));

  printf("\nsplit[0]:\n");
  ngx_queue_foreach(q, p){
    printf("p->v: %d\n",p->v);
  }

  obj new;
  obj* nq = &new;
  ngx_queue_init(nq);
  ngx_queue_insert_head(&buf[6], nq);

  printf("\nsplit[1]:\n");
  ngx_queue_foreach(nq, p){
    printf("p->v: %d\n",p->v);
  }

  ngx_queue_insert_tail(q, &buf[7]);
  ngx_queue_insert_tail(q, &buf[8]);
  ngx_queue_insert_tail(q, &buf[9]);
  printf("\nsplit[0]_appended:\n");
  ngx_queue_foreach(q, p){
    printf("p->v: %d\n",p->v);
  }
  
  printf("\nsplit[1]:\n");
  ngx_queue_foreach(nq, p){
    printf("p->v: %d\n",p->v);
  }

  ngx_queue_add(q, nq);
  printf("\nmerge(split[0], split[1]):\n");
  ngx_queue_foreach(q, p){
    printf("p->v: %d\n",p->v);
  }

  memset(&new, 0, sizeof(new));

  printf("\ncleaned_split[1]:\n");
  ngx_queue_foreach(nq, p){
    printf("cleaned nq p->v: %d\n",p->v);
  }
  
  printf("\nmerge(split[0], split[1]):\n");
  ngx_queue_foreach(q, p){
    printf("p->v: %d\n",p->v);
  }

  return 0;
}
