##########################################################
#
# $Id:$
#
# $Log:$
#
#

CC?=gcc
CFLAGS?=-O2 -g
CFLAGS+=-Wall -DNORMALUNIX -DLINUX
LDFLAGS?=
LIBS=-lm

# Audio backend: "pulse" (default), "stream" or "oss".
#
# The OSS backend writes to /dev/dsp, which no current kernel provides, and
# PulseAudio's OSS shim (padsp) was removed upstream in PulseAudio 16. The
# pulse backend talks to the sound server directly instead; PipeWire accepts
# PulseAudio clients unchanged.
#
# The stream backend writes raw PCM to a pipe, for a machine with no sound
# device and no sound daemon -- which is what the container is. See stream.c.
SNDBACKEND?=pulse

ifeq ($(SNDBACKEND),oss)
BACKEND_OBJ=$(O)/linux.o
BACKEND_LIBS=
else ifeq ($(SNDBACKEND),pulse)
BACKEND_OBJ=$(O)/pulse.o
BACKEND_LIBS=-lpulse-simple -lpulse
else ifeq ($(SNDBACKEND),stream)
BACKEND_OBJ=$(O)/stream.o
BACKEND_LIBS=
else
$(error SNDBACKEND must be "pulse", "stream" or "oss", got "$(SNDBACKEND)")
endif

O=linux

all:	 $(O)/sndserver

$(O):
	mkdir -p $(O)

clean:
	rm -f *.o *~ *.flc
	rm -f linux/*

# Target
$(O)/sndserver: $(O) \
	$(O)/soundsrv.o \
	$(O)/sounds.o \
	$(O)/wadread.o \
	$(BACKEND_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) \
	$(O)/soundsrv.o \
	$(O)/sounds.o \
	$(O)/wadread.o \
	$(BACKEND_OBJ) -o $(O)/sndserver $(LIBS) $(BACKEND_LIBS)
	echo make complete.

# Rule
$(O)/%.o: %.c | $(O)
	$(CC) $(CFLAGS) -c $< -o $@


