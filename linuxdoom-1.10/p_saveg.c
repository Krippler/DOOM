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
//	Archiving: SaveGame I/O.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: p_tick.c,v 1.4 1997/02/03 16:47:55 b1 Exp $";

#include <stddef.h>
#include <stdlib.h>

#include "i_system.h"
#include "z_zone.h"
#include "p_local.h"

// State.
#include "doomstat.h"
#include "r_state.h"
#include "w_wad.h"
#include "doomdata.h"
#include "m_swap.h"

extern int	numtextures;

byte*		save_p;


// Pads save_p to a 4-byte boundary
//  so that the load/save works on SGI&Gecko.
#define PADSAVEP()	save_p += (4 - ((intptr_t) save_p & 3)) & 3



//
// P_ArchiveSize
// id's savegame buffer was a fixed size, and the code checked it had not
// been overrun after writing the whole game into it. The level decides now:
// a player, a sector, a line with both its sides, and a thinker of the
// biggest kind for every thinker there is, each with its type byte and the
// padding PADSAVEP can add.
//
int P_ArchiveSize (void)
{
    thinker_t*	th;
    int		thinkers = 0;
    int		biggest = MOBJ_SAVESIZE;

    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
	thinkers++;

#define BIGGEST(t)	if ((int)sizeof(t) > biggest) biggest = sizeof(t)
    BIGGEST(ceiling_t);
    BIGGEST(vldoor_t);
    BIGGEST(floormove_t);
    BIGGEST(plat_t);
    BIGGEST(lightflash_t);
    BIGGEST(strobe_t);
    BIGGEST(glow_t);
#undef BIGGEST

    return MAXPLAYERS * (3 + (int)sizeof(player_t))
	+ numsectors * 7 * 2
	+ numlines * (3 + 2*5) * 2
	+ thinkers * (4 + biggest)
	+ 2;				// the two end markers
}



//
// P_ArchivePlayers
//
void P_ArchivePlayers (void)
{
    int		i;
    int		j;
    player_t*	dest;
		
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;
	
	PADSAVEP();

	dest = (player_t *)save_p;
	memcpy (dest,&players[i],sizeof(player_t));
	save_p += sizeof(player_t);
	for (j=0 ; j<NUMPSPRITES ; j++)
	{
	    if (dest->psprites[j].state)
	    {
		dest->psprites[j].state 
		    = (state_t *)(dest->psprites[j].state-states);
	    }
	}
    }
}



//
// P_UnArchivePlayers
//
void P_UnArchivePlayers (void)
{
    int		i;
    int		j;
	
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;
	
	PADSAVEP();

	memcpy (&players[i],save_p, sizeof(player_t));
	save_p += sizeof(player_t);
	
	// will be set when unarc thinker
	players[i].mo = NULL;	
	players[i].message = NULL;
	players[i].attacker = NULL;

	for (j=0 ; j<NUMPSPRITES ; j++)
	{
	    if (players[i]. psprites[j].state)
	    {
		players[i]. psprites[j].state 
		    = &states[ (intptr_t)players[i].psprites[j].state ];
	    }
	}
    }
}


//
// P_ArchiveWorld
//
void P_ArchiveWorld (void)
{
    int			i;
    int			j;
    sector_t*		sec;
    line_t*		li;
    side_t*		si;
    short*		put;
	
    put = (short *)save_p;
    
    // do sectors
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
	*put++ = sec->floorheight >> FRACBITS;
	*put++ = sec->ceilingheight >> FRACBITS;
	*put++ = sec->floorpic;
	*put++ = sec->ceilingpic;
	*put++ = sec->lightlevel;
	*put++ = sec->special;		// needed?
	*put++ = sec->tag;		// needed?
    }

    
    // do lines
    for (i=0, li = lines ; i<numlines ; i++,li++)
    {
	*put++ = li->flags;
	*put++ = li->special;
	*put++ = li->tag;
	for (j=0 ; j<2 ; j++)
	{
	    if (li->sidenum[j] == -1)
		continue;
	    
	    si = &sides[li->sidenum[j]];

	    *put++ = si->textureoffset >> FRACBITS;
	    *put++ = si->rowoffset >> FRACBITS;
	    *put++ = si->toptexture;
	    *put++ = si->bottomtexture;
	    *put++ = si->midtexture;	
	}
    }
	
    save_p = (byte *)put;
}



