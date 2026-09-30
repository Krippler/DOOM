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
// Revision 1.3  1997/01/29 20:10
// DESCRIPTION:
//	Preparation of data for rendering,
//	generation of lookups, caching, retrieval by name.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: r_data.c,v 1.4 1997/02/03 16:47:55 b1 Exp $";

#include "i_system.h"
#include "z_zone.h"

#include "m_swap.h"

#include "w_wad.h"

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"

#include "doomstat.h"
#include "r_sky.h"

#ifdef LINUX
#include  <alloca.h>
#include  <stdlib.h>
#include  <ctype.h>
#include  <strings.h>
#endif


#include "r_data.h"

//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
// 



//
// Texture definition.
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//
typedef struct
{
    short	originx;
    short	originy;
    short	patch;
    short	stepdir;
    short	colormap;
} mappatch_t;


//
// Texture definition.
// A DOOM wall texture is a list of patches
// which are to be combined in a predefined order.
//
typedef struct
{
    char		name[8];
    int32_t		masked;	
    short		width;
    short		height;
    int32_t		columndirectory;	// OBSOLETE
    short		patchcount;
    mappatch_t	patches[1];
} maptexture_t;

// These two are read straight out of the WAD, so their layout is the file
// format, not the host's. The obsolete columndirectory field was declared as
// a pointer, which on a 64-bit target is 8 bytes instead of the 4 on disk and
// shifted patchcount and patches out of place.
_Static_assert (sizeof(mappatch_t) == 10, "mappatch_t must match the WAD layout");
_Static_assert (offsetof(maptexture_t, masked) == 8, "maptexture_t layout");
_Static_assert (offsetof(maptexture_t, width) == 12, "maptexture_t layout");
_Static_assert (offsetof(maptexture_t, height) == 14, "maptexture_t layout");
_Static_assert (offsetof(maptexture_t, columndirectory) == 16, "maptexture_t layout");
_Static_assert (offsetof(maptexture_t, patchcount) == 20, "maptexture_t layout");
_Static_assert (offsetof(maptexture_t, patches) == 22, "maptexture_t layout");


// A single patch from a texture definition,
//  basically a rectangular area within
//  the texture rectangle.
typedef struct
{
    // Block origin (allways UL),
    // which has allready accounted
    // for the internal origin of the patch.
    int		originx;	
    int		originy;
    int		patch;
} texpatch_t;


// A maptexturedef_t describes a rectangular texture,
//  which is composed of one or more mappatch_t structures
//  that arrange graphic patches.
typedef struct
{
    // Keep name for switch changing, etc.
    char	name[8];		
    short	width;
    short	height;
    
    // All the patches[patchcount]
    //  are drawn back to front into the cached texture.
    short	patchcount;
    texpatch_t	patches[1];		
    
} texture_t;



int		numflats;

// The lump each flat number stands for, and the other way round (-1 for a
// lump that is not a flat). See R_InitFlats.
int*		flatlumps;
static int*	lumpflat;

int		firstpatch;
int		lastpatch;
int		numpatches;

int		firstspritelump;
int		lastspritelump;
int		numspritelumps;

int		numtextures;
texture_t**	textures;


int*			texturewidthmask;
// needed for texture pegging
fixed_t*		textureheight;		
int*			texturecompositesize;
short**			texturecolumnlump;
unsigned int**		texturecolumnofs;	// past 64 KB of composite
byte**			texturecomposite;

// for global animation
int*		flattranslation;
int*		texturetranslation;

// needed for pre rendering
fixed_t*	spritewidth;	
fixed_t*	spriteoffset;
fixed_t*	spritetopoffset;

lighttable_t	*colormaps;

// Boom's, from C_START/C_END (R_InitColormaps)
static lighttable_t**	extracolormaps;
static int*		extracolormaplumps;
static int		numextracolormaps;


//
// MAPTEXTURE_T CACHING
// When a texture is first needed,
//  it counts the number of composite columns
//  required in the texture and allocates space
//  for a column directory and any new columns.
// The directory will simply point inside other patches
//  if there is only one patch in a given column,
//  but any columns with multiple patches
//  will have new column_ts generated.
//



//
// R_DrawColumnInCache
// Clip and draw a column
//  from a patch into a cached post.
//
void
R_DrawColumnInCache
( column_t*	patch,
  byte*		cache,
  int		originy,
  int		cacheheight )
{
    int		count;
    int		position;
    byte*	source;
    byte*	dest;
	
    int		top = -1;

    dest = (byte *)cache + 3;

    while (patch->topdelta != 0xff)
    {
	// tall patches: see R_DrawMaskedColumn
	if (patch->topdelta <= top)
	    top += patch->topdelta;
	else
	    top = patch->topdelta;

	source = (byte *)patch + 3;
	count = patch->length;
	position = originy + top;

	if (position < 0)
	{
	    count += position;
	    position = 0;
	}

	if (position + count > cacheheight)
	    count = cacheheight - position;

	if (count > 0)
	    memcpy (cache + position, source, count);
		
	patch = (column_t *)(  (byte *)patch + patch->length + 4); 
    }
}



