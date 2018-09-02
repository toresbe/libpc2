CC=cc
CXX=g++
RM=rm -f
CPPFLAGS=-g -fPIC -std=gnu++11 -Wall -Wextra -I. $(shell pkg-config --cflags libusb-1.0)
LDFLAGS=-g
LDLIBS=/usr/lib/i386-linux-gnu/libboost_log.a $(shell pkg-config --libs libusb-1.0) -lrabbitmq -lboost_iostreams -lboost_thread -lboost_system -lpthread -lboost_log

PROG=pc2d
SRCS=$(wildcard pc2/*.cpp) $(wildcard amqp/*.cpp) $(wildcard ml/*.cpp)
OBJS=$(subst .cpp,.o,$(SRCS))

all:            $(PROG)

pc2d:    $(OBJS)
	$(CXX) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDLIBS)

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) pc2

install: pc2d
	install -m 644 etc/pc2.service /etc/systemd/system/
	install -m 644 services/pc2_hue.service /etc/systemd/system/
	install -m 644 services/pc2_projector.service /etc/systemd/system/
	install -m 755 pc2d /usr/local/bin
	install -m 644 etc/50-pc2.rules /etc/udev/rules.d
