PROG=test_jsontpl
CFLAGS=--std=c99 --pedantic -Wall -Werror -ggdb -I.. -I../jansson
LFLAGS= -L../jansson -ljansson
CFILES=$(wildcard ../*.c) test_jsontpl.c
OFILES=$(CFILES:.c=.o)

$(PROG): $(OFILES)
	$(CC) -o $(PROG) $(OFILES) $(LFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o ../*.o