##########################################################
#
# The mixer that gets the game's audio out of the container.
# See audiostream.c for why this exists at all.
#

CC?=gcc
CFLAGS?=-O2 -g
CFLAGS+=-Wall -D_GNU_SOURCE
LDFLAGS?=

O=linux

all:	 $(O)/audiostream

$(O):
	mkdir -p $(O)

clean:
	rm -f *.o *~
	rm -f $(O)/*

$(O)/audiostream: $(O)/audiostream.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(O)/audiostream.o -o $(O)/audiostream

$(O)/%.o: %.c | $(O)
	$(CC) $(CFLAGS) -c $< -o $@
