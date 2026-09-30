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
//	Do all the WAD I/O, get map description,
//	set up initial state and misc. LUTs.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: p_setup.c,v 1.5 1997/02/03 22:45:12 b1 Exp $";


#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "p_inflate.h"
#include "m_argv.h"
#include "z_zone.h"

#include "m_swap.h"
#include "m_bbox.h"

#include "g_game.h"

#include "i_system.h"
#include "w_wad.h"

#include "doomdef.h"
#include "p_local.h"
#include "r_lerp.h"

#include "s_sound.h"

#include "doomstat.h"
#include "d_main.h"


void	P_SpawnMapThing (mapthing_t*	mthing);


//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
int		numvertexes;
vertex_t*	vertexes;

int		numsegs;
seg_t*		segs;

int		numsectors;
sector_t*	sectors;

int		numsubsectors;
subsector_t*	subsectors;

int		numnodes;
node_t*		nodes;

int		numlines;
line_t*		lines;

int		numsides;
side_t*		sides;


// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.
int		bmapwidth;
int		bmapheight;	// size in mapblocks
int*		blockmap;	// int for larger maps: see P_LoadBlockMap
// offsets in blockmap are from here
int*		blockmaplump;		
// origin of block map
fixed_t		bmaporgx;
fixed_t		bmaporgy;
// for thing chains
mobj_t**	blocklinks;		


// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without special effect, this could be
//  used as a PVS lookup as well.
//
byte*		rejectmatrix;


// Maintain single and multi player starting spots.
mapthing_t*	deathmatchstarts;
mapthing_t*	deathmatch_p;
int		maxdeathmatchstarts;
mapthing_t	playerstarts[MAXPLAYERS];





//
// P_LoadVertexes
//
void P_LoadVertexes (int lump)
{
    byte*		data;
    int			i;
    mapvertex_t*	ml;
    vertex_t*		li;

    // Determine number of lumps:
    //  total lump length / vertex record length.
    numvertexes = W_LumpLength (lump) / sizeof(mapvertex_t);

    // Allocate zone memory for buffer.
    vertexes = Z_Malloc (numvertexes*sizeof(vertex_t),PU_LEVEL,0);	

    // Load data into cache.
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    ml = (mapvertex_t *)data;
    li = vertexes;

    // Copy and convert vertex coordinates,
    // internal representation as fixed.
    for (i=0 ; i<numvertexes ; i++, li++, ml++)
    {
	li->x = SHORT(ml->x)<<FRACBITS;
	li->y = SHORT(ml->y)<<FRACBITS;
    }

    // Free buffer memory.
    Z_Free (data);
}



//
// One seg, from whichever format it was stored in. Every index is checked:
// a map that points outside itself is stopped with a reason rather than left
// to crash somewhere later.
//
static void P_SetSeg (seg_t* li, unsigned v1, unsigned v2, unsigned linedef,
		      int side, angle_t angle, fixed_t offset)
{
    line_t*	ldef;

    if (v1 >= (unsigned) numvertexes || v2 >= (unsigned) numvertexes)
	I_Error ("P_SetSeg: seg %d has vertex %u or %u, of %d",
		 (int) (li - segs), v1, v2, numvertexes);
    if (linedef >= (unsigned) numlines)
	I_Error ("P_SetSeg: seg %d has line %u, of %d",
		 (int) (li - segs), linedef, numlines);

    side &= 1;
    ldef = &lines[linedef];
    if (ldef->sidenum[side] < 0)
	I_Error ("P_SetSeg: seg %d is on line %u's side %d, which it has not",
		 (int) (li - segs), linedef, side);

    li->v1 = &vertexes[v1];
    li->v2 = &vertexes[v2];
    li->angle = angle;
    li->offset = offset;
    li->linedef = ldef;
    li->sidedef = &sides[ldef->sidenum[side]];
    li->frontsector = sides[ldef->sidenum[side]].sector;
    if ((ldef->flags & ML_TWOSIDED) && ldef->sidenum[side^1] >= 0)
	li->backsector = sides[ldef->sidenum[side^1]].sector;
    else
	li->backsector = 0;
}


//
// P_LoadSegs
//
void P_LoadSegs (int lump)
{
    byte*		data;
    int			i;
    mapseg_t*		ml;

    numsegs = W_LumpLength (lump) / sizeof(mapseg_t);
    segs = Z_Malloc (numsegs*sizeof(seg_t),PU_LEVEL,0);
    memset (segs, 0, numsegs*sizeof(seg_t));
    data = W_CacheLumpNum (lump,PU_STATIC);

    ml = (mapseg_t *)data;
    for (i=0 ; i<numsegs ; i++, ml++)
	P_SetSeg (&segs[i],
		  (unsigned short) SHORT(ml->v1),
		  (unsigned short) SHORT(ml->v2),
		  (unsigned short) SHORT(ml->linedef),
		  SHORT(ml->side),
		  (SHORT(ml->angle))<<16,
		  (SHORT(ml->offset))<<16);

    Z_Free (data);
}

//
// P_LoadSubsectors
//
void P_LoadSubsectors (int lump)
{
    byte*		data;
    int			i;
    mapsubsector_t*	ms;
    subsector_t*	ss;

    numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
    subsectors = Z_Malloc (numsubsectors*sizeof(subsector_t),PU_LEVEL,0);
    data = W_CacheLumpNum (lump,PU_STATIC);

    ms = (mapsubsector_t *)data;
    memset (subsectors,0, numsubsectors*sizeof(subsector_t));
    ss = subsectors;

    for (i=0 ; i<numsubsectors ; i++, ss++, ms++)
    {
	ss->numlines = (unsigned short) SHORT(ms->numsegs);
	ss->firstline = (unsigned short) SHORT(ms->firstseg);
    }

    Z_Free (data);
}


