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
//	Enemy thinking, AI.
//	Action Pointer Functions
//	that are associated with states/frames. 
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: p_enemy.c,v 1.5 1997/02/03 22:45:11 b1 Exp $";

#include <stdlib.h>

#include "m_random.h"
#include "i_system.h"

#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

#include "g_game.h"
#include "u_mapinfo.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "sounds.h"




typedef enum
{
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
    
} dirtype_t;


//
// P_NewChaseDir related LUT.
//
dirtype_t opposite[] =
{
  DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST,
  DI_EAST, DI_NORTHEAST, DI_NORTH, DI_NORTHWEST, DI_NODIR
};

dirtype_t diags[] =
{
    DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST
};





void A_Fall (mobj_t *actor);


//
// ENEMY THINKING
// Enemies are allways spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//


//
// Called by P_NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//

mobj_t*		soundtarget;

void
P_RecursiveSound
( sector_t*	sec,
  int		soundblocks )
{
    int		i;
    line_t*	check;
    sector_t*	other;
	
    // wake up all monsters in this sector
    if (sec->validcount == validcount
	&& sec->soundtraversed <= soundblocks+1)
    {
	return;		// already flooded
    }
    
    sec->validcount = validcount;
    sec->soundtraversed = soundblocks+1;
    sec->soundtarget = soundtarget;
	
    for (i=0 ;i<sec->linecount ; i++)
    {
	check = sec->lines[i];
	if (! (check->flags & ML_TWOSIDED) )
	    continue;
	
	P_LineOpening (check);

	if (openrange <= 0)
	    continue;	// closed door
	
	if ( sides[ check->sidenum[0] ].sector == sec)
	    other = sides[ check->sidenum[1] ] .sector;
	else
	    other = sides[ check->sidenum[0] ].sector;
	
	if (check->flags & ML_SOUNDBLOCK)
	{
	    if (!soundblocks)
		P_RecursiveSound (other, 1);
	}
	else
	    P_RecursiveSound (other, soundblocks);
    }
}



//
// P_NoiseAlert
// If a monster yells at a player,
// it will alert other monsters to the player.
//
void
P_NoiseAlert
( mobj_t*	target,
  mobj_t*	emmiter )
{
    soundtarget = target;
    validcount++;
    P_RecursiveSound (emmiter->subsector->sector, 0);
}




//
// P_CheckMeleeRange
//
// Within range of its target, and seeing it; a friend never has another
// friend in range (MBF).
static boolean P_CheckRange (mobj_t* actor, fixed_t range)
{
    mobj_t*	pl = actor->target;

    return pl && !(actor->flags & pl->flags & MF_FRIEND)
	&& P_AproxDistance (pl->x-actor->x, pl->y-actor->y) < range
	&& P_CheckSight (actor, pl);
}

// id's MELEERANGE, or the thing's own (MBF21's Melee range)
boolean P_CheckMeleeRange (mobj_t*	actor)
{
    if (!actor->target)
	return false;
    return P_CheckRange (actor, actor->info->meleerange - 20*FRACUNIT
			 + actor->target->info->radius);
}

//
// P_CheckMissileRange
//
boolean P_CheckMissileRange (mobj_t* actor)
{
    fixed_t	dist;
	
    if (! P_CheckSight (actor, actor->target) )
	return false;
	
    if ( actor->flags & MF_JUSTHIT )
    {
	// the target just hit the enemy,
	// so fight back! (MBF: not a friend at a friend)
	actor->flags &= ~MF_JUSTHIT;
	return !(actor->flags & actor->target->flags & MF_FRIEND);
    }

    // MBF: friends do not fire at friends, the player among them
    if (actor->flags & actor->target->flags & MF_FRIEND)
	return false;
	
    if (actor->reactiontime)
	return false;	// do not attack yet
		
    // OPTIMIZE: get this from a global checksight
    dist = P_AproxDistance ( actor->x-actor->target->x,
			     actor->y-actor->target->y) - 64*FRACUNIT;
    
    if (!actor->info->meleestate)
	dist -= 128*FRACUNIT;	// no melee attack, so fire more

    dist >>= 16;

    // What id's code did for the Arch-vile, the Revenant, the Cyberdemon,
    // the Spider Mastermind and the Lost Soul by type, MBF21 does by flag;
    // those are the flags the types have (D_InitInfo).
    if (actor->flags2 & MF2_SHORTMRANGE)
    {
	if (dist > 14*64)	
	    return false;	// too far away
    }

    if (actor->flags2 & MF2_LONGMELEE)
    {
	if (dist < 196)	
	    return false;	// close for fist attack
    }

    if (actor->flags2 & MF2_RANGEHALF)
	dist >>= 1;
    
    if (dist > 200)
	dist = 200;
		
    if ((actor->flags2 & MF2_HIGHERMPROB) && dist > 160)
	dist = 160;
		
    if (P_Random () < dist)
	return false;
		
    return true;
}


//
// P_Move
// Move in the current direction,
// returns false if the move is blocked.
//
fixed_t	xspeed[8] = {FRACUNIT,47000,0,-47000,-FRACUNIT,-47000,0,47000};
fixed_t yspeed[8] = {0,47000,FRACUNIT,47000,0,-47000,-FRACUNIT,-47000};

#define MAXSPECIALCROSS	8

extern	line_t**	spechit;
extern	int	numspechit;

boolean P_Move (mobj_t*	actor)
{
    fixed_t	tryx;
    fixed_t	tryy;
    
    line_t*	ld;
    
    // warning: 'catch', 'throw', and 'try'
    // are all C++ reserved words
    boolean	try_ok;
    boolean	good;
		
    if (actor->movedir == DI_NODIR)
	return false;
		
    if ((unsigned)actor->movedir >= 8)
	I_Error ("Weird actor->movedir!");
		
    tryx = actor->x + actor->info->speed*xspeed[actor->movedir];
    tryy = actor->y + actor->info->speed*yspeed[actor->movedir];

    try_ok = P_TryMove (actor, tryx, tryy);

    if (!try_ok)
    {
	// open any specials
	if (actor->flags & MF_FLOAT && floatok)
	{
	    // must adjust height
	    if (actor->z < tmfloorz)
		actor->z += FLOATSPEED;
	    else
		actor->z -= FLOATSPEED;

	    actor->flags |= MF_INFLOAT;
	    return true;
	}
		
	if (!numspechit)
	    return false;
			
	actor->movedir = DI_NODIR;
	good = false;
	while (numspechit--)
	{
	    ld = spechit[numspechit];
	    // if the special is not a door
	    // that can be opened,
	    // return false
	    if (P_UseSpecialLine (actor, ld,0))
		good = true;
	}
	return good;
    }
    else
    {
	actor->flags &= ~MF_INFLOAT;
    }
	
	
    if (! (actor->flags & MF_FLOAT) )	
	actor->z = actor->floorz;
    return true; 
}


//
// TryWalk
// Attempts to move actor on
// in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//
boolean P_TryWalk (mobj_t* actor)
{	
    if (!P_Move (actor))
    {
	return false;
    }

    actor->movecount = P_Random()&15;
    return true;
}




void P_NewChaseDir (mobj_t*	actor)
{
    fixed_t	deltax;
    fixed_t	deltay;
    
    dirtype_t	d[3];
    
    int		tdir;
    dirtype_t	olddir;
    
    dirtype_t	turnaround;

    if (!actor->target)
	I_Error ("P_NewChaseDir: called with no target");
		
    olddir = actor->movedir;
    turnaround=opposite[olddir];

    deltax = actor->target->x - actor->x;
    deltay = actor->target->y - actor->y;

    if (deltax>10*FRACUNIT)
	d[1]= DI_EAST;
    else if (deltax<-10*FRACUNIT)
	d[1]= DI_WEST;
    else
	d[1]=DI_NODIR;

    if (deltay<-10*FRACUNIT)
	d[2]= DI_SOUTH;
    else if (deltay>10*FRACUNIT)
	d[2]= DI_NORTH;
    else
	d[2]=DI_NODIR;

    // try direct route
    if (d[1] != DI_NODIR
	&& d[2] != DI_NODIR)
    {
	actor->movedir = diags[((deltay<0)<<1)+(deltax>0)];
	if (actor->movedir != turnaround && P_TryWalk(actor))
	    return;
    }

    // try other directions
    if (P_Random() > 200
	||  abs(deltay)>abs(deltax))
    {
	tdir=d[1];
	d[1]=d[2];
	d[2]=tdir;
    }

    if (d[1]==turnaround)
	d[1]=DI_NODIR;
    if (d[2]==turnaround)
	d[2]=DI_NODIR;
	
    if (d[1]!=DI_NODIR)
    {
	actor->movedir = d[1];
	if (P_TryWalk(actor))
	{
	    // either moved forward or attacked
	    return;
	}
    }

    if (d[2]!=DI_NODIR)
    {
	actor->movedir =d[2];

	if (P_TryWalk(actor))
	    return;
    }

    // there is no direct path to the player,
    // so pick another direction.
    if (olddir!=DI_NODIR)
    {
	actor->movedir =olddir;

	if (P_TryWalk(actor))
	    return;
    }

    // randomly determine direction of search
    if (P_Random()&1) 	
    {
	for ( tdir=DI_EAST;
	      tdir<=DI_SOUTHEAST;
	      tdir++ )
	{
	    if (tdir!=turnaround)
	    {
		actor->movedir =tdir;
		
		if ( P_TryWalk(actor) )
		    return;
	    }
	}
    }
    else
    {
	for ( tdir=DI_SOUTHEAST;
	      tdir != (DI_EAST-1);
	      tdir-- )
	{
	    if (tdir!=turnaround)
	    {
		actor->movedir =tdir;
		
		if ( P_TryWalk(actor) )
		    return;
	    }
	}
    }

    if (turnaround !=  DI_NODIR)
    {
	actor->movedir =turnaround;
	if ( P_TryWalk(actor) )
	    return;
    }

    actor->movedir = DI_NODIR;	// can not move
}