//
// P_UnArchiveWorld
//
void P_UnArchiveWorld (void)
{
    int			i;
    int			j;
    sector_t*		sec;
    line_t*		li;
    side_t*		si;
    short*		get;
	
    get = (short *)save_p;
    
    // do sectors
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
	sec->floorheight = *get++ << FRACBITS;
	sec->ceilingheight = *get++ << FRACBITS;
	sec->floorpic = *get++;
	sec->ceilingpic = *get++;
	sec->lightlevel = *get++;
	sec->special = *get++;		// needed?
	sec->tag = *get++;		// needed?
	sec->specialdata = 0;
	sec->soundtarget = 0;
    }
    
    // do lines
    for (i=0, li = lines ; i<numlines ; i++,li++)
    {
	li->flags = *get++;
	li->special = *get++;
	li->tag = *get++;
	for (j=0 ; j<2 ; j++)
	{
	    if (li->sidenum[j] == -1)
		continue;
	    si = &sides[li->sidenum[j]];
	    si->textureoffset = *get++ << FRACBITS;
	    si->rowoffset = *get++ << FRACBITS;
	    si->toptexture = *get++;
	    si->bottomtexture = *get++;
	    si->midtexture = *get++;
	}
    }
    save_p = (byte *)get;	
}





//
// Thinkers
//
typedef enum
{
    tc_end,
    tc_mobj

} thinkerclass_t;



//
// P_ArchiveThinkers
//
//
// Targets and tracers.
//
// id's savegame kept each thing's target and tracer as the pointers they
// were, and P_UnArchiveThinkers set target to NULL and left tracer as it was:
// a pointer into the process that saved. A monster loaded in the middle of
// an attack then fired at nothing -- a Mancubus's A_FatAttack read NULL->x,
// harmless under DOS and a segmentation fault here -- and a revenant's
// missile followed its old tracer into whatever memory was there.
//
// They are saved now as the number of the thing they point to, counting
// things from 1 in the order they are saved, and 0 for none or for one not
// saved (a thing removed but still pointed to). A save from before holds
// pointers, and both are cleared for it; G_DoLoadGame says which kind of
// save it has (savegamerefs). id's engine clears target whichever it is.
//
boolean			savegamerefs;
static mobj_t**		savemobjs;
static int		numsavemobjs, maxsavemobjs;

static void P_AddSaveMobj (mobj_t* mo)
{
    if (numsavemobjs == maxsavemobjs)
    {
	maxsavemobjs = maxsavemobjs ? maxsavemobjs * 2 : 1024;
	savemobjs = realloc (savemobjs, maxsavemobjs * sizeof(*savemobjs));
	if (!savemobjs)
	    I_Error ("P_AddSaveMobj: no memory for %d things", maxsavemobjs);
    }
    savemobjs[numsavemobjs++] = mo;
}

// A pointer as a number, from the things being saved. What the pointer
// points to may be freed, so the number read from it is only trusted once
// the table agrees.
static intptr_t P_SaveRef (mobj_t* mo)
{
    int		i;

    if (!mo)
	return 0;
    i = mo->saveindex;
    if (i < 1 || i > numsavemobjs || savemobjs[i-1] != mo)
	return 0;
    return i;
}

static mobj_t* P_LoadRef (intptr_t i)
{
    return i >= 1 && i <= numsavemobjs ? savemobjs[i-1] : NULL;
}


