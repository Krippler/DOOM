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
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: g_game.c,v 1.8 1997/02/03 22:45:09 b1 Exp $";

#include <string.h>
#include <stdlib.h>

#include "doomdef.h" 
#include "doomstat.h"

#include "z_zone.h"
#include "f_finale.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "i_system.h"

#include "p_setup.h"
#include "p_saveg.h"
#include "p_tick.h"

#include "d_main.h"

#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"

// Needs access to LFB.
#include "v_video.h"

#include "w_wad.h"

#include "p_local.h" 

#include "s_sound.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

// SKY handling - still the wrong place.
#include "r_data.h"
#include "r_sky.h"



#include "i_pad.h"
#include "u_mapinfo.h"
#include "g_game.h"


// 180K in 1994, and taken from the screen buffers, past the end of which
// a bigger map's savegame went on writing before this was checked. Its own
// buffer now, and room for any map there is.
#define SAVEGAMESIZE	(8*1024*1024)
#define SAVESTRINGSIZE	24



boolean	G_CheckDemoStatus (void); 
void	G_ReadDemoTiccmd (ticcmd_t* cmd); 
void	G_WriteDemoTiccmd (ticcmd_t* cmd); 
void	G_PlayerReborn (int player); 
void	G_InitNew (skill_t skill, int episode, int map); 
 
void	G_DoReborn (int playernum); 
 
void	G_DoLoadLevel (void); 
void	G_DoNewGame (void); 
void	G_DoLoadGame (void); 
void	G_DoPlayDemo (void); 
void	G_DoCompleted (void); 
void	G_DoVictory (void); 
void	G_DoWorldDone (void); 
void	G_DoSaveGame (void); 
 
 
gameaction_t    gameaction; 
gamestate_t     gamestate; 
skill_t         gameskill; 
boolean		respawnmonsters;
int             gameepisode; 
int             gamemap; 
 
boolean         paused; 
boolean         sendpause;             	// send a pause event next tic 
boolean         sendsave;             	// send a save event next tic 
boolean         usergame;               // ok to save / end game 
 
boolean         timingdemo;             // if true, exit with report on completion 
boolean         nodrawers;              // for comparative timing purposes 
boolean         noblit;                 // for comparative timing purposes 
int             starttime;          	// for comparative timing purposes  	 
 
boolean         viewactive; 
 
boolean         deathmatch;           	// only if started as net death 
boolean         netgame;                // only true if packets are broadcast 
boolean         playeringame[MAXPLAYERS]; 
player_t        players[MAXPLAYERS]; 
 
int             consoleplayer;          // player taking events and displaying 
int             displayplayer;          // view being displayed 
int             gametic; 
int             levelstarttic;          // gametic at level start 
int             totalkills, totalitems, totalsecret;    // for intermission 
 
char            demoname[32]; 
boolean         demorecording; 
boolean         demoplayback; 
boolean		netdemo; 
byte*		demobuffer;
byte*		demo_p;
byte*		demoend; 
boolean         singledemo;            	// quit after playing a demo from cmdline 
 
boolean         precache = true;        // if true, load all graphics at start 
 
wbstartstruct_t wminfo;               	// parms for world map / intermission 
 
short		consistancy[MAXPLAYERS][BACKUPTICS]; 
 
byte*		savebuffer;
 
 
// 
// controls (have defaults) 
// 
int             key_right;
int		key_left;

int		key_up;
int		key_down; 
int             key_strafeleft;
int		key_straferight; 
int             key_fire;
int		key_use;
int		key_strafe;
int		key_speed; 

// A second key that opens the menu, for when Escape cannot be used.
// Browsers hand Escape to themselves while the pointer is captured, so
// pressing it lets the mouse go instead of reaching the game. Backquote is
// the default because nothing else in the game uses it and no browser claims
// it; Escape is still read directly, so this only ever adds a key.
int		key_menu = '`';

// Next and previous weapon, which 1997 DOOM did not have: the wheel, by
// default. The controller has its own, KEY_WEAPNEXT and KEY_WEAPPREV.
int		key_nextweapon = KEY_MWHEELUP;
int		key_prevweapon = KEY_MWHEELDOWN;

// Asked for since the last tic: 1 next, -1 previous, 0 neither.
static int	weaponcycle;

// Whether pushing the mouse forward walks the player forward.
//
// The original used the mouse's Y axis for movement, because there was
// nothing to aim vertically at. Every later port made that optional and
// most default it off: with mouse look for turning, the same hand moving
// up and down walks you about by accident.
int		novert = 1;
 
int             mousebfire; 
int             mousebstrafe; 
int             mousebforward; 
 
int             joybfire; 
int             joybstrafe; 
int             joybuse; 
int             joybspeed; 
 
 
 
#define MAXPLMOVE		(forwardmove[1]) 
 
#define TURBOTHRESHOLD	0x32

fixed_t		forwardmove[2] = {0x19, 0x32}; 
fixed_t		sidemove[2] = {0x18, 0x28}; 
fixed_t		angleturn[3] = {640, 1280, 320};	// + slow turn 

#define SLOWTURNTICS	6 
 
#define NUMKEYS		256 

boolean         gamekeydown[NUMKEYS]; 

//
// Is this key held?
//
// Asked with whatever is in key_fire and the rest, which come straight out of
// .doomrc through sscanf("%i") with no range check of any kind -- and which the
// Controls menu could until recently set to any keysym X handed it. Anything
// outside the array is not a key anybody can press: G_Responder below only
// records 0..NUMKEYS-1, so the rest read memory that belongs to something else.
// "key_fire 2000000000" in a config file was a reproducible segmentation fault
// on the first tic of play, every time.
//
static boolean keyheld (int key)
{
    return key >= 0 && key < NUMKEYS && gamekeydown[key];
}
int             turnheld;				// for accelerative turning 
 
boolean		mousearray[4]; 
boolean*	mousebuttons = &mousearray[1];		// allow [-1]

// mouse values are used once 
int             mousex;
int		mousey;         

int             dclicktime;
int		dclickstate;
int		dclicks; 
int             dclicktime2;
int		dclickstate2;
int		dclicks2;

// joystick values are repeated 
int             joyxmove;
int		joyymove;
boolean         joyarray[5]; 
boolean*	joybuttons = &joyarray[1];		// allow [-1] 
 
int		savegameslot; 
char		savedescription[32]; 
 
 
#define	BODYQUESIZE	32

mobj_t*		bodyque[BODYQUESIZE]; 
int		bodyqueslot; 
 
void*		statcopy;				// for statistics driver
 
 
 
