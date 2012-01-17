#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <errno.h>
#include "eio.h"
#include "ev.h"
#include "ngx-queue.h"
#include "dir_node.h"

#define MAXFDNUM 1024
static dir_node dir_cluster[MAXFDNUM];

int
main (int argc, char**argv) {
  if ( !argv[1] ) {
    printf("need dir parameter\n");
    exit(1);
  }

  /* prepare slots */
  memset(dir_cluster, 0, sizeof(dir_cluster));

  /* queue */
  dir_node empty_node;
  dir_node* q = &empty_node;
  memset(q, 0, sizeof(empty_node));
  ngx_queue_init(q);
  assert(ngx_queue_empty(q));

  /* add nodes */
  char* path = argv[1];
  add_root_node(path, dir_cluster, q);

  /* inspect */
  dir_node* p;
  int count = 0;
  ngx_queue_foreach(q, p){
    dump_dir_node(p);
    count ++;
  }
  printf("count: %d\n",count);
  assert(count == 28);
  
  dir_node* the_root = ngx_queue_head(q);
  assert(the_root);
  dir_node* first = ngx_queue_next(the_root);
  dir_node* ffirst = ngx_queue_next(first);

  remove_nodes(ffirst, q);
  count = dump_queue(q, "\nINSPECT AFTER MIDDLE REMOVE\n\n");
  assert(count == 24);

  DIR* first_dirp = first->dir_ptr;
  dir_node_rewind(first);
  struct dirent* middle_ent = readdir(first_dirp);
  assert(middle_ent);
  dir_node* middle_node;
  middle_node = create_dir_node(middle_ent, first, dir_cluster);
  assert(middle_node);
  insert_nodes(middle_node, first, dir_cluster, q);
  count = dump_queue(q, "\nINSPECT AFTER MIDDLE INSERT\n\n");
  assert(count == 28);

  remove_nodes(first, q);
  count = dump_queue(q, "\nINSPECT AFTER FIRST CHILD REMOVE\n\n");
  assert(count == 19);
  
  first = ngx_queue_next(the_root);
  remove_nodes(first, q);
  count = dump_queue(q, "\nINSPECT AFTER LATEST FIRST CHILD REMOVE\n\n");
  assert(count == 10);
  
  DIR* root_dirp = the_root->dir_ptr;
  dir_node_rewind(the_root);
  struct dirent* new_ent = readdir(root_dirp);
  assert(new_ent);
  dir_node* new_node;
  new_node = create_dir_node(new_ent, the_root, dir_cluster);
  assert(new_node);
  DIR* new_dirp = new_node->dir_ptr;
  int new_fd = dirfd(new_dirp);
  ngx_queue_insert_tail(q, &dir_cluster[new_fd]);
  add_nodes(new_node, dir_cluster, q);
  count = dump_queue(q, "\nINSPECT AFTER NEW TREE APPEND TO ROOT\n\n");
  assert(count == 19);
  
  remove_nodes(the_root, q);
  count = dump_queue(q, "\nINSPECT AFTER ALL REMOVE\n\n");
  assert(count == 0);
  
  ngx_queue_foreach(q, p){
    clean_dir_node(p);
  }

  printf("sizeof(dir_cluster): %lu\n",sizeof(dir_cluster));
  return 0;
}