CC=cc
CXX=g++
RM=rm -f
CPPFLAGS=-g -fPIC -std=gnu++11 -Wall -Wextra -I../../ -I. -DBOOST_LOG_DYN_LINK $(shell pkg-config --cflags libusb-1.0) 
LDFLAGS=-g
LDLIBS=$(shell pkg-config --libs libusb-1.0) -lboost_iostreams -lboost_thread -lboost_system -lpthread -lboost_log_setup -lboost_log

PROG=pprint
SRCS=$(wildcard *.cpp)
OBJS=$(subst .cpp,.o,$(SRCS))

all:    $(PROG)

../../libpc2.a:
	$(MAKE) -C ../../

$(PROG):$(OBJS) ../../libpc2.a
	$(CXX) $(LDFLAGS) -o $(PROG) $(OBJS) ../../libpc2.a $(LDLIBS)

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) pprint
