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

char* pwd = NULL;               /* path concat buffer pointer*/
time_t now;                     /* current time */

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

char*
eio_readdir_r(const char *path, int flags, int pri, eio_cb cb)
{
  char *p = strdup(path);
  eio_readdir(p, flags, pri, cb, p);
  return p;
}

int
readdir_cb (eio_req *req)
{
  if (EIO_RESULT (req) < 0)
    return 0;

  
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
        freelist[freelist_len + i] = eio_readdir_r(pwd, EIO_READDIR_DENTS|EIO_READDIR_DIRS_FIRST, 0, readdir_cb);
      }
      if (later_than(now, st.st_ctimespec)) {
        printf ("name[#%d]: %s/%s\n", i, (char*)req->data, name);
      }
    }
  freelist_len += req->result;

  return 0;
}

ev_timer timeout_watcher;
static ev_idle repeat_watcher;
static ev_async ready_watcher;
static ev_io dir_watcher;
static ev_io cmd_watcher;
static struct ev_loop *loop;

static void
timeout_cb (EV_P_ ev_timer *w, int revents)
{
  printf("timeout\n");
  printf("getpid(): %d\n",getpid());
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

void
original_cb_dump (const char* path)
{
  char pwd_buf[MAXPATHLEN];
  /* for free initial path duplication */
  char* root = NULL;
  int i;
  do
    {
      realpath(path, pwd_buf);
      pwd = pwd_buf;
      freelist = NULL;
      freelist_len = 0;

      time(&now);
      now = now - 2 * 3600;      /* should be now, take 2 second before make testing convenient  */
      root = eio_readdir_r (pwd, EIO_READDIR_DENTS|EIO_READDIR_DIRS_FIRST, 0, readdir_cb);
 
      free(root);
      printf("count: %d\n", (int)freelist_len); 
 
      /* free all allocated path */
      if ( freelist ) {
        for (i = 0; i < freelist_len; ++i ) {
          if (freelist[i]) {
            free(freelist[i]);
          }
        }
        free(freelist);
      }
    }
  while (0);
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
    system(line);
  }
}

static void
dir_cb (EV_P_ ev_io *w, int revents)
{
  
  assert(revents == EV_LIBUV_KQUEUE_HACK);
  printf("dir_cb revents: %d\n",revents);
}

int
main (int argc, char**argv)
{
  if ( !argv[1] ) {
    printf("need dir parameter\n");
    exit(1);
  }
  char pwd_buf[MAXPATHLEN];
  char * argvp = NULL;
  argvp = realpath(argv[1], pwd_buf);
  pwd = pwd_buf;
  DIR* dp = opendir(pwd);
  if ( errno ) {
    printf("%s, %s is not valid directory\n", strerror(errno), pwd);
    exit(1);
  }

  int dfd = dirfd(dp);
  loop = ev_loop_new (EVBACKEND_KQUEUE);


   


  
  ev_timer_init (&timeout_watcher, timeout_cb, 5, 0.);
  ev_timer_start (loop, &timeout_watcher);
  
  ev_io_init (&dir_watcher, dir_cb, dfd, EV_LIBUV_KQUEUE_HACK);
  ev_io_start (loop, &dir_watcher);

  ev_io_init (&cmd_watcher, cmd_cb, 0, EV_READ);
  ev_io_start (loop, &cmd_watcher);
  
  ev_idle_init (&repeat_watcher, repeat);

  ev_async_init (&ready_watcher, ready);
  ev_async_start (loop, &ready_watcher);

  if (eio_init (want_poll, 0)) {
    abort ();
  };
  
  ev_run (loop, 0);

  
  return 0;
}