//
// Masked textures made of more than one patch: the "Medusa effect".
//
// A see-through middle texture is drawn post by post, the runs of opaque
// pixels a patch column is made of, so a column that is one patch's is drawn
// straight from that patch. A column where two patches overlap has no patch
// of its own: id's code composited it into plain pixels for walls and handed
// the masked drawer those same pixels, which it then read as posts -- run
// lengths and offsets made of colours. It draws garbage, and can go on doing
// so for a very long time or into memory it should not touch: the stall, the
// freeze or the crash known as the Medusa effect. Classic maps avoid such
// textures on two-sided lines; SIGIL II has them on E6M2 (SW1LION) and E6M7
// (WOOD1, WOOD5), and its own demo of E6M2 ran ten times slower for it.
//
// So for masked drawing a multi-patch column gets posts of its own, built from
// where each patch actually has pixels. Walls go on using the plain
// composite; nothing about them changes.
//
static byte**	texturemasked;		// per texture, NULL until first needed
static int**	texturemaskedofs;	// where each column's posts start

static void R_GenerateMaskedComposite (int texnum)
{
    texture_t*	texture = textures[texnum];
    texpatch_t*	patch;
    patch_t*	realpatch;
    column_t*	patchcol;
    byte*	pixels;
    byte*	opaque;
    byte*	block;
    byte*	out;
    int		height = texture->height;
    int		rows;
    int		x, x1, x2, i, y, start, len, pos, count;
    int		ptop, otop;
    int		size = 0;

    // Worst case per column: every other row opaque, 4 bytes of post
    // around each single pixel, the empty posts that step an offset past
    // 254 (see below), and the end marker.
    rows = height;
    for (x = 0; x < texture->width; x++)
	if (texturecolumnlump[texnum][x] < 0)
	    size += rows + 4 * ((rows + 1) / 2) + 4 * (rows / 127 + 2) + 1;

    texturemaskedofs[texnum] = Z_Malloc (texture->width * sizeof(int),
					  PU_STATIC, 0);
    block = Z_Malloc (size + 1, PU_STATIC, 0);
    pixels = Z_Malloc (height, PU_STATIC, 0);
    opaque = Z_Malloc (height, PU_STATIC, 0);
    out = block;

    for (x = 0; x < texture->width; x++)
    {
	texturemaskedofs[texnum][x] = out - block;
	if (texturecolumnlump[texnum][x] >= 0)
	    continue;

	memset (opaque, 0, height);

	for (i = 0, patch = texture->patches; i < texture->patchcount;
	     i++, patch++)
	{
	    realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
	    x1 = patch->originx;
	    x2 = x1 + SHORT(realpatch->width);
	    if (x < x1 || x >= x2)
		continue;

	    patchcol = (column_t *)((byte *)realpatch
				    + LONG(realpatch->columnofs[x-x1]));
	    ptop = -1;
	    while (patchcol->topdelta != 0xff)
	    {
		if (patchcol->topdelta <= ptop)
		    ptop += patchcol->topdelta;
		else
		    ptop = patchcol->topdelta;
		pos = patch->originy + ptop;
		count = patchcol->length;
		for (y = 0; y < count; y++)
		    if (pos + y >= 0 && pos + y < height)
		    {
			pixels[pos + y] = ((byte *)patchcol)[3 + y];
			opaque[pos + y] = 1;
		    }
		patchcol = (column_t *)((byte *)patchcol + patchcol->length + 4);
	    }
	}

	// The opaque runs as posts: offset, length, a pad byte, the pixels,
	// another pad byte, as a patch column is laid out. Past row 254 an
	// offset is counted from the post before, as tall patches have it,
	// with empty posts to climb when the step is too far for one.
	otop = -1;
	for (y = 0; y < rows; )
	{
	    if (!opaque[y])
	    {
		y++;
		continue;
	    }
	    for (start = y; y < height && opaque[y] && y - start < 255; y++)
		;
	    len = y - start;
	    // Absolute when it can be (up to 254, and past the last);
	    // otherwise counted from the last, which a reader takes when the
	    // offset is no more than the last row. When the step is too big
	    // for that, empty posts climb first: to 254, then up to 254 rows
	    // at a time.
	    while (!(start <= 254 && start > otop)
		   && !(start - otop <= otop && start - otop <= 254))
	    {
		// 254 is absolute below row 254 and a step of 254 above it
		*out++ = 254;
		*out++ = 0;
		*out++ = 0;
		*out++ = 0;
		otop = otop < 254 ? 254 : otop + 254;
	    }
	    *out++ = start <= 254 && start > otop ? start : start - otop;
	    otop = start;
	    *out++ = len;
	    *out++ = pixels[start];
	    memcpy (out, pixels + start, len);
	    out += len;
	    *out++ = pixels[start + len - 1];
	}
	*out++ = 0xff;
    }

    Z_Free (pixels);
    Z_Free (opaque);
    texturemasked[texnum] = block;
}

//
// A column as the masked drawer wants it: posts, whatever the texture is
// made of.
//
column_t* R_GetMaskedColumn (int tex, int col)
{
    int		lump;

    col &= texturewidthmask[tex];
    lump = texturecolumnlump[tex][col];

    if (lump > 0)
	return (column_t *)((byte *)W_CacheLumpNum (lump, PU_CACHE)
			    + texturecolumnofs[tex][col] - 3);

    if (!texturemasked)
    {
	texturemasked = Z_Malloc (numtextures * sizeof(*texturemasked),
				  PU_STATIC, 0);
	texturemaskedofs = Z_Malloc (numtextures * sizeof(*texturemaskedofs),
				     PU_STATIC, 0);
	memset (texturemasked, 0, numtextures * sizeof(*texturemasked));
    }
    if (!texturemasked[tex])
	R_GenerateMaskedComposite (tex);

    return (column_t *)(texturemasked[tex] + texturemaskedofs[tex][col]);
}


