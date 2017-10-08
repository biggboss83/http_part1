CC = gcc
CPPFLAGS =
CFLAGS = -std=c11 -g -D_XOPEN_SOURCE=700 -O2 -Wall -Wextra -Wformat=2 $(shell pkg-config --cflags glib-2.0)
LDFLAGS =
LOADLIBES =
LDLIBS = $(shell pkg-config --libs glib-2.0)
LIBS = 

.DEFAULT: all
.PHONY: all
all: httpd

clean:
	rm -f *.o

distclean: clean
	rm -f httpd
