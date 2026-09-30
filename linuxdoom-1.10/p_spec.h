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
// DESCRIPTION:  none
//	Implements special effects:
//	Texture animation, height or lighting changes
//	 according to adjacent sectors, respective
//	 utility functions, etc.
//
//-----------------------------------------------------------------------------


#ifndef __P_SPEC__
#define __P_SPEC__

#include <stddef.h>

// How long a mover was before Boom's fields were added after id's, as a
// save from then has it: up to the first of them, and the padding the
// structure had at its end, to its alignment (that of a pointer).
#define OLD_SAVESIZE(off) \
    (((off) + sizeof(void*) - 1) & ~(sizeof(void*) - 1))


//
// End-level timer (-TIMER option)
//
extern	boolean levelTimer;
extern	int	levelTimeCount;


//      Define values for map objects
#define MO_TELEPORTMAN          14


//
// Boom's generalized types (p_genlin.c), from Boom 2.02 by way of Woof.
// A generalized line special is a set of bit fields: which kind of mover,
// how it is set off, how fast, how far, and what it changes.
//

// Generalized sector types: bits above DOOM's own (0-31, the light and
// damage kinds), which a generalized sector keeps alongside one of them.
#define DAMAGE_MASK		0x60
#define DAMAGE_SHIFT		5
#define SECRET_MASK		0x80
#define SECRET_SHIFT		7
#define FRICTION_MASK		0x100
#define FRICTION_SHIFT		8
#define PUSH_MASK		0x200
#define PUSH_SHIFT		9
// MBF21's: instant death, with or without the suit, or of everyone and
// the level ended
#define DEATH_MASK		0x1000
#define KILL_MONSTERS_MASK	0x2000

// The first special of each kind, from the top down.
#define GenFloorBase		0x6000
#define GenCeilingBase		0x4000
#define GenDoorBase		0x3c00
#define GenLockedBase		0x3800
#define GenLiftBase		0x3400
#define GenStairsBase		0x3000
#define GenCrusherBase		0x2F80
#define GenEnd			0x8000

#define TriggerType		0x0007
#define TriggerTypeShift	0

#define FloorCrush		0x1000
#define FloorChange		0x0c00
#define FloorTarget		0x0380
#define FloorDirection		0x0040
#define FloorModel		0x0020
#define FloorSpeed		0x0018
#define FloorCrushShift		12
#define FloorChangeShift	10
#define FloorTargetShift	7
#define FloorDirectionShift	6
#define FloorModelShift		5
#define FloorSpeedShift		3

#define CeilingCrush		0x1000
#define CeilingChange		0x0c00
#define CeilingTarget		0x0380
#define CeilingDirection	0x0040
#define CeilingModel		0x0020
#define CeilingSpeed		0x0018
#define CeilingCrushShift	12
#define CeilingChangeShift	10
#define CeilingTargetShift	7
#define CeilingDirectionShift	6
#define CeilingModelShift	5
#define CeilingSpeedShift	3

#define LiftTarget		0x0300
#define LiftDelay		0x00c0
#define LiftMonster		0x0020
#define LiftSpeed		0x0018
#define LiftTargetShift		8
#define LiftDelayShift		6
#define LiftMonsterShift	5
#define LiftSpeedShift		3

#define StairIgnore		0x0200
#define StairDirection		0x0100
#define StairStep		0x00c0
#define StairMonster		0x0020
#define StairSpeed		0x0018
#define StairIgnoreShift	9
#define StairDirectionShift	8
#define StairStepShift		6
#define StairMonsterShift	5
#define StairSpeedShift		3

#define CrusherSilent		0x0040
#define CrusherMonster		0x0020
#define CrusherSpeed		0x0018
#define CrusherSilentShift	6
#define CrusherMonsterShift	5
#define CrusherSpeedShift	3

#define DoorDelay		0x0300
#define DoorMonster		0x0080
#define DoorKind		0x0060
#define DoorSpeed		0x0018
#define DoorDelayShift		8
#define DoorMonsterShift	7
#define DoorKindShift		5
#define DoorSpeedShift		3

#define LockedNKeys		0x0200
#define LockedKey		0x01c0
#define LockedKind		0x0020
#define LockedSpeed		0x0018
#define LockedNKeysShift	9
#define LockedKeyShift		6
#define LockedKindShift		5
#define LockedSpeedShift	3

typedef enum
{
    WalkOnce,
    WalkMany,
    SwitchOnce,
    SwitchMany,
    GunOnce,
    GunMany,
    PushOnce,
    PushMany
} triggertype_e;

