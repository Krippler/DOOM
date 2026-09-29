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
//	Here is a core component: drawing the floors and ceilings,
//	 while maintaining a per column clipping list only.
//	Moreover, the sky areas have to be determined.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: r_plane.c,v 1.4 1997/02/03 16:47:55 b1 Exp $";

#include <stdlib.h>
#include <stdint.h>

#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "doomdef.h"
#include "doomstat.h"

#include "r_local.h"

void R_ReportOutOfRange (char* where, int a, int b, int c);
#include "r_sky.h"



planefunction_t		floorfunc;
planefunction_t		ceilingfunc;

//
// opening
//

// Here comes the obnoxious "visplane".
//
// This and the other tables of the renderer and the play code were sized in
// 1993 for id's own maps. SIGIL's E5M6 wants 132 visplanes where there were
// 128, and the game stopped; they were raised eight times over, and then, for
// maps made for limit-removing engines, made to grow as far as a map needs.
//
// Visplanes, the floor and ceiling areas seen this frame. id's array held
// 128; this one had been raised to 1024 and stopped with "no more visplanes"
// past it, and R_CheckPlane wrote past it with no check at all. Now there
// are as many as a frame needs. Each is allocated once and kept, so the
// planes the BSP walk holds on to (floorplane, ceilingplane) never move, and
// they are looked up through a hash rather than one by one -- a big open map
// makes thousands. A chain is kept in the order planes were made, so the one
// found is the first made, as id's search through the array found it.
//
#define VISPLANEHASH	128

static visplane_t**	visplanes;		// this frame's, in order made
static int		numvisplanes;
static int		maxvisplanes;
static visplane_t*	visplanehash[VISPLANEHASH];

static unsigned R_VisplaneHash (fixed_t height, int picnum, int lightlevel)
{
    return ((unsigned) height * 7 + (unsigned) picnum * 3
	    + (unsigned) lightlevel) % VISPLANEHASH;
}

static visplane_t* R_NewVisplane (fixed_t height, int picnum, int lightlevel)
{
    visplane_t*		pl;
    visplane_t**	link;
    int			i;

    if (numvisplanes == maxvisplanes)
    {
	maxvisplanes = maxvisplanes ? maxvisplanes * 2 : 128;
	visplanes = realloc (visplanes, maxvisplanes * sizeof(*visplanes));
	if (!visplanes)
	    I_Error ("R_NewVisplane: no memory for %d planes", maxvisplanes);
	// Zeroed, as id's static array was: a column the plane does not
	// cover is marked by its top alone, and R_MakeSpans reads its bottom
	// too -- a bottom of 255 there would be taken for a span on row 255.
	for (i = numvisplanes; i < maxvisplanes; i++)
	    if (!(visplanes[i] = calloc (1, sizeof(visplane_t))))
		I_Error ("R_NewVisplane: no memory for %d planes",
			 maxvisplanes);
    }

    pl = visplanes[numvisplanes++];
    pl->height = height;
    pl->picnum = picnum;
    pl->lightlevel = lightlevel;
    pl->next = NULL;

    link = &visplanehash[R_VisplaneHash (height, picnum, lightlevel)];
    while (*link)
	link = &(*link)->next;
    *link = pl;

    return pl;
}
visplane_t*		floorplane;
visplane_t*		ceilingplane;

// ?
//
// Openings: the clipping each wall segment leaves for the sprites and masked
// textures drawn after it. The array had a fixed size and was checked only
// after a frame had filled it. It grows now; the segments already pointing
// into it are moved with it (R_EnsureOpenings).
//
short*			openings;
short*			lastopening;
static int		maxopenings;


//
// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1
//
short			floorclip[SCREENWIDTH];
short			ceilingclip[SCREENWIDTH];

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
int			spanstart[SCREENHEIGHT];
int			spanstop[SCREENHEIGHT];

//
// texture mapping
//
lighttable_t**		planezlight;
fixed_t			planeheight;

fixed_t			yslope[SCREENHEIGHT];
fixed_t			distscale[SCREENWIDTH];
fixed_t			basexscale;
fixed_t			baseyscale;

fixed_t			cachedheight[SCREENHEIGHT];
fixed_t			cacheddistance[SCREENHEIGHT];
fixed_t			cachedxstep[SCREENHEIGHT];
fixed_t			cachedystep[SCREENHEIGHT];



//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes (void)
{
  // Doh!
}


