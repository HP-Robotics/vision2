ARCH = $(shell uname -m)

CFLAGS=-g -Wall `pkg-config --cflags opencv libv4l2`
LDFLAGS=`pkg-config --libs opencv libv4l2`
OBJS=\
    capture-$(ARCH).o \
    image-$(ARCH).o \
    vision-$(ARCH).o

all: vision-$(ARCH)

capture-$(ARCH).o: capture.c capture.h Makefile
	gcc -c $(CFLAGS) -o $@ $<

vision-$(ARCH).o: vision.c image.h capture.h Makefile
	gcc -c $(CFLAGS) -o $@ $<

image-$(ARCH).o: image.cpp image.h Makefile
	g++ -c $(CFLAGS) -o $@ $<

vision-$(ARCH): main.cpp $(OBJS) image.h capture.h Makefile
	g++    $(CFLAGS) -o $@ $(LDFLAGS) $< $(OBJS)

clean:
	rm -f $(OBJS)
	rm -f vision2-$(ARCH)
	
rebuild: clean all
