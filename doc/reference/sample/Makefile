LIBPATH=../../../lib
test:evio
	touch Watcher/some.txt
	./evio Watcher
evio:evio.c
	gcc -g -I$(LIBPATH)/libeio -I$(LIBPATH)/libev -o evio evio.c $(LIBPATH)/libeio/.libs/libeio.a  $(LIBPATH)/libev/.libs/libev.a
memcheck:evio
	touch Watcher/some.txt
	valgrind --leak-check=yes ./evio Watcher
clean:
	rm evio
