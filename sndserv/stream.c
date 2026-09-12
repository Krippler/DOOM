// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	Raw PCM output to a pipe, for a machine with no sound device at all.
//
//	The container that this is usually built for has no sound card and no
//	sound daemon; what it has is a browser at the other end of a socket.
//	So the mixed output goes into a pipe, and audiostream reads it from
//	there, adds the music to it and sends it on.
//
//	Like the OSS output in linux.c and the PulseAudio one in pulse.c, the
//	write blocks until the far end will take more, and that is what paces
//	the sound server's main loop. Here the far end is a reader working to
//	a real-time schedule, which paces it just as well as a sound card did.
//
//-----------------------------------------------------------------------------

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "soundsrv.h"

// -1 when there is nowhere to write. The server then keeps running and throws
// the audio away rather than exiting, because the engine talks to it over a
// pipe and would take a SIGPIPE if it died.
static int	out_fd = -1;

// Complain once rather than once per buffer.
static int	write_failed = 0;


void I_InitMusic(void)
{
}


void
I_InitSound
( int	samplerate,
  int	samplesize )
{
    const char*	path;

    if (samplesize != 16)
    {
	fprintf(stderr, "Only 16 bit samples are supported, got %d\n",
		samplesize);
	return;
    }

    path = getenv("DOOM_SFX_PIPE");

    if (!path || !*path)
    {
	fprintf(stderr, "DOOM_SFX_PIPE is not set, running without sound\n");
	return;
    }

    // The reader holds a write end of its own, so this does not wait for one
    // and the pipe is already the size it wants.
    out_fd = open(path, O_WRONLY);

    if (out_fd < 0)
    {
	fprintf(stderr, "Could not open %s (%s), running without sound\n",
		path, strerror(errno));
	return;
    }

    // The reader can go away -- a browser closing is not a reason to die.
    signal(SIGPIPE, SIG_IGN);

    fprintf(stderr, "PCM stream to %s: %d Hz, %d bit, stereo\n",
	    path, samplerate, samplesize);
}


void
I_SubmitOutputBuffer
( void*	samples,
  int	samplecount )
{
    // Two channels of 16 bit samples, as set up in I_InitSound.
    size_t	left = (size_t)samplecount * 4;
    const char*	p = samples;

    if (out_fd < 0)
	return;

    while (left)
    {
	ssize_t	n = write(out_fd, p, left);

	if (n > 0)
	{
	    p += n;
	    left -= (size_t) n;
	    continue;
	}

	if (n < 0 && errno == EINTR)
	    continue;

	if (!write_failed)
	{
	    write_failed = 1;
	    fprintf(stderr, "PCM write failed (%s), continuing without sound\n",
		    strerror(errno));
	}

	// Stop trying so the loop does not spin on a dead pipe, and let the
	// server keep answering the engine.
	close(out_fd);
	out_fd = -1;
	return;
    }
}


void I_ShutdownSound(void)
{
    if (out_fd < 0)
	return;

    close(out_fd);
    out_fd = -1;
}


void I_ShutdownMusic(void)
{
}