//
// P_LookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//
//
// P_LookForMonsters
// MBF's friends look for the nearest monster in sight that is not one,
// in front of them unless allaround. A simpler search than MBF's own,
// which kept friends and enemies in lists of their own and had friends
// follow and help the player; it keeps a friend off the player's side.
//
static boolean P_LookForMonsters (mobj_t* actor, boolean allaround)
{
    thinker_t*	th;
    mobj_t*	mo;
    mobj_t*	best = NULL;
    fixed_t	bestdist = 32*64*FRACUNIT;
    fixed_t	dist;
    angle_t	an;

    for (th = thinkercap.next ; th != &thinkercap ; th = th->next)
    {
	if (th->function.acp1 != (actionf_p1) P_MobjThinker)
	    continue;
	mo = (mobj_t *) th;
	if (mo == actor || mo->health <= 0 || (mo->flags & MF_FRIEND)
	    || !(mo->flags & MF_SHOOTABLE)
	    || !((mo->flags & MF_COUNTKILL) || mo->player))
	    continue;
	dist = P_AproxDistance (mo->x - actor->x, mo->y - actor->y);
	if (dist >= bestdist)
	    continue;
	if (!allaround)
	{
	    an = R_PointToAngle2 (actor->x, actor->y, mo->x, mo->y)
		 - actor->angle;
	    if (an > ANG90 && an < ANG270 && dist > MELEERANGE)
		continue;	// behind its back
	}
	if (!P_CheckSight (actor, mo))
	    continue;
	best = mo;
	bestdist = dist;
    }
    if (!best)
	return false;
    actor->target = best;
    return true;
}

boolean
P_LookForPlayers
( mobj_t*	actor,
  boolean	allaround )
{
    int		c;
    int		stop;
    player_t*	player;
    sector_t*	sector;
    angle_t	an;
    fixed_t	dist;

    // MBF: a friend looks for the player's enemies instead
    if (actor->flags & MF_FRIEND)
	return P_LookForMonsters (actor, allaround);
		
    sector = actor->subsector->sector;
	
    c = 0;
    stop = (actor->lastlook-1)&3;
	
    for ( ; ; actor->lastlook = (actor->lastlook+1)&3 )
    {
	if (!playeringame[actor->lastlook])
	    continue;
			
	if (c++ == 2
	    || actor->lastlook == stop)
	{
	    // done looking
	    return false;	
	}
	
	player = &players[actor->lastlook];

	if (player->health <= 0)
	    continue;		// dead

	if (!P_CheckSight (actor, player->mo))
	    continue;		// out of sight
			
	if (!allaround)
	{
	    an = R_PointToAngle2 (actor->x,
				  actor->y, 
				  player->mo->x,
				  player->mo->y)
		- actor->angle;
	    
	    if (an > ANG90 && an < ANG270)
	    {
		dist = P_AproxDistance (player->mo->x - actor->x,
					player->mo->y - actor->y);
		// if real close, react anyway
		if (dist > MELEERANGE)
		    continue;	// behind back
	    }
	}
		
	actor->target = player->mo;
	return true;
    }

    return false;
}


//
// A_KeenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
void A_KeenDie (mobj_t* mo)
{
    thinker_t*	th;
    mobj_t*	mo2;
    line_t	junk;

    A_Fall (mo);
    
    // scan the remaining thinkers
    // to see if all Keens are dead
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function.acp1 != (actionf_p1)P_MobjThinker)
	    continue;

	mo2 = (mobj_t *)th;
	if (mo2 != mo
	    && mo2->type == mo->type
	    && mo2->health > 0)
	{
	    // other Keen not dead
	    return;		
	}
    }

    junk.tag = 666;
    EV_DoDoor(&junk,open);
}


//
// ACTION ROUTINES
//

//
// A_Look
// Stay in state until a player is sighted.
//
void A_Look (mobj_t* actor)
{
    mobj_t*	targ;
	
    actor->threshold = 0;	// any shot will wake up
    targ = actor->subsector->sector->soundtarget;

    // (MBF: a friend is not woken against a friend)
    if (targ
	&& (targ->flags & MF_SHOOTABLE)
	&& !(targ->flags & actor->flags & MF_FRIEND) )
    {
	actor->target = targ;

	if ( actor->flags & MF_AMBUSH )
	{
	    if (P_CheckSight (actor, actor->target))
		goto seeyou;
	}
	else
	    goto seeyou;
    }
	
	
    if (!P_LookForPlayers (actor, false) )
	return;
		
    // go into chase state
  seeyou:
    if (actor->info->seesound)
    {
	int		sound;
		
	switch (actor->info->seesound)
	{
	  case sfx_posit1:
	  case sfx_posit2:
	  case sfx_posit3:
	    sound = sfx_posit1+P_Random()%3;
	    break;

	  case sfx_bgsit1:
	  case sfx_bgsit2:
	    sound = sfx_bgsit1+P_Random()%2;
	    break;

	  default:
	    sound = actor->info->seesound;
	    break;
	}

	if (actor->flags2 & MF2_FULLVOLSOUNDS)
	{
	    // full volume
	    S_StartSound (NULL, sound);
	}
	else
	    S_StartSound (actor, sound);
    }

    P_SetMobjState (actor, actor->info->seestate);
}


//
// A_Chase
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void A_Chase (mobj_t*	actor)
{
    int		delta;

    if (actor->reactiontime)
	actor->reactiontime--;
				

    // modify target threshold
    if  (actor->threshold)
    {
	if (!actor->target
	    || actor->target->health <= 0)
	{
	    actor->threshold = 0;
	}
	else
	    actor->threshold--;
    }
    
    // turn towards movement direction if not there yet
    if (actor->movedir < 8)
    {
	actor->angle &= (7<<29);
	delta = actor->angle - (actor->movedir << 29);
	
	if (delta > 0)
	    actor->angle -= ANG90/2;
	else if (delta < 0)
	    actor->angle += ANG90/2;
    }

    if (!actor->target
	|| !(actor->target->flags&MF_SHOOTABLE))
    {
	// look for a new target
	if (P_LookForPlayers(actor,true))
	    return; 	// got a new target
	
	P_SetMobjState (actor, actor->info->spawnstate);
	return;
    }
    
    // do not attack twice in a row
    if (actor->flags & MF_JUSTATTACKED)
    {
	actor->flags &= ~MF_JUSTATTACKED;
	if (gameskill != sk_nightmare && !fastparm)
	    P_NewChaseDir (actor);
	return;
    }
    
    // check for melee attack
    if (actor->info->meleestate
	&& P_CheckMeleeRange (actor))
    {
	if (actor->info->attacksound)
	    S_StartSound (actor, actor->info->attacksound);

	P_SetMobjState (actor, actor->info->meleestate);
	return;
    }
    
    // check for missile attack
    if (actor->info->missilestate)
    {
	if (gameskill < sk_nightmare
	    && !fastparm && actor->movecount)
	{
	    goto nomissile;
	}
	
	if (!P_CheckMissileRange (actor))
	    goto nomissile;
	
	P_SetMobjState (actor, actor->info->missilestate);
	actor->flags |= MF_JUSTATTACKED;
	return;
    }

    // ?
  nomissile:
    // possibly choose another target
    if (netgame
	&& !actor->threshold
	&& !P_CheckSight (actor, actor->target) )
    {
	if (P_LookForPlayers(actor,true))
	    return;	// got a new target
    }
    
    // chase towards player
    if (--actor->movecount<0
	|| !P_Move (actor))
    {
	P_NewChaseDir (actor);
    }
    
    // make active sound
    if (actor->info->activesound
	&& P_Random () < 3)
    {
	S_StartSound (actor, actor->info->activesound);
    }
}