typedef enum
{
    SpeedSlow,
    SpeedNormal,
    SpeedFast,
    SpeedTurbo
} motionspeed_e;

typedef enum
{
    FtoHnF,
    FtoLnF,
    FtoNnF,
    FtoLnC,
    FtoC,
    FbyST,
    Fby24,
    Fby32
} floortarget_e;

typedef enum
{
    FNoChg,
    FChgZero,
    FChgTxt,
    FChgTyp
} floorchange_e;

typedef enum
{
    CtoHnC,
    CtoLnC,
    CtoNnC,
    CtoHnF,
    CtoF,
    CbyST,
    Cby24,
    Cby32
} ceilingtarget_e;

typedef enum
{
    CNoChg,
    CChgZero,
    CChgTxt,
    CChgTyp
} ceilingchange_e;

typedef enum
{
    F2LnF,
    F2NnF,
    F2LnC,
    LnF2HnF
} lifttarget_e;

typedef enum
{
    OdCDoor,
    ODoor,
    CdODoor,
    CDoor
} doorkind_e;

typedef enum
{
    AnyKey,
    RCard,
    BCard,
    YCard,
    RSkull,
    BSkull,
    YSkull,
    AllKeys
} keykind_e;

// Which of a sector's three thinkers (P_SectorActive).
typedef enum
{
    floor_special,
    ceiling_special,
    lighting_special
} special_e;

typedef enum
{
    trigChangeOnly,
    numChangeOnly
} change_e;

// Whether a sector's floor, ceiling or light is already being moved: in a
// map of DOOM's own, any of them counts, as id's one specialdata did.
int P_SectorActive (special_e t, sector_t* sec);


// at game start
void    P_InitPicAnims (void);

// at map load
void    P_SpawnSpecials (void);

// every tic
void    P_UpdateSpecials (void);

// when needed
boolean
P_UseSpecialLine
( mobj_t*	thing,
  line_t*	line,
  int		side );

void
P_ShootSpecialLine
( mobj_t*	thing,
  line_t*	line );

void
P_CrossSpecialLine
( int		linenum,
  int		side,
  mobj_t*	thing );

// A line special with no line of the map's to cross or press.
void
P_ActivateLineSpecial
( int		special,
  int		tag,
  mobj_t*	thing );

boolean P_LineEffect (mobj_t* thing, int special, int tag);

void    P_PlayerInSpecialSector (player_t* player);

int
twoSided
( int		sector,
  int		line );

sector_t*
getSector
( int		currentSector,
  int		line,
  int		side );

side_t*
getSide
( int		currentSector,
  int		line,
  int		side );

fixed_t P_FindLowestFloorSurrounding(sector_t* sec);
fixed_t P_FindHighestFloorSurrounding(sector_t* sec);

fixed_t
P_FindNextHighestFloor
( sector_t*	sec,
  int		currentheight );

fixed_t P_FindLowestCeilingSurrounding(sector_t* sec);
fixed_t P_FindHighestCeilingSurrounding(sector_t* sec);

int
P_FindSectorFromLineTag
( const line_t*	line,
  int		start );

// Chains the sectors and lines with each tag, for the two finders above
// and P_FindLineFromLineTag (P_SetupLevel).
void P_InitTagLists (void);

int
P_FindMinSurroundingLight
( sector_t*	sector,
  int		max );

sector_t*
getNextSector
( line_t*	line,
  sector_t*	sec );


//
// SPECIAL
//



//
// P_LIGHTS
//
typedef struct
{
    thinker_t	thinker;
    sector_t*	sector;
    int		count;
    int		maxlight;
    int		minlight;
    
} fireflicker_t;



typedef struct
{
    thinker_t	thinker;
    sector_t*	sector;
    int		count;
    int		maxlight;
    int		minlight;
    int		maxtime;
    int		mintime;
    
} lightflash_t;



typedef struct
{
    thinker_t	thinker;
    sector_t*	sector;
    int		count;
    int		minlight;
    int		maxlight;
    int		darktime;
    int		brighttime;
    
} strobe_t;




typedef struct
{
    thinker_t	thinker;
    sector_t*	sector;
    int		minlight;
    int		maxlight;
    int		direction;

} glow_t;


#define GLOWSPEED			8
#define STROBEBRIGHT		5
#define FASTDARK			15
#define SLOWDARK			35

void    P_SpawnFireFlicker (sector_t* sector);
void    T_LightFlash (lightflash_t* flash);
void    P_SpawnLightFlash (sector_t* sector);
void    T_StrobeFlash (strobe_t* flash);

void
P_SpawnStrobeFlash
( sector_t*	sector,
  int		fastOrSlow,
  int		inSync );

