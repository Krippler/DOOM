##########################################################
#
# The mixer that gets the game's audio out of the container.
# See audiostream.c for why this exists at all.
#
# The sound effects are mixed by the original 1997 mixer, compiled from
# ../sndserv with its main() left out: SNDMIX_LIB takes the standalone sound
# server's process out and leaves the mixing behind. Building it here rather
# than copying it means there is one copy of that code, and the standalone
# server still builds from the same files for the PulseAudio path.
#

CC?=gcc
CFLAGS?=-O2 -g
CFLAGS+=-Wall -D_GNU_SOURCE -DNORMALUNIX -DLINUX -DSNDMIX_LIB -I../sndserv
LDFLAGS?=
LIBS=-lm

O=linux
SND=../sndserv

all:	 $(O)/audiostream

$(O):
	mkdir -p $(O)

clean:
	rm -f *.o *~
	rm -f $(O)/*

$(O)/audiostream: $(O)/audiostream.o $(O)/soundsrv.o $(O)/sounds.o $(O)/wadread.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(O)/audiostream.o $(O)/soundsrv.o \
	    $(O)/sounds.o $(O)/wadread.o -o $(O)/audiostream $(LIBS)

$(O)/%.o: %.c | $(O)
	$(CC) $(CFLAGS) -c $< -o $@

$(O)/%.o: $(SND)/%.c | $(O)
	$(CC) $(CFLAGS) -c $< -o $@
