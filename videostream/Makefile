##########################################################
#
# Sends the game's picture to the browser, in place of VNC.
# See videostream.c for why.
#

CC?=gcc
CFLAGS?=-O2 -g
CFLAGS+=-Wall -D_GNU_SOURCE
LDFLAGS?=

O=linux

all:	 $(O)/videostream

$(O):
	mkdir -p $(O)

clean:
	rm -f *.o *~
	rm -f $(O)/*

$(O)/videostream: $(O)/videostream.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(O)/videostream.o -o $(O)/videostream

$(O)/%.o: %.c | $(O)
	$(CC) $(CFLAGS) -c $< -o $@