int     EV_StartLightStrobing(line_t* line);
int     EV_TurnTagLightsOff(line_t* line);

int
EV_LightTurnOn
( line_t*	line,
  int		bright );

// Boom: the light of a door's tagged sectors, as far between darkest and
// brightest around as the door is open (level: 0 to FRACUNIT)
int     EV_LightTurnOnPartway (line_t* line, fixed_t level);
void    T_FireFlicker (fireflicker_t* flick);

void    T_Glow(glow_t* g);
void    P_SpawnGlowingLight(sector_t* sector);




//
// P_SWITCH
//
typedef struct
{
    char	name1[9];
    char	name2[9];
    short	episode;
    
} switchlist_t;


typedef enum
{
    top,
    middle,
    bottom

} bwhere_e;


typedef struct
{
    line_t*	line;
    bwhere_e	where;
    int		btexture;
    int		btimer;
    mobj_t*	soundorg;

} button_t;




 // max # of wall switches in a level
#define MAXSWITCHES		50

 // 4 players, 4 buttons each at once, max.
// Buttons, lifts and crushers in motion: as many as a map sets going. id's
// tables held 16, 30 and 30 (raised here to 128, 240 and 240), and went past
// them with "no button slots left", "no more plats", or for crushers by
// silently losing track of one, which then could not be stopped. They grow
// now, filling the first free slot as id's did.

 // 1 second, in ticks. 
#define BUTTONTIME      35             

extern button_t**	buttonlist;	// each allocated once: sounds play from them
extern int		maxbuttons; 

void
P_ChangeSwitchTexture
( line_t*	line,
  int		useAgain );

void P_InitSwitchList(void);


//
// P_PLATS
//
typedef enum
{
    up,
    down,
    waiting,
    in_stasis

} plat_e;



typedef enum
{
    perpetualRaise,
    downWaitUpStay,
    raiseAndChange,
    raiseToNearestAndChange,
    blazeDWUS,
    // Boom's, after id's so that saves keep their numbers
    genLift,
    genPerpetual,
    toggleUpDn

} plattype_e;



typedef struct
{
    thinker_t	thinker;
    sector_t*	sector;
    fixed_t	speed;
    fixed_t	low;
    fixed_t	high;
    int		wait;
    int		count;
    plat_e	status;
    plat_e	oldstatus;
    boolean	crush;
    int		tag;
    plattype_e	type;
    
} plat_t;



#define PLATWAIT		3
#define PLATSPEED		FRACUNIT



extern plat_t**	activeplats;
extern int	maxplats;

void    T_PlatRaise(plat_t*	plat);

int
EV_DoPlat
( line_t*	line,
  plattype_e	type,
  int		amount );

void    P_AddActivePlat(plat_t* plat);
void    P_RemoveActivePlat(plat_t* plat);
int     EV_StopPlat(line_t* line);
void    P_ActivateInStasis(int tag);


//
// P_DOORS
//
typedef enum
{
    normal,
    close30ThenOpen,
    close,
    open,
    raiseIn5Mins,
    blazeRaise,
    blazeOpen,
    blazeClose,
    // Boom's
    genRaise,
    genBlazeRaise,
    genOpen,
    genBlazeOpen,
    genClose,
    genBlazeClose,
    genCdO,
    genBlazeCdO

} vldoor_e;



typedef struct
{
    thinker_t	thinker;
    vldoor_e	type;
    sector_t*	sector;
    fixed_t	topheight;
    fixed_t	speed;

    // 1 = up, 0 = waiting at top, -1 = down
    int             direction;
    
    // tics to wait at the top
    int             topwait;
    // (keep in case a door going down is reset)
    // when it reaches 0, start going down
    int             topcountdown;

    // Boom's, after id's fields so that a save from before still reads
    // (VLDOOR_SAVESIZE): the line that opened it, for its light (lighttag).
    line_t*	line;
    int		lighttag;
    
} vldoor_t;

#define VLDOOR_SAVESIZE		OLD_SAVESIZE(offsetof(vldoor_t, line))



#define VDOORSPEED		FRACUNIT*2
#define VDOORWAIT		150

int
EV_VerticalDoor
( line_t*	line,
  mobj_t*	thing );

int
EV_DoDoor
( line_t*	line,
  vldoor_e	type );

int
EV_DoLockedDoor
( line_t*	line,
  vldoor_e	type,
  mobj_t*	thing );

void    T_VerticalDoor (vldoor_t* door);
void    P_SpawnDoorCloseIn30 (sector_t* sec);

