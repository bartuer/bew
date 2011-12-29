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
#include "eio.h"
#include "ev.h"

char* pwd = NULL;

/* use heap simulate stack, so readdir can be called recursively */
char **freelist = NULL;
size_t freelist_len = 0;

int respipe [2];
void
want_poll (void)
{
  char dummy;
  /* printf ("want_poll ()\n"); */
  write (respipe [1], &dummy, 1);
}

void
done_poll (void)
{
  char dummy;
  /* printf ("done_poll ()\n"); */
  read (respipe [0], &dummy, 1);
}

void
event_loop (void)
{
  // an event loop. yeah.
  struct pollfd pfd;
  pfd.fd     = respipe [0];
  pfd.events = POLLIN;

  printf ("\nentering event loop\n");
  while (eio_nreqs ())
    {
      poll (&pfd, 1, -1);
      eio_poll();
      /* printf ("eio_poll () = %d\n", eio_poll ()); */
    }
  printf ("leaving event loop\n");
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

  int i;
  struct eio_dirent *ents = (struct eio_dirent *)req->ptr1;
  char *names = (char *)req->ptr2;

  if ( freelist ) {
    freelist = realloc(freelist, (req->result + freelist_len) * sizeof(char*));    
  } else {
    freelist = calloc(req->result, sizeof(char*));
  }

  for (i = 0; i < req->result; ++i)
    {
      struct eio_dirent *ent = ents + i;
      char *name = names + ent->nameofs;
      freelist[freelist_len + i]  = 0;
      printf ("name[#%d]: %s/%s\n", i, req->data, name);
      if ( ent->type ==  EIO_DT_DIR) {
        snprintf(pwd, MAXPATHLEN, "%s/%s", req->data, name);
        freelist[freelist_len + i] = eio_readdir_r(pwd, EIO_READDIR_DENTS|EIO_READDIR_DIRS_FIRST, 0, readdir_cb);
      }
   }
  freelist_len += req->result;
   return 0;
}

int
stat_cb (eio_req *req)
{
  struct stat *buf = EIO_STAT_BUF (req);

  if (req->type == EIO_FSTAT)
    printf ("fstat_cb = %d\n", EIO_RESULT (req));
  else
    printf ("stat_cb(%s) = %d\n", EIO_PATH (req), EIO_RESULT (req));

  if (!EIO_RESULT (req))
    printf ("stat size %d mtime 0%o\n", buf->st_size, buf->st_ctimespec);

  return 0;
}

int
main (int argc, char**argv)
{
  char pwd_buf[MAXPATHLEN];
  realpath(argv[1], pwd_buf);
  pwd = pwd_buf;
  printf ("pipe ()\n");
  if (pipe (respipe)) abort ();


  printf ("eio_init ()\n");
  if (eio_init (want_poll, done_poll)) abort ();
  snprintf(pwd, "%s/%s", pwd, "Watcher");

  /* for free initial path duplication */
  char* root = NULL;
  do
    {
      eio_lstat (argv[1], 0, stat_cb, "stat");
      root = eio_readdir_r (pwd, EIO_READDIR_DENTS|EIO_READDIR_DIRS_FIRST, 0, readdir_cb);
      event_loop ();
    }
  while (0);
  free(root);
  int i;

  printf("count: %d\n",freelist_len); 
  if ( freelist ) {
    for (i = 0; i < freelist_len; ++i ) {
      if (freelist[i]) {
         free(freelist[i]);
     }
   }
    free(freelist);
  }

  return 0;
}