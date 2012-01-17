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

char pwd[MAXPATHLEN];               /* path concat buffer pointer*/
char pwdb[MAXPATHLEN];               /* path concat buffer pointer*/
char pwdc[MAXPATHLEN];
char* fdmap[10];

time_t now;                     /* current time */
DIR* dp = NULL;
char* root = NULL;
/* use heap simulate stack, so readdir can be called recursively */
char **freelist = NULL;
size_t freelist_len = 0;


int respipe [2];

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

void
eio_readdir_r(const char *path, int flags, int pri, eio_cb cb, int i)
{
  freelist[i] = strdup(path);
  eio_readdir(freelist[i], flags, pri, cb, freelist[i]);
  return;
}

int
readdir_cb (eio_req *req)
{
  if (EIO_RESULT (req) < 0) {
    struct stat st;
    int ret = lstat(req->ptr1, &st);
    if (ret && errno == ENOENT) {
       printf("remove subdir: %s\n",req->ptr1);
    }
    return 0;
  }
  
  struct eio_dirent *ents = (struct eio_dirent *)req->ptr1;
  
  if ( ents->type != EIO_DT_DIR ) {
    struct stat leaf_st ;
    lstat(req->data, &leaf_st);
    if (!later_than(now, leaf_st.st_ctimespec)) {
      return 0;
    }
  }
  
  char *names = (char *)req->ptr2;

  if ( freelist ) {
    freelist = realloc(freelist, (req->result + freelist_len) * sizeof(char*));    
  } else {
    freelist = calloc(req->result, sizeof(char*));
  }
  
  int i;
  for (i = 0; i < req->result; ++i)
    {
      struct eio_dirent *ent = ents + i;
      char *name = names + ent->nameofs;
      freelist[freelist_len + i]  = 0;

      snprintf(pwd, MAXPATHLEN, "%s/%s", (char*)req->data, name);
      struct stat st;
      lstat(pwd, &st);
      if ( ent->type ==  EIO_DT_DIR) {
        if ( later_than(now, st.st_birthtimespec) ) {
          printf ("add subdir:  %s/%s\n", (char*)req->data, name);
          eio_readdir_r(pwd, EIO_READDIR_DENTS|EIO_READDIR_DIRS_FIRST, 0, readdir_cb, freelist_len + i);
        }
        else if ( later_than(now, st.st_ctimespec) ) {
          printf ("change subdir:  %s/%s\n", (char*)req->data, name);
        }
      } else {
        if (later_than(now, st.st_ctimespec)) {
          printf ("file change:  %s/%s\n", (char*)req->data, name);
        }
      }
    }
  freelist_len += req->result;

  return 0;
}

ev_timer timeout_watcher;
static ev_idle repeat_watcher;
static ev_async ready_watcher;
static ev_io dir_watcher_a;
static ev_io dir_watcher_b;
static ev_io dir_watcher_c;

static ev_io cmd_watcher;
static struct ev_loop *loop;

static void
timeout_cb (EV_P_ ev_timer *w, int revents)
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

  ev_io_stop (loop, w);
  char line[256];
  memset(line, 0, 256);
  buf_t cmd = {line,256};
  read(w->fd, cmd.base, cmd.len);
  printf("line: %s\n",line);

  pid_t child_pid;
  child_pid = fork();
  if ( !child_pid ) {
    return;
  } else {
    char cmd[MAXPATHLEN];
    snprintf(cmd, MAXPATHLEN, "%s", line);
    system(cmd);
  }
}

static void
dir_cb (EV_P_ ev_io *w, int revents)
{
  assert(revents == EV_LIBUV_KQUEUE_HACK);
  freelist = NULL;
  freelist_len = 0;

  time(&now);
  printf("fdmap[%d]: %s\n",w->fd, fdmap[w->fd]);
  root = strdup(fdmap[w->fd]);
  eio_readdir(root, EIO_READDIR_DENTS|EIO_READDIR_DIRS_FIRST, 0, readdir_cb, root);
}

int
main (int argc, char**argv)
{
  if ( !argv[1] ) {
    printf("need dir parameter\n");
    exit(1);
  }

  realpath(argv[1], pwd);
  dp = opendir(pwd);
  if ( errno ) {
    printf("%s, %s is ? not valid directory\n", strerror(errno), pwd);
    exit(1);
  }
  int dfd = dirfd(dp);
  fdmap[dfd] = pwd;
  
  realpath(argv[2], pwdb);
  DIR* dpb = opendir(pwdb);
  if ( errno ) {
    printf("%s, %s is ? not valid directory\n", strerror(errno), pwd);
    exit(1);
  }
  int dfdb = dirfd(dpb);
  fdmap[dfdb] = pwdb;

  realpath(argv[3], pwdc);
  DIR* dpc = opendir(pwdc);
  if ( errno ) {
    printf("%s, %s is not ? valid directory\n", strerror(errno), pwd);
    exit(1);
  }
  int dfdc = dirfd(dpc);
  fdmap[dfdc] = pwdc;

  loop = ev_loop_new (EVBACKEND_KQUEUE);

  ev_io_init (&cmd_watcher, cmd_cb, 0, EV_READ);
  ev_io_start (loop, &cmd_watcher);

  ev_timer_init (&timeout_watcher, timeout_cb, 1, 0.);
  ev_timer_start (loop, &timeout_watcher);
  
  ev_io_init (&dir_watcher_a, dir_cb, dfd, EV_LIBUV_KQUEUE_HACK);
  ev_io_init (&dir_watcher_b, dir_cb, dfdb, EV_LIBUV_KQUEUE_HACK);    
  ev_io_init (&dir_watcher_c, dir_cb, dfdc, EV_LIBUV_KQUEUE_HACK);    
  ev_io_start (loop, &dir_watcher_a);
  ev_io_start (loop, &dir_watcher_b);
  ev_io_start (loop, &dir_watcher_c);
  
  ev_idle_init (&repeat_watcher, repeat);

  ev_async_init (&ready_watcher, ready);
  ev_async_start (loop, &ready_watcher);

  if (eio_init (want_poll, 0)) {
    abort ();
  };
  
  ev_run (loop, 0);

  if ( root ) {
    free(root);
  }

  /* free all allocated path */
  int i;
  if ( freelist ) {
    for (i = 0; i < freelist_len; ++i ) {
      if (freelist[i]) {
        free(freelist[i]);
      }
    }
    free(freelist);
  }
  if ( dp ) {
    closedir(dp);
  }

  return 0;
}