void
P_SpawnDoorRaiseIn5Mins
( sector_t*	sec,
  int		secnum );



#if 0 // UNUSED
//
//      Sliding doors...
//
typedef enum
{
    sd_opening,
    sd_waiting,
    sd_closing

} sd_e;



typedef enum
{
    sdt_openOnly,
    sdt_closeOnly,
    sdt_openAndClose

} sdt_e;




typedef struct
{
    thinker_t	thinker;
    sdt_e	type;
    line_t*	line;
    int		frame;
    int		whichDoorIndex;
    int		timer;
    sector_t*	frontsector;
    sector_t*	backsector;
    sd_e	 status;

} slidedoor_t;



typedef struct
{
    char	frontFrame1[9];
    char	frontFrame2[9];
    char	frontFrame3[9];
    char	frontFrame4[9];
    char	backFrame1[9];
    char	backFrame2[9];
    char	backFrame3[9];
    char	backFrame4[9];
    
} slidename_t;



typedef struct
{
    int             frontFrames[4];
    int             backFrames[4];

} slideframe_t;



// how many frames of animation
#define SNUMFRAMES		4

#define SDOORWAIT		35*3
#define SWAITTICS		4

// how many diff. types of anims
#define MAXSLIDEDOORS	5                            

void P_InitSlidingDoorFrames(void);

void
EV_SlidingDoor
( line_t*	line,
  mobj_t*	thing );
#endif



//
// P_CEILNG
//
typedef enum
{
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise,
    silentCrushAndRaise,
    // Boom's
    lowerToLowest,
    lowerToMaxFloor,
    genCeiling,
    genCeilingChg,
    genCeilingChg0,
    genCeilingChgT,
    genCrusher,
    genSilentCrusher

} ceiling_e;



typedef struct
{
    thinker_t	thinker;
    ceiling_e	type;
    sector_t*	sector;
    fixed_t	bottomheight;
    fixed_t	topheight;
    fixed_t	speed;
    boolean	crush;

    // 1 = up, 0 = waiting, -1 = down
    int		direction;

    // ID
    int		tag;                   
    int		olddirection;

    // Boom's (CEILING_SAVESIZE): what a generalized ceiling changes its
    // sector's ceiling texture and special to, and its speed before it
    // slowed on something it crushes.
    int		newspecial;
    int		oldspecial;
    short	texture;
    fixed_t	oldspeed;
    
} ceiling_t;

#define CEILING_SAVESIZE	OLD_SAVESIZE(offsetof(ceiling_t, newspecial))





#define CEILSPEED		FRACUNIT
#define CEILWAIT		150


extern ceiling_t**	activeceilings;
extern int		maxceilings;

// The index of a free slot in a table of n pointers, which is doubled (the new
// half empty) when there is none. p_plats.c.
int P_FreeSlot (void*** table, int* n);

int
EV_DoCeiling
( line_t*	line,
  ceiling_e	type );

void    T_MoveCeiling (ceiling_t* ceiling);
void    P_AddActiveCeiling(ceiling_t* c);
void    P_RemoveActiveCeiling(ceiling_t* c);
int	EV_CeilingCrushStop(line_t* line);
int     P_ActivateInStasisCeiling(line_t* line);


//
// P_FLOOR
//
typedef enum
{
    // lower floor to highest surrounding floor
    lowerFloor,
    
    // lower floor to lowest surrounding floor
    lowerFloorToLowest,
    
    // lower floor to highest surrounding floor VERY FAST
    turboLower,
    
    // raise floor to lowest surrounding CEILING
    raiseFloor,
    
    // raise floor to next highest surrounding floor
    raiseFloorToNearest,

    // raise floor to shortest height texture around it
    raiseToTexture,
    
    // lower floor to lowest surrounding floor
    //  and change floorpic
    lowerAndChange,
  
    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,

     // raise to next highest floor, turbo-speed
    raiseFloorTurbo,       
    donutRaise,
    raiseFloor512,
    // Boom's
    lowerFloorToNearest,
    lowerFloor24,
    lowerFloor32Turbo,
    raiseFloor32Turbo,
    genFloor,
    genFloorChg,
    genFloorChg0,
    genFloorChgT,
    buildStair,
    genBuildStair
    
} floor_e;




typedef enum
{
    build8,	// slowly build by 8
    turbo16	// quickly build by 16
    
} stair_e;



typedef struct
{
    thinker_t	thinker;
    floor_e	type;
    boolean	crush;
    sector_t*	sector;
    int		direction;
    int		newspecial;
    short	texture;
    fixed_t	floordestheight;
    fixed_t	speed;

    // Boom's (FLOOR_SAVESIZE): the special a change gives back.
    int		oldspecial;

} floormove_t;

