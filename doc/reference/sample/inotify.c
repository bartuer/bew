#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include "eio.h"
#include "ev.h"

char pwd[MAXPATHLEN];
time_t now;
char* root = NULL;

int respipe [2];
ev_timer timeout_watcher;
static ev_idle repeat_watcher;
static ev_async ready_watcher;
static ev_io dir_watcher;
static ev_io cmd_watcher;
static struct ev_loop *loop;

static void
timeout_cb (EV_P_ ev_timer *w, int revents)
{
  printf("suicide\n");
  ev_break (EV_A_ EVBREAK_ONE);
}

# define EV_INOTIFY_BUFSIZE (sizeof (struct inotify_event) * 2 + NAME_MAX)

static void
dir_cb (EV_P_ ev_io *w, int revents)
{
  char buf [EV_INOTIFY_BUFSIZE];
  int ofs;
  int len = read (w->fd, buf, sizeof (buf));

  for (ofs = 0; ofs < len; )
    {
      struct inotify_event *ev = (struct inotify_event *)(buf + ofs);

      switch ( ev->mask )
        {
        case IN_ATTRIB:
          printf("event: %s, %s\n","IN_ATTRIB", ev->len ? ev->name : "none");
        case IN_CREATE:
          printf("event: %s, %s\n","IN_CREATE", ev->len ? ev->name : "none");
          break;
        case IN_MODIFY:
          printf("event: %s, %s\n","IN_MODIFY", ev->len ? ev->name : "none");
          break;
        case IN_DELETE:
          printf("event: %s, %s\n","IN_DELETE", ev->len ? ev->name : "none");
          break;
        case IN_DELETE_SELF:
          printf("event: %s, %s\n","IN_DELETE_SELF", ev->len ? ev->name : "none");
          break;
        case IN_MOVED_FROM:
          printf("event: %s, %s\n","IN_MOVED_FROM", ev->len ? ev->name : "none");
          break;
        case IN_MOVED_TO:
          printf("event: %s, %s\n","IN_MOVED_TO", ev->len ? ev->name : "none");
          break;
        case IN_CLOSE_WRITE:
        case IN_CLOSE_NOWRITE:
          printf("event: %s, %s\n","IN_CLOSE", ev->len ? ev->name : "none");
          break;
        default:
          printf("event: %d, %s cookie[%d]\n",ev->mask, ev->len ? ev->name : "none", ev->cookie);
        }
      ofs += sizeof (struct inotify_event) + ev->len;
    }
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
  ev_io_stop (loop, w);
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

int
infy_newfd (void)
{
#if defined (IN_CLOEXEC) && defined (IN_NONBLOCK)
  int fd = inotify_init1 (IN_CLOEXEC | IN_NONBLOCK);
  if (fd >= 0)
    return fd;
#endif
  return inotify_init ();
}

int main(int argc, char *argv[])
{
  if ( !argv[1] ) {
    printf("need dir parameter\n");
    exit(1);
  }

  char * argvp = NULL;
  argvp = realpath(argv[1], pwd);

  if ( errno ) {
    printf("%s, %s is not valid directory\n", strerror(errno), pwd);
    exit(1);
  }

  int fd = infy_newfd();
  assert(fd > 0);
  int wd = inotify_add_watch(fd, argv[1],
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
  assert(wd != -1);
  
  loop = ev_loop_new (EVBACKEND_EPOLL);

  ev_io_init (&cmd_watcher, cmd_cb, 0, EV_READ);
  ev_io_start (loop, &cmd_watcher);

  ev_timer_init (&timeout_watcher, timeout_cb, 2, 0.);
  ev_timer_start (loop, &timeout_watcher);
  
  ev_io_init (&dir_watcher, dir_cb, fd, EV_READ);
  ev_io_start (loop, &dir_watcher);

  ev_idle_init (&repeat_watcher, repeat);

  ev_async_init (&ready_watcher, ready);
  ev_async_start (loop, &ready_watcher);

  if (eio_init (want_poll, 0)) {
    abort ();
  };

  ev_run (loop, 0);

  return 0;
}
