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
//	Standard MIDI File, so it can be handed to an ordinary synthesiser.
//
//-----------------------------------------------------------------------------

#ifndef __MUS2MID__
#define __MUS2MID__

#include <stddef.h>

#include "doomtype.h"


//
// Reads the length of a MUS lump from its header, which is how much of the
// lump is actually music. Returns 0 if this does not look like a MUS lump.
//
size_t MUS_LumpLength (const void* data);


//
// Converts a MUS lump into a Standard MIDI File held in memory.
//
// On success returns true, sets *out to a malloc'd buffer the caller owns
// and *outlen to its length. On failure returns false and leaves *out alone.
//
boolean	mus2mid (const void* mus, size_t muslen, byte** out, size_t* outlen);


#endif
