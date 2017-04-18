PRGDIR	= $(DESTDIR)/usr/bin
MANDIR	= $(DESTDIR)/usr/share/man/man1
PRGNAME	= json_merger

CC	= gcc
CFLAGS	= -O3 -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Werror -Wextra \
	  -Wmissing-prototypes -Wold-style-definition \
	  -Wno-unused-function

AR	= ar

CDEBUG	= -std=c99 -g -D_POSIX_C_SOURCE=200809L -DJXDEBUG -DJXDEBUGFREE

CFILES	= json_merger.c jx.c jx_parse.c jx_serialize.c jx_merge.c
HFILES	= jx.h
TFILES	= $(CFILES) $(HFILES)
OFILES	= *.o

# Build

all:	$(PRGNAME)

$(PRGNAME):	jx.o jx_parse.o jx_serialize.o jx_merge.o json_merger.o
	$(CC) $(CFLAGS) -o $(PRGNAME) \
		jx.o jx_parse.o jx_serialize.o jx_merge.o json_merger.o

json_merger.o:	jx.h json_merger.c
jx.o:	jx.h jx.c
jx_parse.o:	jx.h jx_parse.c
jx_serialize.o:	jx.h jx_serialize.c
jx_merge.o:	jx.h jx_merge.c

# Build with Symbols

debug:	clean
	$(MAKE) CFLAGS="$(CDEBUG)"

# Docs

README:	man.1
	MANWIDTH=80 man -l man.1 > README

# Install

install:	$(PRGNAME) README man.1
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

