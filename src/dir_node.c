#include "dir_node.h"
#include <fcntl.h>
#include <sys/inotify.h>
#include <ulimit.h>
#include "cbt.h"

extern dir_node dir_cluster[MAXFDNUM];
extern ev_io dir_watcher[MAXFDNUM];
extern struct ev_loop* loop;
extern void* publisher;
cbt_tree cbt = {0};

int
infy_add ( char* path ) {
  int fd;
#if defined (IN_CLOEXEC) && defined (IN_NONBLOCK)
  fd = inotify_init1 (IN_CLOEXEC | IN_NONBLOCK);
#else
  fd = inotify_init ();    
#endif

  assert(fd > 0);
  int wd = inotify_add_watch(fd, path,
                             IN_CREATE |
                             IN_MOVED_FROM |
                             IN_MOVED_TO |
                             IN_MODIFY |
                             IN_DELETE |
                             IN_DELETE_SELF |
                             IN_MOVE_SELF |
                             IN_CLOSE |
                             IN_ATTRIB |
                             IN_MASK_ADD
                             );

  if ( wd == -1 ) {
    printf("path make wd == -1: %s\n",path);
  }
  assert(wd != -1);
  return fd;
}

int
empty_dir_node ( dir_node* node ) {
  int parent = node->parent == NULL;
  return parent;
}

dir_node*
create_root_dir_node (char* path, dir_node* slot) {
  dir_node node;
  node.parent = &node;
  /* add to queue maybe change these 2 pointer */

  node.path = strdup(path);
  int fd = infy_add(path);
  assert(fd < MAXFDNUM);
  assert(empty_dir_node(slot + fd));
  memcpy((dir_node*)(slot + fd), &node, sizeof(dir_node));

  ev_io_init(&dir_watcher[fd], dir_cb, fd, EV_READ);
  ev_io_start(loop, &dir_watcher[fd]);
  return slot + fd;
}

dir_node*
create_dir_node (char* d_name,
                 dir_node* parent, 
                 dir_node* slot
                 ) {
  dir_node node;

  node.parent = parent;

  unsigned int namelen, p_namelen;
  char * p_name = parent->path;
  namelen = strlen(d_name);
  p_namelen = strlen(p_name);
  char* full_path = NULL;
  full_path = malloc(namelen + p_namelen + 2);
  assert(full_path);
  sprintf(full_path, "%s/%s", p_name, d_name);
  full_path[p_namelen + namelen + 1] = 0;
  node.path = full_path;

  int fd = infy_add(node.path);
  assert(fd < MAXFDNUM);
  memcpy((dir_node*)(slot + fd), &node, sizeof(node));
  cbt_insert(&cbt, node.path, &fd);
  z_dir(slot[fd].path, "dir add");

  ev_io_init(&dir_watcher[fd], dir_cb, fd, EV_READ);
  ev_io_start(loop, &dir_watcher[fd]);
  return slot + fd;
}


void dump_dir_node(dir_node* node) {
  if ( empty_dir_node(node) ) {
    printf(" cleaned node; \n");
  } else {
    size_t path_len = strlen(node->path);
    printf("\nnode->path[%d]: %s\n", path_len, node->path);
  }
}

void clean_dir_node(dir_node* node) {
  if ( node == NULL ) {
    return;
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
  char * root_path = root->path;
  DIR* root_dirp = opendir(root->path);
  dir_node r = *root;
  assert(root_dirp);

  while ( (ent = readdir(root_dirp)) ) {
    assert(ent);
    if ( ent->d_type == DT_DIR  ) {
      if ( strcmp(ent->d_name, ".") != 0 &&
           strcmp(ent->d_name, "..") != 0) {
        dir_node* node;
        node = create_dir_node(ent->d_name, &r, slot);
        assert(node);
        sum += add_nodes(node, slot);
      }
    }
  }
  closedir(root_dirp);
  return sum;
}

unsigned int
add_root_node ( char* path, dir_node* slot ) {
  unsigned int sum = 0;
  dir_node* root_node;
  root_node = create_root_dir_node(path, dir_cluster);
  assert(root_node);
  int root_fd = infy_add(root_node->path);
  cbt_insert(&cbt, root_node->path, &root_fd);
  z_dir(path, "dir add");
  return add_nodes(root_node, slot);
}

static int
remove_nodes_cb ( const char *elem, void* value, void *arg ) {
  if ( value == NULL ) {
    return 0;
  }

  int fd  = *(int*)value;
  dir_node* p = &dir_cluster[fd];

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
