LIBPATH=../../../lib
LIBEV=$(LIBPATH)/libev/.libs/libev.a
LIBEVPATH=$(LIBPATH)/libev
test:evio /tmp/evio
	echo "echo 'there' > `mktemp /tmp/evio/test.XXXXXX` || exit 1" |./evio /tmp/evio

evio:evio.c $(LIBEV)
	gcc -g -I$(LIBPATH)/libeio -I$(LIBPATH)/libev -o evio evio.c $(LIBPATH)/libeio/.libs/libeio.a  $(LIBPATH)/libev/.libs/libev.a

/tmp/evio:
	mkdir /tmp/evio

$(LIBEV):$(LIBEVPATH)/ev_kqueue.c $(LIBEVPATH)/ev.c $(LIBEVPATH)/ev_select.c
	cd /Users/bartuer/local/src/bew/lib/libev; make

memcheck:evio
	touch Watcher/some.txt
	valgrind --leak-check=yes ./evio Watcher

clean:
	rm evio