int G_CmdChecksum (ticcmd_t* cmd) 
{ 
    int		i;
    int		sum = 0; 
	 
    for (i=0 ; i< sizeof(*cmd)/4 - 1 ; i++) 
	sum += ((int *)cmd)[i]; 
		 
    return sum; 
} 
 

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer. 
// If recording a demo, write it out 
// 
void G_BuildTiccmd (ticcmd_t* cmd) 
{ 
    int		i; 
    boolean	strafe;
    boolean	bstrafe; 
    int		speed;
    int		tspeed; 
    int		forward;
    int		side;
    
    ticcmd_t*	base;

    base = I_BaseTiccmd ();		// empty, or external driver
    memcpy (cmd,base,sizeof(*cmd)); 
	
    cmd->consistancy = 
	consistancy[consoleplayer][maketic%BACKUPTICS]; 

 
    strafe = keyheld(key_strafe) || mousebuttons[mousebstrafe] 
	|| joybuttons[joybstrafe]; 
    speed = keyheld(key_speed) || joybuttons[joybspeed];
 
    forward = side = 0;
    
    // use two stage accelerative turning
    // on the keyboard and joystick
    if (joyxmove < 0
	|| joyxmove > 0  
	|| keyheld(key_right)
	|| keyheld(key_left)) 
	turnheld += ticdup; 
    else 
	turnheld = 0; 

    if (turnheld < SLOWTURNTICS) 
	tspeed = 2;             // slow turn 
    else 
	tspeed = speed;
    
    // let movement keys cancel each other out
    if (strafe) 
    { 
	if (keyheld(key_right)) 
	{
	    // fprintf(stderr, "strafe right\n");
	    side += sidemove[speed]; 
	}
	if (keyheld(key_left)) 
	{
	    //	fprintf(stderr, "strafe left\n");
	    side -= sidemove[speed]; 
	}
	if (joyxmove > 0) 
	    side += sidemove[speed]; 
	if (joyxmove < 0) 
	    side -= sidemove[speed]; 
 
    } 
    else 
    { 
	if (keyheld(key_right)) 
	    cmd->angleturn -= angleturn[tspeed]; 
	if (keyheld(key_left)) 
	    cmd->angleturn += angleturn[tspeed]; 
	if (joyxmove > 0) 
	    cmd->angleturn -= angleturn[tspeed]; 
	if (joyxmove < 0) 
	    cmd->angleturn += angleturn[tspeed]; 
    } 
 
    if (keyheld(key_up)) 
    {
	// fprintf(stderr, "up\n");
	forward += forwardmove[speed]; 
    }
    if (keyheld(key_down)) 
    {
	// fprintf(stderr, "down\n");
	forward -= forwardmove[speed]; 
    }
    if (joyymove < 0) 
	forward += forwardmove[speed]; 
    if (joyymove > 0) 
	forward -= forwardmove[speed]; 
    if (keyheld(key_straferight)) 
	side += sidemove[speed]; 
    if (keyheld(key_strafeleft)) 
	side -= sidemove[speed];
    
    // buttons
    cmd->chatchar = HU_dequeueChatChar(); 
 
    if (keyheld(key_fire) || mousebuttons[mousebfire] 
	|| joybuttons[joybfire]) 
	cmd->buttons |= BT_ATTACK; 
 
    if (keyheld(key_use) || joybuttons[joybuse] ) 
    { 
	cmd->buttons |= BT_USE;
	// clear double clicks if hit use button 
	dclicks = 0;                   
    } 

    // chainsaw overrides
    for (i=0 ; i<NUMWEAPONS-1 ; i++)
	if (gamekeydown['1'+i])
	{
	    cmd->buttons |= BT_CHANGE;
	    cmd->buttons |= i<<BT_WEAPONSHIFT;
	    break;
	}

    // next or previous, when no number key has said which
    if (weaponcycle && !(cmd->buttons & BT_CHANGE))
    {
	int	slot = G_CycleWeapon (weaponcycle);

	if (slot >= 0)
	    cmd->buttons |= BT_CHANGE | slot<<BT_WEAPONSHIFT;
    }
    weaponcycle = 0;
    
    // mouse
    if (mousebuttons[mousebforward]) 
	forward += forwardmove[speed];
    
    // forward double click
    if (mousebuttons[mousebforward] != dclickstate && dclicktime > 1 ) 
    { 
	dclickstate = mousebuttons[mousebforward]; 
	if (dclickstate) 
	    dclicks++; 
	if (dclicks == 2) 
	{ 
	    cmd->buttons |= BT_USE; 
	    dclicks = 0; 
	} 
	else 
	    dclicktime = 0; 
    } 
    else 
    { 
	dclicktime += ticdup; 
	if (dclicktime > 20) 
	{ 
	    dclicks = 0; 
	    dclickstate = 0; 
	} 
    }
    
    // strafe double click
    bstrafe =
	mousebuttons[mousebstrafe] 
	|| joybuttons[joybstrafe]; 
    if (bstrafe != dclickstate2 && dclicktime2 > 1 ) 
    { 
	dclickstate2 = bstrafe; 
	if (dclickstate2) 
	    dclicks2++; 
	if (dclicks2 == 2) 
	{ 
	    cmd->buttons |= BT_USE; 
	    dclicks2 = 0; 
	} 
	else 
	    dclicktime2 = 0; 
    } 
    else 
    { 
	dclicktime2 += ticdup; 
	if (dclicktime2 > 20) 
	{ 
	    dclicks2 = 0; 
	    dclickstate2 = 0; 
	} 
    } 
 
    // A controller's sticks, as speeds rather than keys.
    I_PadTiccmd (cmd, &forward, &side, speed);

    if (!novert) 
	forward += mousey; 
    if (strafe) 
	side += mousex*2; 
    else 
	cmd->angleturn -= mousex*0x8; 

    mousex = mousey = 0; 
	 
    if (forward > MAXPLMOVE) 
	forward = MAXPLMOVE; 
    else if (forward < -MAXPLMOVE) 
	forward = -MAXPLMOVE; 
    if (side > MAXPLMOVE) 
	side = MAXPLMOVE; 
    else if (side < -MAXPLMOVE) 
	side = -MAXPLMOVE; 
 
    cmd->forwardmove += forward; 
    cmd->sidemove += side;
    
    // special buttons
    if (sendpause) 
    { 
	sendpause = false; 
	cmd->buttons = BT_SPECIAL | BTS_PAUSE; 
    } 
 
    if (sendsave) 
    { 
	sendsave = false; 
	cmd->buttons = BT_SPECIAL | BTS_SAVEGAME | (savegameslot<<BTS_SAVESHIFT); 
    } 
} 
 

//
// G_MapExists
// Whether the game data has a map, by its lump.
//
boolean G_MapExists (int episode, int map)
{
    char	name[9];

    U_MapName (name, episode, map);
    return W_CheckNumForName (name) >= 0;
}