//
// A_FaceTarget
//
void A_FaceTarget (mobj_t* actor)
{	
    if (!actor->target)
	return;
    
    actor->flags &= ~MF_AMBUSH;
	
    actor->angle = R_PointToAngle2 (actor->x,
				    actor->y,
				    actor->target->x,
				    actor->target->y);
    
    if (actor->target->flags & MF_SHADOW)
	actor->angle += (P_Random()-P_Random())<<21;
}


//
// A_PosAttack
//
void A_PosAttack (mobj_t* actor)
{
    int		angle;
    int		damage;
    int		slope;
	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    angle = actor->angle;
    slope = P_AimLineAttack (actor, angle, MISSILERANGE);

    S_StartSound (actor, sfx_pistol);
    angle += (P_Random()-P_Random())<<20;
    damage = ((P_Random()%5)+1)*3;
    P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
}

void A_SPosAttack (mobj_t* actor)
{
    int		i;
    int		angle;
    int		bangle;
    int		damage;
    int		slope;
	
    if (!actor->target)
	return;

    S_StartSound (actor, sfx_shotgn);
    A_FaceTarget (actor);
    bangle = actor->angle;
    slope = P_AimLineAttack (actor, bangle, MISSILERANGE);

    for (i=0 ; i<3 ; i++)
    {
	angle = bangle + ((P_Random()-P_Random())<<20);
	damage = ((P_Random()%5)+1)*3;
	P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
    }
}

void A_CPosAttack (mobj_t* actor)
{
    int		angle;
    int		bangle;
    int		damage;
    int		slope;
	
    if (!actor->target)
	return;

    S_StartSound (actor, sfx_shotgn);
    A_FaceTarget (actor);
    bangle = actor->angle;
    slope = P_AimLineAttack (actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random()-P_Random())<<20);
    damage = ((P_Random()%5)+1)*3;
    P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
}

void A_CPosRefire (mobj_t* actor)
{	
    // keep firing unless target got out of sight
    A_FaceTarget (actor);

    if (P_Random () < 40)
	return;

    if (!actor->target
	|| actor->target->health <= 0
	|| !P_CheckSight (actor, actor->target) )
    {
	P_SetMobjState (actor, actor->info->seestate);
    }
}


void A_SpidRefire (mobj_t* actor)
{	
    // keep firing unless target got out of sight
    A_FaceTarget (actor);

    if (P_Random () < 10)
	return;

    if (!actor->target
	|| actor->target->health <= 0
	|| !P_CheckSight (actor, actor->target) )
    {
	P_SetMobjState (actor, actor->info->seestate);
    }
}

void A_BspiAttack (mobj_t *actor)
{	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);

    // launch a missile
    P_SpawnMissile (actor, actor->target, MT_ARACHPLAZ);
}


//
// A_TroopAttack
//
void A_TroopAttack (mobj_t* actor)
{
    int		damage;
	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
	S_StartSound (actor, sfx_claw);
	damage = (P_Random()%8+1)*3;
	P_DamageMobj (actor->target, actor, actor, damage);
	return;
    }

    
    // launch a missile
    P_SpawnMissile (actor, actor->target, MT_TROOPSHOT);
}


void A_SargAttack (mobj_t* actor)
{
    int		damage;

    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
	damage = ((P_Random()%10)+1)*4;
	P_DamageMobj (actor->target, actor, actor, damage);
    }
}

void A_HeadAttack (mobj_t* actor)
{
    int		damage;
	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
	damage = (P_Random()%6+1)*10;
	P_DamageMobj (actor->target, actor, actor, damage);
	return;
    }
    
    // launch a missile
    P_SpawnMissile (actor, actor->target, MT_HEADSHOT);
}

void A_CyberAttack (mobj_t* actor)
{	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    P_SpawnMissile (actor, actor->target, MT_ROCKET);
}


void A_BruisAttack (mobj_t* actor)
{
    int		damage;
	
    if (!actor->target)
	return;
		
    if (P_CheckMeleeRange (actor))
    {
	S_StartSound (actor, sfx_claw);
	damage = (P_Random()%8+1)*10;
	P_DamageMobj (actor->target, actor, actor, damage);
	return;
    }
    
    // launch a missile
    P_SpawnMissile (actor, actor->target, MT_BRUISERSHOT);
}


//
// A_SkelMissile
//
void A_SkelMissile (mobj_t* actor)
{	
    mobj_t*	mo;
	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    actor->z += 16*FRACUNIT;	// so missile spawns higher
    mo = P_SpawnMissile (actor, actor->target, MT_TRACER);
    actor->z -= 16*FRACUNIT;	// back to normal

    mo->x += mo->momx;
    mo->y += mo->momy;
    mo->tracer = actor->target;
}

int	TRACEANGLE = 0xc000000;

void A_Tracer (mobj_t* actor)
{
    angle_t	exact;
    fixed_t	dist;
    fixed_t	slope;
    mobj_t*	dest;
    mobj_t*	th;
		
    if (gametic & 3)
	return;
    
    // spawn a puff of smoke behind the rocket		
    P_SpawnPuff (actor->x, actor->y, actor->z);
	
    th = P_SpawnMobj (actor->x-actor->momx,
		      actor->y-actor->momy,
		      actor->z, MT_SMOKE);
    
    th->momz = FRACUNIT;
    th->tics -= P_Random()&3;
    if (th->tics < 1)
	th->tics = 1;
    
    // adjust direction
    dest = actor->tracer;
	
    if (!dest || dest->health <= 0)
	return;
    
    // change angle	
    exact = R_PointToAngle2 (actor->x,
			     actor->y,
			     dest->x,
			     dest->y);

    if (exact != actor->angle)
    {
	if (exact - actor->angle > 0x80000000)
	{
	    actor->angle -= TRACEANGLE;
	    if (exact - actor->angle < 0x80000000)
		actor->angle = exact;
	}
	else
	{
	    actor->angle += TRACEANGLE;
	    if (exact - actor->angle > 0x80000000)
		actor->angle = exact;
	}
    }
	
    exact = actor->angle>>ANGLETOFINESHIFT;
    actor->momx = FixedMul (actor->info->speed, finecosine[exact]);
    actor->momy = FixedMul (actor->info->speed, finesine[exact]);
    
    // change slope
    dist = P_AproxDistance (dest->x - actor->x,
			    dest->y - actor->y);
    
    dist = dist / actor->info->speed;

    if (dist < 1)
	dist = 1;
    slope = (dest->z+40*FRACUNIT - actor->z) / dist;

    if (slope < actor->momz)
	actor->momz -= FRACUNIT/8;
    else
	actor->momz += FRACUNIT/8;
}


void A_SkelWhoosh (mobj_t*	actor)
{
    if (!actor->target)
	return;
    A_FaceTarget (actor);
    S_StartSound (actor,sfx_skeswg);
}

void A_SkelFist (mobj_t*	actor)
{
    int		damage;

    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
	
    if (P_CheckMeleeRange (actor))
    {
	damage = ((P_Random()%10)+1)*6;
	S_StartSound (actor, sfx_skepch);
	P_DamageMobj (actor->target, actor, actor, damage);
    }
}



//
// PIT_VileCheck
// Detect a corpse that could be raised.
//
mobj_t*		corpsehit;
mobj_t*		vileobj;
fixed_t		viletryx;
fixed_t		viletryy;

static fixed_t	viletryradius;

boolean PIT_VileCheck (mobj_t*	thing)
{
    int		maxdist;
    boolean	check;
	
    if (!(thing->flags & MF_CORPSE) )
	return true;	// not a monster
    
    if (thing->tics != -1)
	return true;	// not lying still yet
    
    if (thing->info->raisestate == S_NULL)
	return true;	// monster doesn't have a raise state
    
    maxdist = thing->info->radius + viletryradius;
	
    if ( abs(thing->x - viletryx) > maxdist
	 || abs(thing->y - viletryy) > maxdist )
	return true;		// not actually touching
		
    corpsehit = thing;
    corpsehit->momx = corpsehit->momy = 0;
    if (comp[comp_vile])
    {
	// id's: checked a quarter as tall as it will be (so a crushed
	// monster, 0 tall, comes back 0 tall, a "ghost")
	corpsehit->height <<= 2;
	check = P_CheckPosition (corpsehit, corpsehit->x, corpsehit->y);
	corpsehit->height >>= 2;
    }
    else
    {
	// Boom's: as big as it will be
	fixed_t	height = corpsehit->height;
	fixed_t	radius = corpsehit->radius;

	corpsehit->height = corpsehit->info->height;
	corpsehit->radius = corpsehit->info->radius;
	corpsehit->flags |= MF_SOLID;
	check = P_CheckPosition (corpsehit, corpsehit->x, corpsehit->y);
	corpsehit->height = height;
	corpsehit->radius = radius;
	corpsehit->flags &= ~MF_SOLID;
    }

    if (!check)
	return true;		// doesn't fit here
		
    return false;		// got one, so stop checking
}