//
// R_GenerateComposite
// Using the texture definition,
//  the composite texture is created from the patches,
//  and each column is cached.
//
void R_GenerateComposite (int texnum)
{
    byte*		block;
    texture_t*		texture;
    texpatch_t*		patch;	
    patch_t*		realpatch;
    int			x;
    int			x1;
    int			x2;
    int			i;
    column_t*		patchcol;
    short*		collump;
    unsigned int*	colofs;
	
    texture = textures[texnum];

    block = Z_Malloc (texturecompositesize[texnum],
		      PU_STATIC,
		      &texturecomposite[texnum]);
    memset (block, 0, texturecompositesize[texnum]);

    collump = texturecolumnlump[texnum];
    colofs = texturecolumnofs[texnum];
    
    // Composite the columns together.
    patch = texture->patches;
		
    for (i=0 , patch = texture->patches;
	 i<texture->patchcount;
	 i++, patch++)
    {
	realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
	x1 = patch->originx;
	x2 = x1 + SHORT(realpatch->width);

	if (x1<0)
	    x = 0;
	else
	    x = x1;
	
	if (x2 > texture->width)
	    x2 = texture->width;

	for ( ; x<x2 ; x++)
	{
	    // Column does not have multiple patches?
	    if (collump[x] >= 0)
		continue;
	    
	    patchcol = (column_t *)((byte *)realpatch
				    + LONG(realpatch->columnofs[x-x1]));
	    R_DrawColumnInCache (patchcol,
				 block + colofs[x],
				 patch->originy,
				 texture->height);
	}
						
    }

    // Now that the texture has been built in column cache,
    //  it is purgable from zone memory.
    Z_ChangeTag (block, PU_CACHE);
}



//
// R_GenerateLookup
//
void R_GenerateLookup (int texnum)
{
    texture_t*		texture;
    byte*		patchcount;	// patchcount[texture->width]
    texpatch_t*		patch;	
    patch_t*		realpatch;
    int			x;
    int			x1;
    int			x2;
    int			i;
    short*		collump;
    unsigned int*	colofs;
	
    texture = textures[texnum];

    // Composited texture not created yet.
    texturecomposite[texnum] = 0;
    
    texturecompositesize[texnum] = 0;
    collump = texturecolumnlump[texnum];
    colofs = texturecolumnofs[texnum];
    
    // Now count the number of columns
    //  that are covered by more than one patch.
    // Fill in the lump / offset, so columns
    //  with only a single patch are all done.
    patchcount = (byte *)alloca (texture->width);
    memset (patchcount, 0, texture->width);
    patch = texture->patches;
		
    for (i=0 , patch = texture->patches;
	 i<texture->patchcount;
	 i++, patch++)
    {
	realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
	x1 = patch->originx;
	x2 = x1 + SHORT(realpatch->width);
	
	if (x1 < 0)
	    x = 0;
	else
	    x = x1;

	if (x2 > texture->width)
	    x2 = texture->width;
	for ( ; x<x2 ; x++)
	{
	    patchcount[x]++;
	    collump[x] = patch->patch;
	    colofs[x] = LONG(realpatch->columnofs[x-x1])+3;

	    // A wall reads a one-patch column straight from the patch, as
	    // one run of pixels from the top. For id's textures -- 128 tall,
	    // or 64 -- that is what it is. A taller or odd-height texture's
	    // patch is often in more than one post, or shorter, or offset,
	    // and read straight it shows what lies between the posts; such
	    // a column is composited instead, as if patches overlapped.
	    if (texture->height > 128
		|| (texture->height & (texture->height - 1)))
	    {
		column_t*	col = (column_t *)((byte *)realpatch
				  + LONG(realpatch->columnofs[x-x1]));

		if (patch->originy != 0 || col->topdelta != 0
		    || col->length < texture->height
		    || ((column_t *)((byte *)col + col->length + 4))->topdelta
		       != 0xff)
		    patchcount[x]++;
	    }
	}
    }

    for (x=0 ; x<texture->width ; x++)
    {
	// A column no patch covers: id's code said so and gave up on the
	// rest of the texture, whose columns were then read from anywhere.
	// It is an empty composite column now, drawn as nothing.
	if (patchcount[x] != 1)
	{
	    // Use the cached block.
	    collump[x] = -1;
	    colofs[x] = texturecompositesize[texnum];

	    texturecompositesize[texnum] += texture->height;
	}
    }	
}




//
// R_GetColumn
//
byte*
R_GetColumn
( int		tex,
  int		col )
{
    int		lump;
    int		ofs;
	
    col &= texturewidthmask[tex];
    lump = texturecolumnlump[tex][col];
    ofs = texturecolumnofs[tex][col];
    
    if (lump > 0)
	return (byte *)W_CacheLumpNum(lump,PU_CACHE)+ofs;

    if (!texturecomposite[tex])
	R_GenerateComposite (tex);

    return texturecomposite[tex] + ofs;
}




//
// R_InitTextures
// Initializes the texture list
//  with the textures from the world map.
//
//
// Wall textures: TEXTURE1, TEXTURE2 and the PNAMES they name patches from.
//
// id's list is the last TEXTURE1 loaded, then the last TEXTURE2, in their
// own order, and that is kept exactly: savegames store wall textures by
// their number in it. What changes:
//
//   - Each TEXTURE lump names its patches through the PNAMES of its own
//     file. id's code used the last PNAMES for both lumps, so a mod with a
//     TEXTURE1 and PNAMES of its own had the IWAD's TEXTURE2 read through
//     the mod's patch list -- the wrong patches, or "Missing patch".
//   - Textures defined only in other TEXTURE lumps -- a mod that ships its
//     new textures alone, or an earlier mod's -- are added after, the latest
//     file's definition first, so they are there to be used rather than
//     stopping the game with "texture not found".
//
static texture_t**	texlist;
static int		texcount;
static int		texcap;