void P_ArchiveThinkers (void)
{
    thinker_t*		th;
    mobj_t*		mobj;

    // number the things first, for what points to them
    numsavemobjs = 0;
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
	if (th->function.acp1 == (actionf_p1)P_MobjThinker)
	{
	    P_AddSaveMobj ((mobj_t *)th);
	    ((mobj_t *)th)->saveindex = numsavemobjs;
	}
	
    // save off the current thinkers
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function.acp1 == (actionf_p1)P_MobjThinker)
	{
	    *save_p++ = tc_mobj;
	    PADSAVEP();
	    mobj = (mobj_t *)save_p;
	    memcpy (mobj, th, MOBJ_SAVESIZE);
	    save_p += MOBJ_SAVESIZE;
	    mobj->state = (state_t *)(mobj->state - states);
	    mobj->target = (mobj_t *) P_SaveRef (((mobj_t *)th)->target);
	    mobj->tracer = (mobj_t *) P_SaveRef (((mobj_t *)th)->tracer);
	    
	    if (mobj->player)
		mobj->player = (player_t *)((mobj->player-players) + 1);
	    continue;
	}
		
	// I_Error ("P_ArchiveThinkers: Unknown thinker function");
    }

    // add a terminating marker
    *save_p++ = tc_end;	
}



//
// P_UnArchiveThinkers
//
void P_UnArchiveThinkers (void)
{
    byte		tclass;
    thinker_t*		currentthinker;
    thinker_t*		next;
    mobj_t*		mobj;
    int			i;
    
    // remove all the current thinkers
    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
	next = currentthinker->next;
	
	if (currentthinker->function.acp1 == (actionf_p1)P_MobjThinker)
	    P_RemoveMobj ((mobj_t *)currentthinker);
	else
	    Z_Free (currentthinker);

	currentthinker = next;
    }
    P_InitThinkers ();
    numsavemobjs = 0;
	
    // read in saved thinkers
    while (1)
    {
	tclass = *save_p++;
	switch (tclass)
	{
	  case tc_end:
	    // what they point to, now that all of them are here
	    for (i = 0; i < numsavemobjs; i++)
	    {
		mobj = savemobjs[i];
		mobj->target = savegamerefs
		    ? P_LoadRef ((intptr_t) mobj->target) : NULL;
		mobj->tracer = savegamerefs
		    ? P_LoadRef ((intptr_t) mobj->tracer) : NULL;
	    }
	    return; 	// end of list
			
	  case tc_mobj:
	    PADSAVEP();
	    mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);
	    memcpy (mobj, save_p, MOBJ_SAVESIZE);
	    save_p += MOBJ_SAVESIZE;
	    mobj->oldx = mobj->x;
	    mobj->oldy = mobj->y;
	    mobj->oldz = mobj->z;
	    mobj->oldangle = mobj->angle;
	    mobj->state = &states[(intptr_t)mobj->state];
	    P_AddSaveMobj (mobj);	// target and tracer at the end
	    if (mobj->player)
	    {
		mobj->player = &players[(intptr_t)mobj->player-1];
		mobj->player->mo = mobj;
	    }
	    P_SetThingPosition (mobj);
	    mobj->info = &mobjinfo[mobj->type];
	    mobj->floorz = mobj->subsector->sector->floorheight;
	    mobj->ceilingz = mobj->subsector->sector->ceilingheight;
	    mobj->thinker.function.acp1 = (actionf_p1)P_MobjThinker;
	    P_AddThinker (&mobj->thinker);
	    break;
			
	  default:
	    I_Error ("Unknown tclass %i in savegame",tclass);
	}
	
    }

}


//
// P_ArchiveSpecials
//
enum
{
    tc_ceiling,
    tc_door,
    tc_floor,
    tc_plat,
    tc_flash,
    tc_strobe,
    tc_glow,
    tc_endspecials

} specials_e;	



