ARCH = $(shell uname -m)

all: vision2-$(ARCH)

capture-$(ARCH).o: capture.c capture.h Makefile
	gcc -c -g -Wall  -o $@ `pkg-config --cflags opencv libv4l2` capture.c

vision2-$(ARCH): vision2.cpp capture-$(ARCH).o capture.h Makefile
	g++ -g -Wall   -o $@ `pkg-config --cflags --libs opencv libv4l2` vision2.cpp capture-$(ARCH).o

clean:
	rm -f capture-$(ARCH).o
	rm -f vision2-$(ARCH)
	rm -f /home/pi/vision2-$(ARCH)
	
rebuild: clean all