//
// G_Ending
// How the game ends after the map being played, if it does: as its UMAPINFO
// says, else after an ExM8 in DOOM and MAP30 in DOOM II.
//
umending_t G_Ending (void)
{
    umapentry_t*	map = U_ThisMap ();

    if (map && map->ending != UM_END_UNSET)
	return map->ending;
    if (gamemode == commercial)
	return gamemap == 30 ? UM_END_DEFAULT : UM_END_NONE;
    return gamemap == 8 ? UM_END_DEFAULT : UM_END_NONE;
}


//
// G_SetSky
// The sky of the map about to be played: the one its UMAPINFO names, or the
// game's own -- by episode in DOOM, by map number in DOOM II.
//
static void G_SetSky (void)
{
    umapentry_t*	map = U_ThisMap ();
    char		name[9];

    if (map && map->skytexture[0]
	&& R_CheckTextureNumForName (map->skytexture) >= 0)
    {
	skytexture = R_TextureNumForName (map->skytexture);
	return;
    }

    if (gamemode == commercial)
	strcpy (name, gamemap < 12 ? "SKY1" : gamemap < 21 ? "SKY2" : "SKY3");
    else
    {
	// SKY1 to SKY4 for the four episodes id made; one past them takes
	// the sky of its number when the game data has one.
	sprintf (name, "SKY%d", gameepisode);
	if (R_CheckTextureNumForName (name) < 0)
	    strcpy (name, "SKY1");
    }
    skytexture = R_TextureNumForName (name);
}


//
// G_CycleWeapon
// The weapon after, or before, the one in hand or on its way, in the order
// the number keys give them -- fist, chainsaw, pistol, shotgun, super
// shotgun, chaingun, rocket launcher, plasma rifle, BFG -- passing over any
// not owned or without the ammunition to fire. Returned as the number key's
// slot, which is all a tic command can carry: the fist and the chainsaw
// share 1, and the shotgun and the super shotgun 3, and P_PlayerThink
// chooses between them as it does for the keys. That means the shotgun is
// reached going back from the super shotgun, not forward from the pistol,
// when both are owned. -1 when nothing else can be chosen.
//
static const struct { weapontype_t weapon; int slot; } weaponorder[] =
{
    {wp_fist, 0}, {wp_chainsaw, 0}, {wp_pistol, 1}, {wp_shotgun, 2},
    {wp_supershotgun, 2}, {wp_chaingun, 3}, {wp_missile, 4},
    {wp_plasma, 5}, {wp_bfg, 6}
};

#define NUMWEAPONORDER	(int)(sizeof(weaponorder)/sizeof(*weaponorder))

static boolean G_WeaponSelectable (player_t* p, weapontype_t w)
{
    ammotype_t	ammo = weaponinfo[w].ammo;
    int		need = w == wp_bfg ? 40 : w == wp_supershotgun ? 2 : 1;

    if (!p->weaponowned[w])
	return false;
    if (w == wp_supershotgun && gamemode != commercial)
	return false;
    // 1 gives the chainsaw when there is one, unless berserk and holding it
    if (w == wp_fist && p->weaponowned[wp_chainsaw] && !p->powers[pw_strength])
	return false;
    return ammo == am_noammo || p->ammo[ammo] >= need;
}

int G_CycleWeapon (int dir)
{
    player_t*		p = &players[consoleplayer];
    weapontype_t	now;
    int			i;
    int			j;

    now = p->pendingweapon != wp_nochange ? p->pendingweapon : p->readyweapon;
    for (i = 0; i < NUMWEAPONORDER; i++)
	if (weaponorder[i].weapon == now)
	    break;
    if (i == NUMWEAPONORDER)
	i = 0;

    for (j = 1; j < NUMWEAPONORDER; j++)
    {
	int	k = (i + dir * j + NUMWEAPONORDER * j) % NUMWEAPONORDER;

	if (G_WeaponSelectable (p, weaponorder[k].weapon))
	    return weaponorder[k].slot;
    }
    return -1;
}


//
// G_DoLoadLevel
//
extern  gamestate_t     wipegamestate;
 
