LIBPATH=../../../lib
test:evio
	./evio
evio:evio.c
	gcc -I$(LIBPATH)/libeio -I$(LIBPATH)/libev -o evio evio.c $(LIBPATH)/libeio/.libs/libeio.a  $(LIBPATH)/libev/.libs/libev.a
clean:
	rm evio
