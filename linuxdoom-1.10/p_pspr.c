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
//	Weapon sprite animation, weapon objects.
//	Action functions for weapons.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: p_pspr.c,v 1.5 1997/02/03 22:45:12 b1 Exp $";

#include "doomdef.h"
#include "d_event.h"


#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"

// State.
#include "doomstat.h"

// Data.
#include "sounds.h"

#include "p_pspr.h"
#include "i_pad.h"
#include "d_deh.h"

#define LOWERSPEED		FRACUNIT*6
#define RAISESPEED		FRACUNIT*6

#define WEAPONBOTTOM	128*FRACUNIT
#define WEAPONTOP		32*FRACUNIT


// plasma cells for a bfg attack
#define BFGCELLS		40		


//
// P_SetPsprite
//
void
P_SetPsprite
( player_t*	player,
  int		position,
  statenum_t	stnum ) 
{
    pspdef_t*	psp;
    state_t*	state;
	
    psp = &player->psprites[position];
	
    do
    {
	if (!stnum)
	{
	    // object removed itself
	    psp->state = NULL;
	    break;	
	}
	
	state = &states[stnum];
	psp->state = state;
	psp->tics = state->tics;	// could be 0

	if (state->misc1)
	{
	    // coordinate set
	    psp->sx = state->misc1 << FRACBITS;
	    psp->sy = state->misc2 << FRACBITS;
	}
	
	// Call action routine.
	// Modified handling.
	if (state->action.acp2)
	{
	    state->action.acp2(player, psp);
	    if (!psp->state)
		break;
	}
	
	stnum = psp->state->nextstate;
	
    } while (!psp->tics);
    // an initial state of 0 could cycle through
}



//
// P_CalcSwing
//	
fixed_t		swingx;
fixed_t		swingy;

void P_CalcSwing (player_t*	player)
{
    fixed_t	swing;
    int		angle;
	
    // OPTIMIZE: tablify this.
    // A LUT would allow for different modes,
    //  and add flexibility.

    swing = player->bob;

    angle = (FINEANGLES/70*leveltime)&FINEMASK;
    swingx = FixedMul ( swing, finesine[angle]);

    angle = (FINEANGLES/70*leveltime+FINEANGLES/2)&FINEMASK;
    swingy = -FixedMul ( swingx, finesine[angle]);
}



//
// P_BringUpWeapon
// Starts bringing the pending weapon up
// from the bottom of the screen.
// Uses player
//
void P_BringUpWeapon (player_t* player)
{
    statenum_t	newstate;
	
    if (player->pendingweapon == wp_nochange)
	player->pendingweapon = player->readyweapon;
		
    if (player->pendingweapon == wp_chainsaw)
	S_StartSound (player->mo, sfx_sawup);
		
    newstate = weaponinfo[player->pendingweapon].upstate;

    player->pendingweapon = wp_nochange;
    player->psprites[ps_weapon].sy = WEAPONBOTTOM;

    P_SetPsprite (player, ps_weapon, newstate);
}