void G_DoLoadLevel (void) 
{ 
    int             i; 

    // Set the sky map.
    // First thing, we have a dummy sky texture name,
    //  a flat. The data is in the WAD only because
    //  we look for an actual index, instead of simply
    //  setting one.
    skyflatnum = R_FlatNumForName ( SKYFLATNAME );

    // Here for every game, not only DOOM II: a UMAPINFO exit can lead into
    // another episode, and so under another sky.
    G_SetSky ();

    levelstarttic = gametic;        // for time calculation
    
    if (wipegamestate == GS_LEVEL) 
	wipegamestate = -1;             // force a wipe 

    gamestate = GS_LEVEL; 

    for (i=0 ; i<MAXPLAYERS ; i++) 
    { 
	if (playeringame[i] && players[i].playerstate == PST_DEAD) 
	    players[i].playerstate = PST_REBORN; 
	memset (players[i].frags,0,sizeof(players[i].frags)); 
    } 
		 
    P_SetupLevel (gameepisode, gamemap, 0, gameskill);    
    displayplayer = consoleplayer;		// view the guy you are playing    
    starttime = I_GetTime (); 
    gameaction = ga_nothing; 
    Z_CheckHeap ();
    
    // clear cmd building stuff
    memset (gamekeydown, 0, sizeof(gamekeydown));
    weaponcycle = 0; 
    joyxmove = joyymove = 0; 
    mousex = mousey = 0; 
    sendpause = sendsave = paused = false; 
    memset (mousebuttons, 0, sizeof(mousebuttons)); 
    memset (joybuttons, 0, sizeof(joybuttons)); 
} 
 
 
//
// G_Responder  
// Get info needed to make ticcmd_ts for the players.
// 
boolean G_Responder (event_t* ev) 
{ 
    // allow spy mode changes even during the demo
    if (gamestate == GS_LEVEL && ev->type == ev_keydown 
	&& ev->data1 == KEY_F12 && (singledemo || !deathmatch) )
    {
	// spy mode 
	do 
	{ 
	    displayplayer++; 
	    if (displayplayer == MAXPLAYERS) 
		displayplayer = 0; 
	} while (!playeringame[displayplayer] && displayplayer != consoleplayer); 
	return true; 
    }
    
    // any other key pops up menu if in demos
    if (gameaction == ga_nothing && !singledemo && 
	(demoplayback || gamestate == GS_DEMOSCREEN) 
	) 
    { 
	if (ev->type == ev_keydown ||  
	    (ev->type == ev_mouse && ev->data1) || 
	    (ev->type == ev_joystick && ev->data1) ) 
	{ 
	    M_StartControlPanel (); 
	    return true; 
	} 
	return false; 
    } 
 
    if (gamestate == GS_LEVEL) 
    { 
#if 0 
	if (devparm && ev->type == ev_keydown && ev->data1 == ';') 
	{ 
	    G_DeathMatchSpawnPlayer (0); 
	    return true; 
	} 
#endif 
	if (HU_Responder (ev)) 
	    return true;	// chat ate the event 
	if (ST_Responder (ev)) 
	    return true;	// status window ate it 
	if (AM_Responder (ev)) 
	    return true;	// automap ate it 
    } 
	 
    if (gamestate == GS_FINALE) 
    { 
	if (F_Responder (ev)) 
	    return true;	// finale ate the event 
    } 
	 
    switch (ev->type) 
    { 
      case ev_keydown:
	if (ev->data1 == KEY_PAUSE)
	{
	    sendpause = true;
	    return true;
	}
	if (gamestate == GS_LEVEL
	    && (ev->data1 == key_nextweapon || ev->data1 == KEY_WEAPNEXT))
	{
	    weaponcycle = 1;
	    return true;
	}
	if (gamestate == GS_LEVEL
	    && (ev->data1 == key_prevweapon || ev->data1 == KEY_WEAPPREV))
	{
	    weaponcycle = -1;
	    return true;
	} 
	if (ev->data1 <NUMKEYS) 
	    gamekeydown[ev->data1] = true; 
	return true;    // eat key down events 
 
      case ev_keyup: 
	if (ev->data1 <NUMKEYS) 
	    gamekeydown[ev->data1] = false; 
	return false;   // always let key up events filter down 
		 
      case ev_mouse: 
	mousebuttons[0] = ev->data1 & 1; 
	mousebuttons[1] = ev->data1 & 2; 
	mousebuttons[2] = ev->data1 & 4; 
	// Accumulate, do not assign. Every mouse event that arrives inside one
	// tic contributes; the pair is zeroed in G_BuildTiccmd once it has been
	// turned into a command.
	//
	// Assigning meant only the last event of the tic counted and the rest
	// were dropped. A mouse reporting faster than the 35Hz tic -- which is
	// all of them, and especially a pointer-locked browser sending one
	// event per report -- had most of its movement thrown away, leaving a
	// twitch in place of a turn.
	mousex += ev->data2*(mouseSensitivity+5)/10; 
	mousey += ev->data3*(mouseSensitivity+5)/10; 
	return true;    // eat events 
 
      case ev_joystick: 
	joybuttons[0] = ev->data1 & 1; 
	joybuttons[1] = ev->data1 & 2; 
	joybuttons[2] = ev->data1 & 4; 
	joybuttons[3] = ev->data1 & 8; 
	joyxmove = ev->data2; 
	joyymove = ev->data3; 
	return true;    // eat events 
 
      default: 
	break; 
    } 
 
    return false; 
} 
 
 
 
//
// G_Ticker
// Make ticcmd_ts for the players.
//
void G_Ticker (void) 
{ 
    int		i;
    int		buf; 
    ticcmd_t*	cmd;
    
    // do player reborns if needed
    for (i=0 ; i<MAXPLAYERS ; i++) 
	if (playeringame[i] && players[i].playerstate == PST_REBORN) 
	    G_DoReborn (i);
    
    // do things to change the game state
    while (gameaction != ga_nothing) 
    { 
	switch (gameaction) 
	{ 
	  case ga_loadlevel: 
	    G_DoLoadLevel (); 
	    break; 
	  case ga_newgame: 
	    G_DoNewGame (); 
	    break; 
	  case ga_loadgame: 
	    G_DoLoadGame (); 
	    break; 
	  case ga_savegame: 
	    G_DoSaveGame (); 
	    break; 
	  case ga_playdemo: 
	    G_DoPlayDemo (); 
	    break; 
	  case ga_completed: 
	    G_DoCompleted (); 
	    break; 
	  case ga_victory: 
	    F_StartFinale (); 
	    break; 
	  case ga_worlddone: 
	    G_DoWorldDone (); 
	    break; 
	  case ga_screenshot: 
	    M_ScreenShot (); 
	    gameaction = ga_nothing; 
	    break; 
	  case ga_nothing: 
	    break; 
	} 
    }
    
    // get commands, check consistancy,
    // and build new consistancy check
    buf = (gametic/ticdup)%BACKUPTICS; 
 
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i]) 
	{ 
	    cmd = &players[i].cmd; 
 
	    memcpy (cmd, &netcmds[i][buf], sizeof(ticcmd_t)); 
 
	    if (demoplayback) 
		G_ReadDemoTiccmd (cmd); 
	    if (demorecording) 
		G_WriteDemoTiccmd (cmd);
	    
	    // check for turbo cheats
	    if (cmd->forwardmove > TURBOTHRESHOLD 
		&& !(gametic&31) && ((gametic>>5)&3) == i )
	    {
		static char turbomessage[80];
		extern char *player_names[4];
		sprintf (turbomessage, "%s is turbo!",player_names[i]);
		players[consoleplayer].message = turbomessage;
	    }
			
	    if (netgame && !netdemo && !(gametic%ticdup) ) 
	    { 
		if (gametic > BACKUPTICS 
		    && consistancy[i][buf] != cmd->consistancy) 
		{ 
		    I_Error ("consistency failure (%i should be %i)",
			     cmd->consistancy, consistancy[i][buf]); 
		} 
		if (players[i].mo) 
		    consistancy[i][buf] = players[i].mo->x; 
		else 
		    consistancy[i][buf] = rndindex; 
	    } 
	}
    }
    
    // check for special buttons
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i]) 
	{ 
	    if (players[i].cmd.buttons & BT_SPECIAL) 
	    { 
		switch (players[i].cmd.buttons & BT_SPECIALMASK) 
		{ 
		  case BTS_PAUSE: 
		    paused ^= 1; 
		    if (paused) 
			S_PauseSound (); 
		    else 
			S_ResumeSound (); 
		    break; 
					 
		  case BTS_SAVEGAME: 
		    if (!savedescription[0]) 
			strcpy (savedescription, "NET GAME"); 
		    savegameslot =  
			(players[i].cmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT; 
		    gameaction = ga_savegame; 
		    break; 
		} 
	    } 
	}
    }
    
    // do main actions
    switch (gamestate) 
    { 
      case GS_LEVEL: 
	P_Ticker (); 
	ST_Ticker (); 
	AM_Ticker (); 
	HU_Ticker ();            
	break; 
	 
      case GS_INTERMISSION: 
	WI_Ticker (); 
	break; 
			 
      case GS_FINALE: 
	F_Ticker (); 
	break; 
 
      case GS_DEMOSCREEN: 
	D_PageTicker (); 
	break; 
    }        
} 
 
 
//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_InitPlayer 
// Called at the start.
// Called by the game initialization functions.
//
void G_InitPlayer (int player) 
{ 
    player_t*	p; 
 
    // set up the saved info         
    p = &players[player]; 
	 
    // clear everything else to defaults 
    G_PlayerReborn (player); 
	 
} 
 
 

