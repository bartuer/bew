#include <czmq.h>
#include <ev.h>
#include <eio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

static int
timer_cb (zloop_t *loop, zmq_pollitem_t *item, void *output) {
    
  int zipcode, temperature, relhumidity;
  zipcode     = randof (100000);
  temperature = randof (215) - 80;
  relhumidity = randof (50) + 10;

  char update [20];
  sprintf (update, "%05d %d %d", zipcode, temperature, relhumidity);

  zstr_send(output, update);
  return 0;
}

ev_timer timeout_watcher;
static ev_idle repeat_watcher;
static ev_async ready_watcher;
static ev_io io_watcher;
static struct ev_loop *loop;
static void* publisher;

static void
timeout_cb (EV_P_ ev_timer *w, int revents)
{
  int zipcode, temperature, relhumidity;
  zipcode     = randof (100000);
  temperature = randof (215) - 80;
  relhumidity = randof (50) + 10;

  char update [20];
  sprintf (update, "%05d %d %d", zipcode, temperature, relhumidity);

  zstr_send(publisher, update);
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
  if (eio_poll () == -1) {
    ev_idle_start (EV_A_ &repeat_watcher);
  }
}
    
static void
want_poll (void)
{
  ev_async_send (loop, &ready_watcher);
}

int main (void)
{
  void *context = zmq_init (1);
  publisher = zmq_socket (context, ZMQ_PUB);
  zmq_bind (publisher, "tcp://*:5556");

  srandom ((unsigned) time (NULL));
  loop = EV_DEFAULT;
  ev_timer_init (&timeout_watcher, timeout_cb, 0.001, 0.001);
  ev_timer_start (loop, &timeout_watcher);
    
  ev_idle_init (&repeat_watcher, repeat);
  ev_async_init (&ready_watcher, ready);
  ev_async_start (loop, &ready_watcher);
    
  if (eio_init (want_poll, 0)) {
    abort ();
  }
    
  ev_run (loop, 0);

  zmq_close (publisher);
  zmq_term (context);
  return 0;
}