//
// P_HealCorpse
// Raise a corpse the actor is about to step on, going to healstate and
// making healsound: the Arch-vile's, or MBF21's A_HealChase's.
//
static boolean
P_HealCorpse
( mobj_t*	actor,
  fixed_t	radius,
  int		healstate,
  int		healsound )
{
    int			xl;
    int			xh;
    int			yl;
    int			yh;
    
    int			bx;
    int			by;

    mobjinfo_t*		info;
    mobj_t*		temp;
	
    if (actor->movedir != DI_NODIR)
    {
	// check for corpses to raise
	viletryx =
	    actor->x + actor->info->speed*xspeed[actor->movedir];
	viletryy =
	    actor->y + actor->info->speed*yspeed[actor->movedir];

	xl = (viletryx - bmaporgx - MAXRADIUS*2)>>MAPBLOCKSHIFT;
	xh = (viletryx - bmaporgx + MAXRADIUS*2)>>MAPBLOCKSHIFT;
	yl = (viletryy - bmaporgy - MAXRADIUS*2)>>MAPBLOCKSHIFT;
	yh = (viletryy - bmaporgy + MAXRADIUS*2)>>MAPBLOCKSHIFT;
	
	vileobj = actor;
	viletryradius = radius;
	for (bx=xl ; bx<=xh ; bx++)
	{
	    for (by=yl ; by<=yh ; by++)
	    {
		// Call PIT_VileCheck to check
		// whether object is a corpse
		// that canbe raised.
		if (!P_BlockThingsIterator(bx,by,PIT_VileCheck))
		{
		    // got one!
		    temp = actor->target;
		    actor->target = corpsehit;
		    A_FaceTarget (actor);
		    actor->target = temp;
					
		    P_SetMobjState (actor, healstate);
		    S_StartSound (corpsehit, healsound);
		    info = corpsehit->info;
		    
		    P_SetMobjState (corpsehit,info->raisestate);
		    if (comp[comp_vile])
			corpsehit->height <<= 2;
		    else
		    {
			corpsehit->height = info->height;	// no ghosts
			corpsehit->radius = info->radius;
		    }
		    // MBF: its raiser's side
		    corpsehit->flags = (info->flags & ~MF_FRIEND)
				       | (actor->flags & MF_FRIEND);
		    corpsehit->health = info->spawnhealth;
		    corpsehit->target = NULL;
		    if (!demo_compatibility)
		    {
			corpsehit->lastenemy = NULL;
			corpsehit->flags &= ~MF_JUSTHIT;
		    }

		    return true;
		}
	    }
	}
    }
    return false;
}

//
// A_VileChase
// Check for ressurecting a body
//
void A_VileChase (mobj_t* actor)
{
    if (!P_HealCorpse (actor, mobjinfo[MT_VILE].radius, S_VILE_HEAL1,
		       sfx_slop))
	A_Chase (actor);	// Return to normal attack.
}


//
// A_VileStart
//
void A_VileStart (mobj_t* actor)
{
    S_StartSound (actor, sfx_vilatk);
}


//
// A_Fire
// Keep fire in front of player unless out of sight
//
void A_Fire (mobj_t* actor);

void A_StartFire (mobj_t* actor)
{
    S_StartSound(actor,sfx_flamst);
    A_Fire(actor);
}

void A_FireCrackle (mobj_t* actor)
{
    S_StartSound(actor,sfx_flame);
    A_Fire(actor);
}

void A_Fire (mobj_t* actor)
{
    mobj_t*	dest;
    unsigned	an;
		
    dest = actor->tracer;
    if (!dest)
	return;
		
    // don't move it if the vile lost sight
    if (!P_CheckSight (actor->target, dest) )
	return;

    an = dest->angle >> ANGLETOFINESHIFT;

    P_UnsetThingPosition (actor);
    actor->x = dest->x + FixedMul (24*FRACUNIT, finecosine[an]);
    actor->y = dest->y + FixedMul (24*FRACUNIT, finesine[an]);
    actor->z = dest->z;
    P_SetThingPosition (actor);
}



//
// A_VileTarget
// Spawn the hellfire
//
void A_VileTarget (mobj_t*	actor)
{
    mobj_t*	fog;
	
    if (!actor->target)
	return;

    A_FaceTarget (actor);

    fog = P_SpawnMobj (actor->target->x,
		       actor->target->x,
		       actor->target->z, MT_FIRE);
    
    actor->tracer = fog;
    fog->target = actor;
    fog->tracer = actor->target;
    A_Fire (fog);
}




//
// A_VileAttack
//
void A_VileAttack (mobj_t* actor)
{	
    mobj_t*	fire;
    int		an;
	
    if (!actor->target)
	return;
    
    A_FaceTarget (actor);

    if (!P_CheckSight (actor, actor->target) )
	return;

    S_StartSound (actor, sfx_barexp);
    P_DamageMobj (actor->target, actor, actor, 20);
    actor->target->momz = 1000*FRACUNIT/actor->target->info->mass;
	
    an = actor->angle >> ANGLETOFINESHIFT;

    fire = actor->tracer;

    if (!fire)
	return;
		
    // move the fire between the vile and the player
    fire->x = actor->target->x - FixedMul (24*FRACUNIT, finecosine[an]);
    fire->y = actor->target->y - FixedMul (24*FRACUNIT, finesine[an]);	
    P_RadiusAttack (fire, actor, 70, 70);
}




//
// Mancubus attack,
// firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it. 
//
#define	FATSPREAD	(ANG90/8)

void A_FatRaise (mobj_t *actor)
{
    A_FaceTarget (actor);
    S_StartSound (actor, sfx_manatk);
}


void A_FatAttack1 (mobj_t* actor)
{
    mobj_t*	mo;
    int		an;
	
    // nothing to fire at: a game loaded with the Mancubus mid-attack, or a
    // DEHACKED frame that calls this; id's went on to read target->x
    if (!actor->target)
	return;
    A_FaceTarget (actor);
    // Change direction  to ...
    actor->angle += FATSPREAD;
    P_SpawnMissile (actor, actor->target, MT_FATSHOT);

    mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
    mo->angle += FATSPREAD;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul (mo->info->speed, finecosine[an]);
    mo->momy = FixedMul (mo->info->speed, finesine[an]);
}

void A_FatAttack2 (mobj_t* actor)
{
    mobj_t*	mo;
    int		an;

    // nothing to fire at: a game loaded with the Mancubus mid-attack, or a
    // DEHACKED frame that calls this; id's went on to read target->x
    if (!actor->target)
	return;
    A_FaceTarget (actor);
    // Now here choose opposite deviation.
    actor->angle -= FATSPREAD;
    P_SpawnMissile (actor, actor->target, MT_FATSHOT);

    mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
    mo->angle -= FATSPREAD*2;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul (mo->info->speed, finecosine[an]);
    mo->momy = FixedMul (mo->info->speed, finesine[an]);
}

void A_FatAttack3 (mobj_t*	actor)
{
    mobj_t*	mo;
    int		an;

    // nothing to fire at: a game loaded with the Mancubus mid-attack, or a
    // DEHACKED frame that calls this; id's went on to read target->x
    if (!actor->target)
	return;
    A_FaceTarget (actor);
    
    mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
    mo->angle -= FATSPREAD/2;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul (mo->info->speed, finecosine[an]);
    mo->momy = FixedMul (mo->info->speed, finesine[an]);

    mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
    mo->angle += FATSPREAD/2;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul (mo->info->speed, finecosine[an]);
    mo->momy = FixedMul (mo->info->speed, finesine[an]);
}


//
// SkullAttack
// Fly at the player like a missile.
//
#define	SKULLSPEED		(20*FRACUNIT)

void A_SkullAttack (mobj_t* actor)
{
    mobj_t*		dest;
    angle_t		an;
    int			dist;

    if (!actor->target)
	return;
		
    dest = actor->target;	
    actor->flags |= MF_SKULLFLY;

    S_StartSound (actor, actor->info->attacksound);
    A_FaceTarget (actor);
    an = actor->angle >> ANGLETOFINESHIFT;
    actor->momx = FixedMul (SKULLSPEED, finecosine[an]);
    actor->momy = FixedMul (SKULLSPEED, finesine[an]);
    dist = P_AproxDistance (dest->x - actor->x, dest->y - actor->y);
    dist = dist / SKULLSPEED;
    
    if (dist < 1)
	dist = 1;
    actor->momz = (dest->z+(dest->height>>1) - actor->z) / dist;
}


