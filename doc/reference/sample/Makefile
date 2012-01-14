LIBPATH=../../../lib
LIBEV=$(LIBPATH)/libev/.libs/libev.a
LIBEVPATH=$(LIBPATH)/libev
CC=clang
CXX=clang++
SYNTAX_CHECK_FLAG=--analyze -fno-caret-diagnostics -fno-color-diagnostics -fno-show-column
ZMQBASE=/Users/bartuer/local

evio_test:evio /tmp/evio
	mkdir -p /tmp/evio/fail
	echo "/tmp/evio/XX" |./evio /tmp/evio
	echo "/tmp/evio/fail/XX" |./evio /tmp/evio

evio_memcheck:evio
	touch Watcher/some.txt
	valgrind --leak-check=yes ./evio Watcher

evio:evio.c $(LIBEV)
	$(CC) -g -I$(LIBPATH)/libeio -I$(LIBPATH)/libev -o evio evio.c $(LIBPATH)/libeio/.libs/libeio.a  $(LIBPATH)/libev/.libs/libev.a

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


dir_memcheck:dir test_dir
	valgrind --leak-check=yes ./dir /tmp/a

dir_test:dir test_dir
	./dir /tmp/a

test_dir:
	rm -rf /tmp/a
	mkdir -p /tmp/a/{b,e,f}/{c,g}/{d,h,'汉 语'}
	touch /tmp/a/b/{i,j,k}

dir:dir.c
	$(CC) -g -I. -I$(LIBPATH)/libeio -I$(LIBPATH)/libev -o dir dir.c $(LIBPATH)/libeio/.libs/libeio.a  $(LIBPATH)/libev/.libs/libev.a

check-syntax:
	$(CC) $(SYNTAX_CHECK_FLAG) -I. -I$(LIBPATH)/libeio -I$(LIBPATH)/libev $(CHK_SOURCES)

clean:
	rm *.o
	rm evio
	rm pub
	rm sub
	rm dir