//
// P_LoadSectors
//
void P_LoadSectors (int lump)
{
    byte*		data;
    int			i;
    mapsector_t*	ms;
    sector_t*		ss;
	
    numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
    sectors = Z_Malloc (numsectors*sizeof(sector_t),PU_LEVEL,0);	
    memset (sectors, 0, numsectors*sizeof(sector_t));
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    ms = (mapsector_t *)data;
    ss = sectors;
    for (i=0 ; i<numsectors ; i++, ss++, ms++)
    {
	ss->floorheight = SHORT(ms->floorheight)<<FRACBITS;
	ss->ceilingheight = SHORT(ms->ceilingheight)<<FRACBITS;
	ss->floorpic = R_FlatNumForName(ms->floorpic);
	ss->ceilingpic = R_FlatNumForName(ms->ceilingpic);
	ss->lightlevel = SHORT(ms->lightlevel);
	ss->special = SHORT(ms->special);
	ss->tag = SHORT(ms->tag);
	ss->thinglist = NULL;

	// Boom's: none of its effects until its specials say
	ss->heightsec = -1;
	ss->floorlightsec = -1;
	ss->ceilinglightsec = -1;
	ss->friction = ORIG_FRICTION;
	ss->movefactor = ORIG_FRICTION_FACTOR;
	ss->stairlock = 0;
	ss->prevsec = -1;
	ss->nextsec = -1;
    }
	
    Z_Free (data);
}


//
// P_LoadNodes
//
void P_LoadNodes (int lump)
{
    byte*	data;
    int		i;
    int		j;
    int		k;
    mapnode_t*	mn;
    node_t*	no;
	
    numnodes = W_LumpLength (lump) / sizeof(mapnode_t);
    nodes = Z_Malloc (numnodes*sizeof(node_t),PU_LEVEL,0);	
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    mn = (mapnode_t *)data;
    no = nodes;
    
    for (i=0 ; i<numnodes ; i++, no++, mn++)
    {
	no->x = SHORT(mn->x)<<FRACBITS;
	no->y = SHORT(mn->y)<<FRACBITS;
	no->dx = SHORT(mn->dx)<<FRACBITS;
	no->dy = SHORT(mn->dy)<<FRACBITS;
	for (j=0 ; j<2 ; j++)
	{
	    unsigned	c = (unsigned short) SHORT(mn->children[j]);

	    no->children[j] = c & NF_SUBSECTOR_CLASSIC
		? (c & ~NF_SUBSECTOR_CLASSIC) | NF_SUBSECTOR : c;
	    for (k=0 ; k<4 ; k++)
		no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
	}
    }
	
    Z_Free (data);
}


//
// Extended nodes.
//
// The classic NODES, SEGS and SSECTORS lumps number everything in 16 bits,
// and a subsector in 15: a map past 32767 of any of them cannot be described
// at all. Node builders for bigger maps write one of two formats instead,
// which UZDoom reads, and so does this:
//
//   DeePBSP: NODES begins "xNd4\0\0\0\0"; nodes have 32-bit children, and
//	SSECTORS and SEGS are in wider records of their own.
//   ZDBSP: NODES begins "XNOD", or "ZNOD" and the rest zlib-compressed, and
//	holds all three itself -- plus vertices the builder added, the
//	subsectors as seg counts, and segs without angle or offset, which are
//	worked out here. SSECTORS and SEGS are left empty.
//
static unsigned P_Long (byte* p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (unsigned) p[3] << 24;
}

static int P_Short (byte* p)
{
    return (short) (p[0] | p[1] << 8);
}

static void P_SetNode (node_t* no, byte* p, int wide)
{
    int		j, k;

    no->x = P_Short (p) << FRACBITS;
    no->y = P_Short (p + 2) << FRACBITS;
    no->dx = P_Short (p + 4) << FRACBITS;
    no->dy = P_Short (p + 6) << FRACBITS;
    for (j = 0; j < 2; j++)
	for (k = 0; k < 4; k++)
	    no->bbox[j][k] = P_Short (p + 8 + j*8 + k*2) << FRACBITS;
    for (j = 0; j < 2; j++)
    {
	unsigned c = wide ? P_Long (p + 24 + j*4)
			  : (unsigned short) P_Short (p + 24 + j*2);

	if (!wide && (c & NF_SUBSECTOR_CLASSIC))
	    c = (c & ~NF_SUBSECTOR_CLASSIC) | NF_SUBSECTOR;
	no->children[j] = c;
    }
}

static void P_CheckNodes (void)
{
    int		i, j;

    for (i = 0; i < numnodes; i++)
	for (j = 0; j < 2; j++)
	{
	    unsigned	c = nodes[i].children[j];

	    if (c & NF_SUBSECTOR ? (c & ~NF_SUBSECTOR) >= (unsigned) numsubsectors
				 : c >= (unsigned) numnodes)
		I_Error ("P_LoadNodes: node %d points at %s %u, of %d", i,
			 c & NF_SUBSECTOR ? "subsector" : "node",
			 c & ~NF_SUBSECTOR,
			 c & NF_SUBSECTOR ? numsubsectors : numnodes);
	}
    for (i = 0; i < numsubsectors; i++)
	if (subsectors[i].firstline < 0 || subsectors[i].numlines < 0
	    || subsectors[i].firstline + subsectors[i].numlines > numsegs)
	    I_Error ("P_LoadSubsectors: subsector %d has segs %d to %d, of %d",
		     i, subsectors[i].firstline,
		     subsectors[i].firstline + subsectors[i].numlines, numsegs);
}

