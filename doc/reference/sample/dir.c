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
};

#define MAXFDNUM 1024
static dir_node dir_cluster[MAXFDNUM];

int
empty_dir_node ( dir_node* node ) {
  int dir_ent = node->dir_ent == NULL;
  int dir_ptr = node->dir_ptr == NULL;
  int parent = node->parent == NULL;
  int next = node->next == NULL;
  int prev = node->prev == NULL;
  return (dir_ent && dir_ptr && parent && next && prev);
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

  node.dir_ent = current;
  node.dir_ptr = dirp;
  node.parent = NULL;
  /* add to queue maybe change these 2 pointer */
  node.next = NULL;
  node.prev = NULL;

  char* d_name = current->d_name;
  char* root_path = realpath(path, d_name + 2);
  if ( strlen(root_path) > MAXPATHLEN - 3  ) {           
    node.path.name = strdup(root_path);
    node.path.allocated = 1;
  } else {                                               /* use dirent allocation */
    node.path.name = d_name + 2;
    node.path.allocated = 0;
  }

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
  if ( 2 * namelen + p_namelen + 3  <= MAXPATHLEN ) {
    full_path = d_name + namelen + 1;
    node.path.allocated = 0;
  } else {
    full_path = malloc(namelen + p_namelen + 2);
    assert(full_path);
    node.path.allocated = 1;
  }
  sprintf(full_path, "%s/%s", p_name, d_name);
  full_path[p_namelen + namelen + 1] = 0;
  node.path.name = full_path;

  DIR* dirp = opendir(node.path.name);
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

  int fd = dirfd(dirp);
  assert(fd < MAXFDNUM);
  assert(empty_dir_node(slot + fd));
  memcpy((dir_node*)(slot + fd), &node, sizeof(node));
  return slot + fd;
}

void dump_dir_node(dir_node* node) {
  size_t path_len = strlen(node->path.name);
  size_t name_len = strlen(node->dir_ent->d_name);
  printf("\nnode->path.name[%lu]: %s\n", path_len, node->path.name);
  printf("node->dir_ent->d_name[%lu]: %s\n", name_len, node->dir_ent->d_name);
  printf("node->dir_ptr->dd_fd: %d\n",dirfd(node->dir_ptr));
}

void clean_dir_node(dir_node* node) {
  assert(node);
  if ( node->dir_ptr ) {
    closedir(node->dir_ptr);
    node->dir_ptr = NULL;
  }
  memset(node, 0, sizeof(*node));
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

int
main (int argc, char**argv) {
  memset(dir_cluster, 0, sizeof(dir_cluster));

  /* queue */
  dir_node empty_node;
  dir_node* q = &empty_node;
  memset(q, 0, sizeof(empty_node));
  ngx_queue_init(q);
  assert(ngx_queue_empty(q));

  /* root */
  char* path = argv[1];
  dir_node* root_node;
  root_node = create_root_dir_node(path, dir_cluster);
  assert(root_node);
  int root_fd = dirfd(root_node->dir_ptr);
  ngx_queue_insert_head(q, &dir_cluster[root_fd]);
  dir_node* root = ngx_queue_head(q);
  assert(!ngx_queue_empty(q));

  /* node */
  int node_total = add_nodes(root, dir_cluster, q);
  printf("node_total: %d\n",node_total + 1);

  dir_node* p;
  ngx_queue_foreach(q, p){
    dump_dir_node(p);
  }
  ngx_queue_foreach(q, p){
    clean_dir_node(p);
  }

  printf("sizeof(dir_cluster): %lu\n",sizeof(dir_cluster));
  return 0;
}