//
// Things to handle:
//
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
//
void P_ArchiveSpecials (void)
{
    thinker_t*		th;
    ceiling_t*		ceiling;
    vldoor_t*		door;
    floormove_t*	floor;
    plat_t*		plat;
    lightflash_t*	flash;
    strobe_t*		strobe;
    glow_t*		glow;
    int			i;
	
    // save off the current thinkers
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function.acv == (actionf_v)NULL)
	{
	    for (i = 0; i < maxceilings;i++)
		if (activeceilings[i] == (ceiling_t *)th)
		    break;
	    
	    if (i<maxceilings)
	    {
		*save_p++ = tc_ceiling;
		PADSAVEP();
		ceiling = (ceiling_t *)save_p;
		memcpy (ceiling, th, sizeof(*ceiling));
		save_p += sizeof(*ceiling);
		ceiling->sector = (sector_t *)(ceiling->sector - sectors);
	    }
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_MoveCeiling)
	{
	    *save_p++ = tc_ceiling;
	    PADSAVEP();
	    ceiling = (ceiling_t *)save_p;
	    memcpy (ceiling, th, sizeof(*ceiling));
	    save_p += sizeof(*ceiling);
	    ceiling->sector = (sector_t *)(ceiling->sector - sectors);
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_VerticalDoor)
	{
	    *save_p++ = tc_door;
	    PADSAVEP();
	    door = (vldoor_t *)save_p;
	    memcpy (door, th, sizeof(*door));
	    save_p += sizeof(*door);
	    door->sector = (sector_t *)(door->sector - sectors);
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_MoveFloor)
	{
	    *save_p++ = tc_floor;
	    PADSAVEP();
	    floor = (floormove_t *)save_p;
	    memcpy (floor, th, sizeof(*floor));
	    save_p += sizeof(*floor);
	    floor->sector = (sector_t *)(floor->sector - sectors);
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_PlatRaise)
	{
	    *save_p++ = tc_plat;
	    PADSAVEP();
	    plat = (plat_t *)save_p;
	    memcpy (plat, th, sizeof(*plat));
	    save_p += sizeof(*plat);
	    plat->sector = (sector_t *)(plat->sector - sectors);
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_LightFlash)
	{
	    *save_p++ = tc_flash;
	    PADSAVEP();
	    flash = (lightflash_t *)save_p;
	    memcpy (flash, th, sizeof(*flash));
	    save_p += sizeof(*flash);
	    flash->sector = (sector_t *)(flash->sector - sectors);
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_StrobeFlash)
	{
	    *save_p++ = tc_strobe;
	    PADSAVEP();
	    strobe = (strobe_t *)save_p;
	    memcpy (strobe, th, sizeof(*strobe));
	    save_p += sizeof(*strobe);
	    strobe->sector = (sector_t *)(strobe->sector - sectors);
	    continue;
	}
			
	if (th->function.acp1 == (actionf_p1)T_Glow)
	{
	    *save_p++ = tc_glow;
	    PADSAVEP();
	    glow = (glow_t *)save_p;
	    memcpy (glow, th, sizeof(*glow));
	    save_p += sizeof(*glow);
	    glow->sector = (sector_t *)(glow->sector - sectors);
	    continue;
	}
    }
	
    // add a terminating marker
    *save_p++ = tc_endspecials;	

}


