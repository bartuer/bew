LIBPATH=../lib
LIBEV=$(LIBPATH)/libev/.libs/libev.a
LIBEVPATH=$(LIBPATH)/libev
CC=clang
CXX=clang++
SYNTAX_CHECK_FLAG=--analyze -fno-caret-diagnostics -fno-color-diagnostics -fno-show-column
ZMQBASE=/Users/bartuer/local

test:evio
	rm -Rf /tmp/a/*
	./evio /tmp/a &
	sh ./regress_fixture
	@echo #./evio /tmp/a < regress_fixture

evio_memcheck:evio
	rm -Rf /tmp/a/*
	valgrind --dsymutil=yes --leak-check=yes --track-fds=yes ./evio /tmp/a 15 
	sh ./regress_fixture

evio:evio.o dir_node.o cbt.o
	$(CXX) -o evio evio.o dir_node.o cbt.o $(LIBPATH)/libeio/.libs/libeio.a $(LIBPATH)/libev/.libs/libev.a $(ZMQBASE)/lib/libzmq.a $(ZMQBASE)/lib/libczmq.a

evio.o:evio.c $(LIBEV) 
	$(CC) -I$(LIBPATH)/libeio -I$(LIBPATH)/libev -I$(ZMQBASE)/include -c evio.c 

/tmp/evio:
	mkdir /tmp/evio

$(LIBEV):$(LIBEVPATH)/ev_kqueue.c $(LIBEVPATH)/ev.c $(LIBEVPATH)/ev_select.c
	cd /Users/bartuer/local/src/bew/lib/libev; make

sub_memcheck:sub
	valgrind --leak-check=yes ./sub tcp://localhost:5556 10001 

evsub:sub.o
	$(CXX) -o evsub sub.o $(ZMQBASE)/lib/libzmq.a $(ZMQBASE)/lib/libczmq.a

sub.o:sub.c
	$(CC) -c -O3 -I. -I$(ZMQBASE)/include sub.c -o sub.o 

pub:pub.o
	$(CXX) -L$(ZMQBASE)/lib -lzmq -lczmq -lev -leio -o pub pub.o

pub.o:pub.c
	$(CC) -c -O3 -I. -I$(ZMQBASE)/include pub.c -o pub.o


dir_node.o:dir_node.c
	$(CC) -c -I. -I$(LIBPATH)/libeio -I$(LIBPATH)/libev -o dir_node.o dir_node.c

cbt.o:cbt.c
	$(CC) -c -o cbt.o cbt.c

check-syntax:
	$(CC) -I$(ZMQBASE)/include $(SYNTAX_CHECK_FLAG) -I. -I$(LIBPATH)/libeio -I$(LIBPATH)/libev $(CHK_SOURCES)

clean:
	rm *.o
	rm evio
	rm pub
	rm sub