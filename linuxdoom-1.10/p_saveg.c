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
#include "p_saveg.h"

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
    int		biggest = MOBJ_SAVESIZE + MOBJ_MBFSIZE;

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
    BIGGEST(fireflicker_t);
    BIGGEST(elevator_t);
    BIGGEST(scroll_t);
    BIGGEST(pusher_t);
#undef BIGGEST

    return MAXPLAYERS * (3 + (int)sizeof(player_t))
	+ numsectors * 7 * 2
	+ numlines * (3 + 2*5) * 2
	+ 4 + numsectors * 16		// Boom's sector offsets
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

    if (savegameversion < 2)
	return;

    // Boom's: the sectors' texture offsets, which its scrollers move
    PADSAVEP();
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
	memcpy (save_p, &sec->floor_xoffs, sizeof(fixed_t));
	memcpy (save_p + 4, &sec->floor_yoffs, sizeof(fixed_t));
	memcpy (save_p + 8, &sec->ceiling_xoffs, sizeof(fixed_t));
	memcpy (save_p + 12, &sec->ceiling_yoffs, sizeof(fixed_t));
	save_p += 16;
    }
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
	sec->floordata = sec->ceilingdata = sec->lightingdata = NULL;
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

    if (savegameversion >= 2)
    {
	PADSAVEP();
	for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
	{
	    memcpy (&sec->floor_xoffs, save_p, sizeof(fixed_t));
	    memcpy (&sec->floor_yoffs, save_p + 4, sizeof(fixed_t));
	    memcpy (&sec->ceiling_xoffs, save_p + 8, sizeof(fixed_t));
	    memcpy (&sec->ceiling_yoffs, save_p + 12, sizeof(fixed_t));
	    save_p += 16;
	}
    }
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
int			savegameversion;
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

	    // Boom's format: MBF's and MBF21's fields after id's
	    if (savegameversion >= 2)
	    {
		int	f[3];

		f[0] = ((mobj_t *)th)->flags2;
		f[1] = ((mobj_t *)th)->intflags;
		f[2] = P_SaveRef (((mobj_t *)th)->lastenemy);
		memcpy (save_p, f, MOBJ_MBFSIZE);
		save_p += MOBJ_MBFSIZE;
	    }
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
		mobj->lastenemy = savegameversion >= 2
		    ? P_LoadRef ((intptr_t) mobj->lastenemy) : NULL;
	    }
	    return; 	// end of list
			
	  case tc_mobj:
	    PADSAVEP();
	    mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);
	    memset (mobj, 0, sizeof(*mobj));
	    memcpy (mobj, save_p, MOBJ_SAVESIZE);
	    save_p += MOBJ_SAVESIZE;
	    if (savegameversion >= 2)
	    {
		int	f[3];

		memcpy (f, save_p, MOBJ_MBFSIZE);
		save_p += MOBJ_MBFSIZE;
		mobj->flags2 = f[0];
		mobj->intflags = f[1];
		mobj->lastenemy = (mobj_t *) (intptr_t) f[2];
	    }
	    else	// as the type has them
		mobj->flags2 = mobjinfo[mobj->type].flags2;
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
    tc_endspecials,
    // Boom's, after id's end marker so that its number stays put
    tc_elevator,
    tc_scroll,
    tc_pusher,
    tc_flicker

} specials_e;	



//
// The specials: each thinker that moves a floor, ceiling or light, or
// scrolls or pushes (Boom's), as its class and then the structure as it is
// in memory, with its sector (and any other pointer) as a number.
//
// A save before Boom's (savegameversion under 2) has id's classes only,
// and its ceilings, doors and floors are as long as id made them
// (CEILING_SAVESIZE and the rest, p_spec.h); Boom's fields after those
// stay 0.
//
// Things to handle:
//
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle, line_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
// T_FireFlicker, T_MoveElevator (sector_t *), T_Scroll,
// T_Pusher (mobj_t * source, as P_SaveRef numbers it)
//