static void P_LoadDeePBSP (int lumpnum)
{
    byte*	data;
    byte*	p;
    int		i;

    data = W_CacheLumpNum (lumpnum+ML_SSECTORS, PU_STATIC);
    numsubsectors = W_LumpLength (lumpnum+ML_SSECTORS) / 6;
    subsectors = Z_Malloc (numsubsectors*sizeof(subsector_t), PU_LEVEL, 0);
    memset (subsectors, 0, numsubsectors*sizeof(subsector_t));
    for (i = 0, p = data; i < numsubsectors; i++, p += 6)
    {
	subsectors[i].numlines = (unsigned short) P_Short (p);
	subsectors[i].firstline = P_Long (p + 2);
    }
    Z_Free (data);

    data = W_CacheLumpNum (lumpnum+ML_NODES, PU_STATIC);
    numnodes = (W_LumpLength (lumpnum+ML_NODES) - 8) / 32;
    nodes = Z_Malloc (numnodes*sizeof(node_t), PU_LEVEL, 0);
    for (i = 0, p = data + 8; i < numnodes; i++, p += 32)
	P_SetNode (&nodes[i], p, 1);
    Z_Free (data);

    data = W_CacheLumpNum (lumpnum+ML_SEGS, PU_STATIC);
    numsegs = W_LumpLength (lumpnum+ML_SEGS) / 16;
    segs = Z_Malloc (numsegs*sizeof(seg_t), PU_LEVEL, 0);
    memset (segs, 0, numsegs*sizeof(seg_t));
    for (i = 0, p = data; i < numsegs; i++, p += 16)
	P_SetSeg (&segs[i], P_Long (p), P_Long (p + 4),
		  (unsigned short) P_Short (p + 10), P_Short (p + 12),
		  (angle_t) (unsigned short) P_Short (p + 8) << 16,
		  P_Short (p + 14) << 16);
    Z_Free (data);

    P_CheckNodes ();
}

static void P_LoadZDBSP (int lumpnum, boolean compressed)
{
    byte*	lump;
    byte*	data;
    byte*	p;
    byte*	end;
    int		len = W_LumpLength (lumpnum+ML_NODES);
    unsigned	orgverts, newverts, n;
    unsigned	i, first;
    vertex_t*	old = vertexes;

    lump = W_CacheLumpNum (lumpnum+ML_NODES, PU_STATIC);

    if (compressed)
    {
	data = P_Inflate (lump + 4, len - 4, &len);
	p = data;
    }
    else
    {
	data = NULL;
	p = lump + 4;
	len -= 4;
    }
    end = p + len;

#define NEED(n) if (p + (n) > end) \
	I_Error ("P_LoadZDBSP: the nodes lump ends early")

    // vertices: the map's first orgverts, then the builder's own
    NEED (8);
    orgverts = P_Long (p);
    newverts = P_Long (p + 4);
    p += 8;
    if (orgverts > (unsigned) numvertexes)
	I_Error ("P_LoadZDBSP: nodes want %u of the map's vertices; it has %d",
		 orgverts, numvertexes);
    NEED (newverts * 8);
    vertexes = Z_Malloc ((orgverts + newverts) * sizeof(vertex_t), PU_LEVEL, 0);
    memcpy (vertexes, old, orgverts * sizeof(vertex_t));
    for (i = 0; i < newverts; i++, p += 8)
    {
	vertexes[orgverts + i].x = P_Long (p);
	vertexes[orgverts + i].y = P_Long (p + 4);
    }
    // the lines were given pointers into the old array
    for (i = 0; i < (unsigned) numlines; i++)
    {
	unsigned	a = lines[i].v1 - old, b = lines[i].v2 - old;

	if (a >= orgverts || b >= orgverts)
	    I_Error ("P_LoadZDBSP: line %u uses a vertex the nodes dropped", i);
	lines[i].v1 = &vertexes[a];
	lines[i].v2 = &vertexes[b];
    }
    Z_Free (old);
    numvertexes = orgverts + newverts;

    // subsectors, as counts of consecutive segs
    NEED (4);
    numsubsectors = P_Long (p);
    p += 4;
    NEED ((unsigned) numsubsectors * 4);
    subsectors = Z_Malloc (numsubsectors*sizeof(subsector_t), PU_LEVEL, 0);
    memset (subsectors, 0, numsubsectors*sizeof(subsector_t));
    for (i = 0, first = 0; i < (unsigned) numsubsectors; i++, p += 4)
    {
	subsectors[i].firstline = first;
	subsectors[i].numlines = P_Long (p);
	first += subsectors[i].numlines;
    }

    // segs: vertices, line and side; angle and offset are worked out
    NEED (4);
    numsegs = P_Long (p);
    p += 4;
    if (first != (unsigned) numsegs)
	I_Error ("P_LoadZDBSP: subsectors hold %u segs; there are %d",
		 first, numsegs);
    NEED ((unsigned) numsegs * 11);
    segs = Z_Malloc (numsegs*sizeof(seg_t), PU_LEVEL, 0);
    memset (segs, 0, numsegs*sizeof(seg_t));
    for (i = 0; i < (unsigned) numsegs; i++, p += 11)
    {
	seg_t*		li = &segs[i];
	vertex_t*	from;
	double		dx, dy;

	P_SetSeg (li, P_Long (p), P_Long (p + 4),
		  (unsigned short) P_Short (p + 8), p[10], 0, 0);
	li->angle = R_PointToAngle2 (li->v1->x, li->v1->y,
				     li->v2->x, li->v2->y);
	from = p[10] ? li->linedef->v2 : li->linedef->v1;
	dx = (double) (li->v1->x - from->x);
	dy = (double) (li->v1->y - from->y);
	li->offset = (fixed_t) sqrt (dx*dx + dy*dy);
    }

    // nodes
    NEED (4);
    n = P_Long (p);
    p += 4;
    NEED (n * 32);
    numnodes = n;
    nodes = Z_Malloc (numnodes*sizeof(node_t), PU_LEVEL, 0);
    for (i = 0; i < n; i++, p += 32)
	P_SetNode (&nodes[i], p, 1);
#undef NEED

    free (data);
    Z_Free (lump);

    P_CheckNodes ();
}

