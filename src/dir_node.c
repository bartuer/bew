#include "dir_node.h"
#include "cbt.h"

extern dir_node dir_cluster[MAXFDNUM];
extern ev_io dir_watcher[MAXFDNUM];
extern struct ev_loop* loop;
extern void* publisher;
cbt_tree cbt = {0};

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
  node.parent = &node;
  /* add to queue maybe change these 2 pointer */

  char* d_name = current->d_name;
  char* root_path = realpath(path, d_name + 2);
  node.path = strdup(root_path);

  int fd = dirfd(dirp);
  assert(fd < MAXFDNUM);
  assert(empty_dir_node(slot + fd));
  memcpy((dir_node*)(slot + fd), &node, sizeof(dir_node));

  ev_io_init(&dir_watcher[fd], dir_cb, fd, EV_LIBUV_KQUEUE_HACK);
  ev_io_start(loop, &dir_watcher[fd]);
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
  ev_io_init(&dir_watcher[fd], dir_cb, fd, EV_LIBUV_KQUEUE_HACK);
  ev_io_start(loop, &dir_watcher[fd]);
  return slot + fd;
}


void dump_dir_node(dir_node* node) {
  if ( empty_dir_node(node) ) {
    printf(" cleaned node; \n");
  } else {
    size_t path_len = strlen(node->path);
    size_t name_len = strlen(node->dir_ent->d_name);
    printf("\nnode->path[%lu]: %s\n", path_len, node->path);
    printf("node.->dd_fd: %d\n",dirfd(node->dir_ptr));
    printf("node.->d_name[%lu]: %s\n", name_len, node->dir_ent->d_name);
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
add_nodes (dir_node* root, dir_node* slot) {
  struct dirent* ent;
  unsigned int sum = 0;
  while ( (ent = readdir(root->dir_ptr)) ) {
    assert(ent);
    if ( ent->d_type == DT_DIR  ) {
      dir_node* node;
      node = create_dir_node(ent, root, slot);
      assert(node);
      int fd = dirfd(node->dir_ptr);

      sum++;
      cbt_insert(&cbt, node->path, &fd);
      z_dir(slot[fd].path, "dir add");
      sum += add_nodes(node, slot);
    } else {
      unsigned int namelen, p_namelen;
      char * d_name = ent->d_name;
      char * p_name = root->path;
      namelen = strlen(d_name);
      p_namelen = strlen(p_name);
      char* path = NULL;
      path = malloc(namelen + p_namelen + 2);
      assert(path);
      sprintf(path, "%s/%s", p_name, d_name);
      if ( !cbt_contains(&cbt, path)) {
        int fd = open(path, O_NONBLOCK|O_RDONLY|O_CLOEXEC);
        if ( fd < 0 ) {
           printf("%s open error %s\n", path, strerror(errno));
           abort();
        }
        ev_io_init(&dir_watcher[fd], file_cb, fd, EV_LIBUV_KQUEUE_HACK);
        ev_io_start(loop, &dir_watcher[fd]);
        assert(empty_dir_node(&dir_cluster[fd]));
        memset(&dir_cluster[fd], 0, sizeof(dir_node));
        dir_cluster[fd].path = path;
        cbt_insert(&cbt, path, &fd);
        z_dir(path, "file add");
      } else {
        free(path);
      }
    }
  }
  return sum;
}

unsigned int
add_root_node ( char* path, dir_node* slot ) {
  unsigned int sum = 0;
  dir_node* root_node;
  root_node = create_root_dir_node(path, dir_cluster);
  assert(root_node);
  int root_fd = dirfd(root_node->dir_ptr);
  cbt_insert(&cbt, root_node->path, &root_fd);
  z_dir(path, "dir add");
  return add_nodes(root_node, slot);
}

unsigned int
insert_nodes ( dir_node* root,     /* root of tree to be inserted*/
               dir_node* parent,   /* inserted subtree's parent */
               dir_node* slot) {   /* operate on this heap */
  unsigned sum = 0;
  assert(!empty_dir_node(root));
  assert(parent);

  DIR* root_dirp = root->dir_ptr;
  int root_fd = dirfd(root_dirp);
  root->parent = parent;
  sum++;
  cbt_insert(&cbt, root->path, &root_fd);
  z_dir(root->path, "dir add");
  sum += add_nodes(root, slot);
  return sum;
}

static int
remove_nodes_cb ( const char *elem, void* value, void *arg ) {
  if ( value == NULL ) {
    return 0;
  }

  int fd  = *(int*)value;
  dir_node* p = &dir_cluster[fd];

  if ( p->dir_ptr == NULL ) {
    /* do not handle file watcher */
    return 0;
  }
  ev_io_stop(loop, &dir_watcher[fd]);
  z_dir((char*)elem, "dir remove");
  if ( !empty_dir_node(&dir_cluster[fd]) ) {
    clean_dir_node(p);        
  }
  cbt_delete(&cbt, elem);
  return 1;
}

unsigned int
remove_nodes ( dir_node* root) {
  unsigned int sum = 0;
  if ( cbt_contains(&cbt, root->path) ) {
    cbt_allprefixed(&cbt, root->path, remove_nodes_cb, NULL);      
  }

  return sum;
}

unsigned int
remove_node ( dir_node* p ) {
  if ( !empty_dir_node(p) ) {
    int fd = p - &dir_cluster[0];
    assert(p == &dir_cluster[fd]);
    ev_io_stop(loop, &dir_watcher[fd]);
    /*
      it is posssible subdir node remove before parent:
      if subdir remove event arrive before parent dir remove event
    */
    z_dir(p->path, "dir remove");
    clean_dir_node(p);    
  }
  return 1;
}

void
dir_node_rewind ( dir_node* node ) {
  if ( !empty_dir_node(node) ) {
    DIR* dirp = node->dir_ptr;
    assert(dirp);
    assert(node->loc != 0);
    seekdir(dirp, node->loc);
  }
}

static int total_dir_watcher = 0;

static int
check_cb ( const char *elem, void* value, void *arg ) {
   if ( value == NULL ) {
    return 0;
  }
  (*((unsigned int *)arg))++;
  int fd  = *(int*)value;
  printf("%s [%d]\n", elem, fd);
  return 1;
}

int
check_cbt(const char* path) {
  int count = 0;
  cbt_allprefixed(&cbt, path, check_cb, &count);
  if ( total_dir_watcher != count ) {
    total_dir_watcher = count;
  }
  return count;
}