#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "eio.h"
#include "ev.h"

int respipe [2];
void
want_poll (void)
{
  char dummy;
  printf ("want_poll ()\n");
  write (respipe [1], &dummy, 1);
}

void
done_poll (void)
{
  char dummy;
  printf ("done_poll ()\n");
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
      printf ("eio_poll () = %d\n", eio_poll ());
    }
  printf ("leaving event loop\n");
}

int
readdir_cb (eio_req *req)
{
  if (EIO_RESULT (req) < 0)
    return 0;

  int i;
  struct eio_dirent *ents = (struct eio_dirent *)req->ptr1;
  char *names = (char *)req->ptr2;

  for (i = 0; i < req->result; ++i)
    {
      struct eio_dirent *ent = ents + i;
      char *name = names + ent->nameofs;
      printf ("name[#%d,%d]: %s, type : %d, inode : %d\n", i, ent->namelen, name, ent->type, ent->inode);
   }

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
main (void)
{
  printf ("pipe ()\n");
  if (pipe (respipe)) abort ();

  printf ("eio_init ()\n");
  if (eio_init (want_poll, done_poll)) abort ();

  do
    {
      eio_lstat ("Watcher", 0, stat_cb, "stat");
      eio_readdir ("Watcher", EIO_READDIR_DENTS|EIO_READDIR_DIRS_FIRST, 0, readdir_cb, "readdir");
      event_loop ();
    }
  while (0);

  return 0;
}

