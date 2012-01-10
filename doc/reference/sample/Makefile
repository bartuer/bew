LIBPATH=../../../lib
LIBEV=$(LIBPATH)/libev/.libs/libev.a
LIBEVPATH=$(LIBPATH)/libev
ZMQBASE=/Users/bartuer/local
test:evio /tmp/evio
	mkdir -p /tmp/evio/fail
	echo "/tmp/evio/XX" |./evio /tmp/evio
	echo "/tmp/evio/fail/XX" |./evio /tmp/evio

evio:evio.c $(LIBEV)
	gcc -g -I$(LIBPATH)/libeio -I$(LIBPATH)/libev -o evio evio.c $(LIBPATH)/libeio/.libs/libeio.a  $(LIBPATH)/libev/.libs/libev.a

/tmp/evio:
	mkdir /tmp/evio

$(LIBEV):$(LIBEVPATH)/ev_kqueue.c $(LIBEVPATH)/ev.c $(LIBEVPATH)/ev_select.c
	cd /Users/bartuer/local/src/bew/lib/libev; make

memcheck:evio
	touch Watcher/some.txt
	valgrind --leak-check=yes ./evio Watcher

pub:pub.o
	g++ -L$(ZMQBASE)/lib -lzmq -lczmq -lev -leio -o pub pub.o

pub.o:pub.c
	gcc -c -O3 -I. -I$(ZMQBASE)/include pub.c -o pub.o

sub:sub.o
	g++ -L$(ZMQBASE)/lib -lzmq -lczmq -o sub sub.o

sub.o:sub.c
	gcc -c -O3 -I. -I$(ZMQBASE)/include sub.c -o sub.o

submemcheck:sub
	valgrind --leak-check=yes ./sub tcp://localhost:5556 10001 

clean:
	rm *.o
	rm evio
	rm pub
	rm sub