//
// G_PlayerFinishLevel
// Can when a player completes a level.
//
void G_PlayerFinishLevel (int player) 
{ 
    player_t*	p; 
	 
    p = &players[player]; 
	 
    memset (p->powers, 0, sizeof (p->powers)); 
    memset (p->cards, 0, sizeof (p->cards)); 
    p->mo->flags &= ~MF_SHADOW;		// cancel invisibility 
    p->extralight = 0;			// cancel gun flashes 
    p->fixedcolormap = 0;		// cancel ir gogles 
    p->damagecount = 0;			// no palette changes 
    p->bonuscount = 0; 
} 
 

//
// G_PlayerReborn
// Called after a player dies 
// almost everything is cleared and initialized 
//
void G_PlayerReborn (int player) 
{ 
    player_t*	p; 
    int		i; 
    int		frags[MAXPLAYERS]; 
    int		killcount;
    int		itemcount;
    int		secretcount; 
	 
    memcpy (frags,players[player].frags,sizeof(frags)); 
    killcount = players[player].killcount; 
    itemcount = players[player].itemcount; 
    secretcount = players[player].secretcount; 
	 
    p = &players[player]; 
    memset (p, 0, sizeof(*p)); 
 
    memcpy (players[player].frags, frags, sizeof(players[player].frags)); 
    players[player].killcount = killcount; 
    players[player].itemcount = itemcount; 
    players[player].secretcount = secretcount; 
 
    p->usedown = p->attackdown = true;	// don't do anything immediately 
    p->playerstate = PST_LIVE;       
    p->health = MAXHEALTH; 
    p->readyweapon = p->pendingweapon = wp_pistol; 
    p->weaponowned[wp_fist] = true; 
    p->weaponowned[wp_pistol] = true; 
    p->ammo[am_clip] = 50; 
	 
    for (i=0 ; i<NUMAMMO ; i++) 
	p->maxammo[i] = maxammo[i]; 
		 
}

//
// G_CheckSpot  
// Returns false if the player cannot be respawned
// at the given mapthing_t spot  
// because something is occupying it 
//
void P_SpawnPlayer (mapthing_t* mthing); 
 
boolean
G_CheckSpot
( int		playernum,
  mapthing_t*	mthing ) 
{ 
    fixed_t		x;
    fixed_t		y; 
    subsector_t*	ss; 
    unsigned		an; 
    mobj_t*		mo; 
    int			i;
	
    if (!players[playernum].mo)
    {
	// first spawn of level, before corpses
	for (i=0 ; i<playernum ; i++)
	    if (players[i].mo->x == mthing->x << FRACBITS
		&& players[i].mo->y == mthing->y << FRACBITS)
		return false;	
	return true;
    }
		
    x = mthing->x << FRACBITS; 
    y = mthing->y << FRACBITS; 
	 
    if (!P_CheckPosition (players[playernum].mo, x, y) ) 
	return false; 
 
    // flush an old corpse if needed 
    if (bodyqueslot >= BODYQUESIZE) 
	P_RemoveMobj (bodyque[bodyqueslot%BODYQUESIZE]); 
    bodyque[bodyqueslot%BODYQUESIZE] = players[playernum].mo; 
    bodyqueslot++; 
	
    // spawn a teleport fog 
    ss = R_PointInSubsector (x,y); 
    an = ( ANG45 * (mthing->angle/45) ) >> ANGLETOFINESHIFT; 
 
    mo = P_SpawnMobj (x+20*finecosine[an], y+20*finesine[an] 
		      , ss->sector->floorheight 
		      , MT_TFOG); 
	 
    if (players[consoleplayer].viewz != 1) 
	S_StartSound (mo, sfx_telept);	// don't start sound on first frame 
 
    return true; 
} 


//
// G_DeathMatchSpawnPlayer 
// Spawns a player at one of the random death match spots 
// called at level load and each death 
//
void G_DeathMatchSpawnPlayer (int playernum) 
{ 
    int             i,j; 
    int				selections; 
	 
    selections = deathmatch_p - deathmatchstarts; 
    if (selections < 4) 
	I_Error ("Only %i deathmatch spots, 4 required", selections); 
 
    for (j=0 ; j<20 ; j++) 
    { 
	i = P_Random() % selections; 
	if (G_CheckSpot (playernum, &deathmatchstarts[i]) ) 
	{ 
	    deathmatchstarts[i].type = playernum+1; 
	    P_SpawnPlayer (&deathmatchstarts[i]); 
	    return; 
	} 
    } 
 
    // no good spot, so the player will probably get stuck 
    P_SpawnPlayer (&playerstarts[playernum]); 
} 

//
// G_DoReborn 
// 
void G_DoReborn (int playernum) 
{ 
    int                             i; 
	 
    if (!netgame)
    {
	// reload the level from scratch
	gameaction = ga_loadlevel;  
    }
    else 
    {
	// respawn at the start

	// first dissasociate the corpse 
	players[playernum].mo->player = NULL;   
		 
	// spawn at random spot if in death match 
	if (deathmatch) 
	{ 
	    G_DeathMatchSpawnPlayer (playernum); 
	    return; 
	} 
		 
	if (G_CheckSpot (playernum, &playerstarts[playernum]) ) 
	{ 
	    P_SpawnPlayer (&playerstarts[playernum]); 
	    return; 
	}
	
	// try to spawn at one of the other players spots 
	for (i=0 ; i<MAXPLAYERS ; i++)
	{
	    if (G_CheckSpot (playernum, &playerstarts[i]) ) 
	    { 
		playerstarts[i].type = playernum+1;	// fake as other player 
		P_SpawnPlayer (&playerstarts[i]); 
		playerstarts[i].type = i+1;		// restore 
		return; 
	    }	    
	    // he's going to be inside something.  Too bad.
	}
	P_SpawnPlayer (&playerstarts[playernum]); 
    } 
} 
 
 
void G_ScreenShot (void) 
{ 
    gameaction = ga_screenshot; 
} 
 


// DOOM Par Times
int pars[4][10] = 
{ 
    {0}, 
    {0,30,75,120,90,165,180,180,30,165}, 
    {0,90,90,90,120,90,360,240,30,170}, 
    {0,90,45,90,150,90,90,165,30,135} 
}; 

// DOOM II Par Times
int cpars[32] =
{
    30,90,120,120,90,150,120,120,270,90,	//  1-10
    210,150,150,150,210,150,420,150,210,150,	// 11-20
    240,150,180,150,150,300,330,420,300,180,	// 21-30
    120,30					// 31-32
};
 