//
// Which format the map's nodes are in, and read them.
//
static void P_LoadNodeFormat (int lumpnum)
{
    int		len = W_LumpLength (lumpnum+ML_NODES);
    byte	head[8];

    memset (head, 0, sizeof(head));
    if (len >= 8)
    {
	byte*	data = W_CacheLumpNum (lumpnum+ML_NODES, PU_STATIC);

	memcpy (head, data, 8);
	Z_Free (data);
    }

    if (!memcmp (head, "xNd4\0\0\0\0", 8))
    {
	printf ("\nP_SetupLevel: DeePBSP extended nodes");
	P_LoadDeePBSP (lumpnum);
    }
    else if (!memcmp (head, "XNOD", 4) || !memcmp (head, "ZNOD", 4))
    {
	printf ("\nP_SetupLevel: ZDBSP extended nodes%s",
		head[0] == 'Z' ? ", compressed" : "");
	P_LoadZDBSP (lumpnum, head[0] == 'Z');
    }
    else if (!memcmp (head, "XGL", 3) || !memcmp (head, "ZGL", 3))
	I_Error ("P_SetupLevel: this map has only GL nodes (%.4s), which need"
		 " a GL engine; rebuild it with regular nodes", head);
    else if (!len)
	I_Error ("P_SetupLevel: this map has no nodes; it needs a node"
		 " builder run on it");
    else
    {
	P_LoadSubsectors (lumpnum+ML_SSECTORS);
	P_LoadNodes (lumpnum+ML_NODES);
	P_LoadSegs (lumpnum+ML_SEGS);
	P_CheckNodes ();
    }
}


//
// P_LoadThings
//
void P_LoadThings (int lump)
{
    byte*		data;
    int			i;
    mapthing_t*		mt;
    int			numthings;
    boolean		spawn;
	
    data = W_CacheLumpNum (lump,PU_STATIC);
    numthings = W_LumpLength (lump) / sizeof(mapthing_t);
	
    mt = (mapthing_t *)data;
    for (i=0 ; i<numthings ; i++, mt++)
    {
	spawn = true;

	// Do not spawn cool, new monsters if !commercial
	if ( gamemode != commercial)
	{
	    switch(mt->type)
	    {
	      case 68:	// Arachnotron
	      case 64:	// Archvile
	      case 88:	// Boss Brain
	      case 89:	// Boss Shooter
	      case 69:	// Hell Knight
	      case 67:	// Mancubus
	      case 71:	// Pain Elemental
	      case 65:	// Former Human Commando
	      case 66:	// Revenant
	      case 84:	// Wolf SS
		spawn = false;
		break;
	    }
	}
	if (spawn == false)
	    break;

	// Do spawn all other stuff. 
	mt->x = SHORT(mt->x);
	mt->y = SHORT(mt->y);
	mt->angle = SHORT(mt->angle);
	mt->type = SHORT(mt->type);
	mt->options = SHORT(mt->options);
	
	P_SpawnMapThing (mt);
    }
	
    Z_Free (data);
}


//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//
void P_LoadLineDefs (int lump)
{
    byte*		data;
    int			i;
    int			j;
    maplinedef_t*	mld;
    line_t*		ld;
    vertex_t*		v1;
    vertex_t*		v2;
	
    numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
    lines = Z_Malloc (numlines*sizeof(line_t),PU_LEVEL,0);	
    memset (lines, 0, numlines*sizeof(line_t));
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    mld = (maplinedef_t *)data;
    ld = lines;
    for (i=0 ; i<numlines ; i++, mld++, ld++)
    {
	ld->flags = SHORT(mld->flags);
	ld->special = SHORT(mld->special);
	ld->tag = SHORT(mld->tag);
	{
	    unsigned	i1 = (unsigned short) SHORT(mld->v1);
	    unsigned	i2 = (unsigned short) SHORT(mld->v2);

	    if (i1 >= (unsigned) numvertexes || i2 >= (unsigned) numvertexes)
		I_Error ("P_LoadLineDefs: line %d has vertex %u or %u, of %d",
			 i, i1, i2, numvertexes);
	    v1 = ld->v1 = &vertexes[i1];
	    v2 = ld->v2 = &vertexes[i2];
	}
	ld->dx = v2->x - v1->x;
	ld->dy = v2->y - v1->y;
	
	if (!ld->dx)
	    ld->slopetype = ST_VERTICAL;
	else if (!ld->dy)
	    ld->slopetype = ST_HORIZONTAL;
	else
	{
	    if (FixedDiv (ld->dy , ld->dx) > 0)
		ld->slopetype = ST_POSITIVE;
	    else
		ld->slopetype = ST_NEGATIVE;
	}
		
	if (v1->x < v2->x)
	{
	    ld->bbox[BOXLEFT] = v1->x;
	    ld->bbox[BOXRIGHT] = v2->x;
	}
	else
	{
	    ld->bbox[BOXLEFT] = v2->x;
	    ld->bbox[BOXRIGHT] = v1->x;
	}

	if (v1->y < v2->y)
	{
	    ld->bbox[BOXBOTTOM] = v1->y;
	    ld->bbox[BOXTOP] = v2->y;
	}
	else
	{
	    ld->bbox[BOXBOTTOM] = v2->y;
	    ld->bbox[BOXTOP] = v1->y;
	}

	// 0xffff is none; anything else is a sidedef, up to 65534 of them
	for (j = 0; j < 2; j++)
	{
	    unsigned	sn = (unsigned short) SHORT(mld->sidenum[j]);

	    ld->sidenum[j] = sn == 0xffff ? -1 : (int) sn;
	    if (ld->sidenum[j] >= numsides)
		I_Error ("P_LoadLineDefs: line %d has sidedef %d, of %d",
			 i, ld->sidenum[j], numsides);
	}

	if (ld->sidenum[0] != -1)
	    ld->frontsector = sides[ld->sidenum[0]].sector;
	else
	    ld->frontsector = 0;

	if (ld->sidenum[1] != -1)
	    ld->backsector = sides[ld->sidenum[1]].sector;
	else
	    ld->backsector = 0;
    }
	
    // Boom's translucent middle textures (260): this line, or with a tag,
    // every line of that tag; its side said which table (P_LoadSideDefs)
    for (i = 0; i < numlines; i++)
	lines[i].tranlump = -1;
    for (i = 0; i < numlines; i++)
	if (lines[i].special == 260 && lines[i].sidenum[0] != -1)
	{
	    int	lump = sides[lines[i].sidenum[0]].special;

	    if (!lines[i].tag)
		lines[i].tranlump = lump;
	    else
		for (j = 0; j < numlines; j++)
		    if (lines[j].tag == lines[i].tag)
			lines[j].tranlump = lump;
	}
	
    Z_Free (data);
}