// A TEXTURE lump's PNAMES: its own file's, and in a file with more than one
// -- mods merged into one WAD -- the one nearest it, as each set is kept
// together.
static int R_PnamesFor (int lump)
{
    int		i;
    int		found = -1;

    for (i = 0; i < numlumps; i++)
	if (lumpinfo[i].handle == lumpinfo[lump].handle
	    && !strncasecmp (lumpinfo[i].name, "PNAMES", 8)
	    && (found < 0 || abs (i - lump) <= abs (found - lump)))
	    found = i;

    return found >= 0 ? found : W_GetNumForName ("PNAMES");
}

static boolean R_TextureListed (char* name)
{
    int		i;

    for (i = 0; i < texcount; i++)
	if (!strncasecmp (texlist[i]->name, name, 8))
	    return true;
    return false;
}

//
// The textures of one TEXTURE lump. In id's list (strict) every one is
// added, and one with a patch that is not there stops the game as it did;
// from other lumps only those not already listed, and a broken one is left
// out with a word.
//
static void R_AddTextureLump (int lump, boolean strict)
{
    int*		maptex = W_CacheLumpNum (lump, PU_STATIC);
    int			maxoff = W_LumpLength (lump);
    int			n = LONG(*maptex);
    int*		directory = maptex + 1;
    char*		names;
    int*		patchlookup;
    int			npatches;
    int			i, j, offset;
    char		name[9];
    maptexture_t*	mtexture;
    mappatch_t*		mpatch;
    texture_t*		texture;
    texpatch_t*		patch;

    names = W_CacheLumpNum (R_PnamesFor (lump), PU_STATIC);
    npatches = LONG (*((int *)names));
    patchlookup = malloc ((npatches > 0 ? npatches : 1) * sizeof(int));
    name[8] = 0;
    for (i = 0; i < npatches; i++)
    {
	strncpy (name, names + 4 + i*8, 8);
	patchlookup[i] = W_CheckNumForName (name);
    }
    Z_Free (names);

    for (i = 0; i < n; i++, directory++)
    {
	boolean	bad = false;

	offset = LONG(*directory);
	if (offset < 0 || offset > maxoff)
	    I_Error ("R_InitTextures: bad texture directory");
	mtexture = (maptexture_t *) ((byte *)maptex + offset);

	if (!strict && R_TextureListed (mtexture->name))
	    continue;

	texture = Z_Malloc (sizeof(texture_t)
			    + sizeof(texpatch_t)*(SHORT(mtexture->patchcount)-1),
			    PU_STATIC, 0);
	texture->width = SHORT(mtexture->width);
	texture->height = SHORT(mtexture->height);
	texture->patchcount = SHORT(mtexture->patchcount);
	memcpy (texture->name, mtexture->name, sizeof(texture->name));

	mpatch = &mtexture->patches[0];
	patch = &texture->patches[0];
	for (j = 0; j < texture->patchcount; j++, mpatch++, patch++)
	{
	    int	p = SHORT(mpatch->patch);

	    patch->originx = SHORT(mpatch->originx);
	    patch->originy = SHORT(mpatch->originy);
	    patch->patch = p >= 0 && p < npatches ? patchlookup[p] : -1;
	    if (patch->patch == -1)
		bad = true;
	}

	if (bad)
	{
	    memcpy (name, texture->name, 8);
	    if (strict)
		I_Error ("R_InitTextures: Missing patch in texture %s", name);
	    printf ("\nR_InitTextures: %s left out: a patch it names is missing",
		    name);
	    Z_Free (texture);
	    continue;
	}

	if (texcount == texcap)
	{
	    texcap = texcap ? texcap * 2 : 256;
	    texlist = realloc (texlist, texcap * sizeof(*texlist));
	    if (!texlist)
		I_Error ("R_InitTextures: no memory for %d textures", texcap);
	}
	texlist[texcount++] = texture;
    }

    free (patchlookup);
    Z_Free (maptex);
}

