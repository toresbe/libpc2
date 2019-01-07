CC=cc
CXX=g++
RM=rm -f
CPPFLAGS=-g -fPIC -std=gnu++11 -Wall -Wextra -I. -DBOOST_LOG_DYN_LINK $(shell pkg-config --cflags libusb-1.0 libnotify) 
LDFLAGS=-g
LDLIBS=$(shell pkg-config --libs libusb-1.0 libnotify) -lrabbitmq -lboost_iostreams -lboost_thread -lboost_system -lpthread -lboost_log_setup -lboost_log

SRCS=$(wildcard pc2/*.cpp) $(wildcard amqp/*.cpp) $(wildcard ml/*.cpp)
OBJS=$(subst .cpp,.o,$(SRCS))

all:	libpc2

libpc2: $(OBJS)
	ar rcu libpc2.a $(OBJS)
	ranlib libpc2.a

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) libpc2.a