//
// P_LoadSideDefs
//
//
// A 242 or 260 line's side names colormaps or a translucency table where
// textures would go (Boom). Each side is told its line's special first,
// from the LINEDEFS lump, since the lines are read after the sides.
//
static void P_SideSpecials (int linelump)
{
    maplinedef_t*	ml = W_CacheLumpNum (linelump, PU_STATIC);
    int			n = W_LumpLength (linelump) / sizeof(maplinedef_t);
    int			i;
    unsigned		sn;

    for (i = 0; i < n; i++)
    {
	short	special = SHORT(ml[i].special);

	if (special != 242 && special != 260)
	    continue;
	sn = (unsigned short) SHORT(ml[i].sidenum[0]);
	if (sn < (unsigned) numsides)
	    sides[sn].special = special;
    }
    Z_Free (ml);
}

// A 242 line's texture: a colormap in C_START/C_END (*map, and no texture)
// if it names one, else a texture as ever
static short P_ColormapOrTexture (char* name, int* map)
{
    if ((*map = R_ColormapNumForName (name)) >= 0)
	return 0;
    *map = 0;
    return R_TextureNumForName (name);
}

void P_LoadSideDefs (int lump, int linelump)
{
    byte*		data;
    int			i;
    mapsidedef_t*	msd;
    side_t*		sd;
	
    numsides = W_LumpLength (lump) / sizeof(mapsidedef_t);
    sides = Z_Malloc (numsides*sizeof(side_t),PU_LEVEL,0);	
    memset (sides, 0, numsides*sizeof(side_t));
    data = W_CacheLumpNum (lump,PU_STATIC);

    P_SideSpecials (linelump);
	
    msd = (mapsidedef_t *)data;
    sd = sides;
    for (i=0 ; i<numsides ; i++, msd++, sd++)
    {
	sd->textureoffset = SHORT(msd->textureoffset)<<FRACBITS;
	sd->rowoffset = SHORT(msd->rowoffset)<<FRACBITS;
	{
	    unsigned	sec = (unsigned short) SHORT(msd->sector);

	    if (sec >= (unsigned) numsectors)
		I_Error ("P_LoadSideDefs: sidedef %d has sector %u, of %d",
			 i, sec, numsectors);
	    sd->sector = &sectors[sec];
	}
	switch (sd->special)
	{
	  case 242:
	    // Boom: the colormaps under, in and above the water of the
	    // sectors it is the heights of (r_bsp.c)
	    sd->bottomtexture = P_ColormapOrTexture (msd->bottomtexture,
						     &sd->sector->bottommap);
	    sd->midtexture = P_ColormapOrTexture (msd->midtexture,
						  &sd->sector->midmap);
	    sd->toptexture = P_ColormapOrTexture (msd->toptexture,
						  &sd->sector->topmap);
	    break;

	  case 260:
	    // Boom: the middle texture may name a translucency table, a
	    // 64K lump, or TRANMAP for the one made from the palette;
	    // special keeps which (the lump plus one, 0 for TRANMAP)
	    if (!strncasecmp (msd->midtexture, "TRANMAP", 8))
	    {
		sd->special = 0;
		sd->midtexture = 0;
	    }
	    else
	    {
		int	l = W_CheckNumForName (msd->midtexture);

		if (l >= 0 && W_LumpLength (l) == 65536)
		{
		    sd->special = l + 1;
		    sd->midtexture = 0;
		}
		else
		{
		    sd->special = 0;
		    sd->midtexture = R_TextureNumForName (msd->midtexture);
		}
	    }
	    sd->toptexture = R_TextureNumForName (msd->toptexture);
	    sd->bottomtexture = R_TextureNumForName (msd->bottomtexture);
	    break;

	  default:
	    sd->toptexture = R_TextureNumForName(msd->toptexture);
	    sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
	    sd->midtexture = R_TextureNumForName(msd->midtexture);
	    break;
	}
    }
	
    Z_Free (data);
}


//
// Whether a blockmap read from the map makes sense: its offsets inside the
// lump and past its own table, each list ending before the lump does, and
// every line number a line. A blockmap too big for 16-bit offsets wraps
// them round into its own table, which this catches.
//
static boolean P_BlockMapValid (int count)
{
    int		w = blockmaplump[2];
    int		h = blockmaplump[3];
    int		table;
    int		i, j;

    if (w <= 0 || h <= 0 || 4 + (long) w * h > count)
	return false;

    table = 4 + w * h;
    for (i = 4; i < table; i++)
    {
	int	ofs = blockmaplump[i];

	if (ofs < table || ofs >= count)
	    return false;
	for (j = ofs; j < count && blockmaplump[j] != -1; j++)
	    if (blockmaplump[j] >= numlines && numlines)
		return false;
	if (j == count)
	    return false;
    }
    return true;
}

static void P_ClearBlockLinks (void)
{
    int		count = sizeof(*blocklinks) * bmapwidth * bmapheight;

    blocklinks = Z_Malloc (count, PU_LEVEL, 0);
    memset (blocklinks, 0, count);
}