// Write a thinker of a class, size bytes of it; in a save before Boom's,
// olddata bytes and the padding after them (OLD_SAVESIZE). Where it went.
static byte* P_ArchiveOne (int tclass, thinker_t* th, size_t size,
			   size_t olddata)
{
    byte*	p;

    *save_p++ = tclass;
    PADSAVEP();
    p = save_p;
    if (savegameversion >= 2)
    {
	memcpy (p, th, size);
	save_p += size;
    }
    else
    {
	memset (p, 0, OLD_SAVESIZE(olddata));
	memcpy (p, th, olddata);
	save_p += OLD_SAVESIZE(olddata);
    }
    return p;
}

//
// P_SaveVersion
// The save format a level needs: id's (1, with the WADs listed) when it
// has nothing id's format cannot hold, so the save still loads on an engine
// from before Boom's; Boom's (2) for a Boom map, or for a fire flicker (a
// sector of type 17 in any map), which id's format leaves out.
//
int P_SaveVersion (void)
{
    thinker_t*	th;

    if (!demo_compatibility)
	return 2;
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
	if (th->function.acp1 == (actionf_p1)P_MobjThinker)
	{
	    // MBF's and MBF21's fields, if a patch's code pointers changed
	    // them from what the type gives
	    mobj_t*	mo = (mobj_t *) th;

	    if (mo->flags2 != mo->info->flags2 || mo->intflags
		|| mo->lastenemy)
		return 2;
	}
	else if (th->function.acp1 == (actionf_p1)T_FireFlicker
	    || th->function.acp1 == (actionf_p1)T_MoveElevator
	    || th->function.acp1 == (actionf_p1)T_Scroll
	    || th->function.acp1 == (actionf_p1)T_Pusher)
	    return 2;
    return 1;
}

#define SECTORNUM(T, p) \
    (((T*)(p))->sector = (sector_t *)(((T*)(p))->sector - sectors))

// A thinker in stasis (a stopped crusher or plat) has no function; the
// active lists say which it is.
static int P_StasisClass (thinker_t* th)
{
    int		i;

    for (i = 0; i < maxceilings; i++)
	if (activeceilings[i] == (ceiling_t *)th)
	    return tc_ceiling;
    for (i = 0; i < maxplats; i++)
	if (activeplats[i] == (plat_t *)th)
	    return tc_plat;
    return -1;
}

