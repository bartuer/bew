//
//  Simple usage
//       - no magic language binding
//       - no fantanstic architecture routing pattern
//       - process fork
//  Single subscribe
//       - only one address
//       - only one title
//       - only sink type
//  String message
//       - no multipart
//       - no binary
//       - no payload format
//       - no protocol at all
//  Fast implement
//       - no malloc/free
//       - no while(1)
//

#include <czmq.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

static int job_count = 0;

static int
subscriber_cb (zloop_t *loop, zmq_pollitem_t *item, void *arg) {
  zmq_msg_t message;
  zmq_msg_init (&message);
  if (zmq_recv (item->socket, &message, 0)) {
    return 1;
  }  
  size_t size = zmq_msg_size (&message);
  if ( arg != NULL ) {

    int pipes[2];
    assert(pipe(pipes) == 0);
    pid_t child_pid = vfork();
    assert(child_pid != -1);
    char ** args = (char **)arg;
    
    if(child_pid == 0) {        /* child */
      close(pipes[1]);          /* close write end */
      dup2(pipes[0], 0);        /* duplicate read end */
      close(pipes[0]);          /* close read end */

      job_count += 1;
      int ret = execvp(args[0], args);
      if ( ret == -1 ) {        /* seldom return, except syscall failed */
        perror(strerror(errno));
        exit(1);
      } 

    } else {                    /* parent */
      close(pipes[0]);          /* close read end */
      int ret1 = write(pipes[1], zmq_msg_data(&message), size);
      int ret2 = write(pipes[1], "\n", 1);
      assert(ret1 != -1 && ret2 != -1);
      close(pipes[1]);          /* after send message to child, close write end */

      int status;
      pid_t cpid = wait(&status);
      if ( !WIFEXITED(status) ) {
        printf("child process %d failed when %s\n", cpid, args[0]);
      } else {
        /* printf("child process done: %d, total %d\n", cpid, job_count); */
      }
    }

  } else {
    int ret1 = write(1, zmq_msg_data(&message), size);
    int ret2 = write(1, "\n", 1);
    assert(ret1 != -1 && ret2 != -1);
  }
  zmq_msg_close (&message);

  /* Returns 0 if OK, -1 if there was an error */
  return 0;
}

/* url subtitle arguments */
int main (int argc, char *argv []) {
  void *context = zmq_init (1);

  void *subscriber = zmq_socket (context, ZMQ_SUB);
  size_t title_len = 0;
  assert(zmq_connect (subscriber, argv[1]) == 0);
  assert(title_len = strlen(argv[2]) > 1);
  assert(zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, argv[2], title_len) == 0);

  Bool verbose = 0;
  zloop_t *loop = zloop_new ();
  assert (loop);
  zloop_set_verbose (loop, verbose);

  if ( argv[3] ) {
    pid_t p = getpid();
    int fd = open(argv[3], O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IXOTH);
    assert(fd != -1);
    char pid[16];
    sprintf(pid, "%d", p);
    int l = write(fd, pid, strlen(pid));
    assert(l > 0);
    printf("pid: %s\n",pid);
    close(fd);
  }

  zmq_pollitem_t sub_event = { subscriber, 0, ZMQ_POLLIN };
  if ( argc > 4 ) {
    assert(zloop_poller (loop, &sub_event, subscriber_cb, (void*)(argv + 4)) == 0);    
  } else {
    assert(zloop_poller (loop, &sub_event, subscriber_cb, NULL) == 0);    
  }

  zloop_start (loop);

  zloop_destroy (&loop);
  assert (loop == NULL);
  zmq_close (subscriber);
  zmq_term (context);

  return 0;
}