//
// P_CheckAmmo
// Returns true if there is enough ammo to shoot.
// If not, selects the next weapon to use.
//
boolean P_CheckAmmo (player_t* player)
{
    ammotype_t		ammo;
    int			count;

    ammo = weaponinfo[player->readyweapon].ammo;

    // Minimal amount for one shot varies: a patch's Ammo per shot
    // (MBF21), or id's.
    if (weaponinfo[player->readyweapon].intflags & WIF_ENABLEAPS)
	count = weaponinfo[player->readyweapon].ammopershot;
    else if (player->readyweapon == wp_bfg)
	count = deh_bfg_cells_per_shot;	// BFGCELLS, or a patch's Misc
    else if (player->readyweapon == wp_supershotgun)
	count = 2;	// Double barrel.
    else
	count = 1;	// Regular.

    // Some do not need ammunition anyway.
    // Return if current ammunition sufficient.
    if (ammo == am_noammo || player->ammo[ammo] >= count)
	return true;
		
    // Out of ammo, pick a weapon to change to.
    // Preferences are set here.
    do
    {
	if (player->weaponowned[wp_plasma]
	    && player->ammo[am_cell]
	    && (gamemode != shareware) )
	{
	    player->pendingweapon = wp_plasma;
	}
	else if (player->weaponowned[wp_supershotgun] 
		 && player->ammo[am_shell]>2
		 && (gamemode == commercial) )
	{
	    player->pendingweapon = wp_supershotgun;
	}
	else if (player->weaponowned[wp_chaingun]
		 && player->ammo[am_clip])
	{
	    player->pendingweapon = wp_chaingun;
	}
	else if (player->weaponowned[wp_shotgun]
		 && player->ammo[am_shell])
	{
	    player->pendingweapon = wp_shotgun;
	}
	else if (player->ammo[am_clip])
	{
	    player->pendingweapon = wp_pistol;
	}
	else if (player->weaponowned[wp_chainsaw])
	{
	    player->pendingweapon = wp_chainsaw;
	}
	else if (player->weaponowned[wp_missile]
		 && player->ammo[am_misl])
	{
	    player->pendingweapon = wp_missile;
	}
	else if (player->weaponowned[wp_bfg]
		 && player->ammo[am_cell]>40
		 && (gamemode != shareware) )
	{
	    player->pendingweapon = wp_bfg;
	}
	else
	{
	    // If everything fails.
	    player->pendingweapon = wp_fist;
	}
	
    } while (player->pendingweapon == wp_nochange);

    // Now set appropriate weapon overlay.
    P_SetPsprite (player,
		  ps_weapon,
		  weaponinfo[player->readyweapon].downstate);

    return false;	
}


//
// P_FireWeapon.
//
//
// How each weapon feels on a controller that vibrates: the heavy motor, the
// light one, and for how long. The BFG goes off when it fires, not when the
// trigger is pulled, so it has its own in A_FireBFG.
//
static const struct { float low, high; int ms; } weaponrumble[NUMWEAPONS] =
{
    { 0.00, 0.30,  60 },	// fist
    { 0.10, 0.45,  70 },	// pistol
    { 0.60, 0.60, 140 },	// shotgun
    { 0.15, 0.50, 110 },	// chaingun
    { 0.50, 0.30, 180 },	// rocket launcher
    { 0.05, 0.35,  60 },	// plasma rifle
    { 0.00, 0.00,   0 },	// BFG9000
    { 0.20, 0.40, 120 },	// chainsaw
    { 0.90, 0.80, 180 }		// super shotgun
};

void P_FireWeapon (player_t* player)
{
    statenum_t	newstate;
	
    if (!P_CheckAmmo (player))
	return;

    if (player == &players[consoleplayer])
	I_PadRumble (weaponrumble[player->readyweapon].low,
		     weaponrumble[player->readyweapon].high,
		     weaponrumble[player->readyweapon].ms);
	
    P_SetMobjState (player->mo, S_PLAY_ATK1);
    newstate = weaponinfo[player->readyweapon].atkstate;
    P_SetPsprite (player, ps_weapon, newstate);
    if (!(weaponinfo[player->readyweapon].flags & WPF_SILENT))
	P_NoiseAlert (player->mo, player->mo);
}


//
// P_SubtractAmmo
// What id's code pointers take for a shot: vanilla_amount, as they did,
// or a patch's Ammo per shot (MBF21), not going below none.
//
static void P_SubtractAmmo (player_t* player, int vanilla_amount)
{
    weaponinfo_t*	w = &weaponinfo[player->readyweapon];

    if (!(w->intflags & WIF_ENABLEAPS))
    {
	player->ammo[w->ammo] -= vanilla_amount;
	return;
    }
    if (w->ammo == am_noammo)
	return;
    player->ammo[w->ammo] -= w->ammopershot;
    if (player->ammo[w->ammo] < 0)
	player->ammo[w->ammo] = 0;
}



