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
//   All the global variables that store the internal state.
//   Theoretically speaking, the internal state of the engine
//    should be found by looking at the variables collected
//    here, and every relevant module will have to include
//    this header file.
//   In practice, things are a bit messy.
//
//-----------------------------------------------------------------------------


#ifndef __D_STATE__
#define __D_STATE__

// We need globally shared data structures,
//  for defining the global state variables.
#include "doomdata.h"
#include "d_net.h"

// We need the playr data structure as well.
#include "d_player.h"


#ifdef __GNUG__
#pragma interface
#endif



// ------------------------
// Command line parameters.
//
extern  boolean	nomonsters;	// checkparm of -nomonsters
extern  boolean	respawnparm;	// checkparm of -respawn
extern  boolean	fastparm;	// checkparm of -fast

extern  boolean	devparm;	// DEBUG: launched with -devparm



// -----------------------------------------------------
// Game Mode - identify IWAD as shareware, retail etc.
//
// How the level being played behaves (P_SetCompatibility, p_setup.c).
//
// demo_compatibility: exactly as DOOM did, for a map of its own -- and its
// demos, which only play back if nothing changes. False for a map made for
// Boom or later (its line or sector types, or a COMPLVL lump saying so),
// which then has Boom's specials and the fixes the comp flags leave on.
// Named as Boom and the ports after it name it, so their code reads the
// same here.
extern boolean	demo_compatibility;

// The same as the version a port's demo of it would be: DV_VANILLA for a
// map of DOOM's own, else the level of the extensions it was made for
// (P_SetCompatibility), which the code taken from Boom's descendants tests
// as they do. mbf21: at least MBF21.
#define DV_VANILLA	109
#define DV_BOOM		202
#define DV_MBF		203
#define DV_MBF21	221
#define DV_ID24		224
extern int	demo_version;
extern boolean	mbf21;

// The fixes to DOOM's behaviour Boom and MBF made, each one off (0) to have
// the fix, as MBF21 sets them; all on under demo_compatibility.
enum
{
    comp_telefrag,
    comp_dropoff,
    comp_vile,
    comp_pain,
    comp_skull,
    comp_blazing,
    comp_doorlight,
    comp_model,
    comp_god,
    comp_falloff,
    comp_floors,
    comp_skymap,
    comp_pursuit,
    comp_doorstuck,
    comp_staylift,
    comp_zombie,
    comp_stairs,
    comp_infcheat,
    comp_zerotags,
    comp_respawn,
    comp_soul,
    comp_ledgeblock,
    comp_friendlyspawn,
    comp_voodooscroller,
    comp_reservedlineflag,
    COMP_TOTAL
};
extern int	comp[COMP_TOTAL];

extern GameMode_t	gamemode;
extern GameMission_t	gamemission;

// Set if homebrew PWAD stuff has been added.
extern  boolean	modifiedgame;

// No Rest for the Living (nerve.wad) is loaded over DOOM II. Its nine maps
// follow the expansion's own rules, as the BFG Edition plays them: see
// nerveinfo in u_mapinfo.c.
extern  boolean	nervepack;


// -------------------------------------------
// Language.
extern  Language_t   language;


// -------------------------------------------
// Selected skill type, map etc.
//

// Defaults for menu, methinks.
extern  skill_t		startskill;
extern  int             startepisode;
extern	int		startmap;

extern  boolean		autostart;

// Selected by user. 
extern  skill_t         gameskill;
extern  int		gameepisode;
extern  int		gamemap;

// Nightmare mode flag, single player.
extern  boolean         respawnmonsters;

// Netgame? Only true if >1 player.
extern  boolean	netgame;

// Flag: true only if started as net deathmatch.
// An enum might handle altdeath/cooperative better.
extern  boolean	deathmatch;	
	
// -------------------------
// Internal parameters for sound rendering.
// These have been taken from the DOS version,
//  but are not (yet) supported with Linux
//  (e.g. no sound volume adjustment with menu.

// These are not used, but should be (menu).
// From m_menu.c:
//  Sound FX volume has default, 0 - 15
//  Music volume has default, 0 - 15
// These are multiplied by 8.
extern int snd_SfxVolume;      // maximum volume for sound
extern int snd_MusicVolume;    // maximum volume for music