//
// A_PainShootSkull
// Spawn a lost soul and launch it at the target
//
void
A_PainShootSkull
( mobj_t*	actor,
  angle_t	angle )
{
    fixed_t	x;
    fixed_t	y;
    fixed_t	z;
    
    mobj_t*	newmobj;
    angle_t	an;
    int		prestep;
    int		count;
    thinker_t*	currentthinker;

    // count total number of skull currently on the level
    count = 0;

    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
	if (   (currentthinker->function.acp1 == (actionf_p1)P_MobjThinker)
	    && ((mobj_t *)currentthinker)->type == MT_SKULL)
	    count++;
	currentthinker = currentthinker->next;
    }

    // if there are allready 20 skulls on the level,
    // don't spit another one
    if (count > 20)
	return;


    // okay, there's playe for another one
    an = angle >> ANGLETOFINESHIFT;
    
    prestep =
	4*FRACUNIT
	+ 3*(actor->info->radius + mobjinfo[MT_SKULL].radius)/2;
    
    x = actor->x + FixedMul (prestep, finecosine[an]);
    y = actor->y + FixedMul (prestep, finesine[an]);
    z = actor->z + 8*FRACUNIT;
		
    newmobj = P_SpawnMobj (x , y, z, MT_SKULL);

    // Check for movements.
    if (!P_TryMove (newmobj, newmobj->x, newmobj->y))
    {
	// kill it immediately
	P_DamageMobj (newmobj,actor,actor,10000);	
	return;
    }
		
    newmobj->target = actor->target;
    A_SkullAttack (newmobj);
}


//
// A_PainAttack
// Spawn a lost soul and launch it at the target
// 
void A_PainAttack (mobj_t* actor)
{
    if (!actor->target)
	return;

    A_FaceTarget (actor);
    A_PainShootSkull (actor, actor->angle);
}


void A_PainDie (mobj_t* actor)
{
    A_Fall (actor);
    A_PainShootSkull (actor, actor->angle+ANG90);
    A_PainShootSkull (actor, actor->angle+ANG180);
    A_PainShootSkull (actor, actor->angle+ANG270);
}






void A_Scream (mobj_t* actor)
{
    int		sound;
	
    switch (actor->info->deathsound)
    {
      case 0:
	return;
		
      case sfx_podth1:
      case sfx_podth2:
      case sfx_podth3:
	sound = sfx_podth1 + P_Random ()%3;
	break;
		
      case sfx_bgdth1:
      case sfx_bgdth2:
	sound = sfx_bgdth1 + P_Random ()%2;
	break;
	
      default:
	sound = actor->info->deathsound;
	break;
    }

    // Check for bosses.
    if (actor->flags2 & MF2_FULLVOLSOUNDS)
    {
	// full volume
	S_StartSound (NULL, sound);
    }
    else
	S_StartSound (actor, sound);
}


void A_XScream (mobj_t* actor)
{
    S_StartSound (actor, sfx_slop);	
}

void A_Pain (mobj_t* actor)
{
    if (actor->info->painsound)
	S_StartSound (actor, actor->info->painsound);	
}



void A_Fall (mobj_t *actor)
{
    // actor is on ground, it can be walked over
    actor->flags &= ~MF_SOLID;

    // So change this if corpse objects
    // are meant to be obstacles.
}


//
// A_Explode
//
void A_Explode (mobj_t* thingy)
{
    P_RadiusAttack ( thingy, thingy->target, 128, 128 );
}


//
// P_BossAction
// The boss deaths a map's UMAPINFO gives in place of the game's: when the
// last of a kind of monster dies, a line special goes off, as if the player
// had crossed or pressed a line carrying it -- the Master Levels' MAP19 and
// MAP20 lower floors so. False when the map gives none and the game's own
// stand; true when it does, even if none is for this monster.
//
static boolean P_BossAction (mobj_t* mo)
{
    umapentry_t*	map = U_ThisMap ();
    thinker_t*		th;
    mobj_t*		mo2;
    int			i;

    if (!map || !map->bossactions_set)
	return false;

    for (i=0 ; i<map->numbossactions ; i++)
	if (map->bossactions[i].type == mo->type)
	    break;
    if (i == map->numbossactions)
	return true;

    // a player alive to do it for, and the last of them dead, as below
    for (i=0 ; i<MAXPLAYERS ; i++)
	if (playeringame[i] && players[i].health > 0 && players[i].mo)
	    break;
    if (i==MAXPLAYERS)
	return true;

    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function.acp1 != (actionf_p1)P_MobjThinker)
	    continue;
	mo2 = (mobj_t *)th;
	if (mo2 != mo && mo2->type == mo->type && mo2->health > 0)
	    return true;
    }

    {
	mobj_t*	activator = players[i].mo;

	for (i=0 ; i<map->numbossactions ; i++)
	    if (map->bossactions[i].type == mo->type)
		P_ActivateLineSpecial (map->bossactions[i].special,
				       map->bossactions[i].tag, activator);
    }
    return true;
}


//
// P_BossActionOnKill
// UMAPINFO can give a boss action to any monster, where the game has one
// only for those whose death frames end in A_BossDeath. The others' happen
// here, as they die.
//
void A_BossDeath (mobj_t* mo);

void P_BossActionOnKill (mobj_t* target)
{
    statenum_t	s = mobjinfo[target->type].deathstate;
    int		n;

    for (n = 0; s != S_NULL && n < 64; n++)
    {
	if (states[s].action.acp1 == (actionf_p1)A_BossDeath)
	    return;			// A_BossDeath will see to it
	if (states[s].tics == -1)
	    break;
	s = states[s].nextstate;
    }
    P_BossAction (target);
}


//
// A_BossDeath
// Possibly trigger special effects
// if on first boss level
//
void A_BossDeath (mobj_t* mo)
{
    thinker_t*	th;
    mobj_t*	mo2;
    line_t	junk;
    int		i;

    if (P_BossAction (mo))
	return;
		
    if ( gamemode == commercial)
    {
	if (gamemap != 7)
	    return;
		
	if (!(mo->flags2 & (MF2_MAP07BOSS1 | MF2_MAP07BOSS2)))
	    return;
    }
    else
    {
	switch(gameepisode)
	{
	  case 1:
	    if (gamemap != 8)
		return;

	    if (!(mo->flags2 & MF2_E1M8BOSS))
		return;
	    break;
	    
	  case 2:
	    if (gamemap != 8)
		return;

	    if (!(mo->flags2 & MF2_E2M8BOSS))
		return;
	    break;
	    
	  case 3:
	    if (gamemap != 8)
		return;
	    
	    if (!(mo->flags2 & MF2_E3M8BOSS))
		return;
	    
	    break;
	    
	  case 4:
	    switch(gamemap)
	    {
	      case 6:
		if (!(mo->flags2 & MF2_E4M6BOSS))
		    return;
		break;
		
	      case 8: 
		if (!(mo->flags2 & MF2_E4M8BOSS))
		    return;
		break;
		
	      default:
		return;
		break;
	    }
	    break;
	    
	  default:
	    if (gamemap != 8)
		return;
	    break;
	}
		
    }

    
    // make sure there is a player alive for victory
    for (i=0 ; i<MAXPLAYERS ; i++)
	if (playeringame[i] && players[i].health > 0)
	    break;
    
    if (i==MAXPLAYERS)
	return;	// no one left alive, so do not end game
    
    // scan the remaining thinkers to see
    // if all bosses are dead
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function.acp1 != (actionf_p1)P_MobjThinker)
	    continue;
	
	mo2 = (mobj_t *)th;
	if (mo2 != mo
	    && mo2->type == mo->type
	    && mo2->health > 0)
	{
	    // other boss not dead
	    return;
	}
    }
	
    // victory!
    if ( gamemode == commercial)
    {
	if (gamemap == 7)
	{
	    if (mo->flags2 & MF2_MAP07BOSS1)
	    {
		junk.tag = 666;
		EV_DoFloor(&junk,lowerFloorToLowest);
		return;
	    }
	    
	    if (mo->flags2 & MF2_MAP07BOSS2)
	    {
		junk.tag = 667;
		EV_DoFloor(&junk,raiseToTexture);
		return;
	    }
	}
    }
    else
    {
	switch(gameepisode)
	{
	  case 1:
	    junk.tag = 666;
	    EV_DoFloor (&junk, lowerFloorToLowest);
	    return;
	    break;
	    
	  case 4:
	    switch(gamemap)
	    {
	      case 6:
		junk.tag = 666;
		EV_DoDoor (&junk, blazeOpen);
		return;
		break;
		
	      case 8:
		junk.tag = 666;
		EV_DoFloor (&junk, lowerFloorToLowest);
		return;
		break;
	    }
	}
    }
	
    G_ExitLevel ();
}


void A_Hoof (mobj_t* mo)
{
    S_StartSound (mo, sfx_hoof);
    A_Chase (mo);
}

void A_Metal (mobj_t* mo)
{
    S_StartSound (mo, sfx_metal);
    A_Chase (mo);
}

void A_BabyMetal (mobj_t* mo)
{
    S_StartSound (mo, sfx_bspwlk);
    A_Chase (mo);
}

void
A_OpenShotgun2
( player_t*	player,
  pspdef_t*	psp )
{
    S_StartSound (player->mo, sfx_dbopn);
}

void
A_LoadShotgun2
( player_t*	player,
  pspdef_t*	psp )
{
    S_StartSound (player->mo, sfx_dbload);
}

void
A_ReFire
( player_t*	player,
  pspdef_t*	psp );