//
// P_DropWeapon
// Player died, so put the weapon away.
//
void P_DropWeapon (player_t* player)
{
    P_SetPsprite (player,
		  ps_weapon,
		  weaponinfo[player->readyweapon].downstate);
}



//
// A_WeaponReady
// The player can fire the weapon
// or change to another weapon at this time.
// Follows after getting weapon up,
// or after previous attack/fire sequence.
//
void
A_WeaponReady
( player_t*	player,
  pspdef_t*	psp )
{	
    statenum_t	newstate;
    int		angle;
    
    // get out of attack state
    if (player->mo->state == &states[S_PLAY_ATK1]
	|| player->mo->state == &states[S_PLAY_ATK2] )
    {
	P_SetMobjState (player->mo, S_PLAY);
    }
    
    if (player->readyweapon == wp_chainsaw
	&& psp->state == &states[S_SAW])
    {
	S_StartSound (player->mo, sfx_sawidl);
    }
    
    // check for change
    //  if player is dead, put the weapon away
    if (player->pendingweapon != wp_nochange || !player->health)
    {
	// change weapon
	//  (pending weapon should allready be validated)
	newstate = weaponinfo[player->readyweapon].downstate;
	P_SetPsprite (player, ps_weapon, newstate);
	return;	
    }
    
    // check for fire
    //  the missile launcher and bfg do not auto fire (MBF21: whatever
    //  has NOAUTOFIRE, as they do)
    if (player->cmd.buttons & BT_ATTACK)
    {
	if ( !player->attackdown
	     || !(weaponinfo[player->readyweapon].flags & WPF_NOAUTOFIRE) )
	{
	    player->attackdown = true;
	    P_FireWeapon (player);		
	    return;
	}
    }
    else
	player->attackdown = false;
    
    // bob the weapon based on movement speed
    angle = (128*leveltime)&FINEMASK;
    psp->sx = FRACUNIT + FixedMul (player->bob, finecosine[angle]);
    angle &= FINEANGLES/2-1;
    psp->sy = WEAPONTOP + FixedMul (player->bob, finesine[angle]);
}



//
// A_ReFire
// The player can re-fire the weapon
// without lowering it entirely.
//
void A_ReFire
( player_t*	player,
  pspdef_t*	psp )
{
    
    // check for fire
    //  (if a weaponchange is pending, let it go through instead)
    if ( (player->cmd.buttons & BT_ATTACK) 
	 && player->pendingweapon == wp_nochange
	 && player->health)
    {
	player->refire++;
	P_FireWeapon (player);
    }
    else
    {
	player->refire = 0;
	P_CheckAmmo (player);
    }
}


void
A_CheckReload
( player_t*	player,
  pspdef_t*	psp )
{
    P_CheckAmmo (player);
#if 0
    if (player->ammo[am_shell]<2)
	P_SetPsprite (player, ps_weapon, S_DSNR1);
#endif
}



//
// A_Lower
// Lowers current weapon,
//  and changes weapon at bottom.
//
void
A_Lower
( player_t*	player,
  pspdef_t*	psp )
{	
    psp->sy += LOWERSPEED;

    // Is already down.
    if (psp->sy < WEAPONBOTTOM )
	return;

    // Player is dead.
    if (player->playerstate == PST_DEAD)
    {
	psp->sy = WEAPONBOTTOM;

	// don't bring weapon back up
	return;		
    }
    
    // The old weapon has been lowered off the screen,
    // so change the weapon and start raising it
    if (!player->health)
    {
	// Player is dead, so keep the weapon off screen.
	P_SetPsprite (player,  ps_weapon, S_NULL);
	return;	
    }
	
    player->readyweapon = player->pendingweapon; 

    P_BringUpWeapon (player);
}


