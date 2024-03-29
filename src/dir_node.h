#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

/* sysctl -A |grep "kern.maxfilesperproc" */
#define MAXFDNUM 60000

typedef struct dir_node_s dir_node;

struct dir_node_s {
  struct dirent* dir_ent;
  DIR*           dir_ptr;
  char*          path;
  dir_node*      parent;
  long           loc;
};

int empty_dir_node ( dir_node* node );
dir_node* create_root_dir_node (char* path,
                      dir_node* slot);
dir_node* create_dir_node (struct dirent* entry,
                 dir_node* parent, 
                 dir_node* slot);
void dump_dir_node(dir_node* node);
void clean_dir_node(dir_node* node);
unsigned int add_root_node ( char* path,
                             dir_node* slot);
unsigned int add_nodes (dir_node* root,
                        dir_node* slot);
unsigned int insert_nodes ( dir_node* root,     
                            dir_node* parent,   
                            dir_node* slot);
unsigned int remove_nodes ( dir_node* root);
unsigned int remove_node ( dir_node* p);
void dir_node_rewind ( dir_node* node );
int check_cbt(const char* path);

extern void dir_cb (EV_P_ ev_io *w, int revents);
extern void file_cb (EV_P_ ev_io *w, int revents);
extern void z_dir ( char* path, char* sub);