void
A_CloseShotgun2
( player_t*	player,
  pspdef_t*	psp )
{
    S_StartSound (player->mo, sfx_dbcls);
    A_ReFire(player,psp);
}



// The spots the boss brain spits cubes at. id's array held 32 and went on
// writing past it; this grows.
mobj_t**	braintargets;
int		numbraintargets;
static int	maxbraintargets;
int		braintargeton;

void A_BrainAwake (mobj_t* mo)
{
    thinker_t*	thinker;
    mobj_t*	m;
	
    // find all the target spots
    numbraintargets = 0;
    braintargeton = 0;
	
    thinker = thinkercap.next;
    for (thinker = thinkercap.next ;
	 thinker != &thinkercap ;
	 thinker = thinker->next)
    {
	if (thinker->function.acp1 != (actionf_p1)P_MobjThinker)
	    continue;	// not a mobj

	m = (mobj_t *)thinker;

	if (m->type == MT_BOSSTARGET )
	{
	    if (numbraintargets == maxbraintargets)
	    {
		maxbraintargets = maxbraintargets ? maxbraintargets * 2 : 32;
		braintargets = realloc (braintargets,
					maxbraintargets * sizeof(*braintargets));
		if (!braintargets)
		    I_Error ("A_BrainAwake: no memory for %d targets",
			     maxbraintargets);
	    }
	    braintargets[numbraintargets] = m;
	    numbraintargets++;
	}
    }
	
    S_StartSound (NULL,sfx_bossit);
}


void A_BrainPain (mobj_t*	mo)
{
    S_StartSound (NULL,sfx_bospn);
}


void A_BrainScream (mobj_t*	mo)
{
    int		x;
    int		y;
    int		z;
    mobj_t*	th;
	
    for (x=mo->x - 196*FRACUNIT ; x< mo->x + 320*FRACUNIT ; x+= FRACUNIT*8)
    {
	y = mo->y - 320*FRACUNIT;
	z = 128 + P_Random()*2*FRACUNIT;
	th = P_SpawnMobj (x,y,z, MT_ROCKET);
	th->momz = P_Random()*512;

	P_SetMobjState (th, S_BRAINEXPLODE1);

	th->tics -= P_Random()&7;
	if (th->tics < 1)
	    th->tics = 1;
    }
	
    S_StartSound (NULL,sfx_bosdth);
}



void A_BrainExplode (mobj_t* mo)
{
    int		x;
    int		y;
    int		z;
    mobj_t*	th;
	
    x = mo->x + (P_Random () - P_Random ())*2048;
    y = mo->y;
    z = 128 + P_Random()*2*FRACUNIT;
    th = P_SpawnMobj (x,y,z, MT_ROCKET);
    th->momz = P_Random()*512;

    P_SetMobjState (th, S_BRAINEXPLODE1);

    th->tics -= P_Random()&7;
    if (th->tics < 1)
	th->tics = 1;
}


void A_BrainDie (mobj_t*	mo)
{
    G_ExitLevel ();
}

void A_BrainSpit (mobj_t*	mo)
{
    mobj_t*	targ;
    mobj_t*	newmobj;
    
    static int	easy = 0;
	
    easy ^= 1;
    if (gameskill <= sk_easy && (!easy))
	return;
		
    // shoot a cube at current target
    targ = braintargets[braintargeton];
    braintargeton = (braintargeton+1)%numbraintargets;

    // spawn brain missile
    newmobj = P_SpawnMissile (mo, targ, MT_SPAWNSHOT);
    newmobj->target = targ;
    newmobj->reactiontime =
	((targ->y - mo->y)/newmobj->momy) / newmobj->state->tics;

    S_StartSound(NULL, sfx_bospit);
}



void A_SpawnFly (mobj_t* mo);

// travelling cube sound
void A_SpawnSound (mobj_t* mo)	
{
    S_StartSound (mo,sfx_boscub);
    A_SpawnFly(mo);
}

void A_SpawnFly (mobj_t* mo)
{
    mobj_t*	newmobj;
    mobj_t*	fog;
    mobj_t*	targ;
    int		r;
    mobjtype_t	type;
	
    if (--mo->reactiontime)
	return;	// still flying
	
    targ = mo->target;

    // First spawn teleport fog.
    fog = P_SpawnMobj (targ->x, targ->y, targ->z, MT_SPAWNFIRE);
    S_StartSound (fog, sfx_telept);

    // Randomly select monster to spawn.
    r = P_Random ();

    // Probability distribution (kind of :),
    // decreasing likelihood.
    if ( r<50 )
	type = MT_TROOP;
    else if (r<90)
	type = MT_SERGEANT;
    else if (r<120)
	type = MT_SHADOWS;
    else if (r<130)
	type = MT_PAIN;
    else if (r<160)
	type = MT_HEAD;
    else if (r<162)
	type = MT_VILE;
    else if (r<172)
	type = MT_UNDEAD;
    else if (r<192)
	type = MT_BABY;
    else if (r<222)
	type = MT_FATSO;
    else if (r<246)
	type = MT_KNIGHT;
    else
	type = MT_BRUISER;		

    newmobj	= P_SpawnMobj (targ->x, targ->y, targ->z, type);
    if (P_LookForPlayers (newmobj, true) )
	P_SetMobjState (newmobj, newmobj->info->seestate);
	
    // telefrag anything in this spot
    P_TeleportMove (newmobj, newmobj->x, newmobj->y);

    // remove self (i.e., cube).
    P_RemoveMobj (mo);
}



void A_PlayerScream (mobj_t* mo)
{
    // Default death sound.
    int		sound = sfx_pldeth;
	
    if ( (gamemode == commercial)
	&& 	(mo->health < -50))
    {
	// IF THE PLAYER DIES
	// LESS THAN -50% WITHOUT GIBBING
	sound = sfx_pdiehi;
    }
    
    S_StartSound (mo, sound);
}



//
// MBF's code pointers (killough, 1998; inspired by Len Pitre), by way
// of Woof. id's engine had none of these, so no DOOM demo reaches one;
// they work whichever way a level plays.
//

// kill it
void A_Die (mobj_t* actor)
{
    P_DamageMobj (actor, NULL, NULL, actor->health);
}

// A_Explode with the thing's own damage
void A_Detonate (mobj_t* mo)
{
    P_RadiusAttack (mo, mo->target, mo->info->damage, mo->info->damage);
}

// an explosion that throws Mancubus fireballs about (Linguica's idea):
// misc1 how high they are aimed, misc2 how fast, as fractions
void A_Mushroom (mobj_t* actor)
{
    int		i, j, n = actor->info->damage;
    fixed_t	misc1 = actor->state->misc1 ? actor->state->misc1 : FRACUNIT*4;
    fixed_t	misc2 = actor->state->misc2 ? actor->state->misc2 : FRACUNIT/2;

    A_Explode (actor);
    for (i = -n; i <= n; i += 8)
	for (j = -n; j <= n; j += 8)
	{
	    mobj_t	target = *actor;
	    mobj_t*	mo;

	    target.x += i << FRACBITS;	// aim in many directions
	    target.y += j << FRACBITS;
	    target.z += P_AproxDistance (i, j) * misc1;	// and fairly high
	    mo = P_SpawnMissile (actor, &target, MT_FATSHOT);
	    mo->momx = FixedMul (mo->momx, misc2);
	    mo->momy = FixedMul (mo->momy, misc2);
	    mo->momz = FixedMul (mo->momz, misc2);
	    mo->flags &= ~MF_NOGRAVITY;	// the debris falls
	}
}

// the beta's lost soul, which bit from where it was
void A_BetaSkullAttack (mobj_t* actor)
{
    int		damage;

    if (!actor->target || actor->target->type == MT_SKULL)
	return;
    if (actor->info->attacksound)
	S_StartSound (actor, actor->info->attacksound);
    A_FaceTarget (actor);
    damage = (P_Random () % 8 + 1) * actor->info->damage;
    P_DamageMobj (actor->target, actor, actor, damage);
}

void A_Stop (mobj_t* actor)
{
    actor->momx = actor->momy = actor->momz = 0;
}

// spawn thing misc1 (numbered from 1), misc2 units up
void A_Spawn (mobj_t* mo)
{
    mobj_t*	newmobj;
    int		type = mo->state->misc1 - 1;

    if (type < 0 || type >= nummobjtypes)
	return;
    newmobj = P_SpawnMobj (mo->x, mo->y, (mo->state->misc2 << FRACBITS) + mo->z,
			   type);
    if (comp[comp_friendlyspawn])
	newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (mo->flags & MF_FRIEND);
}

// turn by misc1 degrees
void A_Turn (mobj_t* mo)
{
    mo->angle += (angle_t) (((uint64_t) mo->state->misc1 << 32) / 360);
}

// face misc1 degrees
void A_Face (mobj_t* mo)
{
    mo->angle = (angle_t) (((uint64_t) mo->state->misc1 << 32) / 360);
}

