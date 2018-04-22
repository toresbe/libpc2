CC=cc
CXX=g++
RM=rm -f
CPPFLAGS=-g -fPIC -std=gnu++11 -Wall -Wextra -I. $(shell pkg-config --cflags libusb-1.0)
LDFLAGS=-g
LDLIBS=/usr/lib/i386-linux-gnu/libboost_log.a $(shell pkg-config --libs libusb-1.0) -lrabbitmq -lboost_iostreams -lboost_thread -lboost_system -lpthread -lboost_log

PROG=pc2d
SRCS=$(wildcard pc2/*.cpp) $(wildcard amqp/*.cpp)
OBJS=$(subst .cpp,.o,$(SRCS))

all:            $(PROG)

pc2d:    $(OBJS)
	$(CXX) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDLIBS)

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) pc2
