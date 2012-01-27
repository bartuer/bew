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

void
test_10 ( ) {
  printf("\ntest_10: %s\n","result");
  obj buf[10];
  int i;
  for ( i = 0; i < 10; ++i ) {
    buf[i].v = i;
  }

  obj head;
  obj* q = &head;
  ngx_queue_init(q);
  assert(ngx_queue_empty(q));
  
  for ( i = 0; i < 7; ++i ) {
    ngx_queue_insert_tail(q, &buf[i]);
  }
  obj* p;
  ngx_queue_foreach(q, p){
    printf("p->v: %d\n",p->v);
  }

  printf("split begin : %d\n",buf[3].v);
  printf("split end   : %d\n",ngx_queue_last(q)->v);
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
}

void
test_2 ( ) {
  printf("\ntest_2: %s\n","result");
  obj buf[5];
  int i;
  for ( i = 0; i < 5; ++i ) {
    buf[i].v = i;
  }

  obj head;
  obj* q = &head;
  ngx_queue_init(q);
  assert(ngx_queue_empty(q));
  
  for ( i = 0; i < 2; ++i ) {
    ngx_queue_insert_tail(q, &buf[i]);
  }
  obj* p;
  ngx_queue_foreach(q, p){
    printf("p->v: %d\n",p->v);
  }
  obj tmp;
  tmp.v = 100;
  ngx_queue_insert_tail(q, &tmp);
  
  printf("split begin: %d\n",buf[1].v);
  printf("split   end: %d\n",tmp.v);
  ngx_queue_split(q, &buf[1], &tmp);

  printf("\nsplit[0]:\n");
  ngx_queue_foreach(q, p){
    printf("p->v: %d\n",p->v);
  }

  obj new;
  obj* nq = &new;
  ngx_queue_init(nq);
  ngx_queue_insert_head(&buf[1], nq);
  printf("\nsplit[1] before tmp tail remove:\n");
  ngx_queue_foreach(nq, p){
    printf("p->v: %d\n",p->v);
  }

  ngx_queue_remove(&tmp);
  
  printf("\nsplit[1] after tmp tail remove:\n");
  ngx_queue_foreach(nq, p){
    printf("p->v: %d\n",p->v);
  }

  ngx_queue_insert_tail(q, &buf[2]);
  ngx_queue_insert_tail(q, &buf[3]);
  ngx_queue_insert_tail(q, &buf[4]);
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
}

void
test_3 ( ) {
  printf("\ntest_3: %s\n","result");
  obj buf[5];
  int i;
  for ( i = 0; i < 5; ++i ) {
    buf[i].v = i;
  }

  obj head;
  obj* q = &head;
  ngx_queue_init(q);
  assert(ngx_queue_empty(q));
  
  for ( i = 0; i < 3; ++i ) {
    ngx_queue_insert_tail(q, &buf[i]);
  }
  obj* p;
  ngx_queue_foreach(q, p){
    printf("p->v: %d\n",p->v);
  }

  printf("split begin: %d\n",buf[1].v);
  printf("split   end: %d\n",buf[2].v);
  ngx_queue_split(q, &buf[1], &buf[2]);

  printf("\nsplit[0]:\n");
  ngx_queue_foreach(q, p){
    printf("p->v: %d\n",p->v);
  }

  obj new;
  obj* nq = &new;
  ngx_queue_init(nq);
  ngx_queue_insert_head(&buf[2], nq);

  printf("\nsplit[1]:\n");
  ngx_queue_foreach(nq, p){
    printf("p->v: %d\n",p->v);
  }

  ngx_queue_insert_tail(q, &buf[3]);
  ngx_queue_insert_tail(q, &buf[4]);
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
}

int main(int argc, char *argv[])
{
  test_2();
  test_3();
  test_10();
  return 0;
}