//
// P_UnArchiveSpecials
//
void P_UnArchiveSpecials (void)
{
    byte		tclass;
    ceiling_t*		ceiling;
    vldoor_t*		door;
    floormove_t*	floor;
    plat_t*		plat;
    lightflash_t*	flash;
    strobe_t*		strobe;
    glow_t*		glow;
	
	
    // read in saved thinkers
    while (1)
    {
	tclass = *save_p++;
	switch (tclass)
	{
	  case tc_endspecials:
	    return;	// end of list
			
	  case tc_ceiling:
	    PADSAVEP();
	    ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVEL, NULL);
	    memcpy (ceiling, save_p, sizeof(*ceiling));
	    save_p += sizeof(*ceiling);
	    ceiling->sector = &sectors[(intptr_t)ceiling->sector];
	    ceiling->sector->specialdata = ceiling;

	    if (ceiling->thinker.function.acp1)
		ceiling->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;

	    P_AddThinker (&ceiling->thinker);
	    P_AddActiveCeiling(ceiling);
	    break;
				
	  case tc_door:
	    PADSAVEP();
	    door = Z_Malloc (sizeof(*door), PU_LEVEL, NULL);
	    memcpy (door, save_p, sizeof(*door));
	    save_p += sizeof(*door);
	    door->sector = &sectors[(intptr_t)door->sector];
	    door->sector->specialdata = door;
	    door->thinker.function.acp1 = (actionf_p1)T_VerticalDoor;
	    P_AddThinker (&door->thinker);
	    break;
				
	  case tc_floor:
	    PADSAVEP();
	    floor = Z_Malloc (sizeof(*floor), PU_LEVEL, NULL);
	    memcpy (floor, save_p, sizeof(*floor));
	    save_p += sizeof(*floor);
	    floor->sector = &sectors[(intptr_t)floor->sector];
	    floor->sector->specialdata = floor;
	    floor->thinker.function.acp1 = (actionf_p1)T_MoveFloor;
	    P_AddThinker (&floor->thinker);
	    break;
				
	  case tc_plat:
	    PADSAVEP();
	    plat = Z_Malloc (sizeof(*plat), PU_LEVEL, NULL);
	    memcpy (plat, save_p, sizeof(*plat));
	    save_p += sizeof(*plat);
	    plat->sector = &sectors[(intptr_t)plat->sector];
	    plat->sector->specialdata = plat;

	    if (plat->thinker.function.acp1)
		plat->thinker.function.acp1 = (actionf_p1)T_PlatRaise;

	    P_AddThinker (&plat->thinker);
	    P_AddActivePlat(plat);
	    break;
				
	  case tc_flash:
	    PADSAVEP();
	    flash = Z_Malloc (sizeof(*flash), PU_LEVEL, NULL);
	    memcpy (flash, save_p, sizeof(*flash));
	    save_p += sizeof(*flash);
	    flash->sector = &sectors[(intptr_t)flash->sector];
	    flash->thinker.function.acp1 = (actionf_p1)T_LightFlash;
	    P_AddThinker (&flash->thinker);
	    break;
				
	  case tc_strobe:
	    PADSAVEP();
	    strobe = Z_Malloc (sizeof(*strobe), PU_LEVEL, NULL);
	    memcpy (strobe, save_p, sizeof(*strobe));
	    save_p += sizeof(*strobe);
	    strobe->sector = &sectors[(intptr_t)strobe->sector];
	    strobe->thinker.function.acp1 = (actionf_p1)T_StrobeFlash;
	    P_AddThinker (&strobe->thinker);
	    break;
				
	  case tc_glow:
	    PADSAVEP();
	    glow = Z_Malloc (sizeof(*glow), PU_LEVEL, NULL);
	    memcpy (glow, save_p, sizeof(*glow));
	    save_p += sizeof(*glow);
	    glow->sector = &sectors[(intptr_t)glow->sector];
	    glow->thinker.function.acp1 = (actionf_p1)T_Glow;
	    P_AddThinker (&glow->thinker);
	    break;
				
	  default:
	    I_Error ("P_UnarchiveSpecials:Unknown tclass %i "
		     "in savegame",tclass);
	}
	
    }

}




