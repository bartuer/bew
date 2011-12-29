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

char* pwd = NULL;
time_t now;

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

      snprintf(pwd, MAXPATHLEN, "%s/%s", req->data, name);
      struct stat st;
      lstat(pwd, &st);
      if ( ent->type ==  EIO_DT_DIR) {
        freelist[freelist_len + i] = eio_readdir_r(pwd, EIO_READDIR_DENTS|EIO_READDIR_DIRS_FIRST, 0, readdir_cb);
      }
      if (later_than(now, st.st_ctimespec)) {
        printf ("name[#%d]: %s/%s\n", i, req->data, name);
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

  if (!EIO_RESULT (req)) {
    printf ("stat size %d mtime 0%o\n", buf->st_size, buf->st_mtimespec);
  }
  return 0;
}

int
main (int argc, char**argv)
{
  char pwd_buf[MAXPATHLEN];

  printf ("pipe ()\n");
  if (pipe (respipe)) abort ();

  printf ("eio_init ()\n");
  if (eio_init (want_poll, done_poll)) abort ();
  

  /* for free initial path duplication */
  char* root = NULL;
  int i;
  do
    {
      realpath(argv[1], pwd_buf);
      pwd = pwd_buf;
      freelist = NULL;
      freelist_len = 0;

      time(&now);
      now = now - 2;      /* should be now, take one hour before make testing convenient  */
      root = eio_readdir_r (pwd, EIO_READDIR_DENTS|EIO_READDIR_DIRS_FIRST, 0, readdir_cb);
      event_loop ();
      free(root);
      printf("count: %d\n", freelist_len); 
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
  while (1);
 
  return 0;
}