//
// A blockmap made from the lines, when the map has none or one that cannot
// be used. Blocks of 128 units from the lower left of all the lines, and in
// each block's list every line that passes through or touches the block, in
// line order -- after a 0, as the node builders of the time began every
// list, so a line 0 behaves as it always has.
//
static void P_CreateBlockMap (void)
{
    fixed_t	minx = MAXINT, miny = MAXINT, maxx = MININT, maxy = MININT;
    int		i, b, total;
    int**	lists;
    int*	counts;
    int*	caps;
    int*	out;
    int		nblocks;

    for (i = 0; i < numlines; i++)
    {
	if (lines[i].bbox[BOXLEFT] < minx)	minx = lines[i].bbox[BOXLEFT];
	if (lines[i].bbox[BOXBOTTOM] < miny)	miny = lines[i].bbox[BOXBOTTOM];
	if (lines[i].bbox[BOXRIGHT] > maxx)	maxx = lines[i].bbox[BOXRIGHT];
	if (lines[i].bbox[BOXTOP] > maxy)	maxy = lines[i].bbox[BOXTOP];
    }
    if (!numlines)
	minx = miny = maxx = maxy = 0;

    bmaporgx = (minx >> FRACBITS) << FRACBITS;
    bmaporgy = (miny >> FRACBITS) << FRACBITS;
    bmapwidth = ((maxx - bmaporgx) >> MAPBLOCKSHIFT) + 1;
    bmapheight = ((maxy - bmaporgy) >> MAPBLOCKSHIFT) + 1;
    nblocks = bmapwidth * bmapheight;

    lists = calloc (nblocks, sizeof(*lists));
    counts = calloc (nblocks, sizeof(*counts));
    caps = calloc (nblocks, sizeof(*caps));
    if (!lists || !counts || !caps)
	I_Error ("P_CreateBlockMap: no memory for %d blocks", nblocks);

    for (i = 0; i < numlines; i++)
    {
	line_t*	ld = &lines[i];
	// a unit past its ends, so that a line along a block's edge is in
	// the blocks on both sides of it
	int	bx0 = (ld->bbox[BOXLEFT] - FRACUNIT - bmaporgx) >> MAPBLOCKSHIFT;
	int	bx1 = (ld->bbox[BOXRIGHT] + FRACUNIT - bmaporgx) >> MAPBLOCKSHIFT;
	int	by0 = (ld->bbox[BOXBOTTOM] - FRACUNIT - bmaporgy) >> MAPBLOCKSHIFT;
	int	by1 = (ld->bbox[BOXTOP] + FRACUNIT - bmaporgy) >> MAPBLOCKSHIFT;
	int	bx, by;

	if (bx0 < 0) bx0 = 0;
	if (by0 < 0) by0 = 0;
	if (bx1 >= bmapwidth) bx1 = bmapwidth - 1;
	if (by1 >= bmapheight) by1 = bmapheight - 1;

	for (by = by0; by <= by1; by++)
	    for (bx = bx0; bx <= bx1; bx++)
	    {
		fixed_t	box[4];

		// a line crosses a block unless all four corners are on one
		// side of it -- the block a unit bigger all round, for a line
		// along its edge: id's rule missed those, and a move beside
		// such a line went through it (a map without a BLOCKMAP lump,
		// with lines on the 128-unit grid)
		box[BOXLEFT] = bmaporgx + (bx << MAPBLOCKSHIFT) - FRACUNIT;
		box[BOXRIGHT] = box[BOXLEFT] + MAPBLOCKSIZE + 2*FRACUNIT;
		box[BOXBOTTOM] = bmaporgy + (by << MAPBLOCKSHIFT) - FRACUNIT;
		box[BOXTOP] = box[BOXBOTTOM] + MAPBLOCKSIZE + 2*FRACUNIT;
		if (P_BoxOnLineSide (box, ld) != -1)
		    continue;

		b = by * bmapwidth + bx;
		if (counts[b] == caps[b])
		{
		    caps[b] = caps[b] ? caps[b] * 2 : 8;
		    lists[b] = realloc (lists[b], caps[b] * sizeof(**lists));
		    if (!lists[b])
			I_Error ("P_CreateBlockMap: no memory");
		}
		lists[b][counts[b]++] = i;
	    }
    }

    total = 4 + nblocks;
    for (b = 0; b < nblocks; b++)
	total += counts[b] + 2;

    blockmaplump = Z_Malloc (total * sizeof(*blockmaplump), PU_LEVEL, 0);
    blockmaplump[0] = bmaporgx >> FRACBITS;
    blockmaplump[1] = bmaporgy >> FRACBITS;
    blockmaplump[2] = bmapwidth;
    blockmaplump[3] = bmapheight;
    out = blockmaplump + 4 + nblocks;
    for (b = 0; b < nblocks; b++)
    {
	blockmaplump[4 + b] = out - blockmaplump;
	*out++ = 0;
	memcpy (out, lists[b], counts[b] * sizeof(*out));
	out += counts[b];
	*out++ = -1;
	free (lists[b]);
    }
    free (lists);
    free (counts);
    free (caps);

    blockmap = blockmaplump + 4;
    P_ClearBlockLinks ();
    printf ("\nP_CreateBlockMap: %d by %d blocks", bmapwidth, bmapheight);
}


//
// P_LoadBlockMap
//
void P_LoadBlockMap (int lump)
{
    int		i;
    int		count;
    short*	data;

    count = W_LumpLength (lump)/2;

    // Read whole, as numbers the engine can use past 32767: the header's
    // origin signed, its size and every offset and line number unsigned, and
    // 0xffff the end of a list. id read it all as signed 16-bit, which a
    // blockmap over 64 KB -- a big map's -- turns into nonsense.
    if (count >= 4 && !M_CheckParm ("-blockmap"))
    {
	data = W_CacheLumpNum (lump, PU_STATIC);
	blockmaplump = Z_Malloc (count * sizeof(*blockmaplump), PU_LEVEL, 0);

	blockmaplump[0] = SHORT(data[0]);
	blockmaplump[1] = SHORT(data[1]);
	for (i = 2; i < count; i++)
	{
	    unsigned	v = (unsigned short) SHORT(data[i]);

	    blockmaplump[i] = v == 0xffff ? -1 : (int) v;
	}
	Z_Free (data);

	if (P_BlockMapValid (count))
	{
	    blockmap = blockmaplump+4;
	    bmaporgx = blockmaplump[0]<<FRACBITS;
	    bmaporgy = blockmaplump[1]<<FRACBITS;
	    bmapwidth = blockmaplump[2];
	    bmapheight = blockmaplump[3];
	    P_ClearBlockLinks ();
	    return;
	}

	Z_Free (blockmaplump);
	printf ("\nP_LoadBlockMap: the map's blockmap is not usable; making one");
    }

    // None, or one past fixing: made from the lines, as UZDoom does. It needs
    // the lines, which are not loaded yet; P_SetupLevel calls again.
    blockmaplump = NULL;
}