//
// P_SaveGameFits
// Whether the archive at p, up to end, is one the four routines above can
// read back onto the map whose marker lump is maplump, for the players in
// ingame -- read without changing anything, before the game in progress is
// given up for it.
//
// A savegame is the level by number: so many sectors, then so many lines
// with so many sides, then things and specials naming sectors, states and
// types by index. Loaded on a map that is not the one it was saved on (the
// same ExMy in another game or mod), those numbers run past the ends of the
// tables and the engine fell over in P_UnArchiveSpecials. Here each is held
// to what this map and this game have.
//
boolean P_SaveGameFits (byte* p, byte* end, int maplump, boolean* ingame)
{
    int			nsectors, nlines, i, j;
    int			nmobjs = 0;
    uintptr_t		maxref = 0;
    maplinedef_t*	ml;
    short		v[5];
    byte		tclass;

#define PAD()	p += (4 - ((intptr_t) p & 3)) & 3
#define NEED(n)	if (end - p < (int)(n)) return false

    if (maplump < 0 || maplump + ML_SECTORS >= numlumps)
	return false;
    nsectors = W_LumpLength (maplump + ML_SECTORS) / sizeof(mapsector_t);
    nlines = W_LumpLength (maplump + ML_LINEDEFS) / sizeof(maplinedef_t);

    // players
    for (i = 0; i < MAXPLAYERS; i++)
    {
	player_t	pl;

	if (!ingame[i])
	    continue;
	PAD();
	NEED(sizeof(pl));
	memcpy (&pl, p, sizeof(pl));
	p += sizeof(pl);
	for (j = 0; j < NUMPSPRITES; j++)
	    if ((uintptr_t) pl.psprites[j].state >= NUMSTATES)
		return false;
    }

    // world: the sectors' flats, and the lines' sides' textures
    for (i = 0; i < nsectors; i++)
    {
	NEED(7 * sizeof(short));
	memcpy (v, p, 4 * sizeof(short));
	p += 7 * sizeof(short);
	if (v[2] < 0 || v[2] >= numflats || v[3] < 0 || v[3] >= numflats)
	    return false;
    }
    ml = W_CacheLumpNum (maplump + ML_LINEDEFS, PU_CACHE);
    for (i = 0; i < nlines; i++)
    {
	NEED(3 * sizeof(short));
	p += 3 * sizeof(short);
	for (j = 0; j < 2; j++)
	{
	    if ((unsigned short) SHORT(ml[i].sidenum[j]) == 0xffff)
		continue;
	    NEED(5 * sizeof(short));
	    memcpy (v, p, 5 * sizeof(short));
	    p += 5 * sizeof(short);
	    if (v[2] < 0 || v[2] >= numtextures
		|| v[3] < 0 || v[3] >= numtextures
		|| v[4] < 0 || v[4] >= numtextures)
		return false;
	}
    }

    // things
    while (1)
    {
	mobj_t	mo;

	NEED(1);
	tclass = *p++;
	if (tclass == tc_end)
	    break;
	if (tclass != tc_mobj)
	    return false;
	PAD();
	NEED(MOBJ_SAVESIZE);
	memcpy (&mo, p, MOBJ_SAVESIZE);
	p += MOBJ_SAVESIZE;
	if ((uintptr_t) mo.state >= NUMSTATES
	    || (unsigned) mo.type >= NUMMOBJTYPES
	    || (uintptr_t) mo.player > MAXPLAYERS
	    || (mo.player && !ingame[(uintptr_t) mo.player - 1]))
	    return false;
	nmobjs++;
	if (savegamerefs)
	{
	    if ((uintptr_t) mo.target > maxref)
		maxref = (uintptr_t) mo.target;
	    if ((uintptr_t) mo.tracer > maxref)
		maxref = (uintptr_t) mo.tracer;
	}
    }
    if (maxref > (uintptr_t) nmobjs)
	return false;

    // specials: each has its sector's number where the sector was
    while (1)
    {
	int		size, at;
	sector_t*	sector;

	NEED(1);
	tclass = *p++;
	switch (tclass)
	{
	  case tc_endspecials:
	    NEED(1);
	    return *p == 0x1d;		// what G_DoLoadGame checks after
	  case tc_ceiling:
	    size = sizeof(ceiling_t);
	    at = offsetof(ceiling_t, sector);
	    break;
	  case tc_door:
	    size = sizeof(vldoor_t);
	    at = offsetof(vldoor_t, sector);
	    break;
	  case tc_floor:
	    size = sizeof(floormove_t);
	    at = offsetof(floormove_t, sector);
	    break;
	  case tc_plat:
	    size = sizeof(plat_t);
	    at = offsetof(plat_t, sector);
	    break;
	  case tc_flash:
	    size = sizeof(lightflash_t);
	    at = offsetof(lightflash_t, sector);
	    break;
	  case tc_strobe:
	    size = sizeof(strobe_t);
	    at = offsetof(strobe_t, sector);
	    break;
	  case tc_glow:
	    size = sizeof(glow_t);
	    at = offsetof(glow_t, sector);
	    break;
	  default:
	    return false;
	}
	PAD();
	NEED(size);
	memcpy (&sector, p + at, sizeof(sector));
	p += size;
	if ((uintptr_t) sector >= (uintptr_t) nsectors)
	    return false;
    }
#undef PAD
#undef NEED
}
