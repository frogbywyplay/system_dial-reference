CC=$(TARGET)gcc

.PHONY: clean dbus-interface
.DEFAULT_GOAL=all
.NOTPARALLEL:

OBJS := main.o dial_server.o mongoose.o quick_ssdp.o url_lib.o dial_data.o LinuxInterfaces.o custom_callbacks.o
HEADERS := $(wildcard *.h)

CFLAGS += $(shell pkg-config --cflags wydbus lxc_genesis) -I dbus/gen/c
LDFLAGS += $(shell pkg-config --libs wydbus lxc_genesis)

%.c: $(HEADERS)

%.o: %.c $(HEADERS)
#	$(CC) -Wall -Werror -g -std=gnu99 $(CFLAGS) -c $*.c -o $*.o
	$(CC) -Wall -g -fPIC -std=gnu99 $(CFLAGS) -c $*.c -o $*.o

all: dbus-interface dialserver test

dbus-interface:
	dg import dbus/com.wyplay.dial.xml dbus/gen/db/
	dg --extend-db dbus/gen/db codegen c-adaptor dial dbus/gen/c
	dg --extend-db dbus/gen/db codegen py-proxy dial dbus/gen/python
	python2.5 ${ROOT}usr/lib/python2.5/compileall.py -q dbus/gen/python
	PYTHONOPTIMIZE=2 python2.5 ${ROOT}usr/lib/python2.5/compileall.py -q dbus/gen/python

dialserver: $(OBJS)
	$(CC) -Wall -Werror -g $(OBJS) -ldl -lpthread $(LDFLAGS) -o dialserver

test:
	make -C tests
#	./tests/run_tests

clean:
	rm -f *.o dialserver *.so
	make -C tests clean
