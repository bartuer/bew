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

typedef struct {
  char*  name;
  int    allocated;
} path_buf;

typedef struct dir_node_s dir_node;

struct dir_node_s {
  struct dirent* dir_ent;
  DIR*           dir_ptr;
  path_buf       path;
  dir_node*      parent;
  dir_node*      next;
  dir_node*      prev;
  long           loc;
};

#define MAXFDNUM 1024
static dir_node dir_cluster[MAXFDNUM];

int
empty_dir_node ( dir_node* node ) {
  int dir_ent = node->dir_ent == NULL;
  int dir_ptr = node->dir_ptr == NULL;
  int parent = node->parent == NULL;
  return (dir_ent && dir_ptr && parent );
}

dir_node*
create_root_dir_node (char* path, dir_node* slot) {
  dir_node node;
  DIR* dirp = opendir(path);
  assert(dirp);
  struct dirent* current = readdir(dirp);
  assert(current);
  assert(!strcmp(current->d_name, "."));                 /* bypass . */
  assert(current->d_type == DT_DIR);
  struct dirent* father = readdir(dirp);                /* bypass .. */
  assert(father);
  assert(!strcmp(father->d_name, ".."));
  assert(father->d_type == DT_DIR);
  node.loc = telldir(dirp);
  
  node.dir_ent = current;
  node.dir_ptr = dirp;
  node.parent = NULL;
  /* add to queue maybe change these 2 pointer */
  node.next = NULL;
  node.prev = NULL;

  char* d_name = current->d_name;
  char* root_path = realpath(path, d_name + 2);
  node.path.name = strdup(root_path);
  node.path.allocated = 1;

  int fd = dirfd(dirp);
  assert(fd < MAXFDNUM);
  assert(empty_dir_node(slot + fd));
  memcpy((dir_node*)(slot + fd), &node, sizeof(node));
  return slot + fd;
}

dir_node*
create_dir_node (struct dirent* entry,
                 dir_node* parent, 
                 dir_node* slot
                 ) {
  dir_node node;
  assert(entry->d_type == DT_DIR);
  node.dir_ent = entry;
  node.parent = parent;
  node.prev = NULL;
  node.next = NULL;
  assert(node.parent->dir_ent);

  /* use allocated dirent save path name when possible */
  unsigned int namelen, p_namelen;
  char * d_name = node.dir_ent->d_name;
  char * p_name = node.parent->path.name;
  namelen = strlen(d_name);
  p_namelen = strlen(p_name);
  char* full_path = NULL;
  full_path = malloc(namelen + p_namelen + 2);
  assert(full_path);
  node.path.allocated = 1;
  sprintf(full_path, "%s/%s", p_name, d_name);
  full_path[p_namelen + namelen + 1] = 0;
  node.path.name = full_path;

  DIR* dirp = opendir(node.path.name);
  if ( dirp == NULL ) {
    printf("node.path.name: %s\n",node.path.name);
  }
  assert(dirp);
  struct dirent* current = readdir(dirp);
  assert(current);
  assert(!strcmp(current->d_name, "."));
  assert(current->d_type == DT_DIR);
  struct dirent* father = readdir(dirp);                /* bypass .. */
  assert(parent);
  assert(!strcmp(father->d_name, ".."));
  assert(father->d_type == DT_DIR);
  node.dir_ptr = dirp;
  node.loc = telldir(dirp);
  
  int fd = dirfd(dirp);
  assert(fd < MAXFDNUM);
  assert(empty_dir_node(slot + fd));
  memcpy((dir_node*)(slot + fd), &node, sizeof(node));
  return slot + fd;
}

void dump_dir_node(dir_node* node) {
  if ( empty_dir_node(node) ) {
    printf(" cleaned node; \n");
  } else {
    size_t path_len = strlen(node->path.name);
    size_t name_len = strlen(node->dir_ent->d_name);
    printf("\nnode->path.name[%lu]: %s\n", path_len, node->path.name);
    printf("node.->d_name[%lu]: %s\n", name_len, node->dir_ent->d_name);
    printf("node.->dd_fd: %d\n",dirfd(node->dir_ptr));
  }
}

