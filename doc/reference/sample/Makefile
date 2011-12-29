LIBPATH=../../../lib
test:evio
	./evio Watcher
evio:evio.c
	gcc -g -I$(LIBPATH)/libeio -I$(LIBPATH)/libev -o evio evio.c $(LIBPATH)/libeio/.libs/libeio.a  $(LIBPATH)/libev/.libs/libev.a
memcheck:evio
	valgrind --leak-check=yes ./evio Watcher
clean:
	rm evio
