LIB=-lreadline -lcurses
CFLAGS=-DUNIX -ansi -pedantic-errors
DEBUG=-g
#DEBUG=

all: shell


shell:	shell.c parse.c parse.h
	gcc $(CFLAGS) $(DEBUG) shell.c parse.c -o shell $(LIB)
clean:
	rm -f shell *~