void R_InitTextures (void)
{
    int		i;
    int		j;
    int		t1, t2;
    int		added;
    texture_t*	texture;

    // id's list: the last TEXTURE1, then the last TEXTURE2
    t1 = W_GetNumForName ("TEXTURE1");
    t2 = W_CheckNumForName ("TEXTURE2");
    R_AddTextureLump (t1, true);
    if (t2 != -1)
	R_AddTextureLump (t2, true);

    // then what only other TEXTURE lumps define, the latest file first
    added = texcount;
    for (i = numlumps - 1; i >= 0; i--)
	if (i != t1 && i != t2
	    && (!strncasecmp (lumpinfo[i].name, "TEXTURE1", 8)
		|| !strncasecmp (lumpinfo[i].name, "TEXTURE2", 8)))
	    R_AddTextureLump (i, false);
    added = texcount - added;
    if (added)
	printf ("\nR_InitTextures: %d textures from other files added", added);

    numtextures = texcount;
    textures = Z_Malloc (numtextures*sizeof(*textures), PU_STATIC, 0);
    memcpy (textures, texlist, numtextures*sizeof(*textures));
    free (texlist);
    texlist = NULL;
    texcount = texcap = 0;

    texturecolumnlump = Z_Malloc (numtextures*sizeof(*texturecolumnlump), PU_STATIC, 0);
    texturecolumnofs = Z_Malloc (numtextures*sizeof(*texturecolumnofs), PU_STATIC, 0);
    texturecomposite = Z_Malloc (numtextures*sizeof(*texturecomposite), PU_STATIC, 0);
    texturecompositesize = Z_Malloc (numtextures*sizeof(*texturecompositesize), PU_STATIC, 0);
    texturewidthmask = Z_Malloc (numtextures*sizeof(*texturewidthmask), PU_STATIC, 0);
    textureheight = Z_Malloc (numtextures*sizeof(*textureheight), PU_STATIC, 0);

    for (i=0 ; i<numtextures ; i++)
    {
	if (!(i&63))
	    printf (".");

	texture = textures[i];
	texturecolumnlump[i] = Z_Malloc (texture->width*sizeof(**texturecolumnlump), PU_STATIC,0);
	texturecolumnofs[i] = Z_Malloc (texture->width*sizeof(**texturecolumnofs), PU_STATIC,0);

	j = 1;
	while (j*2 <= texture->width)
	    j<<=1;

	texturewidthmask[i] = j-1;
	textureheight[i] = texture->height<<FRACBITS;
    }

    // Precalculate whatever possible.
    for (i=0 ; i<numtextures ; i++)
	R_GenerateLookup (i);

    // Create translation table for global animation.
    texturetranslation = Z_Malloc ((numtextures+1)*sizeof(*texturetranslation), PU_STATIC, 0);

    for (i=0 ; i<numtextures ; i++)
	texturetranslation[i] = i;
}



//
// R_InitFlats
//
//
// Every flat, from every file.
//
// id's code took the flats to be the lumps between the last F_START and the
// last F_END, which holds only while the IWAD is the one file with them. A
// PWAD with floors of its own in an F_START/F_END pair of its own -- a new
// episode, SIGIL II among them -- made its markers the last ones: the engine
// saw its handful of flats and nothing else, every one of the IWAD's got a
// negative number, the sky flat included, and the renderer read far outside
// the arrays indexed by it. Where that landed decided whether it drew
// garbage or crashed, in R_GetColumn from R_DrawPlanes. id's answer was for
// mod authors to merge their flats into the IWAD's with a tool.
//
// Here the lumps between each F_START or FF_START and the F_END or FF_END
// after it, in every file, are the flats: a name seen before replaces that
// flat where it stands, so the IWAD's animated sequences stay in order, and a
// new one goes on the end. Everything between the markers counts, the
// markers inside (F1_START and so on) included, as it did for id: a savegame
// stores each floor as its flat number, so with no mod loaded the numbers
// have to come out exactly as they always did.
//
static boolean R_IsFlatMarker (char* name, char* which)
{
    char	n[9];

    memcpy (n, name, 8);
    n[8] = 0;

    if (!strcasecmp (n, which))
	return true;

    // FF_START and FF_END, as DeuTex writes them into a PWAD
    return toupper (n[0]) == 'F' && !strcasecmp (n + 1, which);
}

void R_InitFlats (void)
{
    int		i, j;
    boolean	inflats = false;
    boolean	later = false;		// past the first F_START/F_END
    int		replaced = 0, added = 0;

    flatlumps = Z_Malloc (numlumps * sizeof(*flatlumps), PU_STATIC, 0);
    lumpflat = Z_Malloc (numlumps * sizeof(*lumpflat), PU_STATIC, 0);
    numflats = 0;

    for (i = 0; i < numlumps; i++)
    {
	lumpflat[i] = -1;

	if (R_IsFlatMarker (lumpinfo[i].name, "F_START"))
	{
	    inflats = true;
	    continue;
	}
	if (R_IsFlatMarker (lumpinfo[i].name, "F_END"))
	{
	    inflats = false;
	    later = true;
	    continue;
	}
	if (!inflats)
	    continue;

	for (j = 0; j < numflats; j++)
	    if (!strncasecmp (lumpinfo[flatlumps[j]].name,
			      lumpinfo[i].name, 8))
		break;

	if (j == numflats)
	{
	    numflats++;
	    added += later;
	}
	else
	    replaced += later;
	flatlumps[j] = i;
    }

    // Worth a line when a mod brings floors: what reading them used to break.
    if (replaced || added)
	printf ("\nR_InitFlats: %d flats, %d replaced and %d added by later files",
		numflats, replaced, added);

    if (!numflats)
	I_Error ("R_InitFlats: no flats between F_START and F_END");

    for (j = 0; j < numflats; j++)
	lumpflat[flatlumps[j]] = j;

    // Create translation table for global animation.
    flattranslation = Z_Malloc ((numflats+1)*sizeof(*flattranslation), PU_STATIC, 0);
    
    for (i=0 ; i<numflats ; i++)
	flattranslation[i] = i;
}


//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
//  so the sprite does not need to be cached completely
//  just for having the header info ready during rendering.
//
//
// Every sprite picture, from every file.
//
// As with flats, id's code took the sprites to be the lumps between the last
// S_START and S_END, and a mod with sprites of its own between markers of its
// own left the game with only the mod's. id's partial answer was to look a
// sprite's lump up by name as well, so a same-named lump anywhere replaced
// it -- which found the picture but indexed its size tables out of range.
//
// Here the pictures between each S_START (or SS_START, DeuTex's) and the
// S_END (or SS_END) after it, in every file, make one list. When a later
// file has pictures for a frame of a sprite, the earlier files' pictures for
// that frame go: a frame drawn from eight rotations in the IWAD and one in a
// mod would otherwise be both, which the engine refuses. A lump of the same
// name outside the markers still replaces one, as id's code had it.
//
int*		spritelumps;	// the lump of each sprite picture

