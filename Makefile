PRGDIR	= $(DESTDIR)/usr/bin
MANDIR	= $(DESTDIR)/usr/share/man/man1
PRGNAME	= json_merger
VERSION	= "r$$(git rev-list --count HEAD).$$(git rev-parse --short HEAD)"

CC	= gcc
CFLAGS	= -O3 -std=c89 -D_POSIX_C_SOURCE=200809L -Wall -Werror -Wextra \
	  -Wmissing-prototypes -Wold-style-definition \
	  -Wno-unused-function \
	  -DPROGRAM_VERSION="\"$(VERSION)\""

CDEBUG	= -std=c89 -g -D_POSIX_C_SOURCE=200809L -DJMDEBUG
CFILES	= json_merger.c jm.c jm_parse.c jm_serialize.c jm_merge.c
HFILES	= jm.h
TFILES	= $(CFILES) $(HFILES)
OFILES	= *.o

# Build

all:	$(PRGNAME)

$(PRGNAME):	jm.o jm_parse.o jm_serialize.o jm_merge.o json_merger.o
	$(CC) $(CFLAGS) -o $(PRGNAME) \
		jm.o jm_parse.o jm_serialize.o jm_merge.o json_merger.o

json_merger.o:	jm.h json_merger.c
jm.o:	jm.h jm.c
jm_parse.o:	jm.h jm_parse.c
jm_serialize.o:	jm.h jm_serialize.c
jm_merge.o:	jm.h jm_merge.c

# Build with Symbols

debug:	clean
	$(MAKE) CFLAGS="$(CDEBUG)"

# Docs

README:	man.1
	MANWIDTH=80 man -l man.1 > README

# Install

install:	$(PRGNAME) man.1
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

tags:	$(TFILES)
	ctags $(TFILES)

