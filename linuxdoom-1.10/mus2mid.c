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
//	Conversion of the MUS music format used by the WAD files into a
//	Standard MIDI File.
//
//	MUS is a trimmed down MIDI: one track, delays counted in 140 Hz
//	ticks, and a handful of event types. The MIDI written here uses a
//	division of 70 ticks per quarter note and leaves the tempo at its
//	default of 500000 microseconds per quarter note, which works out at
//	exactly the 140 ticks per second MUS counts in.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "mus2mid.h"


// MUS event types, in the top three bits of an event descriptor byte.
#define MUS_RELEASEKEY		0x00
#define MUS_PRESSKEY		0x10
#define MUS_PITCHWHEEL		0x20
#define MUS_SYSTEMEVENT		0x30
#define MUS_CHANGECONTROLLER	0x40
#define MUS_SCOREEND		0x60

#define MUS_CHANNELS		16

// MUS reserves its last channel for percussion; MIDI uses channel 9.
#define MUS_PERCUSSION_CHANNEL	15
#define MIDI_PERCUSSION_CHANNEL	9

// MUS controller numbers in the order they map onto MIDI controllers.
// Entry 0 is a patch change and is handled separately.
static const byte controller_map[] =
{
    0x00, 0x20, 0x01, 0x07, 0x0a, 0x0b, 0x5b, 0x5d,
    0x40, 0x43, 0x78, 0x7b, 0x7e, 0x7f, 0x79
};

static const byte midi_header[] =
{
    'M', 'T', 'h', 'd',
    0x00, 0x00, 0x00, 0x06,	// header length
    0x00, 0x00,			// format 0
    0x00, 0x01,			// one track
    0x00, 0x46,			// 70 ticks per quarter note
    'M', 'T', 'r', 'k',
    0x00, 0x00, 0x00, 0x00	// track length, filled in at the end
};


//
// Growable output buffer.
//
typedef struct
{
    byte*	data;
    size_t	len;
    size_t	cap;
    boolean	failed;
} buffer_t;


static void BufferWrite (buffer_t* buf, const void* src, size_t n)
{
    if (buf->failed)
	return;

    if (buf->len + n > buf->cap)
    {
	size_t	cap = buf->cap ? buf->cap : 8192;
	byte*	grown;

	while (buf->len + n > cap)
	    cap *= 2;

	grown = realloc(buf->data, cap);

	if (!grown)
	{
	    buf->failed = true;
	    return;
	}

	buf->data = grown;
	buf->cap = cap;
    }

    memcpy (buf->data + buf->len, src, n);
    buf->len += n;
}


static void BufferByte (buffer_t* buf, byte b)
{
    BufferWrite (buf, &b, 1);
}


//
// MIDI delta times are seven bits per byte, most significant group first,
// with the high bit set on every byte but the last.
//
static void BufferVarLength (buffer_t* buf, unsigned int value)
{
    byte	out[4];
    int		n = 0;

    out[n++] = value & 0x7f;
    value >>= 7;

    while (value && n < 4)
    {
	out[n++] = 0x80 | (value & 0x7f);
	value >>= 7;
    }

    while (n > 0)
	BufferByte (buf, out[--n]);
}


//
// Reading side, bounds checked against the end of the lump.
//
typedef struct
{
    const byte*	data;
    size_t	len;
    size_t	pos;
    boolean	overrun;
} reader_t;


static byte ReadByte (reader_t* r)
{
    if (r->pos >= r->len)
    {
	r->overrun = true;
	return 0;
    }

    return r->data[r->pos++];
}


static unsigned short ReadShort (const byte* p)
{
    return (unsigned short)(p[0] | (p[1] << 8));
}


size_t MUS_LumpLength (const void* data)
{
    const byte*	p = (const byte *) data;

    if (!p || memcmp(p, "MUS\x1a", 4))
	return 0;

    // Score length and the offset it starts at.
    return (size_t) ReadShort(p + 4) + ReadShort(p + 6);
}