static boolean R_IsSpriteMarker (char* name, char* which)
{
    char	n[9];

    memcpy (n, name, 8);
    n[8] = 0;
    return !strcasecmp (n, which)
	|| (toupper (n[0]) == 'S' && !strcasecmp (n + 1, which));
}

// whether a picture's name covers sprite name[0..3], frame f
static boolean R_SpriteHasFrame (char* name, char* sprite, int f)
{
    if (strncasecmp (name, sprite, 4))
	return false;
    return toupper (name[4]) == f || (name[6] && toupper (name[6]) == f);
}

void R_InitSpriteLumps (void)
{
    int		i, j, k, start, end, cap = 0;
    patch_t	*patch;

    numspritelumps = 0;
    spritelumps = NULL;

    for (i = 0; i < numlumps; i++)
    {
	if (!R_IsSpriteMarker (lumpinfo[i].name, "S_START"))
	    continue;
	for (end = i + 1; end < numlumps; end++)
	    if (R_IsSpriteMarker (lumpinfo[end].name, "S_END"))
		break;
	start = i + 1;

	// This range's frames replace the earlier ranges' of the same frame,
	// whether in another file or in this one, merged.
	if (numspritelumps)
	    for (k = start; k < end; k++)
	    {
		char*	nm = lumpinfo[k].name;
		int	slot;

		if (lumpinfo[k].size <= 0)
		    continue;
		for (slot = 4; slot <= 6; slot += 2)
		{
		    int	f;
		    int	n;

		    if (slot == 6 && !nm[6])
			break;
		    f = toupper (nm[slot]);
		    for (j = n = 0; j < numspritelumps; j++)
			if (!R_SpriteHasFrame (lumpinfo[spritelumps[j]].name,
					       nm, f))
			    spritelumps[n++] = spritelumps[j];
		    numspritelumps = n;
		}
	    }

	for (k = start; k < end; k++)
	{
	    if (lumpinfo[k].size <= 0)		// nested markers
		continue;
	    for (j = 0; j < numspritelumps; j++)
		if (!strncasecmp (lumpinfo[spritelumps[j]].name,
				  lumpinfo[k].name, 8))
		    break;
	    if (j == numspritelumps)
	    {
		if (numspritelumps == cap)
		{
		    cap = cap ? cap * 2 : 1024;
		    spritelumps = realloc (spritelumps, cap * sizeof(int));
		    if (!spritelumps)
			I_Error ("R_InitSpriteLumps: no memory");
		}
		numspritelumps++;
	    }
	    spritelumps[j] = k;
	}
	i = end;
    }

    // a same-named lump anywhere after replaces a picture, as id's did
    if (modifiedgame)
	for (j = 0; j < numspritelumps; j++)
	    spritelumps[j] = W_GetNumForName (lumpinfo[spritelumps[j]].name);

    spritewidth = Z_Malloc ((numspritelumps+1)*sizeof(*spritewidth), PU_STATIC, 0);
    spriteoffset = Z_Malloc ((numspritelumps+1)*sizeof(*spriteoffset), PU_STATIC, 0);
    spritetopoffset = Z_Malloc ((numspritelumps+1)*sizeof(*spritetopoffset), PU_STATIC, 0);

    for (i=0 ; i< numspritelumps ; i++)
    {
	if (!(i&63))
	    printf (".");

	patch = W_CacheLumpNum (spritelumps[i], PU_CACHE);
	spritewidth[i] = SHORT(patch->width)<<FRACBITS;
	spriteoffset[i] = SHORT(patch->leftoffset)<<FRACBITS;
	spritetopoffset[i] = SHORT(patch->topoffset)<<FRACBITS;
    }
}



//
// R_InitColormaps
//
void R_InitColormaps (void)
{
    int	lump, length;
    int	i, inside = 0;
    
    // Load in the light tables, 
    //  256 byte align tables.
    lump = W_GetNumForName("COLORMAP"); 
    length = W_LumpLength (lump) + 255; 
    colormaps = Z_Malloc (length, PU_STATIC, 0); 
    colormaps = (byte *)( ((intptr_t)colormaps + 255)&~0xff); 
    W_ReadLump (lump,colormaps); 

    // Boom's own colormaps, between C_START and C_END in any file: for
    // the water and sky of a line 242's sectors (R_ColormapNumForName)
    numextracolormaps = 0;
    for (i = 0; i < numlumps; i++)
    {
	if (!strncasecmp (lumpinfo[i].name, "C_START", 8))
	    inside = 1;
	else if (!strncasecmp (lumpinfo[i].name, "C_END", 8))
	    inside = 0;
	else if (inside && W_LumpLength (i) >= 256*32)
	{
	    lighttable_t*	map;

	    extracolormaps = realloc (extracolormaps, (numextracolormaps + 1)
				      * sizeof(*extracolormaps));
	    extracolormaplumps = realloc (extracolormaplumps,
					  (numextracolormaps + 1)
					  * sizeof(*extracolormaplumps));
	    if (!extracolormaps || !extracolormaplumps)
		I_Error ("R_InitColormaps: no memory for colormaps");
	    // as long as COLORMAP, whatever this one's length
	    map = Z_Malloc (length, PU_STATIC, 0);
	    map = (byte *)( ((intptr_t)map + 255)&~0xff);
	    memcpy (map, colormaps, length - 255);
	    W_ReadLump (i, map);
	    extracolormaps[numextracolormaps] = map;
	    extracolormaplumps[numextracolormaps++] = i;
	}
    }
    if (numextracolormaps)
	printf ("R_InitColormaps: %d of Boom's colormaps\n", numextracolormaps);
}


