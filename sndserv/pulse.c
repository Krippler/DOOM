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
//	PulseAudio output for the sound server, as a drop-in replacement for
//	the OSS output in linux.c.
//
//	The original writes to /dev/dsp, which no current kernel provides.
//	PulseAudio's own OSS shim (padsp) was removed upstream in PulseAudio
//	16, so rather than emulate a dead interface this talks to the sound
//	server directly. PipeWire accepts PulseAudio clients unchanged, so
//	this covers current desktops too.
//
//	Like the OSS version, the write blocks until the server will take more
//	audio, which is what paces the sound server's main loop.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#include "soundsrv.h"

// NULL when no connection could be made. The server then keeps running and
// discards audio rather than exiting, because the engine talks to it over a
// pipe and would take a SIGPIPE if it died.
static pa_simple*	pa_handle = NULL;

// Complain once rather than once per buffer.
static int		write_failed = 0;


void I_InitMusic(void)
{
}


void
I_InitSound
( int	samplerate,
  int	samplesize )
{
    pa_sample_spec	spec;
    pa_buffer_attr	attr;
    int			error;

    if (samplesize != 16)
    {
	fprintf(stderr, "Only 16 bit samples are supported, got %d\n",
		samplesize);
	return;
    }

    spec.format = PA_SAMPLE_S16LE;
    spec.rate = samplerate;
    spec.channels = 2;

    // The OSS path asked for two 2048 byte fragments. Keep the server side
    // buffer around the same size so sounds stay in step with the action and
    // the blocking write still paces the loop.
    memset (&attr, 0, sizeof(attr));
    attr.maxlength = (uint32_t) -1;
    attr.tlength = MIXBUFFERSIZE * 2;
    attr.prebuf = (uint32_t) -1;
    attr.minreq = (uint32_t) -1;
    attr.fragsize = (uint32_t) -1;

    pa_handle = pa_simple_new(NULL,		// default server
			      "DOOM",		// application name
			      PA_STREAM_PLAYBACK,
			      NULL,		// default device
			      "Sound Effects",
			      &spec,
			      NULL,		// default channel map
			      &attr,
			      &error);

    if (!pa_handle)
    {
	fprintf(stderr, "Could not connect to PulseAudio (%s), "
		"running without sound\n", pa_strerror(error));
	return;
    }

    fprintf(stderr, "PulseAudio: %d Hz, %d bit, stereo\n",
	    samplerate, samplesize);
}


void
I_SubmitOutputBuffer
( void*	samples,
  int	samplecount )
{
    int		error;

    if (!pa_handle)
	return;

    // Two channels of 16 bit samples, as set up in I_InitSound.
    if (pa_simple_write(pa_handle, samples, (size_t)samplecount * 4, &error) < 0)
    {
	if (!write_failed)
	{
	    write_failed = 1;
	    fprintf(stderr, "PulseAudio write failed (%s), "
		    "continuing without sound\n", pa_strerror(error));
	}

	// The connection is gone; stop trying so the loop does not spin on a
	// dead stream, and let the server keep answering the engine.
	pa_simple_free(pa_handle);
	pa_handle = NULL;
    }
}


void I_ShutdownSound(void)
{
    int		error;

    if (!pa_handle)
	return;

    pa_simple_drain(pa_handle, &error);
    pa_simple_free(pa_handle);
    pa_handle = NULL;
}


void I_ShutdownMusic(void)
{
}
