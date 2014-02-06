PROG=jsontpl
CFLAGS=--std=c99 --pedantic -Wall -Werror -ggdb -Ijansson -DJSONTPL_MAIN
LFLAGS= -Ljansson -ljansson
CFILES=$(wildcard *.c)
OFILES=$(CFILES:.c=.o)

$(PROG): $(OFILES)
	$(CC) -o $(PROG) $(OFILES) $(LFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<