void P_ArchiveSpecials (void)
{
    thinker_t*		th;
    byte*		p;
    actionf_p1		fn;
	
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	fn = th->function.acp1;

	if (th->function.acv == (actionf_v)NULL)
	{
	    switch (P_StasisClass (th))
	    {
	      case tc_ceiling:
		p = P_ArchiveOne (tc_ceiling, th, sizeof(ceiling_t),
				  offsetof(ceiling_t, newspecial));
		SECTORNUM(ceiling_t, p);
		break;
	      case tc_plat:
		p = P_ArchiveOne (tc_plat, th, sizeof(plat_t),
			      sizeof(plat_t));
		SECTORNUM(plat_t, p);
		break;
	    }
	    continue;
	}

	if (fn == (actionf_p1)T_MoveCeiling)
	{
	    p = P_ArchiveOne (tc_ceiling, th, sizeof(ceiling_t),
				  offsetof(ceiling_t, newspecial));
	    SECTORNUM(ceiling_t, p);
	}
	else if (fn == (actionf_p1)T_VerticalDoor)
	{
	    vldoor_t*	door;

	    p = P_ArchiveOne (tc_door, th, sizeof(vldoor_t),
			      offsetof(vldoor_t, line));
	    SECTORNUM(vldoor_t, p);
	    door = (vldoor_t *)p;
	    if (savegameversion >= 2)
		door->line = (line_t *)(door->line ? door->line - lines + 1 : 0);
	}
	else if (fn == (actionf_p1)T_MoveFloor)
	{
	    p = P_ArchiveOne (tc_floor, th, sizeof(floormove_t),
			      offsetof(floormove_t, oldspecial));
	    SECTORNUM(floormove_t, p);
	}
	else if (fn == (actionf_p1)T_PlatRaise)
	{
	    p = P_ArchiveOne (tc_plat, th, sizeof(plat_t),
			      sizeof(plat_t));
	    SECTORNUM(plat_t, p);
	}
	else if (fn == (actionf_p1)T_LightFlash)
	{
	    p = P_ArchiveOne (tc_flash, th, sizeof(lightflash_t),
			      sizeof(lightflash_t));
	    SECTORNUM(lightflash_t, p);
	}
	else if (fn == (actionf_p1)T_StrobeFlash)
	{
	    p = P_ArchiveOne (tc_strobe, th, sizeof(strobe_t),
			      sizeof(strobe_t));
	    SECTORNUM(strobe_t, p);
	}
	else if (fn == (actionf_p1)T_Glow)
	{
	    p = P_ArchiveOne (tc_glow, th, sizeof(glow_t),
			      sizeof(glow_t));
	    SECTORNUM(glow_t, p);
	}
	else if (fn == (actionf_p1)T_FireFlicker)
	{
	    p = P_ArchiveOne (tc_flicker, th, sizeof(fireflicker_t),
			      sizeof(fireflicker_t));
	    SECTORNUM(fireflicker_t, p);
	}
	else if (fn == (actionf_p1)T_MoveElevator)
	{
	    p = P_ArchiveOne (tc_elevator, th, sizeof(elevator_t),
			      sizeof(elevator_t));
	    SECTORNUM(elevator_t, p);
	}
	else if (fn == (actionf_p1)T_Scroll)
	    P_ArchiveOne (tc_scroll, th, sizeof(scroll_t),
			      sizeof(scroll_t));
	else if (fn == (actionf_p1)T_Pusher)
	{
	    pusher_t*	pusher;

	    p = P_ArchiveOne (tc_pusher, th, sizeof(pusher_t),
			      sizeof(pusher_t));
	    pusher = (pusher_t *)p;
	    pusher->source = (mobj_t *) P_SaveRef (((pusher_t *)th)->source);
	}
    }
	
    // add a terminating marker
    *save_p++ = tc_endspecials;	
}


// Read a thinker of size bytes; in a save from before Boom's, olddata
// bytes of it, and the padding after them (OLD_SAVESIZE), with Boom's
// fields left 0.
static void* P_UnArchiveOne (size_t size, size_t olddata)
{
    void*	th;
    boolean	boom = savegameversion >= 2;

    PADSAVEP();
    th = Z_Malloc (size, PU_LEVSPEC, NULL);
    memset (th, 0, size);
    memcpy (th, save_p, boom ? size : olddata);
    save_p += boom ? size : OLD_SAVESIZE(olddata);
    return th;
}

