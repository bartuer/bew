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
#include <sys/param.h>
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


int
later_than(time_t time1, struct timespec time2)
{
  if (time1 < time2.tv_sec)
    return 1 ;
  else if (time1 > time2.tv_sec)
    return 0 ;
  else
    return 1 ;
}

void file_cb (EV_P_ ev_io *w, int revents);

int
readdir_cb (eio_req *req)
{
  if (EIO_RESULT (req) < 0) {
    struct stat st;
    int ret = lstat(req->ptr1, &st);
    if (ret && errno == ENOENT) {
      int fd = *((int*)req->data);
      free(req->data);
      evented_fd = NULL;
      remove_nodes(&dir_cluster[fd]);
    }
    return 0;
  }

  int fd = *((int*)req->data);
  free(req->data);
  evented_fd = NULL;

  char* req_data;
  req_data = dir_cluster[fd].path;
  
  struct eio_dirent *ents = (struct eio_dirent *)req->ptr1;
  
  if ( ents->type != EIO_DT_DIR ) {
    struct stat leaf_st ;
    lstat(req_data, &leaf_st);
    if (!later_than(now, leaf_st.st_ctimespec)) {
      printf("req_data: %s\n",req_data);
      return 0;
    }
  }
  
  char *names = (char *)req->ptr2;
  
  int i;
  for (i = 0; i < req->result; ++i)
    {
      struct eio_dirent *ent = ents + i;
      char *name = names + ent->nameofs;

      snprintf(pwd, MAXPATHLEN, "%s/%s", req_data, name);

      if ( ent->type ==  EIO_DT_DIR) {
        dir_node *father = &dir_cluster[fd];
        dir_node *son;
        DIR* father_dirp = father->dir_ptr;
        dir_node_rewind(father);
        struct dirent* son_ent = NULL;
        while ((son_ent = readdir(father_dirp)))  {
          if ( son_ent->d_type == DT_DIR &&
               strcmp(son_ent->d_name, name) == 0
               ) {
            break;
          } else {
            son_ent = NULL;
          }
        }
        if ( son_ent == NULL ) {
          printf("can not find %s in %s\n", name, father->path);
        }

        if ( !cbt_contains(&cbt, pwd) ) {
          assert(son_ent);
          son = create_dir_node(son_ent, father, dir_cluster);
          assert(son);
          insert_nodes(son, father, dir_cluster);
        }
      } else {
        struct stat st;
        lstat(pwd, &st);
        if (later_than(now, st.st_mtimespec) || later_than(now, st.st_ctimespec)) {
          int fd = open(pwd, O_NONBLOCK|O_RDONLY|O_CLOEXEC);
          ev_io_init(&dir_watcher[fd], file_cb, fd, EV_LIBUV_KQUEUE_HACK);
          ev_io_start(loop, &dir_watcher[fd]);
          assert(empty_dir_node(&dir_cluster[fd]));
          dir_cluster[fd].path = strdup(pwd);
          dir_cluster[fd].parent = NULL;
          dir_cluster[fd].dir_ptr = NULL;
          char update[MAXPATHLEN + 256];
          memset(update, 0, sizeof(update));
          sprintf(update, "direvent file add: %s/%s\n", req_data, name);
          printf("%s", update);
          zstr_send(publisher, update);
        }
      }
    }

  return 0;
}

static void
timeout_cb (EV_P_ ev_timer *w, int revents)
{
  printf("cbt count: %d\n", check_cbt("/private/tmp/a"));
}

static void
suicide_cb (EV_P_ ev_timer *w, int revents)
{
  ev_break (EV_A_ EVBREAK_ONE);
}

static void
repeat (EV_P_ ev_idle *w, int revents)
{
  if (eio_poll () != -1)
    ev_idle_stop (EV_A_ w);
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
file_cb (EV_P_ ev_io *w, int revents)
{
  assert(revents == EV_LIBUV_KQUEUE_HACK);
  time(&now);
  printf("\nFILE_EVENT[%d]: %s\n", w->fd, dir_cluster[w->fd].path);
  struct stat st;
  int ret = stat(dir_cluster[w->fd].path, &st);
  if ( ret < 0 ) {
    printf("direvent file delete: %s\n", dir_cluster[w->fd].path);
    close(w->fd);
    ev_io_stop(loop, w);
    free(dir_cluster[w->fd].path);
    memset(&dir_cluster[w->fd], 0, sizeof(dir_node));
  } else {
    printf("direvent file change: %s\n", dir_cluster[w->fd].path);
  }
}

void
dir_cb (EV_P_ ev_io *w, int revents)
{
  assert(revents == EV_LIBUV_KQUEUE_HACK);

  time(&now);
  printf("\nEVENT[%d]: %s\n", w->fd, dir_cluster[w->fd].path);
  evented_fd = malloc(sizeof(int));
  *evented_fd = w->fd;
  eio_readdir(dir_cluster[w->fd].path,
              EIO_READDIR_DENTS|EIO_READDIR_DIRS_FIRST,
              0,
              readdir_cb,
              evented_fd);
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

  loop = ev_loop_new (EVBACKEND_KQUEUE);

  if ( argv[2] ) {
    int seconds = atoi(argv[2]);
    assert(seconds > 10);
    ev_timer_init (&suicide_watcher, suicide_cb, seconds, 0.);
    ev_timer_start (loop, &suicide_watcher);
  }
  
  ev_io_init (&cmd_watcher, cmd_cb, 0, EV_READ);
  ev_io_start (loop, &cmd_watcher);

  ev_timer_init (&timeout_watcher, timeout_cb, 1, 1.);
  ev_timer_start (loop, &timeout_watcher);
 
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