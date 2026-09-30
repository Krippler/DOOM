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
// DESCRIPTION:
//  Refresh module, data I/O, caching, retrieval of graphics
//  by name.
//
//-----------------------------------------------------------------------------


#ifndef __R_DATA__
#define __R_DATA__

#include "r_defs.h"
#include "r_state.h"

#ifdef __GNUG__
#pragma interface
#endif

// Retrieve column data for span blitting.
byte*
R_GetColumn
( int		tex,
  int		col );


// I/O, setting up the stuff.
void R_InitData (void);
void R_PrecacheLevel (void);


// Retrieval.
// Floor/ceiling opaque texture tiles,
// lookup by name. For animation?
int R_FlatNumForName (char* name);

// A texture column in posts, for see-through middle textures.
column_t* R_GetMaskedColumn (int tex, int col);
int R_CheckFlatNumForName (char* name);	// -1 if there is no such flat

// An animation's frames, start to end, as a file lists them; 0 if none does.
int R_AnimFrames (boolean istexture, char* start, char* end, int* frames,
		  int max);


// Called by P_Ticker for switches and animations,
// returns the texture number for the texture name.
int R_TextureNumForName (char *name);

// Boom's colormaps: which a name is (0 COLORMAP, -1 none), and its table.
int R_ColormapNumForName (char* name);
lighttable_t* R_Colormap (int n);
int R_NumColormaps (void);

// Boom's translucency tables: the one made from the palette (or TRANMAP),
// and the one R_DrawTLColumn draws with.
extern byte*	main_tranmap;
extern byte*	tranmap;
void R_InitTranMap (void);
int R_CheckTextureNumForName (char *name);

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
