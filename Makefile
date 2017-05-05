PRGDIR	?= $(DESTDIR)/usr/bin
MANDIR	?= $(DESTDIR)/usr/share/man/man1
PRGNAME	?= json_merger
VERSION	=  "r$$(git rev-list --count HEAD).$$(git rev-parse --short HEAD)"

CC	?= gcc
CFLAGS	?= -O3 -std=c89 -D_POSIX_C_SOURCE=200809L -Wall -Werror -Wextra \
	   -Wmissing-prototypes -Wold-style-definition \
	   -Wno-unused-function \
	   -DPROGRAM_VERSION="\"$(VERSION)\""

CDEBUG	= -std=c89 -g -D_POSIX_C_SOURCE=200809L -DJMDEBUG
CFILES	= src/*.c
HFILES	= src/*.h
TFILES	= $(CFILES) $(HFILES)
OFILES	= src/jm.o src/jm_query.o src/jm_parse.o src/jm_serialize.o \
	  src/jm_merge.o src/json_merger.o
# Build

all:	$(PRGNAME)

$(PRGNAME):	$(OFILES)
	$(CC) $(CFLAGS) -o $(PRGNAME) $(OFILES)

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
	cp $(PRGNAME) $(PRGDIR)/$(PRGNAME)
	cp man.1 $(MANDIR)/$(PRGNAME).1

uninstall:
	rm $(PRGDIR)/$(PRGNAME) $(MANDIR)/$(PRGNAME).1

# Utilities

clean:
# Output files
	-rm $(PRGNAME) $(OFILES)
# Indent backup files
	-rm $(CFILES:=~) $(HFILES:=~)

indent:
	indent -kr -i8 $(CFILES) $(HFILES)

test:	$(PRGNAME) .TEST
.TEST:
	CDPATH= cd t && for s in ./*.sh; do "$$s"; done

tags:	$(TFILES)
	ctags $(TFILES)