//
// G_DoCompleted 
//
boolean		secretexit; 
extern char*	pagename; 
 
void G_ExitLevel (void) 
{ 
    secretexit = false; 
    gameaction = ga_completed; 
} 

// Here's for the german edition.
void G_SecretExitLevel (void) 
{ 
    umapentry_t*	map = U_ThisMap ();

    // IF NO WOLF3D LEVELS, NO SECRET EXIT!
    // Unless UMAPINFO gives the secret exit somewhere else to go.
    if ( (gamemode == commercial)
      && (W_CheckNumForName("map31")<0)
      && !(map && map->secretmap))
	secretexit = false;
    else
	secretexit = true; 
    gameaction = ga_completed; 
} 
 
void G_DoCompleted (void) 
{ 
    int             i; 
    umapentry_t*    map = U_ThisMap ();
	 
    gameaction = ga_nothing; 
 
    for (i=0 ; i<MAXPLAYERS ; i++) 
	if (playeringame[i]) 
	    G_PlayerFinishLevel (i);        // take away cards and stuff 
	 
    if (automapactive) 
	AM_Stop (); 

    // An ExM9 is its episode's secret level, found. The original tested
    // this, and the ExM8 below, twice over; once is enough.
    if ( (gamemap == 9)
	 && (gamemode != commercial) ) 
    {
	for (i=0 ; i<MAXPLAYERS ; i++) 
	    players[i].didsecret = true; 
    } 

    // The end of the game. DOOM goes from an ExM8 straight to its ending,
    // with no tally; a map whose UMAPINFO ends the game has the tally
    // first, unless it says nointermission, as DOOM II always had.
    if (G_Ending () != UM_END_NONE
	&& ((map && map->ending != UM_END_UNSET)
	    ? map->nointermission : gamemode != commercial))
    {
	gameaction = ga_victory; 
	return; 
    } 

    // A map UMAPINFO describes has a secret exit only if its entry says
    // where it goes; otherwise it leads where the regular exit does. So
    // the Master Levels' MAP15 goes on to their MAP16, not to DOOM II's
    // MAP31.
    if (map && secretexit && !map->secretmap)
	secretexit = false;
	 
    wminfo.didsecret = players[consoleplayer].didsecret; 
    wminfo.epsd = gameepisode -1; 
    wminfo.last = gamemap -1;
    wminfo.nextep = wminfo.epsd;
    
    // wminfo.next is 0 biased, unlike gamemap
    if (map && secretexit)
    {
	// UMAPINFO's exits, which can cross into another episode
	wminfo.nextep = map->secretepisode - 1;
	wminfo.next = map->secretmap - 1;
    }
    else if (map && map->nextmap)
    {
	wminfo.nextep = map->nextepisode - 1;
	wminfo.next = map->nextmap - 1;
    }
    else if ( gamemode == commercial)
    {
	if (secretexit)
	    switch(gamemap)
	    {
	      case 15: wminfo.next = 30; break;
	      case 31: wminfo.next = 31; break;
	    }
	else
	    switch(gamemap)
	    {
	      case 31:
	      case 32: wminfo.next = 15; break;
	      default: wminfo.next = gamemap;
	    }
    }
    else
    {
	if (secretexit) 
	    wminfo.next = 8; 	// go to secret level 
	else if (gamemap == 9) 
	{
	    // returning from secret level 
	    switch (gameepisode) 
	    { 
	      case 1: 
		wminfo.next = 3; 
		break; 
	      case 2: 
		wminfo.next = 5; 
		break; 
	      case 3: 
		wminfo.next = 6; 
		break; 
	      case 4:
		wminfo.next = 2;
		break;
	      default:
		// An episode UMAPINFO added that did not say: its first map
		// rather than an ExM10 there is none of.
		wminfo.next = 0;
		break;
	    }                
	} 
	else 
	    wminfo.next = gamemap;          // go to next level 
    }
		 
    wminfo.maxkills = totalkills; 
    wminfo.maxitems = totalitems; 
    wminfo.maxsecret = totalsecret; 
    wminfo.maxfrags = 0; 
    // None, and none shown, for a map past the tables' ends -- E4, which
    // the original read from past the end of pars[], SIGIL's E5, a MAP33 --
    // unless its UMAPINFO has one.
    if ( map && map->partime > 0 )
	wminfo.partime = 35*map->partime;
    else if ( U_BexPar (gameepisode, gamemap) > 0 )
	wminfo.partime = 35*U_BexPar (gameepisode, gamemap);
    else if ( gamemode == commercial )
	wminfo.partime = gamemap <= 32 ? 35*cpars[gamemap-1] : 0; 
    else
	wminfo.partime = gameepisode <= 3 && gamemap <= 9
	    ? 35*pars[gameepisode][gamemap] : 0; 
    wminfo.pnum = consoleplayer; 
 
    for (i=0 ; i<MAXPLAYERS ; i++) 
    { 
	wminfo.plyr[i].in = playeringame[i]; 
	wminfo.plyr[i].skills = players[i].killcount; 
	wminfo.plyr[i].sitems = players[i].itemcount; 
	wminfo.plyr[i].ssecret = players[i].secretcount; 
	wminfo.plyr[i].stime = leveltime; 
	memcpy (wminfo.plyr[i].frags, players[i].frags 
		, sizeof(wminfo.plyr[i].frags)); 
    } 
 
    gamestate = GS_INTERMISSION; 
    viewactive = false; 
    automapactive = false; 
 
    if (statcopy)
	memcpy (statcopy, &wminfo, sizeof(wminfo));
	
    WI_Start (&wminfo); 
} 


//
// G_WorldDone 
//
void G_WorldDone (void) 
{ 
    gameaction = ga_worlddone; 

    if (secretexit) 
	players[consoleplayer].didsecret = true; 

    // Text between maps, or the end of the game.
    if (F_InterText () || G_Ending () != UM_END_NONE)
	F_StartFinale ();
} 
 
void G_DoWorldDone (void) 
{        
    gamestate = GS_LEVEL; 
    gameepisode = wminfo.nextep+1;
    gamemap = wminfo.next+1; 
    G_DoLoadLevel (); 
    gameaction = ga_nothing; 
    viewactive = true; 
} 
 


//
// G_InitFromSavegame
// Can be called by the startup code or the menu task. 
//
extern boolean setsizeneeded;
void R_ExecuteSetViewSize (void);

char	savename[256];

void G_LoadGame (char* name) 
{ 
    strcpy (savename, name); 
    gameaction = ga_loadgame; 
} 
 
#define VERSIONSIZE		16 


