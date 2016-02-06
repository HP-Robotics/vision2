ARCH = $(shell uname -m)

CFLAGS=-g -Wall `pkg-config --cflags opencv libv4l2`
LDFLAGS=`pkg-config --libs opencv libv4l2`
OBJS=\
    capture-$(ARCH).o \
    image-$(ARCH).o \
    vision-$(ARCH).o

all: vision-$(ARCH)

capture-$(ARCH).o: capture.c capture.h vision.h Makefile
	gcc -c $(CFLAGS) -o $@ $<

vision-$(ARCH).o: vision.c image.h capture.h vision.h Makefile
	gcc -c $(CFLAGS) -Wno-unused-function -o $@ $<

image-$(ARCH).o: image.cpp image.h vision.h Makefile
	g++ -c $(CFLAGS) -o $@ $<

vision-$(ARCH): main.cpp $(OBJS) image.h capture.h vision.h Makefile
	g++    $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

view-$(ARCH): view.c Makefile
	gcc -Wall `pkg-config --cflags --libs gtkimageview gdk-2.0` -o $@ view.c

clean:
	rm -f $(OBJS)
	rm -f vision-$(ARCH)
	
rebuild: clean all

view: view-$(ARCH)
	@echo done

run:
	./vision-$(ARCH)
	stty sane
