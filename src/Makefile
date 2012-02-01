LIBPATH=../lib
LIBEVPATH=$(LIBPATH)/libev
LIBEIOPATH=$(LIBPATH)/libeio
LIBZMQPATH=$(LIBPATH)/zeromq
LIBCZMQPATH=$(LIBPATH)/czmq
LIBZMQINC=-I$(LIBZMQPATH)/include -I$(LIBCZMQPATH)/include
LIBEV=$(LIBEVPATH)/.libs/libev.a
LIBEIO=$(LIBEIOPATH)/.libs/libeio.a
LIBZMQ=$(LIBZMQPATH)/src/.libs/libzmq.a
LIBCZMQ=$(LIBCZMQPATH)/src/.libs/libczmq.a

CC=gcc
CXX=g++
SYNTAX_CHECK_FLAG=--analyze -fno-caret-diagnostics -fno-color-diagnostics -fno-show-column

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
	$(CXX) -O3 -o evio evio.o dir_node.o cbt.o $(LIBEIO) $(LIBEV) $(LIBZMQ) $(LIBCZMQ)

evio.o:evio.c $(LIBEV) 
	$(CC) -I$(LIBEIOPATH) -I$(LIBEVPATH) $(LIBZMQINC) -c evio.c 

/tmp/evio:
	mkdir /tmp/evio

$(LIBEV):$(LIBEVPATH)/ev_kqueue.c $(LIBEVPATH)/ev.c $(LIBEVPATH)/ev_select.c
	cd /Users/bartuer/local/src/bew/lib/libev; make

evsub_memcheck:evsub
	valgrind --leak-check=yes ./evsub tcp://localhost:5556 10001 

evsub:evsub.o
	$(CXX) -O3 -o evsub evsub.o $(LIBZMQ) $(LIBCZMQ)

evsub.o:evsub.c
	$(CC) -c -O3 -I. $(LIBZMQINC) evsub.c -o evsub.o 

dir_node.o:dir_node.c
	$(CC) -c -O3 -I. -I$(LIBEIOPATH) -I$(LIBEVPATH) -o dir_node.o dir_node.c

cbt.o:cbt.c
	$(CC) -c -O3 -o cbt.o cbt.c

check-syntax:
	$(CC) $(SYNTAX_CHECK_FLAG) -I. $(LIBZMQINC) -I$(LIBEIO) -I$(LIBEVPATH) $(CHK_SOURCES)

clean:
	rm *.o
	rm evio
	rm evsub