//
// The REJECT table, one bit per pair of sectors. A lump shorter than the map
// needs -- or none, which some node builders leave -- was read past its end;
// what is missing reads as zero now, "may be able to see", as UZDoom has it.
//
static void P_LoadReject (int lump)
{
    int		need = (numsectors * numsectors + 7) / 8;
    int		have = W_LumpLength (lump);

    if (have >= need)
    {
	rejectmatrix = W_CacheLumpNum (lump, PU_LEVEL);
	return;
    }

    rejectmatrix = Z_Malloc (need, PU_LEVEL, 0);
    memset (rejectmatrix, 0, need);
    if (have)
    {
	byte*	data = W_CacheLumpNum (lump, PU_STATIC);

	memcpy (rejectmatrix, data, have);
	Z_Free (data);
    }
}


//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void P_GroupLines (void)
{
    line_t**		linebuffer;
    int			i;
    int			j;
    int			total;
    line_t*		li;
    sector_t*		sector;
    subsector_t*	ss;
    seg_t*		seg;
    fixed_t		bbox[4];
    int			block;
	
    // look up sector number for each subsector
    ss = subsectors;
    for (i=0 ; i<numsubsectors ; i++, ss++)
    {
	seg = &segs[ss->firstline];
	ss->sector = seg->sidedef->sector;
    }

    // count number of lines in each sector
    li = lines;
    total = 0;
    for (i=0 ; i<numlines ; i++, li++)
    {
	total++;
	li->frontsector->linecount++;

	if (li->backsector && li->backsector != li->frontsector)
	{
	    li->backsector->linecount++;
	    total++;
	}
    }
	
    // build line tables for each sector	
    linebuffer = Z_Malloc (total*sizeof(*linebuffer), PU_LEVEL, 0);
    sector = sectors;
    for (i=0 ; i<numsectors ; i++, sector++)
    {
	M_ClearBox (bbox);
	sector->lines = linebuffer;
	li = lines;
	for (j=0 ; j<numlines ; j++, li++)
	{
	    if (li->frontsector == sector || li->backsector == sector)
	    {
		*linebuffer++ = li;
		M_AddToBox (bbox, li->v1->x, li->v1->y);
		M_AddToBox (bbox, li->v2->x, li->v2->y);
	    }
	}
	if (linebuffer - sector->lines != sector->linecount)
	    I_Error ("P_GroupLines: miscounted");
			
	// set the degenmobj_t to the middle of the bounding box
	sector->soundorg.x = (bbox[BOXRIGHT]+bbox[BOXLEFT])/2;
	sector->soundorg.y = (bbox[BOXTOP]+bbox[BOXBOTTOM])/2;
		
	// adjust bounding box to map blocks
	block = (bbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapheight ? bmapheight-1 : block;
	sector->blockbox[BOXTOP]=block;

	block = (bbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	sector->blockbox[BOXBOTTOM]=block;

	block = (bbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapwidth ? bmapwidth-1 : block;
	sector->blockbox[BOXRIGHT]=block;

	block = (bbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	sector->blockbox[BOXLEFT]=block;
    }
	
}


//
// P_SetCompatibility
// Whether the map is played as DOOM played it, or as a Boom map (by way of
// MBF21, which Boom's descendants play them as): demo_compatibility,
// demo_version, mbf21, and the comp flags (doomstat.h).
//
// Decided for a whole WAD, not map by map, so that a mod's maps all play
// by the rules its author made them for, even one that happens to use
// nothing Boom added: a COMPLVL lump in it says (as DSDA-Doom reads it),
// else any of its maps using a line or sector type Boom added makes it a
// Boom WAD. The IWAD's maps, and a mod's that use nothing of Boom's, are
// DOOM's -- and their demos play back.
//
typedef enum { WADCOMP_UNKNOWN, WADCOMP_VANILLA, WADCOMP_BOOM } wadcomp_e;

static boolean P_MapUsesBoom (int marker)
{
    maplinedef_t*	ml;
    mapsector_t*	ms;
    int			n, i;
    boolean		boom = false;

    if (marker + ML_SECTORS >= numlumps
	|| strncasecmp (lumpinfo[marker + ML_LINEDEFS].name, "LINEDEFS", 8)
	|| strncasecmp (lumpinfo[marker + ML_SECTORS].name, "SECTORS", 8))
	return false;

    ml = W_CacheLumpNum (marker + ML_LINEDEFS, PU_STATIC);
    n = W_LumpLength (marker + ML_LINEDEFS) / sizeof(maplinedef_t);
    for (i = 0; i < n && !boom; i++)
    {
	unsigned	special = (unsigned short) SHORT(ml[i].special);

	// DOOM's go to 141; Boom's and the generalized ones up to 0x7fff.
	// 0xffff is in The Ultimate DOOM's E2M7, and does nothing.
	if (special > 141 && special < GenEnd)
	    boom = true;
    }
    Z_Free (ml);

    ms = W_CacheLumpNum (marker + ML_SECTORS, PU_STATIC);
    n = W_LumpLength (marker + ML_SECTORS) / sizeof(mapsector_t);
    for (i = 0; i < n && !boom; i++)
	if ((unsigned short) SHORT(ms[i].special) >= 32)	// Boom's bits
	    boom = true;
    Z_Free (ms);

    return boom;
}

static wadcomp_e P_WadCompatibility (int handle)
{
    int		i;
    wadcomp_e	c = WADCOMP_VANILLA;

    // a COMPLVL lump, as DSDA-Doom reads it
    for (i = 0; i < numlumps; i++)
	if (lumpinfo[i].handle == handle
	    && !strncasecmp (lumpinfo[i].name, "COMPLVL", 8))
	{
	    char	text[16];
	    char*	data = W_CacheLumpNum (i, PU_STATIC);
	    int		len = W_LumpLength (i), k = 0, j;

	    for (j = 0; j < len && k < 15; j++)
		if (!isspace ((unsigned char) data[j]))
		    text[k++] = tolower ((unsigned char) data[j]);
	    text[k] = 0;
	    Z_Free (data);
	    printf ("COMPLVL: %s\n", text);
	    return strcmp (text, "vanilla") ? WADCOMP_BOOM : WADCOMP_VANILLA;
	}

    for (i = 0; i < numlumps; i++)
	if (lumpinfo[i].handle == handle
	    && i + 1 < numlumps
	    && !strncasecmp (lumpinfo[i + 1].name, "THINGS", 8)
	    && P_MapUsesBoom (i))
	    return WADCOMP_BOOM;
    return c;
}

//
// P_LumpFile
// The name of the file a lump came from, for messages: each file's lumps
// are together, in the order of wadfiles.
//
static const char* P_LumpFile (int lumpnum)
{
    const char*	name;
    const char*	slash;
    int		i;
    int		n = 0;

    for (i = 1; i <= lumpnum; i++)
	if (lumpinfo[i].handle != lumpinfo[i-1].handle)
	    n++;
    for (i = 0; i < n && wadfiles[i]; i++)
	;
    if (!wadfiles[i])
	return "?";
    name = wadfiles[i];
    slash = strrchr (name, '/');
    return slash ? slash + 1 : name;
}

void P_SetCompatibility (int lumpnum)
{
    static int		lasthandle = -1;
    static wadcomp_e	last;
    int			handle = lumpinfo[lumpnum].handle;
    int			i;

    if (handle == lumpinfo[0].handle)
	last = WADCOMP_VANILLA;		// the IWAD's
    else if (handle != lasthandle)
    {
	last = P_WadCompatibility (handle);
	printf ("P_SetupLevel: %s's maps play as %s\n",
		P_LumpFile (lumpnum),
		last == WADCOMP_BOOM ? "Boom's (MBF21)" : "DOOM's");
    }
    lasthandle = handle;

    demo_compatibility = last != WADCOMP_BOOM;
    demo_version = demo_compatibility ? DV_VANILLA : DV_MBF21;
    mbf21 = !demo_compatibility;

    // DOOM's behaviour in full, or the fixes as MBF21 has them
    for (i = 0; i < COMP_TOTAL; i++)
	comp[i] = demo_compatibility;
    if (!demo_compatibility)
    {
	comp[comp_zombie] = 1;
	comp[comp_pursuit] = 1;
	comp[comp_ledgeblock] = 1;
	comp[comp_friendlyspawn] = 1;
	comp[comp_reservedlineflag] = 1;
    }
}


//
// P_SetupLevel
//
void
P_SetupLevel
( int		episode,
  int		map,
  int		playermask,
  skill_t	skill)
{
    int		i;
    char	lumpname[9];
    int		lumpnum;
	
    totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
    wminfo.partime = 180;
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	players[i].killcount = players[i].secretcount 
	    = players[i].itemcount = 0;
    }

    // Initial height of PointOfView
    // will be set by player think.
    players[consoleplayer].viewz = 1;

    // nothing to draw moving until the new level has run a tic
    R_LerpReset ();

    // Make sure all sounds are stopped before Z_FreeTags.
    S_Start ();			

    
#if 0 // UNUSED
    if (debugfile)
    {
	Z_FreeTags (PU_LEVEL, MAXINT);
	Z_FileDumpHeap (debugfile);
    }
    else
#endif
	Z_FreeTags (PU_LEVEL, PU_PURGELEVEL-1);


    // UNUSED W_Profile ();
    P_InitThinkers ();

    // if working with a devlopment map, reload it
    W_Reload ();			
	   
    // find map name
    if ( gamemode == commercial)
    {
	if (map<10)
	    sprintf (lumpname,"map0%i", map);
	else
	    sprintf (lumpname,"map%i", map);
    }
    else
    {
	lumpname[0] = 'E';
	lumpname[1] = '0' + episode;
	lumpname[2] = 'M';
	lumpname[3] = '0' + map;
	lumpname[4] = 0;
    }

    lumpnum = W_GetNumForName (lumpname);
	
    leveltime = 0;
	
    // note: most of this ordering is important
    P_LoadVertexes (lumpnum+ML_VERTEXES);
    P_LoadSectors (lumpnum+ML_SECTORS);
    P_LoadSideDefs (lumpnum+ML_SIDEDEFS, lumpnum+ML_LINEDEFS);

    P_LoadLineDefs (lumpnum+ML_LINEDEFS);

    // Boom: the sectors and lines of each tag, chained; and whether this
    // map plays as DOOM's or as a Boom map (which the things' linking into
    // their sectors needs to know)
    P_InitTagLists ();
    P_ClearSecnodes ();
    P_SetCompatibility (lumpnum);

    // after the lines, which a blockmap's lists name and one made needs
    P_LoadBlockMap (lumpnum+ML_BLOCKMAP);
    if (!blockmaplump)
	P_CreateBlockMap ();
    P_LoadNodeFormat (lumpnum);
    P_LoadReject (lumpnum+ML_REJECT);
    P_GroupLines ();

    bodyqueslot = 0;
    if (!deathmatchstarts)
    {
	maxdeathmatchstarts = 10;
	deathmatchstarts = malloc (maxdeathmatchstarts
				   * sizeof(*deathmatchstarts));
	if (!deathmatchstarts)
	    I_Error ("P_SetupLevel: no memory for deathmatch starts");
    }
    deathmatch_p = deathmatchstarts;
    P_LoadThings (lumpnum+ML_THINGS);
    
    // if deathmatch, randomly spawn the active players
    if (deathmatch)
    {
	for (i=0 ; i<MAXPLAYERS ; i++)
	    if (playeringame[i])
	    {
		players[i].mo = NULL;
		G_DeathMatchSpawnPlayer (i);
	    }
			
    }

    // clear special respawning que
    iquehead = iquetail = 0;		
	
    // set up world state
    P_SpawnSpecials ();
	
    // build subsector connect matrix
    //	UNUSED P_ConnectSubsectors ();

    // preload graphics
    if (precache)
	R_PrecacheLevel ();

    //printf ("free memory: 0x%x\n", Z_FreeMemory());

}



//
// P_Init
//
void P_Init (void)
{
    P_InitSwitchList ();
    P_InitPicAnims ();
    R_InitSprites (sprnames);
}