//
// R_ColormapNumForName
// Boom: which colormap a name is, for a line 242's side (p_setup.c): 0 for
// COLORMAP, one of the C_START/C_END lumps from 1 on, -1 for none.
//
int R_ColormapNumForName (char* name)
{
    int		i;

    if (!strncasecmp (name, "COLORMAP", 8))
	return 0;
    for (i = numextracolormaps - 1; i >= 0; i--)
	if (!strncasecmp (lumpinfo[extracolormaplumps[i]].name, name, 8))
	    return i + 1;
    return -1;
}

//
// R_InitTranMap
// Boom's translucency (line 260, and MBF's things): what each palette
// colour drawn over each other becomes -- tranmap[under<<8 | over] -- two
// thirds of the new over one third of the old, the nearest there is. A
// WAD's own TRANMAP lump, a 64K table, is used if it has one.
//
byte*	main_tranmap;
byte*	tranmap;

void R_InitTranMap (void)
{
    int		lump = W_CheckNumForName ("TRANMAP");
    byte*	pal;
    int		under, over, i;

    if (lump >= 0 && W_LumpLength (lump) == 65536)
    {
	main_tranmap = W_CacheLumpNum (lump, PU_STATIC);
	tranmap = main_tranmap;
	return;
    }

    main_tranmap = Z_Malloc (65536, PU_STATIC, 0);
    pal = W_CacheLumpName ("PLAYPAL", PU_STATIC);
    for (under = 0; under < 256; under++)
	for (over = 0; over < 256; over++)
	{
	    int	r = (pal[over*3]   * 66 + pal[under*3]   * 34) / 100;
	    int	g = (pal[over*3+1] * 66 + pal[under*3+1] * 34) / 100;
	    int	b = (pal[over*3+2] * 66 + pal[under*3+2] * 34) / 100;
	    int	best = 0, bestdist = 0x7fffffff;

	    for (i = 0; i < 256; i++)
	    {
		int	dr = pal[i*3] - r, dg = pal[i*3+1] - g, db = pal[i*3+2] - b;
		int	dist = dr*dr*3 + dg*dg*4 + db*db*2;

		if (dist < bestdist)
		{
		    bestdist = dist;
		    best = i;
		    if (!dist)
			break;
		}
	    }
	    main_tranmap[under<<8 | over] = best;
	}
    Z_ChangeTag (pal, PU_CACHE);
    tranmap = main_tranmap;
}


// How many there are: COLORMAP and Boom's.
int R_NumColormaps (void)
{
    return numextracolormaps + 1;
}

// Colormap n of those: COLORMAP, or Boom's.
lighttable_t* R_Colormap (int n)
{
    return n > 0 && n <= numextracolormaps ? extracolormaps[n-1] : colormaps;
}



//
// R_InitData
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//
void R_InitData (void)
{
    R_InitTextures ();
    printf ("\nInitTextures");
    R_InitFlats ();
    printf ("\nInitFlats");
    R_InitSpriteLumps ();
    printf ("\nInitSprites");
    R_InitColormaps ();
    R_InitTranMap ();
    printf ("\nInitColormaps");
}



//
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
//
int R_CheckFlatNumForName (char* name)
{
    int		i;

    // The last lump of the name is nearly always the flat; a later lump
    // that happens to share it is not, and then the list is searched.
    i = W_CheckNumForName (name);

    if (i >= 0 && lumpflat[i] >= 0)
	return lumpflat[i];

    for (i = numflats - 1; i >= 0; i--)
	if (!strncasecmp (lumpinfo[flatlumps[i]].name, name, 8))
	    return i;

    return -1;
}

int R_FlatNumForName (char* name)
{
    int		i;
    char	namet[9];

    i = R_CheckFlatNumForName (name);

    if (i == -1)
    {
	namet[8] = 0;
	memcpy (namet, name,8);
	I_Error ("R_FlatNumForName: %s not found",namet);
    }
    return i;
}