void G_DoLoadGame (void) 
{ 
    int		length; 
    int		i; 
    int		a,b,c; 
    char	vcheck[VERSIONSIZE]; 
	 
    gameaction = ga_nothing; 
	 
    length = M_ReadFile (savename, &savebuffer); 
    save_p = savebuffer + SAVESTRINGSIZE;
    
    // skip the description field 
    memset (vcheck,0,sizeof(vcheck)); 
    sprintf (vcheck,"version %i",VERSION); 
    if (strcmp (save_p, vcheck)) 
	return;				// bad version 
    save_p += VERSIONSIZE; 
			 
    gameskill = *save_p++; 
    gameepisode = *save_p++; 
    gamemap = *save_p++; 
    for (i=0 ; i<MAXPLAYERS ; i++) 
	playeringame[i] = *save_p++; 

    // load a base level 
    G_InitNew (gameskill, gameepisode, gamemap); 
 
    // get the times 
    a = *save_p++; 
    b = *save_p++; 
    c = *save_p++; 
    leveltime = (a<<16) + (b<<8) + c; 
	 
    // dearchive all the modifications
    P_UnArchivePlayers (); 
    P_UnArchiveWorld (); 
    P_UnArchiveThinkers (); 
    P_UnArchiveSpecials (); 
 
    if (*save_p != 0x1d) 
	I_Error ("Bad savegame");
    
    // done 
    Z_Free (savebuffer); 
 
    if (setsizeneeded)
	R_ExecuteSetViewSize ();
    
    // draw the pattern into the back screen
    R_FillBackScreen ();   
} 
 

//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string 
//
void
G_SaveGame
( int	slot,
  char*	description ) 
{ 
    savegameslot = slot; 
    strcpy (savedescription, description); 
    sendsave = true; 
} 
 
void G_DoSaveGame (void) 
{ 
    char	name[100]; 
    char	name2[VERSIONSIZE]; 
    char*	description; 
    int		length; 
    int		i; 
	
    if (M_CheckParm("-cdrom"))
	sprintf(name,"c:\\doomdata\\"SAVEGAMENAME"%d.dsg",savegameslot);
    else
	sprintf (name,SAVEGAMENAME"%d.dsg",savegameslot); 
    description = savedescription; 
	 
    {
	static byte*	buffer;

	if (!buffer && !(buffer = malloc (SAVEGAMESIZE)))
	    I_Error ("G_DoSaveGame: no memory for the savegame");
	save_p = savebuffer = buffer;
    }
	 
    memcpy (save_p, description, SAVESTRINGSIZE); 
    save_p += SAVESTRINGSIZE; 
    memset (name2,0,sizeof(name2)); 
    sprintf (name2,"version %i",VERSION); 
    memcpy (save_p, name2, VERSIONSIZE); 
    save_p += VERSIONSIZE; 
	 
    *save_p++ = gameskill; 
    *save_p++ = gameepisode; 
    *save_p++ = gamemap; 
    for (i=0 ; i<MAXPLAYERS ; i++) 
	*save_p++ = playeringame[i]; 
    *save_p++ = leveltime>>16; 
    *save_p++ = leveltime>>8; 
    *save_p++ = leveltime; 
 
    P_ArchivePlayers (); 
    P_ArchiveWorld (); 
    P_ArchiveThinkers (); 
    P_ArchiveSpecials (); 
	 
    *save_p++ = 0x1d;		// consistancy marker 
	 
    length = save_p - savebuffer; 
    if (length > SAVEGAMESIZE) 
	I_Error ("Savegame buffer overrun"); 
    M_WriteFile (name, savebuffer, length); 
    gameaction = ga_nothing; 
    savedescription[0] = 0;		 
	 
    players[consoleplayer].message = GGSAVED; 

    // draw the pattern into the back screen
    R_FillBackScreen ();	
} 
 

//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set. 
//
skill_t	d_skill; 
int     d_episode; 
int     d_map; 
 
void
G_DeferedInitNew
( skill_t	skill,
  int		episode,
  int		map) 
{ 
    d_skill = skill; 
    d_episode = episode; 
    d_map = map; 
    gameaction = ga_newgame; 
} 


void G_DoNewGame (void) 
{
    demoplayback = false; 
    netdemo = false;
    netgame = false;
    deathmatch = false;
    playeringame[1] = playeringame[2] = playeringame[3] = 0;
    respawnparm = false;
    fastparm = false;
    nomonsters = false;
    consoleplayer = 0;
    G_InitNew (d_skill, d_episode, d_map); 
    gameaction = ga_nothing; 
} 

// The sky texture to be used instead of the F_SKY1 dummy.
extern  int	skytexture; 


void
G_InitNew
( skill_t	skill,
  int		episode,
  int		map ) 
{ 
    int             i; 
	 
    if (paused) 
    { 
	paused = false; 
	S_ResumeSound (); 
    } 
	

    if (skill > sk_nightmare) 
	skill = sk_nightmare;


    if (episode < 1)
      episode = 1; 
    if (map < 1) 
	map = 1;
    if (gamemode == commercial)
      episode = 1;		// DOOM II has none

    // A map the game data has is played, whatever its number: add-ons go
    // past the original limits -- SIGIL is episode 5, the Master Levels
    // run to MAP21. Anything else is brought inside them, as before.
    if (!G_MapExists (episode, map))
    {
	if ( gamemode == retail )
	{
	    if (episode > 4)
		episode = 4;
	}
	else if ( gamemode == shareware )
	{
	    if (episode > 1) 
		episode = 1;	// only start episode 1 on shareware
	}  
	else if ( gamemode != commercial )
	{
	    if (episode > 3)
		episode = 3;
	}
    
	if ( (map > 9)
	     && ( gamemode != commercial) )
	    map = 9; 
    }
		 
    M_ClearRandom (); 
	 
    if (skill == sk_nightmare || respawnparm )
	respawnmonsters = true;
    else
	respawnmonsters = false;
		
    if (fastparm || (skill == sk_nightmare && gameskill != sk_nightmare) )
    { 
	for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++) 
	    states[i].tics >>= 1; 
	mobjinfo[MT_BRUISERSHOT].speed = 20*FRACUNIT; 
	mobjinfo[MT_HEADSHOT].speed = 20*FRACUNIT; 
	mobjinfo[MT_TROOPSHOT].speed = 20*FRACUNIT; 
    } 
    else if (skill != sk_nightmare && gameskill == sk_nightmare) 
    { 
	for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++) 
	    states[i].tics <<= 1; 
	mobjinfo[MT_BRUISERSHOT].speed = 15*FRACUNIT; 
	mobjinfo[MT_HEADSHOT].speed = 10*FRACUNIT; 
	mobjinfo[MT_TROOPSHOT].speed = 10*FRACUNIT; 
    } 
	 
			 
    // force players to be initialized upon first level load         
    for (i=0 ; i<MAXPLAYERS ; i++) 
	players[i].playerstate = PST_REBORN; 
 
    usergame = true;                // will be set false if a demo 
    paused = false; 
    demoplayback = false; 
    automapactive = false; 
    viewactive = true; 
    gameepisode = episode; 
    gamemap = map; 
    gameskill = skill; 
 
    viewactive = true;
    
    // G_DoLoadLevel sets the sky.
    G_DoLoadLevel (); 
} 
 

