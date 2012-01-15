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

typedef struct dir_node_s dir_node;

struct dir_node_s {
  struct dirent* dir_ent;
  DIR*           dir_ptr;
  char*          path;
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
  node.path = strdup(root_path);

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

  unsigned int namelen, p_namelen;
  char * d_name = node.dir_ent->d_name;
  char * p_name = node.parent->path;
  namelen = strlen(d_name);
  p_namelen = strlen(p_name);
  char* full_path = NULL;
  full_path = malloc(namelen + p_namelen + 2);
  assert(full_path);
  sprintf(full_path, "%s/%s", p_name, d_name);
  full_path[p_namelen + namelen + 1] = 0;
  node.path = full_path;

  DIR* dirp = opendir(node.path);
  if ( dirp == NULL ) {
    printf("node.path: %s\n",node.path);
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
    size_t path_len = strlen(node->path);
    size_t name_len = strlen(node->dir_ent->d_name);
    printf("\nnode->path[%lu]: %s\n", path_len, node->path);
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

  free(node->path);

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
insert_nodes ( dir_node* root,     /* root of tree to be inserted*/
               dir_node* parent,   /* inserted subtree's parent */
               dir_node* slot,     /* operate on this heap */
               dir_node* queue ) { /* the queue simulate directory tree */
  unsigned sum = 0;
  /* find split point */
  assert(!empty_dir_node(root));
  assert(parent);
  dir_node* p = ngx_queue_next(parent);
  assert(p);
  assert(!empty_dir_node(p));
  
  if (p == ngx_queue_last(queue)) { /* it is tail append */
    DIR* root_dirp = root->dir_ptr;
    int root_fd = dirfd(root_dirp);
    ngx_queue_insert_tail(queue, &slot[root_fd]);
    sum++;
    sum += add_nodes(root, slot, queue);
  } else {
    /* split queue */
    dir_node* last = ngx_queue_last(queue);
    ngx_queue_split(queue, p, last);
    /* install head to splited one */
    dir_node new;
    dir_node* nq = &new;
    ngx_queue_init(nq);
    ngx_queue_insert_head(last, nq);

    /* add nodes */
    DIR* root_dirp = root->dir_ptr;
    int root_fd = dirfd(root_dirp);
    ngx_queue_insert_tail(queue, &slot[root_fd]);
    sum++;
    sum += add_nodes(root, slot, queue);
    
    /* merge queue */
    ngx_queue_add(queue, nq);
    memset(nq, 0, sizeof(new));
  }
  return sum;
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

int
dump_queue ( dir_node* q , char* head_line ) {
  printf("\n%s\n", head_line);
  int count = 0;
  dir_node* p;
  ngx_queue_foreach(q, p){
    dump_dir_node(p);
    count++;
  }
  printf("count: %d\n",count);
  return count;
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