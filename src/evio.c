#include <czmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include "eio.h"
#include "ev.h"
#include "dir_node.h"
#include "cbt.h"
#include <pthread.h>


dir_node dir_cluster[MAXFDNUM];
ev_io dir_watcher[MAXFDNUM];
void* publisher;
struct ev_loop *loop;
extern cbt_tree cbt;

static char pwd[MAXPATHLEN];               /* path concat buffer pointer*/
static time_t now;                        /* current time */
static int* evented_fd;
static dir_node empty_node;
static ev_timer timeout_watcher;
static ev_timer suicide_watcher;
static ev_idle repeat_watcher;
static ev_async ready_watcher;
static ev_io cmd_watcher;
static char root_dir_arg[MAXPATHLEN];

void
z_dir ( char* path, char* sub) {
  char update[MAXPATHLEN + 256];
  memset(update, 0, sizeof(update));
  sprintf(update, "direvent %s: %s\n", sub, path);
  printf("%s", update);
  zstr_send(publisher, update);
}

int
later_than(time_t time1, time_t time2)
{
  if (time1 < time2)
    return 1 ;
  else if (time1 > time2)
    return 0 ;
  else
    return 1 ;
}

# define EV_INOTIFY_BUFSIZE (sizeof (struct inotify_event) * 2 + NAME_MAX)

void file_cb (EV_P_ ev_io *w, int revents);


static void
timeout_cb (EV_P_ ev_timer *w, int revents)
{
  printf("cbt count: %d\n", check_cbt(root_dir_arg));
}

static void
suicide_cb (EV_P_ ev_timer *w, int revents)
{
  printf("suicide: %s\n","!");
  ev_break (EV_A_ EVBREAK_ONE);
}

static void
repeat (EV_P_ ev_idle *w, int revents)
{
  if (eio_poll () != -1) {
    ev_idle_stop (EV_A_ w);
  }
}

static void
ready (EV_P_ ev_async *w, int revents)
{
  if (eio_poll () == -1)
    ev_idle_start (EV_A_ &repeat_watcher);
}

static void
want_poll (void)
{
  ev_async_send (loop, &ready_watcher);
}

typedef struct {
  char* base;
  size_t len;
} buf_t;


static void
cmd_cb (struct ev_loop *loop, ev_io *w, int revents)
{
  char line[8096];
  memset(line, 0, 8096);
  buf_t cmd = {line,8096};
  read(w->fd, cmd.base, cmd.len);
  printf("line: %s\n",line);
  pthread_t t;
  pthread_create(&t, NULL, (void *)system, (void *)line);
  pthread_detach(t);
  /*
    now in test mode, suicide after 15 seconds
  */
  ev_timer_init (&suicide_watcher, suicide_cb, 15, 0.);
  ev_timer_start (loop, &suicide_watcher);
  ev_io_stop (loop, w);
}

void
dir_cb (EV_P_ ev_io *w, int revents)
{
  char buf [EV_INOTIFY_BUFSIZE];
  int ofs;
  int len = read (w->fd, buf, sizeof (buf));

  int fd = w->fd;
  char *path = dir_cluster[fd].path;

  for (ofs = 0; ofs < len; )
    {
      struct inotify_event *ev = (struct inotify_event *)(buf + ofs);
      if ( ev->len ) {
        memset(pwd, 0, sizeof(MAXPATHLEN));
        snprintf(pwd, MAXPATHLEN, "%s/%s", path, ev->name);        
      }

      switch ( ev->mask )
        {
        case IN_ATTRIB:
          break;
        case IN_CREATE:
          printf("direvent file add: %s\n",ev->len ? pwd : "none");
          break;
        case IN_MODIFY:
          printf("direvent file change: %s\n",ev->len ? pwd : "none");
          break;
        case IN_DELETE:
          printf("direvent file delete: %s\n",ev->len ? pwd : "none");
          break;
        case IN_DELETE_SELF:
        case IN_MOVE_SELF:
          remove_nodes(&dir_cluster[fd]);
          break;
        case IN_MOVED_FROM:
          printf("direvent file mv from: %s\n",ev->len ? pwd : "none");
          break;
        case IN_MOVED_TO:
          printf("direvent file mv to: %s\n",ev->len ? pwd : "none");
          break;
        case IN_CLOSE_WRITE:
        case IN_CLOSE_NOWRITE:
          printf("direvent file close: %s\n",ev->len ? pwd : "none");
          break;
        default:
          if ( ev->len ) {
            struct stat st;
            int ret = stat(pwd, &st);
            if ( ret != -1 ) {
              if ( S_ISDIR(st.st_mode)) {
                if ( !cbt_contains(&cbt, pwd) ) {
                  dir_node *father = &dir_cluster[fd];
                  assert(!empty_dir_node(father));
                  dir_node* root = create_dir_node(ev->name, father, dir_cluster);
                  assert(root);
                  add_nodes(root, dir_cluster);
                }
              }
            }
          }
        }
      ofs += sizeof (struct inotify_event) + ev->len;
    }
}

int
main (int argc, char**argv)
{
  if ( !argv[1] ) {
    printf("need dir parameter\n");
    exit(1);
  }
  void *context = zmq_init (1);
  publisher = zmq_socket (context, ZMQ_PUB);
  zmq_bind (publisher, "tcp://*:5556");

  /* map fd : path */
  memset(dir_cluster, 0, sizeof(dir_cluster));

  loop = ev_loop_new (EVBACKEND_EPOLL);

  if ( argv[2] ) {
    int seconds = atoi(argv[2]);
    assert(seconds > 1);
    ev_timer_init (&suicide_watcher, suicide_cb, seconds, 0.);
    ev_timer_start (loop, &suicide_watcher);
  }
  
  ev_io_init (&cmd_watcher, cmd_cb, 0, EV_READ);
  ev_io_start (loop, &cmd_watcher);

  /* ev_timer_init (&timeout_watcher, timeout_cb, 1, 1.); */
  /* ev_timer_start (loop, &timeout_watcher); */

  strcpy(root_dir_arg, argv[1]);
  add_root_node(argv[1], dir_cluster);
  
 
  ev_idle_init (&repeat_watcher, repeat);

  ev_async_init (&ready_watcher, ready);
  ev_async_start (loop, &ready_watcher);

  if (eio_init (want_poll, 0)) {
    abort ();
  };
  
  ev_run (loop, 0);
  cbt_clear(&cbt);
  zmq_close (publisher);
  zmq_term (context);
  return 0;
}