//
// R_MapPlane
//
// Uses global vars:
//  planeheight
//  ds_source
//  basexscale
//  baseyscale
//  viewx
//  viewy
//
// BASIC PRIMITIVE
//
void
R_MapPlane
( int		y,
  int		x1,
  int		x2 )
{
    angle_t	angle;
    fixed_t	distance;
    fixed_t	length;
    unsigned	index;
	
#ifdef RANGECHECK
    if (x2 < x1
	|| x1<0
	|| x2>=viewwidth
	|| (unsigned)y>viewheight)
    {
	// Not fatal, for the reason given in r_draw.c: skipping the span is
	// safe, ending the game is not.
	R_ReportOutOfRange ("R_MapPlane", x1, x2, y);
	return;
    }
#endif

    if (planeheight != cachedheight[y])
    {
	cachedheight[y] = planeheight;
	distance = cacheddistance[y] = FixedMul (planeheight, yslope[y]);
	ds_xstep = cachedxstep[y] = FixedMul (distance,basexscale);
	ds_ystep = cachedystep[y] = FixedMul (distance,baseyscale);
    }
    else
    {
	distance = cacheddistance[y];
	ds_xstep = cachedxstep[y];
	ds_ystep = cachedystep[y];
    }
	
    length = FixedMul (distance,distscale[x1]);
    angle = (viewangle + xtoviewangle[x1])>>ANGLETOFINESHIFT;
    ds_xfrac = viewx + FixedMul(finecosine[angle], length);
    ds_yfrac = -viewy - FixedMul(finesine[angle], length);

    if (fixedcolormap)
	ds_colormap = fixedcolormap;
    else
    {
	index = distance >> LIGHTZSHIFT;
	
	if (index >= MAXLIGHTZ )
	    index = MAXLIGHTZ-1;

	ds_colormap = planezlight[index];
    }
	
    ds_y = y;
    ds_x1 = x1;
    ds_x2 = x2;

    // high or low detail
    spanfunc ();	
}


//
// R_ClearPlanes
// At begining of frame.
//
void R_ClearPlanes (void)
{
    int		i;
    angle_t	angle;
    
    // opening / clipping determination
    for (i=0 ; i<viewwidth ; i++)
    {
	floorclip[i] = viewheight;
	ceilingclip[i] = -1;
    }

    numvisplanes = 0;
    memset (visplanehash, 0, sizeof(visplanehash));
    lastopening = openings;
    
    // texture calculation
    memset (cachedheight, 0, sizeof(cachedheight));

    // left to right mapping
    angle = (viewangle-ANG90)>>ANGLETOFINESHIFT;
	
    // scale will be unit scale at SCREENWIDTH/2 distance
    basexscale = FixedDiv (finecosine[angle],centerxfrac);
    baseyscale = -FixedDiv (finesine[angle],centerxfrac);
}




//
// R_FindPlane
//
//
// Room for n more openings. Drawsegs hold pointers into the array, offset by
// their first column, so each is moved if it pointed there -- and left alone
// if it points at one of the fixed arrays the clipping can also use.
//
void R_EnsureOpenings (int n)
{
    int		used = lastopening - openings;
    uintptr_t	old = (uintptr_t) openings;	// compared as an address only
    drawseg_t*	ds;

    if (openings && used + n <= maxopenings)
	return;

    while (used + n > maxopenings)
	maxopenings = maxopenings ? maxopenings * 2 : SCREENWIDTH * 64;

    openings = realloc (openings, maxopenings * sizeof(*openings));
    if (!openings)
	I_Error ("R_EnsureOpenings: no memory for %d", maxopenings);
    lastopening = openings + used;

    if (!old)
	return;

#define R_MOVEOPENING(p) \
    if (p) \
    { \
	uintptr_t	at = (uintptr_t) (p) + ds->x1 * sizeof(short); \
 \
	if (at >= old && at <= old + used * sizeof(short)) \
	    (p) = openings + ((intptr_t) ((uintptr_t) (p) - old) \
			      / (intptr_t) sizeof(short)); \
    }

    for (ds = drawsegs; ds < ds_p; ds++)
    {
	R_MOVEOPENING (ds->maskedtexturecol);
	R_MOVEOPENING (ds->sprtopclip);
	R_MOVEOPENING (ds->sprbottomclip);
    }
#undef R_MOVEOPENING
}


