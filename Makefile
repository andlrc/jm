VERSION	=  "r$$(git rev-list --count HEAD).$$(git rev-parse --short HEAD)"

CC	= gcc
CFLAGS	= -O2 -std=c89 -D_POSIX_C_SOURCE=200809L -Wall -Werror -Wextra \
	  -DPROGRAM_VERSION="\"$(VERSION)\""

CDEBUG	= -g -D_POSIX_C_SOURCE=200809L -DJMDEBUG
OFILES	= src/jm.o src/jm_query.o src/jm_parse.o src/jm_serialize.o \
	  src/jm_merge.o src/json_merger.o

.PHONY:	all debug install uninstall clean test

all:	json_merger

json_merger:	$(OFILES)
	$(CC) $(CFLAGS) -o json_merger $(OFILES)

src/json_merger.o:	src/jm.h src/json_merger.c
src/jm.o:	src/jm.h src/jm.c
src/jm_query.o:	src/jm.h src/jm_query.c
src/jm_parse.o:	src/jm.h src/jm_parse.c
src/jm_serialize.o:	src/jm.h src/jm_serialize.c
src/jm_merge.o:	src/jm.h src/jm_merge.c

# Build with Symbols

debug:	clean
	$(MAKE) CFLAGS="$(CDEBUG)"

# Docs

README:	man.1
	LC_ALL=C MANWIDTH=80 man -l man.1 > README

# Install

install:
	cp json_merger /usr/bin/json_merger
	cp man.1 /usr/share/man/man1/json_merger.1

uninstall:
	rm /usr/bin/json_merger /usr/share/man/man1/json_merger.1

# Utilities

clean:
	-rm json_merger $(OFILES) src/*.c~ src/*.h~

test:	json_merger
	CDPATH= cd t && for s in ./*.sh; do "$$s"; done