//
// A_Raise
//
void
A_Raise
( player_t*	player,
  pspdef_t*	psp )
{
    statenum_t	newstate;
	
    psp->sy -= RAISESPEED;

    if (psp->sy > WEAPONTOP )
	return;
    
    psp->sy = WEAPONTOP;
    
    // The weapon has been raised all the way,
    //  so change to the ready state.
    newstate = weaponinfo[player->readyweapon].readystate;

    P_SetPsprite (player, ps_weapon, newstate);
}



//
// A_GunFlash
//
void
A_GunFlash
( player_t*	player,
  pspdef_t*	psp ) 
{
    P_SetMobjState (player->mo, S_PLAY_ATK2);
    P_SetPsprite (player,ps_flash,weaponinfo[player->readyweapon].flashstate);
}



//
// WEAPON ATTACKS
//


//
// A_Punch
//
void
A_Punch
( player_t*	player,
  pspdef_t*	psp ) 
{
    angle_t	angle;
    int		damage;
    int		slope;
	
    damage = (P_Random ()%10+1)<<1;

    if (player->powers[pw_strength])	
	damage *= 10;

    angle = player->mo->angle;
    angle += (P_Random()-P_Random())<<18;
    slope = P_AimLineAttack (player->mo, angle, MELEERANGE);
    P_LineAttack (player->mo, angle, MELEERANGE, slope, damage);

    // turn to face target
    if (linetarget)
    {
	S_StartSound (player->mo, sfx_punch);
	player->mo->angle = R_PointToAngle2 (player->mo->x,
					     player->mo->y,
					     linetarget->x,
					     linetarget->y);
    }
}


//
// A_Saw
//
void
A_Saw
( player_t*	player,
  pspdef_t*	psp ) 
{
    angle_t	angle;
    int		damage;
    int		slope;

    damage = 2*(P_Random ()%10+1);
    angle = player->mo->angle;
    angle += (P_Random()-P_Random())<<18;
    
    // use meleerange + 1 se the puff doesn't skip the flash
    slope = P_AimLineAttack (player->mo, angle, MELEERANGE+1);
    P_LineAttack (player->mo, angle, MELEERANGE+1, slope, damage);

    if (!linetarget)
    {
	S_StartSound (player->mo, sfx_sawful);
	return;
    }
    S_StartSound (player->mo, sfx_sawhit);
	
    // turn to face target
    angle = R_PointToAngle2 (player->mo->x, player->mo->y,
			     linetarget->x, linetarget->y);
    if (angle - player->mo->angle > ANG180)
    {
	if (angle - player->mo->angle < -ANG90/20)
	    player->mo->angle = angle + ANG90/21;
	else
	    player->mo->angle -= ANG90/20;
    }
    else
    {
	if (angle - player->mo->angle > ANG90/20)
	    player->mo->angle = angle - ANG90/21;
	else
	    player->mo->angle += ANG90/20;
    }
    player->mo->flags |= MF_JUSTATTACKED;
}



//
// A_FireMissile
//
void
A_FireMissile
( player_t*	player,
  pspdef_t*	psp ) 
{
    P_SubtractAmmo (player, 1);
    P_SpawnPlayerMissile (player->mo, MT_ROCKET);
}


//
// A_FireBFG
//
void
A_FireBFG
( player_t*	player,
  pspdef_t*	psp ) 
{
    P_SubtractAmmo (player, deh_bfg_cells_per_shot);
    P_SpawnPlayerMissile (player->mo, MT_BFG);

    if (player == &players[consoleplayer])
	I_PadRumble (1.0, 0.6, 400);
}



//
// A_FirePlasma
//
void
A_FirePlasma
( player_t*	player,
  pspdef_t*	psp ) 
{
    P_SubtractAmmo (player, 1);

    P_SetPsprite (player,
		  ps_flash,
		  weaponinfo[player->readyweapon].flashstate+(P_Random ()&1) );

    P_SpawnPlayerMissile (player->mo, MT_PLASMA);
}