visplane_t*
R_FindPlane
( fixed_t	height,
  int		picnum,
  int		lightlevel )
{
    visplane_t*	check;
	
    if (R_IsSkyFlat (picnum))
    {
	height = 0;			// all skys map together
	lightlevel = 0;
    }
	
    for (check = visplanehash[R_VisplaneHash (height, picnum, lightlevel)];
	 check; check = check->next)
    {
	if (height == check->height
	    && picnum == check->picnum
	    && lightlevel == check->lightlevel)
	    return check;
    }

    check = R_NewVisplane (height, picnum, lightlevel);
    check->minx = SCREENWIDTH;
    check->maxx = -1;
    
    memset (check->top,0xff,sizeof(check->top));
		
    return check;
}


//
// R_CheckPlane
//
visplane_t*
R_CheckPlane
( visplane_t*	pl,
  int		start,
  int		stop )
{
    int		intrl;
    int		intrh;
    int		unionl;
    int		unionh;
    int		x;
	
    if (start < pl->minx)
    {
	intrl = pl->minx;
	unionl = start;
    }
    else
    {
	unionl = pl->minx;
	intrl = start;
    }
	
    if (stop > pl->maxx)
    {
	intrh = pl->maxx;
	unionh = stop;
    }
    else
    {
	unionh = pl->maxx;
	intrh = stop;
    }

    for (x=intrl ; x<= intrh ; x++)
	if (pl->top[x] != 0xff)
	    break;

    if (x > intrh)
    {
	pl->minx = unionl;
	pl->maxx = unionh;

	// use the same one
	return pl;		
    }
	
    // make a new visplane
    pl = R_NewVisplane (pl->height, pl->picnum, pl->lightlevel);
    pl->minx = start;
    pl->maxx = stop;

    memset (pl->top,0xff,sizeof(pl->top));
		
    return pl;
}


//
// R_MakeSpans
//
void
R_MakeSpans
( int		x,
  int		t1,
  int		b1,
  int		t2,
  int		b2 )
{
    while (t1 < t2 && t1<=b1)
    {
	R_MapPlane (t1,spanstart[t1],x-1);
	t1++;
    }
    while (b1 > b2 && b1>=t1)
    {
	R_MapPlane (b1,spanstart[b1],x-1);
	b1--;
    }
	
    while (t2 < t1 && t2<=b2)
    {
	spanstart[t2] = x;
	t2++;
    }
    while (b2 > b1 && b2>=t2)
    {
	spanstart[b2] = x;
	b2--;
    }
}



//
// R_DrawPlanes
// At the end of each frame.
//
void R_DrawPlanes (void)
{
    visplane_t*		pl;
    int			light;
    int			x;
    int			stop;
    int			angle;
    int			i;

    for (i = 0 ; i < numvisplanes ; i++)
    {
	pl = visplanes[i];

	if (pl->minx > pl->maxx)
	    continue;

	
	// sky flat
	if (R_IsSkyFlat (pl->picnum))
	{
	    dc_iscale = pspriteiscale>>detailshift;
	    
	    // Sky is allways drawn full bright,
	    //  i.e. colormaps[0] is used.
	    // Because of this hack, sky is not affected
	    //  by INVUL inverse mapping.
	    dc_colormap = colormaps;
	    dc_texturemid = skytexturemid;
	    for (x=pl->minx ; x <= pl->maxx ; x++)
	    {
		dc_yl = pl->top[x];
		dc_yh = pl->bottom[x];

		if (dc_yl <= dc_yh)
		{
		    angle = (viewangle + xtoviewangle[x])>>ANGLETOSKYSHIFT;
		    dc_x = x;
		    dc_source = R_GetColumn(R_SkyTexture (pl->picnum), angle);
		    dc_texheight =
			textureheight[R_SkyTexture (pl->picnum)]>>FRACBITS;
		    colfunc ();
		}
	    }
	    continue;
	}
	
	// regular flat
	ds_source = W_CacheLumpNum(flatlumps[flattranslation[pl->picnum]],
				   PU_STATIC);
	
	planeheight = abs(pl->height-viewz);
	light = (pl->lightlevel >> LIGHTSEGSHIFT)+extralight;

	if (light >= LIGHTLEVELS)
	    light = LIGHTLEVELS-1;

	if (light < 0)
	    light = 0;

	planezlight = zlight[light];

	pl->top[pl->maxx+1] = 0xff;
	pl->top[pl->minx-1] = 0xff;
		
	stop = pl->maxx + 1;

	for (x=pl->minx ; x<= stop ; x++)
	{
	    R_MakeSpans(x,pl->top[x-1],
			pl->bottom[x-1],
			pl->top[x],
			pl->bottom[x]);
	}
	
	Z_ChangeTag (ds_source, PU_CACHE);
    }
}