// a melee attack of misc1 damage, with sound misc2
void A_Scratch (mobj_t* mo)
{
    if (!mo->target)
	return;
    A_FaceTarget (mo);
    if (!P_CheckMeleeRange (mo))
	return;
    if (mo->state->misc2 > 0 && mo->state->misc2 < numsfx)
	S_StartSound (mo, mo->state->misc2);
    P_DamageMobj (mo->target, mo, mo, mo->state->misc1);
}

// sound misc1, everywhere if misc2
void A_PlaySound (mobj_t* mo)
{
    if (mo->state->misc1 > 0 && mo->state->misc1 < numsfx)
	S_StartSound (mo->state->misc2 ? NULL : mo, mo->state->misc1);
}

// to frame misc1, with a chance of misc2 in 256
void A_RandomJump (mobj_t* mo)
{
    if (P_Random () < mo->state->misc2
	&& mo->state->misc1 >= 0 && mo->state->misc1 < numstates)
	P_SetMobjState (mo, mo->state->misc1);
}

// line special misc1 with tag misc2, as if a player used it and then
// crossed it; a once-only one only once for this thing
void A_LineEffect (mobj_t* mo)
{
    static player_t	player;
    player_t*		oldplayer;

    if ((mo->intflags & MIF_LINEDONE) || !mo->state->misc1 || !numlines)
	return;
    oldplayer = mo->player;
    mo->player = &player;
    player.health = 100;
    if (P_LineEffect (mo, (short) mo->state->misc1, (short) mo->state->misc2))
	mo->intflags |= MIF_LINEDONE;
    mo->player = oldplayer;
}


//
// MBF21's code pointers (Xaser and others), by way of Woof. Their
// arguments are the frame's args, defaulted by D_FinishDehacked; angles
// and spreads are in degrees, in fixed point.
//

angle_t P_FixedToAngle (fixed_t a)
{
    return (angle_t) (((uint64_t) a * ANG1) >> FRACBITS);
}

static fixed_t P_AngleToSlope (int a)
{
    if (a > (int) ANG90)
	return finetangent[0];
    else if (-a > (int) ANG90)
	return finetangent[FINEANGLES/2 - 1];
    else
	return finetangent[(ANG90 - a) >> ANGLETOFINESHIFT];
}

fixed_t P_DegToSlope (fixed_t a)
{
    if (a >= 0)
	return P_AngleToSlope (P_FixedToAngle (a));
    return P_AngleToSlope (-(int) P_FixedToAngle (-a));
}

// a random angle in (-spread, spread)
int P_RandomHitscanAngle (fixed_t spread)
{
    int64_t	bam = P_FixedToAngle (spread < 0 ? -spread : spread);
    int		t = P_Random ();

    return (int) ((bam * (t - P_Random ())) / 255);
}

// the same as a slope
int P_RandomHitscanSlope (fixed_t spread)
{
    int		angle = P_RandomHitscanAngle (spread);

    if (angle > (int) ANG90)
	return finetangent[0];
    else if (-angle > (int) ANG90)
	return finetangent[FINEANGLES/2 - 1];
    return finetangent[(ANG90 - angle) >> ANGLETOFINESHIFT];
}

// is t2 within fov of where t1 faces?
static boolean P_CheckFov (mobj_t* t1, mobj_t* t2, angle_t fov)
{
    angle_t	angle = R_PointToAngle2 (t1->x, t1->y, t2->x, t2->y);
    angle_t	minang = t1->angle - fov / 2;
    angle_t	maxang = t1->angle + fov / 2;

    return minang > maxang ? angle >= minang || angle <= maxang
			   : angle >= minang && angle <= maxang;
}

// 1 if source turns clockwise to face target, 0 if the other way; *delta
// how far
static int P_FaceMobj (mobj_t* source, mobj_t* target, angle_t* delta)
{
    angle_t	angle1 = source->angle;
    angle_t	angle2 = R_PointToAngle2 (source->x, source->y,
					  target->x, target->y);
    angle_t	diff;

    if (angle2 > angle1)
    {
	diff = angle2 - angle1;
	if (diff > ANG180)
	{
	    *delta = ANGLE_MAX - diff;
	    return 0;
	}
	*delta = diff;
	return 1;
    }
    diff = angle1 - angle2;
    if (diff > ANG180)
    {
	*delta = ANGLE_MAX - diff;
	return 1;
    }
    *delta = diff;
    return 0;
}

// steer a missile at *seektarget, turning at most turnmax, and all the
// way if within thresh
static boolean
P_SeekerMissile
( mobj_t*	actor,
  mobj_t**	seektarget,
  angle_t	thresh,
  angle_t	turnmax,
  boolean	seekcenter )
{
    int		dir, dist;
    angle_t	delta, angle;
    mobj_t*	target = *seektarget;

    if (!target)
	return false;
    if (!(target->flags & MF_SHOOTABLE))
    {
	*seektarget = NULL;	// target died
	return false;
    }
    dir = P_FaceMobj (actor, target, &delta);
    if (delta > thresh)
    {
	delta >>= 1;
	if (delta > turnmax)
	    delta = turnmax;
    }
    if (dir)
	actor->angle += delta;
    else
	actor->angle -= delta;
    angle = actor->angle >> ANGLETOFINESHIFT;
    actor->momx = FixedMul (actor->info->speed, finecosine[angle]);
    actor->momy = FixedMul (actor->info->speed, finesine[angle]);
    if (actor->z + actor->height < target->z
	|| target->z + target->height < actor->z || seekcenter)
    {
	dist = P_AproxDistance (target->x - actor->x, target->y - actor->y);
	dist = dist / (actor->info->speed ? actor->info->speed : 1);
	if (dist < 1)
	    dist = 1;
	actor->momz = (target->z + (seekcenter ? target->height/2 : 0)
		       - actor->z) / dist;
    }
    return true;
}

// the first thing in block index that a missile might seek
static mobj_t* P_RoughBlockCheck (mobj_t* mo, int index, angle_t fov)
{
    mobj_t*	link;

    for (link = blocklinks[index]; link; link = link->bnext)
    {
	if (!(link->flags & MF_SHOOTABLE))
	    continue;
	if (link == mo->target)
	    continue;	// its owner
	// not on its owner's side, unless infighting or deathmatching
	if (mo->target && !((link->flags ^ mo->target->flags) & MF_FRIEND)
	    && mo->target->target != link
	    && !(deathmatch && link->player && mo->target->player))
	    continue;
	if (fov > 0 && !P_CheckFov (mo, link, fov))
	    continue;
	if (!P_CheckSight (mo, link))
	    continue;
	return link;
    }
    return NULL;
}

// Hexen's P_RoughMonsterSearch: the nearest block with a target in it,
// out to distance blocks
static mobj_t* P_RoughTargetSearch (mobj_t* mo, angle_t fov, int distance)
{
    int		startx, starty, count, bx, by;
    mobj_t*	target;

    startx = (mo->x - bmaporgx) >> MAPBLOCKSHIFT;
    starty = (mo->y - bmaporgy) >> MAPBLOCKSHIFT;

    if (startx >= 0 && startx < bmapwidth && starty >= 0
	&& starty < bmapheight
	&& (target = P_RoughBlockCheck (mo, starty*bmapwidth + startx, fov)))
	return target;

    for (count = 1; count <= distance; count++)
    {
	// the ring of blocks count away
	for (by = starty - count; by <= starty + count; by++)
	{
	    if (by < 0 || by >= bmapheight)
		continue;
	    for (bx = startx - count; bx <= startx + count; bx++)
	    {
		if (bx < 0 || bx >= bmapwidth)
		    continue;
		if (by != starty - count && by != starty + count
		    && bx != startx - count && bx != startx + count)
		    continue;
		if ((target = P_RoughBlockCheck (mo, by*bmapwidth + bx, fov)))
		    return target;
	    }
	}
    }
    return NULL;
}

// a frame a patch named, if there is one
static boolean P_ValidState (int state)
{
    return state >= 0 && state < numstates;
}

// Spawn args[0] (a thing, from 1) at an angle args[1] from the actor's,
// offset args[2..4], moving args[5..7]. A missile is fired as if by the
// actor, or by the actor's own shooter if the actor is a missile.
void A_SpawnObject (mobj_t* actor)
{
    int		type, angle, ofs_x, ofs_y, ofs_z, vel_x, vel_y, vel_z;
    angle_t	an;
    int		fan, dx, dy;
    mobj_t*	mo;

    if (!actor->state->args[0])
	return;
    type = actor->state->args[0] - 1;
    if (type < 0 || type >= nummobjtypes)
	return;
    angle = actor->state->args[1];
    ofs_x = actor->state->args[2];
    ofs_y = actor->state->args[3];
    ofs_z = actor->state->args[4];
    vel_x = actor->state->args[5];
    vel_y = actor->state->args[6];
    vel_z = actor->state->args[7];

    an = actor->angle + (angle_t) (((int64_t) angle << 16) / 360);
    fan = an >> ANGLETOFINESHIFT;
    dx = FixedMul (ofs_x, finecosine[fan]) - FixedMul (ofs_y, finesine[fan]);
    dy = FixedMul (ofs_x, finesine[fan]) + FixedMul (ofs_y, finecosine[fan]);

    mo = P_SpawnMobj (actor->x + dx, actor->y + dy, actor->z + ofs_z, type);
    mo->angle = an;
    mo->momx = FixedMul (vel_x, finecosine[fan]) - FixedMul (vel_y, finesine[fan]);
    mo->momy = FixedMul (vel_x, finesine[fan]) + FixedMul (vel_y, finecosine[fan]);
    mo->momz = vel_z;

    if (mo->info->flags & (MF_MISSILE | MF_BOUNCES))
    {
	if (actor->info->flags & (MF_MISSILE | MF_BOUNCES))
	{
	    mo->target = actor->target;
	    mo->tracer = actor->tracer;
	}
	else
	{
	    mo->target = actor;
	    mo->tracer = actor->target;
	}
    }
}

