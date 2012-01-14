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

static dir_node dir_cluster[1024];

DIR*
create_root_dir_node (char* path, dir_node* node) {
  DIR* dirp = opendir(path);
  assert(dirp);
  struct dirent* current = readdir(dirp);
  assert(current);
  assert(!strcmp(current->d_name, "."));
  assert(current->d_type == DT_DIR);
  struct dirent* father = readdir(dirp);                /* bypass .. */
  assert(father);
  assert(!strcmp(father->d_name, ".."));
  assert(father->d_type == DT_DIR);

  node->dir_ent = current;
  node->dir_ptr = dirp;
  node->parent = NULL;
  node->next = NULL;
  node->prev = NULL;

  char* d_name = current->d_name;
  char* root_path = realpath(path, d_name);
  /* try to use dirent allocation */
  if ( strlen(root_path) > MAXPATHLEN - 3  ) {
    node->path.name = strdup(root_path);
    node->path.allocated = 1;
  } else {
    strcpy(d_name + 2, root_path);    
    node->path.name = d_name + 2;
    node->path.allocated = 0;
  }
  return dirp;
}

DIR*
create_dir_node (struct dirent* entry,
                 dir_node* parent, dir_node* next, dir_node* prev,
                 dir_node* node) {
  
  node->dir_ent = entry;
  node->parent = parent;
  node->next = next;
  node->prev = prev;
  assert(node->parent->dir_ent);

  /* use allocated dirent save path name when possible */
  unsigned int namelen, p_namelen;
  char * d_name = node->dir_ent->d_name;
  char * p_name = node->parent->path.name;
  namelen = strlen(d_name);
  p_namelen = strlen(p_name);
  char* full_path = NULL;
  if ( 2 * namelen + p_namelen + 3  <= MAXPATHLEN ) {
    full_path = d_name + namelen + 1;
    node->path.allocated = 0;
  } else {
    full_path = malloc(namelen + p_namelen + 2);
    assert(full_path);
    node->path.allocated = 1;
  }
  sprintf(full_path, "%s/%s", p_name, d_name);
  full_path[p_namelen + namelen + 1] = 0;
  node->path.name = full_path;

  DIR* dirp = opendir(node->path.name);
  assert(dirp);
  struct dirent* current = readdir(dirp);
  assert(current);
  assert(!strcmp(current->d_name, "."));
  assert(current->d_type == DT_DIR);
  struct dirent* father = readdir(dirp);                /* bypass .. */
  assert(parent);
  assert(!strcmp(father->d_name, ".."));
  assert(father->d_type == DT_DIR);
  return dirp;
}

void dump_dir_node(dir_node* node) {
  size_t path_len = strlen(node->path.name);
  size_t name_len = strlen(node->dir_ent->d_name);
  printf("\nnode->path.name[%lu]: %s\n", path_len, node->path.name);
  printf("node->dir_ent->d_name[%lu]: %s\n", name_len, node->dir_ent->d_name);
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
  dir_node* slot = (dir_node*)(dir_cluster);
  DIR* dp = create_root_dir_node(path, slot);
  assert(dp);
  ngx_queue_insert_head(q, slot);
  dir_node* root = ngx_queue_head(q);
  assert(!ngx_queue_empty(q));

  /* node */
  struct dirent* ent2 = readdir(dp);
  assert(ent2);
  dir_node* slot2 = (dir_node*)(dir_cluster + 1);
  DIR* dp2 = create_dir_node(ent2, 
                             root, root, root,
                             slot2);
  assert(dp2);
  ngx_queue_insert_tail(q, slot2);

  dump_dir_node(root);
  dump_dir_node(ngx_queue_head(q));
  dump_dir_node(ngx_queue_last(q));
  
  printf("sizeof(dir_cluster): %lu\n",sizeof(dir_cluster));
  return 0;
}