//
// DEMO RECORDING 
// 
#define DEMOMARKER		0x80


void G_ReadDemoTiccmd (ticcmd_t* cmd) 
{ 
    if (*demo_p == DEMOMARKER) 
    {
	// end of demo data stream 
	G_CheckDemoStatus (); 
	return; 
    } 
    cmd->forwardmove = ((signed char)*demo_p++); 
    cmd->sidemove = ((signed char)*demo_p++); 
    cmd->angleturn = ((unsigned char)*demo_p++)<<8; 
    cmd->buttons = (unsigned char)*demo_p++; 
} 


void G_WriteDemoTiccmd (ticcmd_t* cmd) 
{ 
    if (gamekeydown['q'])           // press q to end demo recording 
	G_CheckDemoStatus (); 
    *demo_p++ = cmd->forwardmove; 
    *demo_p++ = cmd->sidemove; 
    *demo_p++ = (cmd->angleturn+128)>>8; 
    *demo_p++ = cmd->buttons; 
    demo_p -= 4; 
    if (demo_p > demoend - 16)
    {
	// no more space 
	G_CheckDemoStatus (); 
	return; 
    } 
	
    G_ReadDemoTiccmd (cmd);         // make SURE it is exactly the same 
} 
 
 
 
//
// G_RecordDemo 
// 
void G_RecordDemo (char* name) 
{ 
    int             i; 
    int				maxsize;
	
    usergame = false; 
    strcpy (demoname, name); 
    strcat (demoname, ".lmp"); 
    maxsize = 0x20000;
    i = M_CheckParm ("-maxdemo");
    if (i && i<myargc-1)
	maxsize = atoi(myargv[i+1])*1024;
    demobuffer = Z_Malloc (maxsize,PU_STATIC,NULL); 
    demoend = demobuffer + maxsize;
	
    demorecording = true; 
} 
 
 
void G_BeginRecording (void) 
{ 
    int             i; 
		
    demo_p = demobuffer;
	
    *demo_p++ = VERSION;
    *demo_p++ = gameskill; 
    *demo_p++ = gameepisode; 
    *demo_p++ = gamemap; 
    *demo_p++ = deathmatch; 
    *demo_p++ = respawnparm;
    *demo_p++ = fastparm;
    *demo_p++ = nomonsters;
    *demo_p++ = consoleplayer;
	 
    for (i=0 ; i<MAXPLAYERS ; i++) 
	*demo_p++ = playeringame[i]; 		 
} 
 

//
// G_PlayDemo 
//

char*	defdemoname; 
 
void G_DeferedPlayDemo (char* name) 
{ 
    defdemoname = name; 
    gameaction = ga_playdemo; 
} 
 
void G_DoPlayDemo (void) 
{ 
    skill_t skill; 
    int             i, episode, map; 
	 
    gameaction = ga_nothing; 
    demobuffer = demo_p = W_CacheLumpName (defdemoname, PU_STATIC); 

    // Every released IWAD carries demos recorded by 1.9, and this source is
    // 1.9's game code with the version number bumped to 1.10, so they play
    // back correctly. Rejecting them left the title screen sitting there with
    // "Demo is from a different game version!" on the console instead of the
    // attract loop the game is supposed to show.
    if ( *demo_p != VERSION && *demo_p != 109)
    {
      fprintf( stderr, "Demo is from a different game version!\n");
      gameaction = ga_nothing;
      Z_ChangeTag (demobuffer, PU_CACHE);
      demobuffer = demo_p = NULL;

      // Returning here leaves no level loaded, so the play loop would tick
      // a player with no mobj. Fall back to the title screen instead;
      // -playdemo/-timedemo used to segfault on a version mismatch.
      timingdemo = false;
      singledemo = false;
      gamestate = GS_DEMOSCREEN;
      D_StartTitle ();
      return;
    }

    // Past the version byte the header is the same either way.
    demo_p++;

    skill = *demo_p++; 
    episode = *demo_p++; 
    map = *demo_p++; 
    deathmatch = *demo_p++;
    respawnparm = *demo_p++;
    fastparm = *demo_p++;
    nomonsters = *demo_p++;
    consoleplayer = *demo_p++;
	
    for (i=0 ; i<MAXPLAYERS ; i++) 
	playeringame[i] = *demo_p++; 
    if (playeringame[1]) 
    { 
	netgame = true; 
	netdemo = true; 
    }

    // don't spend a lot of time in loadlevel 
    precache = false;
    G_InitNew (skill, episode, map); 
    precache = true; 

    usergame = false; 
    demoplayback = true; 
} 

//
// G_TimeDemo 
//
void G_TimeDemo (char* name) 
{ 	 
    nodrawers = M_CheckParm ("-nodraw"); 
    noblit = M_CheckParm ("-noblit"); 
    timingdemo = true; 
    singletics = true; 

    defdemoname = name; 
    gameaction = ga_playdemo; 
} 
 
 
/* 
=================== 
= 
= G_CheckDemoStatus 
= 
= Called after a death or level completion to allow demos to be cleaned up 
= Returns true if a new demo loop action will take place 
=================== 
*/ 
 
boolean G_CheckDemoStatus (void) 
{ 
    int             endtime; 
	 
    if (timingdemo) 
    { 
	endtime = I_GetTime (); 
	I_Error ("timed %i gametics in %i realtics",gametic 
		 , endtime-starttime); 
    } 
	 
    if (demoplayback) 
    { 
	if (singledemo) 
	    I_Quit (); 
			 
	Z_ChangeTag (demobuffer, PU_CACHE); 
	demoplayback = false; 
	netdemo = false;
	netgame = false;
	deathmatch = false;
	playeringame[1] = playeringame[2] = playeringame[3] = 0;
	respawnparm = false;
	fastparm = false;
	nomonsters = false;
	consoleplayer = 0;
	D_AdvanceDemo (); 
	return true; 
    } 
 
    if (demorecording) 
    { 
	*demo_p++ = DEMOMARKER; 
	M_WriteFile (demoname, demobuffer, demo_p - demobuffer); 
	Z_Free (demobuffer); 
	demorecording = false; 
	I_Error ("Demo %s recorded",demoname); 
    } 
	 
    return false; 
} 
 
 
 
