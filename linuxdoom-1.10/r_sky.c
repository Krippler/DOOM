// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
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
// $Log:$
//
// DESCRIPTION:
//  Sky rendering. The DOOM sky is a texture map like any
//  wall, wrapping around. A 1024 columns equal 360 degrees.
//  The default sky map is 256 columns and repeats 4 times
//  on a 320 screen?
//  
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";


// Needed for FRACUNIT.
#include "m_fixed.h"

#include <stdlib.h>
#include <string.h>

// Needed for Flat retrieval.
#include "r_data.h"
#include "r_state.h"
#include "doomstat.h"
#include "w_wad.h"
#include "z_zone.h"


#ifdef __GNUG__
#pragma implementation "r_sky.h"
#endif
#include "r_sky.h"

//
// sky mapping
//
int			skyflatnum;
int			skytexture;
int			skytexturemid;
int*			flatskytexture;



//
// R_InitSkyMap
// Called whenever the view size changes.
//
void R_InitSkyMap (void)
{
  // skyflatnum = R_FlatNumForName ( SKYFLATNAME );
    skytexturemid = 100*FRACUNIT;
}


//
// R_SkyDefString
// The string after "key": between p and end, into out as a lump name.
//
static boolean R_SkyDefString (char* p, char* end, char* key, char* out)
{
    char	quoted[16];
    int		n = 0;

    snprintf (quoted, sizeof(quoted), "\"%s\"", key);
    p = strstr (p, quoted);
    if (!p || p >= end)
	return false;
    p += strlen (quoted);
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r'
		       || *p == '\n' || *p == ':'))
	p++;
    if (p >= end || *p++ != '"')
	return false;
    while (p < end && *p != '"' && n < 8)
	out[n++] = *p++;
    out[n] = 0;
    return n > 0;
}


//
// R_InitSkyDefs
// The 2024 re-release's DOOM II has a SKYDEFS lump, JSON, whose flat mapping
// makes F_RSKY1 to F_RSKY3 into skies of their own -- SKY1 to SKY3 -- to be
// shown whatever the map's sky is. MAP20 has a pit whose floor is F_RSKY3,
// the hell sky, among SKY2's city; the 1997 engine knew only F_SKY1 as sky,
// and drew the pit as a floor tiled with the flat's placeholder pattern.
// Only the flat mapping is read; the "skies" SKYDEFS can also define --
// scrolling and fire skies -- are not.
//
void R_InitSkyDefs (void)
{
    int		lump;
    int		len;
    int		i;
    int		found = 0;
    char*	text;
    char*	p;
    char*	end;

    flatskytexture = Z_Malloc (numflats * sizeof(*flatskytexture),
			       PU_STATIC, 0);
    for (i = 0; i < numflats; i++)
	flatskytexture[i] = -1;

    lump = W_CheckNumForName ("SKYDEFS");
    if (lump < 0)
	return;

    len = W_LumpLength (lump);
    text = malloc (len + 1);
    if (!text)
	return;
    W_ReadLump (lump, text);
    text[len] = 0;

    p = strstr (text, "\"flatmapping\"");
    end = p ? strchr (p, ']') : NULL;
    if (p && end)
	p = strchr (p, '[');

    while (p && end && (p = strchr (p, '{')) && p < end)
    {
	char*	close = strchr (p, '}');
	char	flat[9];
	char	sky[9];
	int	f;
	int	t;

	if (!close || close > end)
	    break;
	if (R_SkyDefString (p, close, "flat", flat)
	    && R_SkyDefString (p, close, "sky", sky))
	{
	    f = R_CheckFlatNumForName (flat);
	    t = R_CheckTextureNumForName (sky);
	    if (f >= 0 && t >= 0)
	    {
		flatskytexture[f] = t;
		found++;
	    }
	    else
		fprintf (stderr, "SKYDEFS: no %s, or no sky %s for it\n",
			 flat, sky);
	}
	p = close + 1;
    }

    free (text);
    if (found)
	printf ("\nR_InitSkyDefs: %d flat%s standing for skies",
		found, found == 1 ? "" : "s");
}