//
// P_BulletSlope
// Sets a slope so a near miss is at aproximately
// the height of the intended target
//
fixed_t		bulletslope;


void P_BulletSlope (mobj_t*	mo)
{
    angle_t	an;
    
    // see which target is to be aimed at
    an = mo->angle;
    bulletslope = P_AimLineAttack (mo, an, 16*64*FRACUNIT);

    if (!linetarget)
    {
	an += 1<<26;
	bulletslope = P_AimLineAttack (mo, an, 16*64*FRACUNIT);
	if (!linetarget)
	{
	    an -= 2<<26;
	    bulletslope = P_AimLineAttack (mo, an, 16*64*FRACUNIT);
	}
    }
}


//
// P_GunShot
//
void
P_GunShot
( mobj_t*	mo,
  boolean	accurate )
{
    angle_t	angle;
    int		damage;
	
    damage = 5*(P_Random ()%3+1);
    angle = mo->angle;

    if (!accurate)
	angle += (P_Random()-P_Random())<<18;

    P_LineAttack (mo, angle, MISSILERANGE, bulletslope, damage);
}


//
// A_FirePistol
//
void
A_FirePistol
( player_t*	player,
  pspdef_t*	psp ) 
{
    S_StartSound (player->mo, sfx_pistol);

    P_SetMobjState (player->mo, S_PLAY_ATK2);
    P_SubtractAmmo (player, 1);

    P_SetPsprite (player,
		  ps_flash,
		  weaponinfo[player->readyweapon].flashstate);

    P_BulletSlope (player->mo);
    P_GunShot (player->mo, !player->refire);
}


//
// A_FireShotgun
//
void
A_FireShotgun
( player_t*	player,
  pspdef_t*	psp ) 
{
    int		i;
	
    S_StartSound (player->mo, sfx_shotgn);
    P_SetMobjState (player->mo, S_PLAY_ATK2);

    P_SubtractAmmo (player, 1);

    P_SetPsprite (player,
		  ps_flash,
		  weaponinfo[player->readyweapon].flashstate);

    P_BulletSlope (player->mo);
	
    for (i=0 ; i<7 ; i++)
	P_GunShot (player->mo, false);
}



//
// A_FireShotgun2
//
void
A_FireShotgun2
( player_t*	player,
  pspdef_t*	psp ) 
{
    int		i;
    angle_t	angle;
    int		damage;
		
	
    S_StartSound (player->mo, sfx_dshtgn);
    P_SetMobjState (player->mo, S_PLAY_ATK2);

    P_SubtractAmmo (player, 2);

    P_SetPsprite (player,
		  ps_flash,
		  weaponinfo[player->readyweapon].flashstate);

    P_BulletSlope (player->mo);
	
    for (i=0 ; i<20 ; i++)
    {
	damage = 5*(P_Random ()%3+1);
	angle = player->mo->angle;
	angle += (P_Random()-P_Random())<<19;
	P_LineAttack (player->mo,
		      angle,
		      MISSILERANGE,
		      bulletslope + ((P_Random()-P_Random())<<5), damage);
    }
}


//
// A_FireCGun
//
void
A_FireCGun
( player_t*	player,
  pspdef_t*	psp ) 
{
    S_StartSound (player->mo, sfx_pistol);

    if (!player->ammo[weaponinfo[player->readyweapon].ammo])
	return;
		
    P_SetMobjState (player->mo, S_PLAY_ATK2);
    P_SubtractAmmo (player, 1);

    P_SetPsprite (player,
		  ps_flash,
		  weaponinfo[player->readyweapon].flashstate
		  + psp->state
		  - &states[S_CHAIN1] );

    P_BulletSlope (player->mo);
	
    P_GunShot (player->mo, !player->refire);
}