// Fire args[0] (a thing, from 1) at the target, args[1] degrees aside and
// args[2] up, from args[3] to the side and args[4] up. The missile's tracer
// is the target, so it can seek it.
void A_MonsterProjectile (mobj_t* actor)
{
    int		type, angle, pitch, spawnofs_xy, spawnofs_z, an;
    mobj_t*	mo;

    if (!actor->target || !actor->state->args[0])
	return;
    type = actor->state->args[0] - 1;
    if (type < 0 || type >= nummobjtypes)
	return;
    angle = actor->state->args[1];
    pitch = actor->state->args[2];
    spawnofs_xy = actor->state->args[3];
    spawnofs_z = actor->state->args[4];

    A_FaceTarget (actor);
    mo = P_SpawnMissile (actor, actor->target, type);
    if (!mo)
	return;

    mo->angle += (angle_t) (((int64_t) angle << 16) / 360);
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul (mo->info->speed, finecosine[an]);
    mo->momy = FixedMul (mo->info->speed, finesine[an]);
    mo->momz += FixedMul (mo->info->speed, P_DegToSlope (pitch));

    an = (actor->angle - ANG90) >> ANGLETOFINESHIFT;
    mo->x += FixedMul (spawnofs_xy, finecosine[an]);
    mo->y += FixedMul (spawnofs_xy, finesine[an]);
    mo->z += spawnofs_z;

    mo->tracer = actor->target;
}

// args[2] bullets, spread args[0] across and args[1] up and down, each
// doing args[3] times 1 to args[4]
void A_MonsterBulletAttack (mobj_t* actor)
{
    int		hspread, vspread, numbullets, damagebase, damagemod;
    int		aimslope, i, damage, angle, slope;

    if (!actor->target)
	return;
    hspread = actor->state->args[0];
    vspread = actor->state->args[1];
    numbullets = actor->state->args[2];
    damagebase = actor->state->args[3];
    damagemod = actor->state->args[4];
    if (damagemod <= 0)
	damagemod = 1;

    A_FaceTarget (actor);
    S_StartSound (actor, actor->info->attacksound);
    aimslope = P_AimLineAttack (actor, actor->angle, MISSILERANGE);
    for (i = 0; i < numbullets; i++)
    {
	damage = (P_Random () % damagemod + 1) * damagebase;
	angle = (int) actor->angle + P_RandomHitscanAngle (hspread);
	slope = aimslope + P_RandomHitscanSlope (vspread);
	P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
    }
}

// a bite of args[0] times 1 to args[1], sound args[2] when it lands,
// reaching args[3] (the thing's melee range if 0)
void A_MonsterMeleeAttack (mobj_t* actor)
{
    int		damagebase, damagemod, hitsound, range, damage;

    if (!actor->target)
	return;
    damagebase = actor->state->args[0];
    damagemod = actor->state->args[1];
    hitsound = actor->state->args[2];
    range = actor->state->args[3];
    if (damagemod <= 0)
	damagemod = 1;
    if (range == 0)
	range = actor->info->meleerange;
    range += actor->target->info->radius - 20*FRACUNIT;

    A_FaceTarget (actor);
    if (!P_CheckRange (actor, range))
	return;
    if (hitsound > 0 && hitsound < numsfx)
	S_StartSound (actor, hitsound);
    damage = (P_Random () % damagemod + 1) * damagebase;
    P_DamageMobj (actor->target, actor, actor, damage);
}

// A_Explode, of args[0] damage reaching args[1]
void A_RadiusDamage (mobj_t* actor)
{
    P_RadiusAttack (actor, actor->target, actor->state->args[0],
		    actor->state->args[1]);
}

// wake the monsters that could hear the actor, to its target
void A_NoiseAlert (mobj_t* actor)
{
    if (actor->target)
	P_NoiseAlert (actor->target, actor);
}

// A_VileChase, going to frame args[0] with sound args[1]
void A_HealChase (mobj_t* actor)
{
    int		state = actor->state->args[0];
    int		sound = actor->state->args[1];

    if (!P_ValidState (state)
	|| !P_HealCorpse (actor, actor->info->radius, state, sound))
	A_Chase (actor);
}

// home on the tracer: straight at it within args[0] degrees, else
// turning at most args[1]
void A_SeekTracer (mobj_t* actor)
{
    P_SeekerMissile (actor, &actor->tracer,
		     P_FixedToAngle (actor->state->args[0]),
		     P_FixedToAngle (actor->state->args[1]), true);
}

// find a tracer, within args[0] degrees (all round if 0) and args[1]
// blocks, unless it has one
void A_FindTracer (mobj_t* actor)
{
    if (actor->tracer)
	return;
    actor->tracer = P_RoughTargetSearch (actor,
					 P_FixedToAngle (actor->state->args[0]),
					 actor->state->args[1]);
}

void A_ClearTracer (mobj_t* actor)
{
    actor->tracer = NULL;
}

// to frame args[0] if health is below args[1]
void A_JumpIfHealthBelow (mobj_t* actor)
{
    if (actor->health < actor->state->args[1]
	&& P_ValidState (actor->state->args[0]))
	P_SetMobjState (actor, actor->state->args[0]);
}

// to frame args[0] if who is seen, within args[1] degrees (all round if 0)
static void P_JumpIfInSight (mobj_t* actor, mobj_t* who)
{
    int		state = actor->state->args[0];
    angle_t	fov = P_FixedToAngle (actor->state->args[1]);

    if (!who)
	return;
    if (fov > 0 && !P_CheckFov (actor, who, fov))
	return;
    if (P_CheckSight (actor, who) && P_ValidState (state))
	P_SetMobjState (actor, state);
}

// to frame args[0] if who is closer than args[1]
static void P_JumpIfCloser (mobj_t* actor, mobj_t* who)
{
    int		state = actor->state->args[0];

    if (who && actor->state->args[1] > P_AproxDistance (actor->x - who->x,
							actor->y - who->y)
	&& P_ValidState (state))
	P_SetMobjState (actor, state);
}

void A_JumpIfTargetInSight (mobj_t* actor)
{
    P_JumpIfInSight (actor, actor->target);
}

void A_JumpIfTargetCloser (mobj_t* actor)
{
    P_JumpIfCloser (actor, actor->target);
}

void A_JumpIfTracerInSight (mobj_t* actor)
{
    P_JumpIfInSight (actor, actor->tracer);
}

void A_JumpIfTracerCloser (mobj_t* actor)
{
    P_JumpIfCloser (actor, actor->tracer);
}

// to frame args[0] if all of flags args[1] and flags2 args[2] are set
void A_JumpIfFlagsSet (mobj_t* actor)
{
    unsigned	flags = actor->state->args[1];
    unsigned	flags2 = actor->state->args[2];

    if ((actor->flags & flags) == flags && (actor->flags2 & flags2) == flags2
	&& P_ValidState (actor->state->args[0]))
	P_SetMobjState (actor, actor->state->args[0]);
}

// set or clear flags args[0] and flags2 args[1]; in or out of the
// blockmap and sector lists as NOBLOCKMAP and NOSECTOR change
static void P_ChangeFlags (mobj_t* actor, boolean add)
{
    unsigned	flags = actor->state->args[0];
    unsigned	flags2 = actor->state->args[1];
    unsigned	now = add ? ~actor->flags : actor->flags;
    boolean	relink = (flags & MF_NOBLOCKMAP & now)
			 || (flags & MF_NOSECTOR & now);

    if (relink)
	P_UnsetThingPosition (actor);
    if (add)
    {
	actor->flags |= flags;
	actor->flags2 |= flags2;
    }
    else
    {
	actor->flags &= ~flags;
	actor->flags2 &= ~flags2;
    }
    if (relink)
	P_SetThingPosition (actor);
}

void A_AddFlags (mobj_t* actor)
{
    P_ChangeFlags (actor, true);
}

void A_RemoveFlags (mobj_t* actor)
{
    P_ChangeFlags (actor, false);
}