#define FLOOR_SAVESIZE		OLD_SAVESIZE(offsetof(floormove_t, oldspecial))



#define FLOORSPEED		FRACUNIT

typedef enum
{
    ok,
    crushed,
    pastdest
    
} result_e;

result_e
T_MovePlane
( sector_t*	sector,
  fixed_t	speed,
  fixed_t	dest,
  boolean	crush,
  int		floorOrCeiling,
  int		direction );

int
EV_BuildStairs
( line_t*	line,
  stair_e	type );

int
EV_DoFloor
( line_t*	line,
  floor_e	floortype );

void T_MoveFloor( floormove_t* floor);

//
// P_TELEPT
//
int
EV_Teleport
( line_t*	line,
  int		side,
  mobj_t*	thing );

// Boom's: without the fog or sound, keeping the angle, to a thing or to
// another line (reversed or not)
int EV_SilentTeleport (line_t* line, int side, mobj_t* thing);
int EV_SilentLineTeleport (line_t* line, int side, mobj_t* thing,
			   boolean reverse);


//
// Boom (p_genlin.c, p_floor.c, p_spec.c)
//
typedef enum
{
    elevateUp,
    elevateDown,
    elevateCurrent
} elevator_e;

// A floor and ceiling moving together.
typedef struct
{
    thinker_t	thinker;
    elevator_e	type;
    sector_t*	sector;
    int		direction;
    fixed_t	floordestheight;
    fixed_t	ceilingdestheight;
    fixed_t	speed;
} elevator_t;

#define ELEVATORSPEED		(FRACUNIT*4)

void T_MoveElevator (elevator_t* elevator);

// Boom's scrollers (p_spec.c)
typedef enum
{
    sc_side,
    sc_floor,
    sc_ceiling,
    sc_carry
} scroller_e;

typedef struct
{
    thinker_t	thinker;
    fixed_t	dx, dy;		// scroll speeds
    int		affectee;	// the sidedef or sector
    int		control;	// sector whose heights drive it, or -1
    fixed_t	last_height;	// of the control sector, last seen
    fixed_t	vdx, vdy;	// the speed it has reached, if accelerating
    int		accel;
    scroller_e	type;
} scroll_t;

void T_Scroll (scroll_t* s);

// Boom's pushers (p_spec.c)
typedef enum
{
    p_push,
    p_pull,
    p_wind,
    p_current
} pusher_e;

typedef struct
{
    thinker_t	thinker;
    pusher_e	type;
    mobj_t*	source;		// the MT_PUSH or MT_PULL, for a point pusher
    int		x_mag;
    int		y_mag;
    int		magnitude;
    int		radius;
    int		x;		// where the source was
    int		y;
    int		affectee;	// the sector
} pusher_t;

void T_Pusher (pusher_t* p);

// Friction: DOOM's, what Boom's type 223 sets instead, and how much a
// push counts for on normal ground (p_map.c)
#define ORIG_FRICTION		0xE800
#define ORIG_FRICTION_FACTOR	2048
int P_GetFriction (const mobj_t* mo, int* factor);
int P_GetMoveFactor (const mobj_t* mo, int* friction);


int EV_DoGenFloor (line_t* line);
int EV_DoGenCeiling (line_t* line);
int EV_DoGenLift (line_t* line);
int EV_DoGenStairs (line_t* line);
int EV_DoGenCrusher (line_t* line);
int EV_DoGenDoor (line_t* line);
int EV_DoGenLockedDoor (line_t* line);
int EV_DoChange (line_t* line, change_e changetype);
int EV_DoElevator (line_t* line, elevator_e type);
int EV_DoDonut (line_t* line);

// Whether the player has the keys a generalized locked door asks for.
boolean P_CanUnlockGenDoor (line_t* line, player_t* player);
// Whether a line's special may act with no tag (Boom checks, and so
// demands one of those that need it).
int P_CheckTag (line_t* line);

fixed_t P_FindNextLowestFloor (sector_t* sec, int currentheight);
fixed_t P_FindNextHighestCeiling (sector_t* sec, int currentheight);
fixed_t P_FindNextLowestCeiling (sector_t* sec, int currentheight);
fixed_t P_FindShortestTextureAround (int secnum);
fixed_t P_FindShortestUpperAround (int secnum);
sector_t* P_FindModelFloorSector (fixed_t floordestheight, int secnum);
sector_t* P_FindModelCeilingSector (fixed_t ceildestheight, int secnum);
int P_FindLineFromLineTag (const line_t* line, int start);

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