// Current music/sfx card - index useless
//  w/o a reference LUT in a sound module.
// Ideally, this would use indices found
//  in: /usr/include/linux/soundcard.h
extern int snd_MusicDevice;
extern int snd_SfxDevice;
// Config file? Same disclaimer as above.
extern int snd_DesiredMusicDevice;
extern int snd_DesiredSfxDevice;


// -------------------------
// Status flags for refresh.
//

// Depending on view size - no status bar?
// Note that there is no way to disable the
//  status bar explicitely.
extern  boolean statusbaractive;

extern  boolean automapactive;	// In AutoMap mode?
extern  boolean	menuactive;	// Menu overlayed?
extern  boolean	paused;		// Game Pause?


extern  boolean		viewactive;

extern  boolean		nodrawers;
extern  boolean		noblit;

extern	int		viewwindowx;
extern	int		viewwindowy;
extern	int		viewheight;
extern	int		viewwidth;
extern	int		scaledviewwidth;






// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
extern  int	viewangleoffset;

// Player taking events, and displaying.
extern  int	consoleplayer;	
extern  int	displayplayer;


// -------------------------------------
// Scores, rating.
// Statistics on a given map, for intermission.
//
extern  int	totalkills;
extern	int	totalitems;
extern	int	totalsecret;

// Timer, for scores.
extern  int	levelstarttic;	// gametic at level start
extern  int	leveltime;	// tics in game play for par



// --------------------------------------
// DEMO playback/recording related stuff.
// No demo, there is a human player in charge?
// Disable save/end game?
extern  boolean	usergame;

//?
extern  boolean	demoplayback;
extern  boolean	demorecording;

// Quit after playing a demo from cmdline.
extern  boolean		singledemo;	




//?
extern  gamestate_t     gamestate;






//-----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
//  according to user inputs. Partly load from
//  WAD, partly set at startup time.



extern	int		gametic;


// Bookkeeping on players - state.
extern	player_t	players[MAXPLAYERS];

// Alive? Disconnected?
extern  boolean		playeringame[MAXPLAYERS];


// Player spawn spots for deathmatch.
// grows past id's 10; p_mobj.c
extern  mapthing_t*     deathmatchstarts;
extern  mapthing_t*	deathmatch_p;
extern  int		maxdeathmatchstarts;

// Player spawn spots.
extern  mapthing_t      playerstarts[MAXPLAYERS];

// Intermission stats.
// Parameters for world map / intermission.
extern  wbstartstruct_t		wminfo;	


// LUT of ammunition limits for each kind.
// This doubles with BackPack powerup item.
extern  int		maxammo[NUMAMMO];





//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
extern	char		basedefault[1024];
extern  FILE*		debugfile;

// if true, load all graphics at level load
extern  boolean         precache;


// wipegamestate can be set to -1
//  to force a wipe on the next draw
extern  gamestate_t     wipegamestate;

extern  int             mouseSensitivity;
//?
// debug flag to cancel adaptiveness
extern  boolean         singletics;	

extern  int             bodyqueslot;



// Needed to store the number of the dummy sky flat.
// Used for rendering,
//  as well as tracking projectiles etc.
extern int		skyflatnum;

// Other flats that stand for a sky, by flat number: the sky texture each
// shows, or -1. The 2024 re-release's SKYDEFS lump makes these; see r_sky.c.
extern int*		flatskytexture;

// Whether a flat is sky: F_SKY1, showing the map's sky, or one of those.
#define R_IsSkyFlat(pic) \
	((pic) & PL_SKYFLAT || (pic) == skyflatnum || flatskytexture[pic] >= 0)

// A visplane's picnum for a sky MBF's 271 or 272 transferred: the line's
// number, with this bit (r_plane.c).
#define PL_SKYFLAT		0x40000000



// Netgame stuff (buffers and pointers, i.e. indices).

// This is ???
extern  doomcom_t*	doomcom;

// This points inside doomcom.
extern  doomdata_t*	netbuffer;	


extern  ticcmd_t	localcmds[BACKUPTICS];
extern	int		rndindex;

extern	int		maketic;
extern  int             nettics[MAXNETNODES];

extern  ticcmd_t        netcmds[MAXPLAYERS][BACKUPTICS];
extern	int		ticdup;



#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