//
// R_AnimFrames
// An animation's frames, from its first to its last, into frames: the
// flats between the two names in the file whose flats list both, or the
// textures between them in the TEXTURE lump that does -- the latest such
// -- each the one its name is now. How many; 0 when no file lists the
// first before the last.
//
// Numbering flats and textures by their place was id's way, and holds
// while the frames are next to each other in the game's lists. Here a
// flat or texture a later file brings again keeps the place of the one it
// replaces, and one it adds goes on the end: a mod that brings a cycle
// whose first frame the game has and whose last it does not -- Legacy of
// Rust's NUKAGE1 to NUKAGE4, of which DOOM II has three -- would cycle
// through every flat in between, 182 of them. With the game alone, the
// names give the numbers they always had.
//
int R_AnimFrames (boolean istexture, char* start, char* end, int* frames,
		  int max)
{
    char	name[9];
    int		i, j, k, n = 0;

    name[8] = 0;
    if (!istexture)
    {
	for (i = numlumps - 1; i >= 0; i--)
	{
	    if (strncasecmp (lumpinfo[i].name, end, 8))
		continue;
	    // back to the first frame, in this file's run of flats
	    for (j = i; j >= 0 && lumpinfo[j].handle == lumpinfo[i].handle; j--)
		if (R_IsFlatMarker (lumpinfo[j].name, "F_START")
		    || R_IsFlatMarker (lumpinfo[j].name, "F_END")
		    || !strncasecmp (lumpinfo[j].name, start, 8))
		    break;
	    if (j < 0 || lumpinfo[j].handle != lumpinfo[i].handle
		|| strncasecmp (lumpinfo[j].name, start, 8))
		continue;
	    for (; j <= i && n < max; j++)
	    {
		memcpy (name, lumpinfo[j].name, 8);
		if ((k = R_CheckFlatNumForName (name)) >= 0)
		    frames[n++] = k;
	    }
	    return n;
	}
	return 0;
    }

    for (i = numlumps - 1; i >= 0; i--)
    {
	int*		maptex;
	int		count, len, first = -1, last = -1;

	if (strncasecmp (lumpinfo[i].name, "TEXTURE1", 8)
	    && strncasecmp (lumpinfo[i].name, "TEXTURE2", 8))
	    continue;
	maptex = W_CacheLumpNum (i, PU_CACHE);
	len = W_LumpLength (i);
	count = len >= 4 ? LONG (*maptex) : 0;
	if (count < 0 || 4 + 4 * count > len)
	    continue;
	for (j = 0; j < count; j++)
	{
	    int	ofs = LONG (maptex[1 + j]);

	    if (ofs < 0 || ofs + 8 > len)
		break;
	    if (first < 0 && !strncasecmp ((char*) maptex + ofs, start, 8))
		first = j;
	    if (first >= 0 && !strncasecmp ((char*) maptex + ofs, end, 8))
	    {
		last = j;
		break;
	    }
	}
	if (first < 0 || last <= first)
	    continue;
	for (j = first; j <= last && n < max; j++)
	{
	    memcpy (name, (char*) maptex + LONG (maptex[1 + j]), 8);
	    if ((k = R_CheckTextureNumForName (name)) >= 0)
		frames[n++] = k;
	}
	return n;
    }
    return 0;
}


//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
int	R_CheckTextureNumForName (char *name)
{
    int		i;

    // "NoTexture" marker.
    if (name[0] == '-')		
	return 0;
		
    for (i=0 ; i<numtextures ; i++)
	if (!strncasecmp (textures[i]->name, name, 8) )
	    return i;
		
    return -1;
}



//
// R_TextureNumForName
// Calls R_CheckTextureNumForName,
//  aborts with error message.
//
int	R_TextureNumForName (char* name)
{
    int		i;
	
    i = R_CheckTextureNumForName (name);

    if (i==-1)
    {
	I_Error ("R_TextureNumForName: %s not found",
		 name);
    }
    return i;
}




//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//
int		flatmemory;
int		texturememory;
int		spritememory;

void R_PrecacheLevel (void)
{
    char*		flatpresent;
    char*		texturepresent;
    char*		spritepresent;

    int			i;
    int			j;
    int			k;
    int			lump;
    
    texture_t*		texture;
    thinker_t*		th;
    spriteframe_t*	sf;

    if (demoplayback)
	return;
    
    // Precache flats.
    flatpresent = alloca(numflats);
    memset (flatpresent,0,numflats);	

    for (i=0 ; i<numsectors ; i++)
    {
	flatpresent[sectors[i].floorpic] = 1;
	flatpresent[sectors[i].ceilingpic] = 1;
    }
	
    flatmemory = 0;

    for (i=0 ; i<numflats ; i++)
    {
	if (flatpresent[i])
	{
	    lump = flatlumps[i];
	    flatmemory += lumpinfo[lump].size;
	    W_CacheLumpNum(lump, PU_CACHE);
	}
    }
    
    // Precache textures.
    texturepresent = alloca(numtextures);
    memset (texturepresent,0, numtextures);
	
    for (i=0 ; i<numsides ; i++)
    {
	texturepresent[sides[i].toptexture] = 1;
	texturepresent[sides[i].midtexture] = 1;
	texturepresent[sides[i].bottomtexture] = 1;
    }

    // Sky texture is always present.
    // Note that F_SKY1 is the name used to
    //  indicate a sky floor/ceiling as a flat,
    //  while the sky texture is stored like
    //  a wall texture, with an episode dependend
    //  name.
    texturepresent[skytexture] = 1;
	
    texturememory = 0;
    for (i=0 ; i<numtextures ; i++)
    {
	if (!texturepresent[i])
	    continue;

	texture = textures[i];
	
	for (j=0 ; j<texture->patchcount ; j++)
	{
	    lump = texture->patches[j].patch;
	    texturememory += lumpinfo[lump].size;
	    W_CacheLumpNum(lump , PU_CACHE);
	}
    }
    
    // Precache sprites.
    spritepresent = alloca(numsprites);
    memset (spritepresent,0, numsprites);
	
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function.acp1 == (actionf_p1)P_MobjThinker)
	    spritepresent[((mobj_t *)th)->sprite] = 1;
    }
	
    spritememory = 0;
    for (i=0 ; i<numsprites ; i++)
    {
	if (!spritepresent[i])
	    continue;

	for (j=0 ; j<sprites[i].numframes ; j++)
	{
	    sf = &sprites[i].spriteframes[j];
	    for (k=0 ; k<8 ; k++)
	    {
		lump = spritelumps[sf->lump[k]];
		spritememory += lumpinfo[lump].size;
		W_CacheLumpNum(lump , PU_CACHE);
	    }
	}
    }
}