//
// ?
//
void A_Light0 (player_t *player, pspdef_t *psp)
{
    player->extralight = 0;
}

void A_Light1 (player_t *player, pspdef_t *psp)
{
    player->extralight = 1;
}

void A_Light2 (player_t *player, pspdef_t *psp)
{
    player->extralight = 2;
}


//
// A_BFGSpray
// Spawn a BFG explosion on every monster in view
//
void A_BFGSpray (mobj_t* mo) 
{
    int			i;
    int			j;
    int			damage;
    angle_t		an;
	
    // offset angles from its attack angle
    for (i=0 ; i<40 ; i++)
    {
	an = mo->angle - ANG90/2 + ANG90/40*i;

	// mo->target is the originator (player)
	//  of the missile
	P_AimLineAttack (mo->target, an, 16*64*FRACUNIT);

	if (!linetarget)
	    continue;

	P_SpawnMobj (linetarget->x,
		     linetarget->y,
		     linetarget->z + (linetarget->height>>2),
		     MT_EXTRABFG);
	
	damage = 0;
	for (j=0;j<15;j++)
	    damage += (P_Random()&7) + 1;

	P_DamageMobj (linetarget, mo->target,mo->target, damage);
    }
}


//
// A_BFGsound
//
void
A_BFGsound
( player_t*	player,
  pspdef_t*	psp )
{
    S_StartSound (player->mo, sfx_bfg);
}



//
// P_SetupPsprites
// Called at start of level for each player.
//
void P_SetupPsprites (player_t* player) 
{
    int	i;
	
    // remove all psprites
    for (i=0 ; i<NUMPSPRITES ; i++)
	player->psprites[i].state = NULL;
		
    // spawn the gun
    player->pendingweapon = player->readyweapon;
    P_BringUpWeapon (player);
}




//
// P_MovePsprites
// Called every tic by player thinking routine.
//
void P_MovePsprites (player_t* player) 
{
    int		i;
    pspdef_t*	psp;
    state_t*	state;
	
    psp = &player->psprites[0];
    for (i=0 ; i<NUMPSPRITES ; i++, psp++)
    {
	// a null state means not active
	if ( (state = psp->state) )	
	{
	    // drop tic count and possibly change state

	    // a -1 tic count never changes
	    if (psp->tics != -1)	
	    {
		psp->tics--;
		if (!psp->tics)
		    P_SetPsprite (player, i, psp->state->nextstate);
	    }				
	}
    }
    
    player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
    player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}




//
// A_FireOldBFG
// MBF's version of the beta's BFG: a stream of alternating plasma
// fireballs, one cell each, by way of Woof.
//
void
A_FireOldBFG
( player_t*	player,
  pspdef_t*	psp )
{
    int		type = MT_PLASMA1;

    P_SubtractAmmo (player, 1);
    player->extralight = 2;

    do
    {
	mobj_t*		th;
	mobj_t*		mo = player->mo;
	angle_t		an = mo->angle;
	angle_t		an1 = ((P_Random () & 127) - 64) * (ANG90/768) + an;
	angle_t		an2 = ((P_Random () & 127) - 64) * (ANG90/640) + ANG90;
	fixed_t		slope;

	// aimed as the player's missiles are
	slope = P_AimLineAttack (mo, an, 16*64*FRACUNIT);
	if (!linetarget)
	    slope = P_AimLineAttack (mo, an += 1<<26, 16*64*FRACUNIT);
	if (!linetarget)
	    slope = P_AimLineAttack (mo, an -= 2<<26, 16*64*FRACUNIT);
	if (!linetarget)
	    slope = 0, an = mo->angle;
	an1 += an - mo->angle;
	if (slope < 0)
	    an2 -= tantoangle[-slope >> DBITS];
	else
	    an2 += tantoangle[slope >> DBITS];

	th = P_SpawnMobj (mo->x, mo->y,
			  mo->z + 62*FRACUNIT - player->psprites[ps_weapon].sy,
			  type);
	th->target = mo;
	th->angle = an1;
	th->momx = finecosine[an1>>ANGLETOFINESHIFT] * 25;
	th->momy = finesine[an1>>ANGLETOFINESHIFT] * 25;
	th->momz = finetangent[an2>>ANGLETOFINESHIFT] * 25;
	P_CheckMissileSpawn (th);
    }
    while (type != MT_PLASMA2 && (type = MT_PLASMA2));
}