#define SECTORPTR(T, p) \
    (((T*)(p))->sector = &sectors[(intptr_t)((T*)(p))->sector])

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
    fireflicker_t*	flick;
    elevator_t*		elevator;
    scroll_t*		scroll;
    pusher_t*		pusher;
	
    // read in saved thinkers
    while (1)
    {
	tclass = *save_p++;
	switch (tclass)
	{
	  case tc_endspecials:
	    return;	// end of list
			
	  case tc_ceiling:
	    ceiling = P_UnArchiveOne (sizeof(*ceiling),
				      offsetof(ceiling_t, newspecial));
	    SECTORPTR(ceiling_t, ceiling);
	    ceiling->sector->ceilingdata = ceiling;
	    if (ceiling->thinker.function.acp1)
		ceiling->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
	    P_AddThinker (&ceiling->thinker);
	    P_AddActiveCeiling(ceiling);
	    break;
				
	  case tc_door:
	    door = P_UnArchiveOne (sizeof(*door), offsetof(vldoor_t, line));
	    SECTORPTR(vldoor_t, door);
	    door->sector->ceilingdata = door;
	    door->line = (intptr_t)door->line > 0
		&& (intptr_t)door->line <= numlines
		? &lines[(intptr_t)door->line - 1] : NULL;
	    door->thinker.function.acp1 = (actionf_p1)T_VerticalDoor;
	    P_AddThinker (&door->thinker);
	    break;
				
	  case tc_floor:
	    floor = P_UnArchiveOne (sizeof(*floor),
				    offsetof(floormove_t, oldspecial));
	    SECTORPTR(floormove_t, floor);
	    floor->sector->floordata = floor;
	    floor->thinker.function.acp1 = (actionf_p1)T_MoveFloor;
	    P_AddThinker (&floor->thinker);
	    break;
				
	  case tc_plat:
	    plat = P_UnArchiveOne (sizeof(*plat), sizeof(*plat));
	    SECTORPTR(plat_t, plat);
	    plat->sector->floordata = plat;
	    if (plat->thinker.function.acp1)
		plat->thinker.function.acp1 = (actionf_p1)T_PlatRaise;
	    P_AddThinker (&plat->thinker);
	    P_AddActivePlat(plat);
	    break;
				
	  case tc_flash:
	    flash = P_UnArchiveOne (sizeof(*flash), sizeof(*flash));
	    SECTORPTR(lightflash_t, flash);
	    flash->thinker.function.acp1 = (actionf_p1)T_LightFlash;
	    P_AddThinker (&flash->thinker);
	    break;
				
	  case tc_strobe:
	    strobe = P_UnArchiveOne (sizeof(*strobe), sizeof(*strobe));
	    SECTORPTR(strobe_t, strobe);
	    strobe->thinker.function.acp1 = (actionf_p1)T_StrobeFlash;
	    P_AddThinker (&strobe->thinker);
	    break;
				
	  case tc_glow:
	    glow = P_UnArchiveOne (sizeof(*glow), sizeof(*glow));
	    SECTORPTR(glow_t, glow);
	    glow->thinker.function.acp1 = (actionf_p1)T_Glow;
	    P_AddThinker (&glow->thinker);
	    break;

	  case tc_flicker:
	    flick = P_UnArchiveOne (sizeof(*flick), sizeof(*flick));
	    SECTORPTR(fireflicker_t, flick);
	    flick->thinker.function.acp1 = (actionf_p1)T_FireFlicker;
	    P_AddThinker (&flick->thinker);
	    break;

	  case tc_elevator:
	    elevator = P_UnArchiveOne (sizeof(*elevator), sizeof(*elevator));
	    SECTORPTR(elevator_t, elevator);
	    elevator->sector->floordata = elevator;
	    elevator->sector->ceilingdata = elevator;
	    elevator->thinker.function.acp1 = (actionf_p1)T_MoveElevator;
	    P_AddThinker (&elevator->thinker);
	    break;

	  case tc_scroll:
	    scroll = P_UnArchiveOne (sizeof(*scroll), sizeof(*scroll));
	    scroll->thinker.function.acp1 = (actionf_p1)T_Scroll;
	    P_AddThinker (&scroll->thinker);
	    break;

	  case tc_pusher:
	    pusher = P_UnArchiveOne (sizeof(*pusher), sizeof(*pusher));
	    pusher->source = P_LoadRef ((intptr_t) pusher->source);
	    pusher->thinker.function.acp1 = (actionf_p1)T_Pusher;
	    P_AddThinker (&pusher->thinker);
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
// The map as numbers: nsectors, and nlines lines as its LINEDEFS lump has
// them, for which sides each has. graphics: the flats and textures are the
// game's running now, and held to its tables too.
static boolean
P_SaveFitsMap
( byte*		p,
  byte*		end,
  int		nsectors,
  int		nlines,
  int		nsides,
  maplinedef_t*	ml,
  boolean	graphics,
  boolean*	ingame )
{
    int			i, j;
    int			nmobjs = 0;
    uintptr_t		maxref = 0;
    short		v[5];
    byte		tclass;

#define PAD()	p += (4 - ((intptr_t) p & 3)) & 3
#define NEED(n)	if (end - p < (int)(n)) return false

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
	    if ((uintptr_t) pl.psprites[j].state >= (uintptr_t) numstates)
		return false;
    }

    // world: the sectors' flats, and the lines' sides' textures
    for (i = 0; i < nsectors; i++)
    {
	NEED(7 * sizeof(short));
	memcpy (v, p, 4 * sizeof(short));
	p += 7 * sizeof(short);
	if (graphics
	    && (v[2] < 0 || v[2] >= numflats || v[3] < 0 || v[3] >= numflats))
	    return false;
    }
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
	    if (graphics
		&& (v[2] < 0 || v[2] >= numtextures
		    || v[3] < 0 || v[3] >= numtextures
		    || v[4] < 0 || v[4] >= numtextures))
		return false;
	}
    }
    if (savegameversion >= 2)	// Boom's sector offsets
    {
	PAD();
	NEED(16 * nsectors);
	p += 16 * nsectors;
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
	if (savegameversion >= 2)
	{
	    int	f[3];

	    NEED(MOBJ_MBFSIZE);
	    memcpy (f, p, MOBJ_MBFSIZE);
	    p += MOBJ_MBFSIZE;
	    if (f[2] < 0)
		return false;
	    if ((uintptr_t) f[2] > maxref)
		maxref = f[2];
	}
	if ((uintptr_t) mo.state >= (uintptr_t) numstates
	    || (unsigned) mo.type >= (unsigned) nummobjtypes
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

    // specials: each has its sector's number where the sector was, and
    // Boom's their lines, sides and things
    while (1)
    {
	int		size, at = -1;
	sector_t*	sector;
	boolean		boom = savegameversion >= 2;

	NEED(1);
	tclass = *p++;
	switch (tclass)
	{
	  case tc_endspecials:
	    NEED(1);
	    return *p == 0x1d;		// what G_DoLoadGame checks after
	  case tc_ceiling:
	    size = boom ? sizeof(ceiling_t) : CEILING_SAVESIZE;
	    at = offsetof(ceiling_t, sector);
	    break;
	  case tc_door:
	    size = boom ? sizeof(vldoor_t) : VLDOOR_SAVESIZE;
	    at = offsetof(vldoor_t, sector);
	    break;
	  case tc_floor:
	    size = boom ? sizeof(floormove_t) : FLOOR_SAVESIZE;
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
	  case tc_flicker:
	    size = sizeof(fireflicker_t);
	    at = offsetof(fireflicker_t, sector);
	    break;
	  case tc_elevator:
	    size = sizeof(elevator_t);
	    at = offsetof(elevator_t, sector);
	    break;
	  case tc_scroll:
	    size = sizeof(scroll_t);
	    break;
	  case tc_pusher:
	    size = sizeof(pusher_t);
	    break;
	  default:
	    return false;
	}
	if (!boom && tclass > tc_endspecials)
	    return false;
	PAD();
	NEED(size);
	if (at >= 0)
	{
	    memcpy (&sector, p + at, sizeof(sector));
	    if ((uintptr_t) sector >= (uintptr_t) nsectors)
		return false;
	}
	if (tclass == tc_door && boom)
	{
	    vldoor_t	d;

	    memcpy (&d, p, sizeof(d));
	    if ((uintptr_t) d.line > (uintptr_t) nlines)
		return false;
	}
	if (tclass == tc_scroll)
	{
	    scroll_t	sc;

	    memcpy (&sc, p, sizeof(sc));
	    if ((unsigned) sc.affectee >= (unsigned)
		(sc.type == sc_side ? nsides : nsectors)
		|| (unsigned) sc.type > sc_carry
		|| (sc.control != -1 && (unsigned) sc.control >= (unsigned) nsectors))
		return false;
	}
	if (tclass == tc_pusher)
	{
	    pusher_t	pu;

	    memcpy (&pu, p, sizeof(pu));
	    if ((unsigned) pu.affectee >= (unsigned) nsectors
		|| (uintptr_t) pu.source > (uintptr_t) nmobjs)
		return false;
	}
	p += size;
    }
#undef PAD
#undef NEED
}


boolean P_SaveGameFits (byte* p, byte* end, int maplump, boolean* ingame)
{
    if (maplump < 0 || maplump + ML_SECTORS >= numlumps)
	return false;
    return P_SaveFitsMap (p, end,
			  W_LumpLength (maplump + ML_SECTORS)
			  / sizeof(mapsector_t),
			  W_LumpLength (maplump + ML_LINEDEFS)
			  / sizeof(maplinedef_t),
			  W_LumpLength (maplump + ML_SIDEDEFS)
			  / sizeof(mapsidedef_t),
			  W_CacheLumpNum (maplump + ML_LINEDEFS, PU_CACHE),
			  true, ingame);
}


//
// P_SaveFitsFile
// Whether the archive fits the map of that episode and number in the WAD
// file at path, by the WAD's own lumps, not the game's loaded: which game
// or mod a save from before saves listed their WADs was made on. Its map
// is looked for by either kind of name, since the game running decides
// which the save's numbers mean. The flats and textures are the running
// game's, so they are not held to anything here.
//
boolean
P_SaveFitsFile
( byte*		p,
  byte*		end,
  char*		path,
  int		episode,
  int		map,
  boolean*	ingame )
{
    FILE*		f = fopen (path, "rb");
    unsigned char	head[12];
    unsigned char*	dir = NULL;
    maplinedef_t*	ml = NULL;
    char		names[2][9];
    int			numlumps, ofs, i, k, found;
    boolean		fits = false;

#define LE32(b)	((b)[0] | ((b)[1] << 8) | ((b)[2] << 16) | ((b)[3] << 24))

    if (!f)
	return false;
    if (fread (head, 1, 12, f) != 12
	|| (memcmp (head, "IWAD", 4) && memcmp (head, "PWAD", 4)))
	goto done;
    numlumps = LE32(head + 4);
    ofs = LE32(head + 8);
    if (numlumps <= 0 || numlumps > 1 << 20
	|| !(dir = malloc (numlumps * 16))
	|| fseek (f, ofs, SEEK_SET)
	|| (int) fread (dir, 16, numlumps, f) != numlumps)
	goto done;

    snprintf (names[0], 9, "E%dM%d", episode % 10, map % 100);
    snprintf (names[1], 9, "MAP%02d", map % 100);
    for (k = 0; k < 2 && !fits; k++)
    {
	// the last of that name, as W_CheckNumForName finds it
	found = -1;
	for (i = 0; i < numlumps - ML_SECTORS; i++)
	    if (!strncasecmp ((char*) dir + i*16 + 8, names[k], 8)
		&& !strncasecmp ((char*) dir + (i + ML_LINEDEFS)*16 + 8,
				 "LINEDEFS", 8)
		&& !strncasecmp ((char*) dir + (i + ML_SECTORS)*16 + 8,
				 "SECTORS", 8))
		found = i;
	if (found < 0)
	    continue;
	{
	    unsigned char*	ld = dir + (found + ML_LINEDEFS)*16;
	    unsigned char*	sd = dir + (found + ML_SECTORS)*16;
	    int			size = LE32(ld + 4);

	    free (ml);
	    if (size < 0 || !(ml = malloc (size ? size : 1))
		|| fseek (f, LE32(ld), SEEK_SET)
		|| (int) fread (ml, 1, size, f) != size)
		break;
	    fits = P_SaveFitsMap (p, end, LE32(sd + 4) / sizeof(mapsector_t),
				  size / sizeof(maplinedef_t),
				  LE32(dir + (found + ML_SIDEDEFS)*16 + 4)
				  / sizeof(mapsidedef_t),
				  ml, false, ingame);
	}
    }
#undef LE32

  done:
    free (ml);
    free (dir);
    fclose (f);
    return fits;
}