void clean_dir_node(dir_node* node) {
  if ( node == NULL ) {
    return;
  }
  
  if ( node->dir_ptr ) {
    closedir(node->dir_ptr);
    node->dir_ptr = NULL;
  }

  if ( node->dir_ent ) {
    node->dir_ent = NULL;
  }

  if ( node->path.allocated ) {
    free(node->path.name);
  }

  if ( node->parent ) {
    node->parent = NULL;
  }
  assert(empty_dir_node(node));
}

unsigned int
add_nodes (dir_node* root, dir_node* slot, dir_node* queue) {
  struct dirent* ent;
  unsigned int sum = 0;
  while ( (ent = readdir(root->dir_ptr)) ) {
    assert(ent);
    if ( ent->d_type == DT_DIR ) {
       dir_node* node;
       node = create_dir_node(ent, root, slot);
       assert(node);
       int fd = dirfd(node->dir_ptr);
       ngx_queue_insert_tail(queue, &slot[fd]);
       assert(ngx_queue_last(queue));
       sum++;
       sum += add_nodes(node, slot, queue);
    }
  }
  return sum;
}

unsigned int
add_root_node ( char* path, dir_node* slot, dir_node* queue ) {
  unsigned int sum = 0;
  dir_node* root_node;
  root_node = create_root_dir_node(path, dir_cluster);
  assert(root_node);
  int root_fd = dirfd(root_node->dir_ptr);
  ngx_queue_insert_head(queue, &dir_cluster[root_fd]);
  dir_node* root = ngx_queue_head(queue);
  assert(!ngx_queue_empty(queue));
  return add_nodes(root, slot, queue);
}

unsigned int
remove_nodes ( dir_node* root, dir_node* queue ) {
  unsigned int sum = 0;
  dir_node* parent = root->parent;
  dir_node* p;
  if ( parent == NULL ) {       /* topest */
    ngx_queue_foreach(queue, p) {
      dir_node* r = p;
      ngx_queue_remove(r);
      clean_dir_node(r);
      sum++;
    }
  } else {                      /* decent of topest */
    for ( p = root;
          p == root || (p->parent != NULL && p->parent != parent);
          p = ngx_queue_next(p), sum++ ) {
      ngx_queue_remove(p);
      clean_dir_node(p);
      } 
  }
  return sum;
}

void
dir_node_rewind ( dir_node* node ) {
  assert(!empty_dir_node(node));
  DIR* dirp = node->dir_ptr;
  assert(dirp);
  assert(node->loc != 0);
  seekdir(dirp, node->loc);
}

void
dump_queue ( dir_node* q , char* head_line ) {
  printf("\n%s\n", head_line);
  int count = 0;
  dir_node* p;
  ngx_queue_foreach(q, p){
    dump_dir_node(p);
    count++;
  }
  printf("count: %d\n",count);
}

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

  dir_node* the_root = ngx_queue_head(q);
  assert(the_root);
  dir_node* first = ngx_queue_next(the_root);

  remove_nodes(first, q);
  dump_queue(q, "\nINSPECT AFTER FIRST CHILD REMOVE\n\n");

  first = ngx_queue_next(the_root);
  remove_nodes(first, q);
  dump_queue(q, "\nINSPECT AFTER LATEST FIRST CHILD REMOVE\n\n");
  
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
  dump_queue(q, "\nINSPECT AFTER NEW TREE APPEND TO ROOT\n\n");

  /* insert tree need split queue, append to one, then add up */

  remove_nodes(the_root, q);
  dump_queue(q, "\nINSPECT AFTER ALL REMOVE\n\n");

  ngx_queue_foreach(q, p){
    clean_dir_node(p);
  }

  printf("sizeof(dir_cluster): %lu\n",sizeof(dir_cluster));
  return 0;
}