//
// MBF21's weapon code pointers, by way of Woof. Their arguments are the
// weapon frame's args, defaulted by D_FinishDehacked.
//

// a weapon frame, if a patch named one there is
static boolean P_ValidPsprState (int state)
{
    return state >= 0 && state < numstates;
}

// Fire args[0] (a thing, from 1), args[1] degrees aside and args[2] up,
// from args[3] to the side and args[4] up; its tracer is what the player
// is aiming at. No ammunition is taken (A_ConsumeAmmo does that).
void
A_WeaponProjectile
( player_t*	player,
  pspdef_t*	psp )
{
    int		type, angle, pitch, spawnofs_xy, spawnofs_z, an;
    mobj_t*	mo;

    if (!psp->state || !psp->state->args[0])
	return;
    type = psp->state->args[0] - 1;
    if (type < 0 || type >= nummobjtypes)
	return;
    angle = psp->state->args[1];
    pitch = psp->state->args[2];
    spawnofs_xy = psp->state->args[3];
    spawnofs_z = psp->state->args[4];

    mo = P_SpawnPlayerMissile (player->mo, type);
    if (!mo)
	return;

    mo->angle += (angle_t) (((int64_t) angle << 16) / 360);
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul (mo->info->speed, finecosine[an]);
    mo->momy = FixedMul (mo->info->speed, finesine[an]);
    mo->momz += FixedMul (mo->info->speed, P_DegToSlope (pitch));

    an = (player->mo->angle - ANG90) >> ANGLETOFINESHIFT;
    mo->x += FixedMul (spawnofs_xy, finecosine[an]);
    mo->y += FixedMul (spawnofs_xy, finesine[an]);
    mo->z += spawnofs_z;

    mo->tracer = linetarget;
}

// args[2] bullets, spread args[0] across and args[1] up and down, each
// doing args[3] times 1 to args[4]
void
A_WeaponBulletAttack
( player_t*	player,
  pspdef_t*	psp )
{
    int		hspread, vspread, numbullets, damagebase, damagemod;
    int		i, damage, angle, slope;

    if (!psp->state)
	return;
    hspread = psp->state->args[0];
    vspread = psp->state->args[1];
    numbullets = psp->state->args[2];
    damagebase = psp->state->args[3];
    damagemod = psp->state->args[4];
    if (damagemod <= 0)
	damagemod = 1;

    P_BulletSlope (player->mo);
    for (i = 0; i < numbullets; i++)
    {
	damage = (P_Random () % damagemod + 1) * damagebase;
	angle = (int) player->mo->angle + P_RandomHitscanAngle (hspread);
	slope = bulletslope + P_RandomHitscanSlope (vspread);
	P_LineAttack (player->mo, angle, MISSILERANGE, slope, damage);
    }
}

