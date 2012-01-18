LIBPATH=../lib
LIBEV=$(LIBPATH)/libev/.libs/libev.a
LIBEVPATH=$(LIBPATH)/libev
CC=clang
CXX=clang++
SYNTAX_CHECK_FLAG=--analyze -fno-caret-diagnostics -fno-color-diagnostics -fno-show-column

test:evio
	mkdir -p /tmp/a/c
	mkdir -p /tmp/a/b
	echo 'rm -Rf /tmp/a/b; touch `mktemp -d /tmp/a/c/XX`/test' | ./evio /tmp/a

evio_test:evio /tmp/evio
	mkdir -p /tmp/evio/fail
	echo 'touch `mktemp -d /tmp/evio/XX`/test' |./evio /tmp/evio 
	echo 'touch `mktemp -d /tmp/evio/fail/XX`/test' |./evio /tmp/evio

evio_memcheck:evio
	touch /tmp/some.txt
	valgrind --leak-check=yes ./evio /tmp

evio:evio.c $(LIBEV) dir_node.o
	$(CC) -g -I$(LIBPATH)/libeio -I$(LIBPATH)/libev -o evio evio.c dir_node.o $(LIBPATH)/libeio/.libs/libeio.a  $(LIBPATH)/libev/.libs/libev.a

/tmp/evio:
	mkdir /tmp/evio

$(LIBEV):$(LIBEVPATH)/ev_kqueue.c $(LIBEVPATH)/ev.c $(LIBEVPATH)/ev_select.c
	cd /Users/bartuer/local/src/bew/lib/libev; make


sub_memcheck:sub
	valgrind --leak-check=yes ./sub tcp://localhost:5556 10001 

sub:sub.o
	g++ -L$(ZMQBASE)/lib -lzmq -lczmq -o sub sub.o

sub.o:sub.c
	$(CC) -c -O3 -I. -I$(ZMQBASE)/include sub.c -o sub.o


pub_memcheck:pub
	valgrind --leak-check=yes ./pub

pub:pub.o
	$(CXX) -L$(ZMQBASE)/lib -lzmq -lczmq -lev -leio -o pub pub.o

pub.o:pub.c
	$(CC) -c -O3 -I. -I$(ZMQBASE)/include pub.c -o pub.o

dir_node.o:dir_node.c
	$(CC) -c -I. -I$(LIBPATH)/libeio -I$(LIBPATH)/libev -o dir_node.o dir_node.c

q_test:q
	./q

q:q.c
	$(CC) -I. -o q q.c

check-syntax:
	$(CC) $(SYNTAX_CHECK_FLAG) -I. -I$(LIBPATH)/libeio -I$(LIBPATH)/libev $(CHK_SOURCES)

clean:
	rm *.o
	rm evio
	rm pub
	rm sub