boolean
mus2mid
( const void*	musdata,
  size_t	muslen,
  byte**	out,
  size_t*	outlen )
{
    const byte*		mus = (const byte *) musdata;
    reader_t		r;
    buffer_t		buf;
    unsigned short	scorestart;
    unsigned int	queuedtime = 0;
    unsigned int	tracklen;
    int			channel_map[MUS_CHANNELS];
    byte		channel_velocity[MUS_CHANNELS];
    int			next_channel = 0;
    int			i;
    boolean		done = false;

    if (!mus || muslen < 16 || memcmp(mus, "MUS\x1a", 4))
	return false;

    scorestart = ReadShort(mus + 6);

    if (scorestart >= muslen)
	return false;

    for (i = 0; i < MUS_CHANNELS; i++)
    {
	channel_map[i] = -1;
	channel_velocity[i] = 127;
    }

    r.data = mus;
    r.len = muslen;
    r.pos = scorestart;
    r.overrun = false;

    memset (&buf, 0, sizeof(buf));
    BufferWrite (&buf, midi_header, sizeof(midi_header));

    while (!done && !r.overrun && !buf.failed)
    {
	byte	descriptor = ReadByte(&r);
	int	event = descriptor & 0x70;
	int	mus_channel = descriptor & 0x0f;
	int	last = descriptor & 0x80;
	int	midi_channel;
	byte	key;
	byte	value;

	if (r.overrun)
	    break;

	// Work out which MIDI channel this MUS channel plays on. MUS's
	// percussion channel is fixed; the rest are handed out in order,
	// skipping the one MIDI reserves for percussion.
	if (mus_channel == MUS_PERCUSSION_CHANNEL)
	{
	    midi_channel = MIDI_PERCUSSION_CHANNEL;
	}
	else
	{
	    if (channel_map[mus_channel] < 0)
	    {
		if (next_channel == MIDI_PERCUSSION_CHANNEL)
		    next_channel++;

		if (next_channel > 15)
		{
		    // Out of MIDI channels; drop this one rather than
		    // corrupting the output.
		    channel_map[mus_channel] = MIDI_PERCUSSION_CHANNEL;
		}
		else
		{
		    channel_map[mus_channel] = next_channel++;
		}
	    }

	    midi_channel = channel_map[mus_channel];
	}

	switch (event)
	{
	  case MUS_RELEASEKEY:
	    key = ReadByte(&r) & 0x7f;
	    BufferVarLength (&buf, queuedtime);
	    BufferByte (&buf, 0x80 | midi_channel);
	    BufferByte (&buf, key);
	    BufferByte (&buf, 0);
	    break;

	  case MUS_PRESSKEY:
	    key = ReadByte(&r);

	    // The high bit says a new velocity for this channel follows.
	    if (key & 0x80)
	    {
		key &= 0x7f;
		channel_velocity[mus_channel] = ReadByte(&r) & 0x7f;
	    }

	    BufferVarLength (&buf, queuedtime);
	    BufferByte (&buf, 0x90 | midi_channel);
	    BufferByte (&buf, key & 0x7f);
	    BufferByte (&buf, channel_velocity[mus_channel]);
	    break;

	  case MUS_PITCHWHEEL:
	    // One byte of MUS resolution spread over MIDI's fourteen bits.
	    value = ReadByte(&r);
	    BufferVarLength (&buf, queuedtime);
	    BufferByte (&buf, 0xe0 | midi_channel);
	    BufferByte (&buf, (value & 1) << 6);
	    BufferByte (&buf, value >> 1);
	    break;

	  case MUS_SYSTEMEVENT:
	    value = ReadByte(&r);

	    if (value < 10 || value > 14)
		goto bad;

	    BufferVarLength (&buf, queuedtime);
	    BufferByte (&buf, 0xb0 | midi_channel);
	    BufferByte (&buf, controller_map[value]);
	    BufferByte (&buf, 0);
	    break;

	  case MUS_CHANGECONTROLLER:
	    key = ReadByte(&r);
	    value = ReadByte(&r) & 0x7f;

	    BufferVarLength (&buf, queuedtime);

	    if (key == 0)
	    {
		// Controller zero is an instrument change.
		BufferByte (&buf, 0xc0 | midi_channel);
		BufferByte (&buf, value);
	    }
	    else
	    {
		if (key > 9)
		    goto bad;

		BufferByte (&buf, 0xb0 | midi_channel);
		BufferByte (&buf, controller_map[key]);
		BufferByte (&buf, value);
	    }
	    break;

	  case MUS_SCOREEND:
	    done = true;
	    break;

	  default:
	    goto bad;
	}

	queuedtime = 0;

	// A set high bit means a delay follows, counted in 140 Hz ticks and
	// stored the same way MIDI stores delta times.
	if (last && !done)
	{
	    unsigned int	delay = 0;
	    byte		b;

	    do
	    {
		b = ReadByte(&r);
		delay = (delay << 7) | (b & 0x7f);
	    } while ((b & 0x80) && !r.overrun);

	    queuedtime = delay;
	}
    }

    if (r.overrun || buf.failed || !done)
	goto bad;

    // End of track.
    BufferVarLength (&buf, queuedtime);
    BufferByte (&buf, 0xff);
    BufferByte (&buf, 0x2f);
    BufferByte (&buf, 0x00);

    if (buf.failed)
	goto bad;

    // Fill in the track length, which excludes the MTrk tag and the length
    // field itself.
    tracklen = (unsigned int)(buf.len - sizeof(midi_header));
    buf.data[sizeof(midi_header) - 4] = (tracklen >> 24) & 0xff;
    buf.data[sizeof(midi_header) - 3] = (tracklen >> 16) & 0xff;
    buf.data[sizeof(midi_header) - 2] = (tracklen >> 8) & 0xff;
    buf.data[sizeof(midi_header) - 1] = tracklen & 0xff;

    *out = buf.data;
    *outlen = buf.len;
    return true;

  bad:
    free (buf.data);
    return false;
}