// a blow of args[0] times 1 to args[1], times args[2] (in fixed point)
// when berserk, sound args[3] if it lands, reaching args[4] (the
// player's melee range if 0); the player turns to what it hit
void
A_WeaponMeleeAttack
( player_t*	player,
  pspdef_t*	psp )
{
    int		damagebase, damagemod, zerkfactor, hitsound, range;
    angle_t	angle;
    int		t, slope, damage;

    if (!psp->state)
	return;
    damagebase = psp->state->args[0];
    damagemod = psp->state->args[1];
    zerkfactor = psp->state->args[2];
    hitsound = psp->state->args[3];
    range = psp->state->args[4];
    if (damagemod <= 0)
	damagemod = 1;
    if (range == 0)
	range = player->mo->info->meleerange;

    damage = (P_Random () % damagemod + 1) * damagebase;
    if (player->powers[pw_strength])
	damage = (damage * zerkfactor) >> FRACBITS;

    angle = player->mo->angle;
    t = P_Random ();
    angle += (t - P_Random ()) << 18;

    slope = P_AimLineAttack (player->mo, angle, range);
    P_LineAttack (player->mo, angle, range, slope, damage);

    if (!linetarget)
	return;
    if (hitsound > 0 && hitsound < numsfx)
	S_StartSound (player->mo, hitsound);
    player->mo->angle = R_PointToAngle2 (player->mo->x, player->mo->y,
					 linetarget->x, linetarget->y);
}

// sound args[0], everywhere if args[1]
void
A_WeaponSound
( player_t*	player,
  pspdef_t*	psp )
{
    if (psp->state && psp->state->args[0] > 0
	&& psp->state->args[0] < numsfx)
	S_StartSound (psp->state->args[1] ? NULL : player->mo,
		      psp->state->args[0]);
}

// wake the monsters that hear the player (for a SILENT weapon)
void
A_WeaponAlert
( player_t*	player,
  pspdef_t*	psp )
{
    P_NoiseAlert (player->mo, player->mo);
}

// to frame args[0], with a chance of args[1] in 256
void
A_WeaponJump
( player_t*	player,
  pspdef_t*	psp )
{
    if (psp->state && P_Random () < psp->state->args[1]
	&& P_ValidPsprState (psp->state->args[0]))
	P_SetPsprite (player, psp - player->psprites, psp->state->args[0]);
}

// take args[0] ammunition, or the weapon's Ammo per shot if 0
void
A_ConsumeAmmo
( player_t*	player,
  pspdef_t*	psp )
{
    int		amount;
    ammotype_t	type = weaponinfo[player->readyweapon].ammo;

    if (!psp->state || type == am_noammo)
	return;
    amount = psp->state->args[0] ? psp->state->args[0]
			      : weaponinfo[player->readyweapon].ammopershot;
    if (player->ammo[type] >= amount)
	player->ammo[type] -= amount;
    else
	player->ammo[type] = 0;
}

// to frame args[0] if there is less ammunition than args[1] (Ammo per
// shot if 0)
void
A_CheckAmmo
( player_t*	player,
  pspdef_t*	psp )
{
    int		amount;
    ammotype_t	type = weaponinfo[player->readyweapon].ammo;

    if (!psp->state || type == am_noammo)
	return;
    amount = psp->state->args[1] ? psp->state->args[1]
			      : weaponinfo[player->readyweapon].ammopershot;
    if (player->ammo[type] < amount && P_ValidPsprState (psp->state->args[0]))
	P_SetPsprite (player, psp - player->psprites, psp->state->args[0]);
}

// to frame args[0] while fire is held (and, unless args[1], there is
// ammunition)
void
A_RefireTo
( player_t*	player,
  pspdef_t*	psp )
{
    if (!psp->state || !P_ValidPsprState (psp->state->args[0]))
	return;
    if ((psp->state->args[1] || P_CheckAmmo (player))
	&& (player->cmd.buttons & BT_ATTACK)
	&& player->pendingweapon == wp_nochange && player->health)
	P_SetPsprite (player, psp - player->psprites, psp->state->args[0]);
}

// the flash to frame args[0]; the player's attack frame too, unless args[1]
void
A_GunFlashTo
( player_t*	player,
  pspdef_t*	psp )
{
    if (!psp->state || !P_ValidPsprState (psp->state->args[0]))
	return;
    if (!psp->state->args[1])
	P_SetMobjState (player->mo, S_PLAY_ATK2);
    P_SetPsprite (player, ps_flash, psp->state->args[0]);
}
