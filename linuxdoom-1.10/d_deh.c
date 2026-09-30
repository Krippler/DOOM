// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	DEHACKED: the patches classic mods change the game with.
//
//	DeHackEd edited the DOS executable itself -- its tables of things,
//	frames, weapons, ammunition and sounds, the text it printed, a few
//	numbers compiled into the code -- and a .deh file is a list of such
//	edits. Ports read the file instead: from -deh on the command line, and
//	from a DEHACKED lump in a WAD, which is how most mods carry one. Boom
//	added sections of its own (BEX): texts, code pointers and names by
//	name rather than by position, and par times.
//
//	What is read: Thing, Frame, Weapon, Ammo, Sound, Sprite (ignored, as
//	DeHackEd's own "Offset" means nothing outside the DOS executable),
//	Pointer, Text, Misc, and BEX's [STRINGS], [CODEPTR], [PARS], [SPRITES],
//	[SOUNDS] and [MUSIC]. What is not: Cheat (the cheats here are id's and
//	their long-standing aliases), [HELPER], INCLUDE, and the frame, thing
//	and sound numbers past id's own that later formats (DEHEXTRA, MBF21)
//	and their code pointers use. Each of those is said, once, in the log.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/types.h>

// from <unistd.h>, which cannot be included here: it declares close(), and
// p_spec.h has a door type of that name
ssize_t readlink (const char* path, char* buf, size_t len);

#include "doomdef.h"
#include "doomstat.h"
#include "d_englsh.h"
#include "d_items.h"
#include "d_main.h"
#include "info.h"
#include "sounds.h"
#include "p_mobj.h"
#include "p_local.h"
#include "m_argv.h"
#include "w_wad.h"
#include "i_system.h"
#include "u_mapinfo.h"
#include "d_deh.h"


//
// The numbers id compiled into the code, which DeHackEd's Misc section
// patched in the executable. id's values; see where each is used.
//
int	deh_initial_health = 100;	// G_PlayerReborn
int	deh_initial_bullets = 50;
int	deh_max_health = 200;		// health bonuses stop here
int	deh_max_armor = 200;		// armor bonuses stop here
int	deh_green_armor_class = 1;
int	deh_blue_armor_class = 2;
int	deh_max_soulsphere = 200;
int	deh_soulsphere_health = 100;
int	deh_megasphere_health = 200;
int	deh_god_mode_health = 100;	// iddqd
int	deh_idfa_armor = 200;
int	deh_idfa_armor_class = 2;
int	deh_idkfa_armor = 200;
int	deh_idkfa_armor_class = 2;
int	deh_bfg_cells_per_shot = 40;
int	deh_species_infighting = 0;	// monsters of a kind hurt each other


//
// Replaced texts, found by what they said before. The game's texts are
// id's macros, compiled in wherever they are used; the few places they
// reach the screen -- the message line, the level's name, the finale,
// the cast, the menus' questions -- ask D_Text for any replacement.
//
typedef struct
{
    char*	from;
    char*	to;
} dehtext_t;

static dehtext_t*	dehtexts;
static int		numdehtexts;

static void D_SetText (const char* from, const char* to)
{
    int		i;

    for (i = 0; i < numdehtexts; i++)
	if (!strcmp (dehtexts[i].from, from))
	{
	    free (dehtexts[i].to);
	    dehtexts[i].to = strdup (to);
	    return;
	}
    dehtexts = realloc (dehtexts, (numdehtexts + 1) * sizeof(*dehtexts));
    if (!dehtexts)
	I_Error ("DEHACKED: out of memory");
    dehtexts[numdehtexts].from = strdup (from);
    dehtexts[numdehtexts].to = strdup (to);
    numdehtexts++;
}

const char* D_Text (const char* s)
{
    int		i;

    if (!s)
	return s;
    for (i = 0; i < numdehtexts; i++)
	if (!strcmp (dehtexts[i].from, s))
	    return dehtexts[i].to;
    return s;
}


// BEX [STRINGS] names: id's macro names, which are what Boom used
// (not HUSTR_KEYGREEN and the other three: those are the chat keys, as
// characters, not texts)
static const struct { const char* name; const char* text; } bexstrings[] =
{
    {"PRESSKEY", PRESSKEY},
    {"PRESSYN", PRESSYN},
    {"QUITMSG", QUITMSG},
    {"LOADNET", LOADNET},
    {"QLOADNET", QLOADNET},
    {"QSAVESPOT", QSAVESPOT},
    {"SAVEDEAD", SAVEDEAD},
    {"QSPROMPT", QSPROMPT},
    {"QLPROMPT", QLPROMPT},
    {"NEWGAME", NEWGAME},
    {"NIGHTMARE", NIGHTMARE},
    {"SWSTRING", SWSTRING},
    {"MSGOFF", MSGOFF},
    {"MSGON", MSGON},
    {"NETEND", NETEND},
    {"ENDGAME", ENDGAME},
    {"DETAILHI", DETAILHI},
    {"DETAILLO", DETAILLO},
    {"GAMMALVL0", GAMMALVL0},
    {"GAMMALVL1", GAMMALVL1},
    {"GAMMALVL2", GAMMALVL2},
    {"GAMMALVL3", GAMMALVL3},
    {"GAMMALVL4", GAMMALVL4},
    {"EMPTYSTRING", EMPTYSTRING},
    {"GOTARMOR", GOTARMOR},
    {"GOTMEGA", GOTMEGA},
    {"GOTHTHBONUS", GOTHTHBONUS},
    {"GOTARMBONUS", GOTARMBONUS},
    {"GOTSTIM", GOTSTIM},
    {"GOTMEDINEED", GOTMEDINEED},
    {"GOTMEDIKIT", GOTMEDIKIT},
    {"GOTSUPER", GOTSUPER},
    {"GOTBLUECARD", GOTBLUECARD},
    {"GOTYELWCARD", GOTYELWCARD},
    {"GOTREDCARD", GOTREDCARD},
    {"GOTBLUESKUL", GOTBLUESKUL},
    {"GOTYELWSKUL", GOTYELWSKUL},
    {"GOTREDSKULL", GOTREDSKULL},
    {"GOTINVUL", GOTINVUL},
    {"GOTBERSERK", GOTBERSERK},
    {"GOTINVIS", GOTINVIS},
    {"GOTSUIT", GOTSUIT},
    {"GOTMAP", GOTMAP},
    {"GOTVISOR", GOTVISOR},
    {"GOTMSPHERE", GOTMSPHERE},
    {"GOTCLIP", GOTCLIP},
    {"GOTCLIPBOX", GOTCLIPBOX},
    {"GOTROCKET", GOTROCKET},
    {"GOTROCKBOX", GOTROCKBOX},
    {"GOTCELL", GOTCELL},
    {"GOTCELLBOX", GOTCELLBOX},
    {"GOTSHELLS", GOTSHELLS},
    {"GOTSHELLBOX", GOTSHELLBOX},
    {"GOTBACKPACK", GOTBACKPACK},
    {"GOTBFG9000", GOTBFG9000},
    {"GOTCHAINGUN", GOTCHAINGUN},
    {"GOTCHAINSAW", GOTCHAINSAW},
    {"GOTLAUNCHER", GOTLAUNCHER},
    {"GOTPLASMA", GOTPLASMA},
    {"GOTSHOTGUN", GOTSHOTGUN},
    {"GOTSHOTGUN2", GOTSHOTGUN2},
    {"PD_BLUEO", PD_BLUEO},
    {"PD_REDO", PD_REDO},
    {"PD_YELLOWO", PD_YELLOWO},
    {"PD_BLUEK", PD_BLUEK},
    {"PD_REDK", PD_REDK},
    {"PD_YELLOWK", PD_YELLOWK},
    {"PD_BLUEC", PD_BLUEC},
    {"PD_REDC", PD_REDC},
    {"PD_YELLOWC", PD_YELLOWC},
    {"PD_BLUES", PD_BLUES},
    {"PD_REDS", PD_REDS},
    {"PD_YELLOWS", PD_YELLOWS},
    {"PD_ANY", PD_ANY},
    {"PD_ALL3", PD_ALL3},
    {"PD_ALL6", PD_ALL6},
    {"GGSAVED", GGSAVED},
    {"HUSTR_MSGU", HUSTR_MSGU},
    {"HUSTR_E1M1", HUSTR_E1M1},
    {"HUSTR_E1M2", HUSTR_E1M2},
    {"HUSTR_E1M3", HUSTR_E1M3},
    {"HUSTR_E1M4", HUSTR_E1M4},
    {"HUSTR_E1M5", HUSTR_E1M5},
    {"HUSTR_E1M6", HUSTR_E1M6},
    {"HUSTR_E1M7", HUSTR_E1M7},
    {"HUSTR_E1M8", HUSTR_E1M8},
    {"HUSTR_E1M9", HUSTR_E1M9},
    {"HUSTR_E2M1", HUSTR_E2M1},
    {"HUSTR_E2M2", HUSTR_E2M2},
    {"HUSTR_E2M3", HUSTR_E2M3},
    {"HUSTR_E2M4", HUSTR_E2M4},
    {"HUSTR_E2M5", HUSTR_E2M5},
    {"HUSTR_E2M6", HUSTR_E2M6},
    {"HUSTR_E2M7", HUSTR_E2M7},
    {"HUSTR_E2M8", HUSTR_E2M8},
    {"HUSTR_E2M9", HUSTR_E2M9},
    {"HUSTR_E3M1", HUSTR_E3M1},
    {"HUSTR_E3M2", HUSTR_E3M2},
    {"HUSTR_E3M3", HUSTR_E3M3},
    {"HUSTR_E3M4", HUSTR_E3M4},
    {"HUSTR_E3M5", HUSTR_E3M5},
    {"HUSTR_E3M6", HUSTR_E3M6},
    {"HUSTR_E3M7", HUSTR_E3M7},
    {"HUSTR_E3M8", HUSTR_E3M8},
    {"HUSTR_E3M9", HUSTR_E3M9},
    {"HUSTR_E4M1", HUSTR_E4M1},
    {"HUSTR_E4M2", HUSTR_E4M2},
    {"HUSTR_E4M3", HUSTR_E4M3},
    {"HUSTR_E4M4", HUSTR_E4M4},
    {"HUSTR_E4M5", HUSTR_E4M5},
    {"HUSTR_E4M6", HUSTR_E4M6},
    {"HUSTR_E4M7", HUSTR_E4M7},
    {"HUSTR_E4M8", HUSTR_E4M8},
    {"HUSTR_E4M9", HUSTR_E4M9},
    {"HUSTR_1", HUSTR_1},
    {"HUSTR_2", HUSTR_2},
    {"HUSTR_3", HUSTR_3},
    {"HUSTR_4", HUSTR_4},
    {"HUSTR_5", HUSTR_5},
    {"HUSTR_6", HUSTR_6},
    {"HUSTR_7", HUSTR_7},
    {"HUSTR_8", HUSTR_8},
    {"HUSTR_9", HUSTR_9},
    {"HUSTR_10", HUSTR_10},
    {"HUSTR_11", HUSTR_11},
    {"HUSTR_12", HUSTR_12},
    {"HUSTR_13", HUSTR_13},
    {"HUSTR_14", HUSTR_14},
    {"HUSTR_15", HUSTR_15},
    {"HUSTR_16", HUSTR_16},
    {"HUSTR_17", HUSTR_17},
    {"HUSTR_18", HUSTR_18},
    {"HUSTR_19", HUSTR_19},
    {"HUSTR_20", HUSTR_20},
    {"HUSTR_21", HUSTR_21},
    {"HUSTR_22", HUSTR_22},
    {"HUSTR_23", HUSTR_23},
    {"HUSTR_24", HUSTR_24},
    {"HUSTR_25", HUSTR_25},
    {"HUSTR_26", HUSTR_26},
    {"HUSTR_27", HUSTR_27},
    {"HUSTR_28", HUSTR_28},
    {"HUSTR_29", HUSTR_29},
    {"HUSTR_30", HUSTR_30},
    {"HUSTR_31", HUSTR_31},
    {"HUSTR_32", HUSTR_32},
    {"PHUSTR_1", PHUSTR_1},
    {"PHUSTR_2", PHUSTR_2},
    {"PHUSTR_3", PHUSTR_3},
    {"PHUSTR_4", PHUSTR_4},
    {"PHUSTR_5", PHUSTR_5},
    {"PHUSTR_6", PHUSTR_6},
    {"PHUSTR_7", PHUSTR_7},
    {"PHUSTR_8", PHUSTR_8},
    {"PHUSTR_9", PHUSTR_9},
    {"PHUSTR_10", PHUSTR_10},
    {"PHUSTR_11", PHUSTR_11},
    {"PHUSTR_12", PHUSTR_12},
    {"PHUSTR_13", PHUSTR_13},
    {"PHUSTR_14", PHUSTR_14},
    {"PHUSTR_15", PHUSTR_15},
    {"PHUSTR_16", PHUSTR_16},
    {"PHUSTR_17", PHUSTR_17},
    {"PHUSTR_18", PHUSTR_18},
    {"PHUSTR_19", PHUSTR_19},
    {"PHUSTR_20", PHUSTR_20},
    {"PHUSTR_21", PHUSTR_21},
    {"PHUSTR_22", PHUSTR_22},
    {"PHUSTR_23", PHUSTR_23},
    {"PHUSTR_24", PHUSTR_24},
    {"PHUSTR_25", PHUSTR_25},
    {"PHUSTR_26", PHUSTR_26},
    {"PHUSTR_27", PHUSTR_27},
    {"PHUSTR_28", PHUSTR_28},
    {"PHUSTR_29", PHUSTR_29},
    {"PHUSTR_30", PHUSTR_30},
    {"PHUSTR_31", PHUSTR_31},
    {"PHUSTR_32", PHUSTR_32},
    {"THUSTR_1", THUSTR_1},
    {"THUSTR_2", THUSTR_2},
    {"THUSTR_3", THUSTR_3},
    {"THUSTR_4", THUSTR_4},
    {"THUSTR_5", THUSTR_5},
    {"THUSTR_6", THUSTR_6},
    {"THUSTR_7", THUSTR_7},
    {"THUSTR_8", THUSTR_8},
    {"THUSTR_9", THUSTR_9},
    {"THUSTR_10", THUSTR_10},
    {"THUSTR_11", THUSTR_11},
    {"THUSTR_12", THUSTR_12},
    {"THUSTR_13", THUSTR_13},
    {"THUSTR_14", THUSTR_14},
    {"THUSTR_15", THUSTR_15},
    {"THUSTR_16", THUSTR_16},
    {"THUSTR_17", THUSTR_17},
    {"THUSTR_18", THUSTR_18},
    {"THUSTR_19", THUSTR_19},
    {"THUSTR_20", THUSTR_20},
    {"THUSTR_21", THUSTR_21},
    {"THUSTR_22", THUSTR_22},
    {"THUSTR_23", THUSTR_23},
    {"THUSTR_24", THUSTR_24},
    {"THUSTR_25", THUSTR_25},
    {"THUSTR_26", THUSTR_26},
    {"THUSTR_27", THUSTR_27},
    {"THUSTR_28", THUSTR_28},
    {"THUSTR_29", THUSTR_29},
    {"THUSTR_30", THUSTR_30},
    {"THUSTR_31", THUSTR_31},
    {"THUSTR_32", THUSTR_32},
    {"HUSTR_CHATMACRO1", HUSTR_CHATMACRO1},
    {"HUSTR_CHATMACRO2", HUSTR_CHATMACRO2},
    {"HUSTR_CHATMACRO3", HUSTR_CHATMACRO3},
    {"HUSTR_CHATMACRO4", HUSTR_CHATMACRO4},
    {"HUSTR_CHATMACRO5", HUSTR_CHATMACRO5},
    {"HUSTR_CHATMACRO6", HUSTR_CHATMACRO6},
    {"HUSTR_CHATMACRO7", HUSTR_CHATMACRO7},
    {"HUSTR_CHATMACRO8", HUSTR_CHATMACRO8},
    {"HUSTR_CHATMACRO9", HUSTR_CHATMACRO9},
    {"HUSTR_CHATMACRO0", HUSTR_CHATMACRO0},
    {"HUSTR_TALKTOSELF1", HUSTR_TALKTOSELF1},
    {"HUSTR_TALKTOSELF2", HUSTR_TALKTOSELF2},
    {"HUSTR_TALKTOSELF3", HUSTR_TALKTOSELF3},
    {"HUSTR_TALKTOSELF4", HUSTR_TALKTOSELF4},
    {"HUSTR_TALKTOSELF5", HUSTR_TALKTOSELF5},
    {"HUSTR_MESSAGESENT", HUSTR_MESSAGESENT},
    {"HUSTR_PLRGREEN", HUSTR_PLRGREEN},
    {"HUSTR_PLRINDIGO", HUSTR_PLRINDIGO},
    {"HUSTR_PLRBROWN", HUSTR_PLRBROWN},
    {"HUSTR_PLRRED", HUSTR_PLRRED},
    {"AMSTR_FOLLOWON", AMSTR_FOLLOWON},
    {"AMSTR_FOLLOWOFF", AMSTR_FOLLOWOFF},
    {"AMSTR_GRIDON", AMSTR_GRIDON},
    {"AMSTR_GRIDOFF", AMSTR_GRIDOFF},
    {"AMSTR_MARKEDSPOT", AMSTR_MARKEDSPOT},
    {"AMSTR_MARKSCLEARED", AMSTR_MARKSCLEARED},
    {"STSTR_MUS", STSTR_MUS},
    {"STSTR_NOMUS", STSTR_NOMUS},
    {"STSTR_DQDON", STSTR_DQDON},
    {"STSTR_DQDOFF", STSTR_DQDOFF},
    {"STSTR_KFAADDED", STSTR_KFAADDED},
    {"STSTR_FAADDED", STSTR_FAADDED},
    {"STSTR_NCON", STSTR_NCON},
    {"STSTR_NCOFF", STSTR_NCOFF},
    {"STSTR_BEHOLD", STSTR_BEHOLD},
    {"STSTR_BEHOLDX", STSTR_BEHOLDX},
    {"STSTR_CHOPPERS", STSTR_CHOPPERS},
    {"STSTR_CLEV", STSTR_CLEV},
    {"E1TEXT", E1TEXT},
    {"E2TEXT", E2TEXT},
    {"E3TEXT", E3TEXT},
    {"E4TEXT", E4TEXT},
    {"C1TEXT", C1TEXT},
    {"C2TEXT", C2TEXT},
    {"C3TEXT", C3TEXT},
    {"C4TEXT", C4TEXT},
    {"C5TEXT", C5TEXT},
    {"C6TEXT", C6TEXT},
    {"P1TEXT", P1TEXT},
    {"P2TEXT", P2TEXT},
    {"P3TEXT", P3TEXT},
    {"P4TEXT", P4TEXT},
    {"P5TEXT", P5TEXT},
    {"P6TEXT", P6TEXT},
    {"T1TEXT", T1TEXT},
    {"T2TEXT", T2TEXT},
    {"T3TEXT", T3TEXT},
    {"T4TEXT", T4TEXT},
    {"T5TEXT", T5TEXT},
    {"T6TEXT", T6TEXT},
    {"CC_ZOMBIE", CC_ZOMBIE},
    {"CC_SHOTGUN", CC_SHOTGUN},
    {"CC_HEAVY", CC_HEAVY},
    {"CC_IMP", CC_IMP},
    {"CC_DEMON", CC_DEMON},
    {"CC_LOST", CC_LOST},
    {"CC_CACO", CC_CACO},
    {"CC_HELL", CC_HELL},
    {"CC_BARON", CC_BARON},
    {"CC_ARACH", CC_ARACH},
    {"CC_PAIN", CC_PAIN},
    {"CC_REVEN", CC_REVEN},
    {"CC_MANCU", CC_MANCU},
    {"CC_ARCH", CC_ARCH},
    {"CC_SPIDER", CC_SPIDER},
    {"CC_CYBER", CC_CYBER},
    {"CC_HERO", CC_HERO},
    {NULL, NULL}
};


// Code pointers by name, for [CODEPTR]
void A_Detonate ();
void A_Mushroom ();
void A_Die ();
void A_Spawn ();
void A_Turn ();
void A_Face ();
void A_Scratch ();
void A_PlaySound ();
void A_RandomJump ();
void A_LineEffect ();
void A_FireOldBFG ();
void A_BetaSkullAttack ();
void A_Stop ();
void A_SpawnObject ();
void A_MonsterProjectile ();
void A_MonsterBulletAttack ();
void A_MonsterMeleeAttack ();
void A_RadiusDamage ();
void A_NoiseAlert ();
void A_HealChase ();
void A_SeekTracer ();
void A_FindTracer ();
void A_ClearTracer ();
void A_JumpIfHealthBelow ();
void A_JumpIfTargetInSight ();
void A_JumpIfTargetCloser ();
void A_JumpIfTracerInSight ();
void A_JumpIfTracerCloser ();
void A_JumpIfFlagsSet ();
void A_AddFlags ();
void A_RemoveFlags ();
void A_WeaponProjectile ();
void A_WeaponBulletAttack ();
void A_WeaponMeleeAttack ();
void A_WeaponSound ();
void A_WeaponAlert ();
void A_WeaponJump ();
void A_ConsumeAmmo ();
void A_CheckAmmo ();
void A_RefireTo ();
void A_GunFlashTo ();
void A_BFGSpray ();
void A_BFGsound ();
void A_BabyMetal ();
void A_BossDeath ();
void A_BrainAwake ();
void A_BrainDie ();
void A_BrainExplode ();
void A_BrainPain ();
void A_BrainScream ();
void A_BrainSpit ();
void A_BruisAttack ();
void A_BspiAttack ();
void A_CPosAttack ();
void A_CPosRefire ();
void A_Chase ();
void A_CheckReload ();
void A_CloseShotgun2 ();
void A_CyberAttack ();
void A_Explode ();
void A_FaceTarget ();
void A_Fall ();
void A_FatAttack1 ();
void A_FatAttack2 ();
void A_FatAttack3 ();
void A_FatRaise ();
void A_FireBFG ();
void A_FireCGun ();
void A_FireCrackle ();
void A_FireMissile ();
void A_FirePistol ();
void A_FirePlasma ();
void A_FireShotgun2 ();
void A_FireShotgun ();
void A_Fire ();
void A_GunFlash ();
void A_HeadAttack ();
void A_Hoof ();
void A_KeenDie ();
void A_Light0 ();
void A_Light1 ();
void A_Light2 ();
void A_LoadShotgun2 ();
void A_Look ();
void A_Lower ();
void A_Metal ();
void A_OpenShotgun2 ();
void A_PainAttack ();
void A_PainDie ();
void A_Pain ();
void A_PlayerScream ();
void A_PosAttack ();
void A_Punch ();
void A_Raise ();
void A_ReFire ();
void A_SPosAttack ();
void A_SargAttack ();
void A_Saw ();
void A_Scream ();
void A_SkelFist ();
void A_SkelMissile ();
void A_SkelWhoosh ();
void A_SkullAttack ();
void A_SpawnFly ();
void A_SpawnSound ();
void A_SpidRefire ();
void A_StartFire ();
void A_Tracer ();
void A_TroopAttack ();
void A_VileAttack ();
void A_VileChase ();
void A_VileStart ();
void A_VileTarget ();
void A_WeaponReady ();
void A_XScream ();

// with, for MBF21's, how many of a frame's args it takes and what they are
// when a patch leaves them unset
static const struct
{
    const char*	name;
    void	(*fn) ();
    int		argcount;
    int		args[MAXSTATEARGS];
} codeptrs[] =
{
    {"BFGSpray", A_BFGSpray},
    {"BFGsound", A_BFGsound},
    {"BabyMetal", A_BabyMetal},
    {"BossDeath", A_BossDeath},
    {"BrainAwake", A_BrainAwake},
    {"BrainDie", A_BrainDie},
    {"BrainExplode", A_BrainExplode},
    {"BrainPain", A_BrainPain},
    {"BrainScream", A_BrainScream},
    {"BrainSpit", A_BrainSpit},
    {"BruisAttack", A_BruisAttack},
    {"BspiAttack", A_BspiAttack},
    {"CPosAttack", A_CPosAttack},
    {"CPosRefire", A_CPosRefire},
    {"Chase", A_Chase},
    {"CheckReload", A_CheckReload},
    {"CloseShotgun2", A_CloseShotgun2},
    {"CyberAttack", A_CyberAttack},
    {"Explode", A_Explode},
    {"FaceTarget", A_FaceTarget},
    {"Fall", A_Fall},
    {"FatAttack1", A_FatAttack1},
    {"FatAttack2", A_FatAttack2},
    {"FatAttack3", A_FatAttack3},
    {"FatRaise", A_FatRaise},
    {"FireBFG", A_FireBFG},
    {"FireCGun", A_FireCGun},
    {"FireCrackle", A_FireCrackle},
    {"FireMissile", A_FireMissile},
    {"FirePistol", A_FirePistol},
    {"FirePlasma", A_FirePlasma},
    {"FireShotgun2", A_FireShotgun2},
    {"FireShotgun", A_FireShotgun},
    {"Fire", A_Fire},
    {"GunFlash", A_GunFlash},
    {"HeadAttack", A_HeadAttack},
    {"Hoof", A_Hoof},
    {"KeenDie", A_KeenDie},
    {"Light0", A_Light0},
    {"Light1", A_Light1},
    {"Light2", A_Light2},
    {"LoadShotgun2", A_LoadShotgun2},
    {"Look", A_Look},
    {"Lower", A_Lower},
    {"Metal", A_Metal},
    {"OpenShotgun2", A_OpenShotgun2},
    {"PainAttack", A_PainAttack},
    {"PainDie", A_PainDie},
    {"Pain", A_Pain},
    {"PlayerScream", A_PlayerScream},
    {"PosAttack", A_PosAttack},
    {"Punch", A_Punch},
    {"Raise", A_Raise},
    {"ReFire", A_ReFire},
    {"SPosAttack", A_SPosAttack},
    {"SargAttack", A_SargAttack},
    {"Saw", A_Saw},
    {"Scream", A_Scream},
    {"SkelFist", A_SkelFist},
    {"SkelMissile", A_SkelMissile},
    {"SkelWhoosh", A_SkelWhoosh},
    {"SkullAttack", A_SkullAttack},
    {"SpawnFly", A_SpawnFly},
    {"SpawnSound", A_SpawnSound},
    {"SpidRefire", A_SpidRefire},
    {"StartFire", A_StartFire},
    {"Tracer", A_Tracer},
    {"TroopAttack", A_TroopAttack},
    {"VileAttack", A_VileAttack},
    {"VileChase", A_VileChase},
    {"VileStart", A_VileStart},
    {"VileTarget", A_VileTarget},
    {"WeaponReady", A_WeaponReady},
    {"XScream", A_XScream},
    // MBF's
    {"Detonate", A_Detonate},
    {"Mushroom", A_Mushroom},
    {"Die", A_Die},
    {"Spawn", A_Spawn},
    {"Turn", A_Turn},
    {"Face", A_Face},
    {"Scratch", A_Scratch},
    {"PlaySound", A_PlaySound},
    {"RandomJump", A_RandomJump},
    {"LineEffect", A_LineEffect},
    {"FireOldBFG", A_FireOldBFG},
    {"BetaSkullAttack", A_BetaSkullAttack},
    {"Stop", A_Stop},
    // MBF21's
    {"SpawnObject", A_SpawnObject, 8},
    {"MonsterProjectile", A_MonsterProjectile, 5},
    {"MonsterBulletAttack", A_MonsterBulletAttack, 5, {0, 0, 1, 3, 5}},
    {"MonsterMeleeAttack", A_MonsterMeleeAttack, 4, {3, 8, 0, 0}},
    {"RadiusDamage", A_RadiusDamage, 2},
    {"NoiseAlert", A_NoiseAlert, 0},
    {"HealChase", A_HealChase, 2},
    {"SeekTracer", A_SeekTracer, 2},
    {"FindTracer", A_FindTracer, 2, {0, 10}},
    {"ClearTracer", A_ClearTracer, 0},
    {"JumpIfHealthBelow", A_JumpIfHealthBelow, 2},
    {"JumpIfTargetInSight", A_JumpIfTargetInSight, 2},
    {"JumpIfTargetCloser", A_JumpIfTargetCloser, 2},
    {"JumpIfTracerInSight", A_JumpIfTracerInSight, 2},
    {"JumpIfTracerCloser", A_JumpIfTracerCloser, 2},
    {"JumpIfFlagsSet", A_JumpIfFlagsSet, 3},
    {"AddFlags", A_AddFlags, 2},
    {"RemoveFlags", A_RemoveFlags, 2},
    {"WeaponProjectile", A_WeaponProjectile, 5},
    {"WeaponBulletAttack", A_WeaponBulletAttack, 5, {0, 0, 1, 5, 3}},
    {"WeaponMeleeAttack", A_WeaponMeleeAttack, 5, {2, 10, FRACUNIT, 0, 0}},
    {"WeaponSound", A_WeaponSound, 2},
    {"WeaponAlert", A_WeaponAlert, 0},
    {"WeaponJump", A_WeaponJump, 2},
    {"ConsumeAmmo", A_ConsumeAmmo, 1},
    {"CheckAmmo", A_CheckAmmo, 2},
    {"RefireTo", A_RefireTo, 2},
    {"GunFlashTo", A_GunFlashTo, 2},
    {NULL, NULL}
};


// Thing flags by name, as BEX allows for Bits
static const struct { const char* name; int bit; } thingflags[] =
{
    {"SPECIAL", MF_SPECIAL},
    {"SOLID", MF_SOLID},
    {"SHOOTABLE", MF_SHOOTABLE},
    {"NOSECTOR", MF_NOSECTOR},
    {"NOBLOCKMAP", MF_NOBLOCKMAP},
    {"AMBUSH", MF_AMBUSH},
    {"JUSTHIT", MF_JUSTHIT},
    {"JUSTATTACKED", MF_JUSTATTACKED},
    {"SPAWNCEILING", MF_SPAWNCEILING},
    {"NOGRAVITY", MF_NOGRAVITY},
    {"DROPOFF", MF_DROPOFF},
    {"PICKUP", MF_PICKUP},
    {"NOCLIP", MF_NOCLIP},
    {"SLIDE", MF_SLIDE},
    {"FLOAT", MF_FLOAT},
    {"TELEPORT", MF_TELEPORT},
    {"MISSILE", MF_MISSILE},
    {"DROPPED", MF_DROPPED},
    {"SHADOW", MF_SHADOW},
    {"NOBLOOD", MF_NOBLOOD},
    {"CORPSE", MF_CORPSE},
    {"INFLOAT", MF_INFLOAT},
    {"COUNTKILL", MF_COUNTKILL},
    {"COUNTITEM", MF_COUNTITEM},
    {"SKULLFLY", MF_SKULLFLY},
    {"NOTDMATCH", MF_NOTDMATCH},
    {"TRANSLATION", MF_TRANSLATION},
    {"TRANSLATION1", 1 << MF_TRANSSHIFT},
    {"TRANSLATION2", 2 << MF_TRANSSHIFT},
    // MBF's
    {"TOUCHY", MF_TOUCHY},
    {"BOUNCES", MF_BOUNCES},
    {"FRIEND", MF_FRIEND},
    {"TRANSLUCENT", MF_TRANSLUCENT},
    {NULL, 0}
};

// MBF21's: a thing's, a frame's and a weapon's own
static const struct { const char* name; int bit; } mbf21flags[] =
{
    {"LOGRAV", MF2_LOGRAV},
    {"SHORTMRANGE", MF2_SHORTMRANGE},
    {"DMGIGNORED", MF2_DMGIGNORED},
    {"NORADIUSDMG", MF2_NORADIUSDMG},
    {"FORCERADIUSDMG", MF2_FORCERADIUSDMG},
    {"HIGHERMPROB", MF2_HIGHERMPROB},
    {"RANGEHALF", MF2_RANGEHALF},
    {"NOTHRESHOLD", MF2_NOTHRESHOLD},
    {"LONGMELEE", MF2_LONGMELEE},
    {"BOSS", MF2_BOSS},
    {"MAP07BOSS1", MF2_MAP07BOSS1},
    {"MAP07BOSS2", MF2_MAP07BOSS2},
    {"E1M8BOSS", MF2_E1M8BOSS},
    {"E2M8BOSS", MF2_E2M8BOSS},
    {"E3M8BOSS", MF2_E3M8BOSS},
    {"E4M6BOSS", MF2_E4M6BOSS},
    {"E4M8BOSS", MF2_E4M8BOSS},
    {"RIP", MF2_RIP},
    {"FULLVOLSOUNDS", MF2_FULLVOLSOUNDS},
    {NULL, 0}
}, frameflags[] =
{
    {"SKILL5FAST", STATEF_SKILL5FAST},
    {NULL, 0}
}, weaponflags[] =
{
    {"NOTHRUST", WPF_NOTHRUST},
    {"SILENT", WPF_SILENT},
    {"NOAUTOFIRE", WPF_NOAUTOFIRE},
    {"FLEEMELEE", WPF_FLEEMELEE},
    {"AUTOSWITCHFROM", WPF_AUTOSWITCHFROM},
    {"NOAUTOSWITCHTO", WPF_NOAUTOSWITCHTO},
    {NULL, 0}
};


//
// The patch being read
//
static const char*	dehname;	// for the log
static int		dehline;
static int		dehchanges;
static actionf_t*	origaction;	// each frame's before any patch
static int		numorigaction;
static unsigned char*	argsset;	// which of a frame's args a patch set
static int		numargsset;

static void D_Warn (const char* fmt, ...)
{
    va_list	ap;

    printf ("DEHACKED: %s, line %d: ", dehname, dehline);
    va_start (ap, fmt);
    vprintf (fmt, ap);
    va_end (ap);
    printf ("\n");
}

static char* D_Trim (char* s)
{
    char*	e;

    while (isspace ((unsigned char) *s))
	s++;
    e = s + strlen (s);
    while (e > s && isspace ((unsigned char) e[-1]))
	*--e = 0;
    return s;
}

static int D_Number (const char* s)
{
    return (int) strtol (s, NULL, 10);
}

// Bits: a number, or BEX's flag names joined by +, |, commas or spaces
typedef struct { const char* name; int bit; } dehflag_t;

static int D_BitsIn (const char* s, const dehflag_t* flags, const char* what)
{
    char	word[32];
    int		bits = 0;
    int		i, n;

    while (isspace ((unsigned char) *s))
	s++;
    if (isdigit ((unsigned char) *s) || *s == '-')
	return D_Number (s);

    while (*s)
    {
	while (*s && (isspace ((unsigned char) *s) || *s == '+' || *s == '|'
		      || *s == ','))
	    s++;
	for (n = 0; *s && !isspace ((unsigned char) *s) && *s != '+'
		 && *s != '|' && *s != ','; s++)
	    if (n < (int) sizeof(word) - 1)
		word[n++] = toupper ((unsigned char) *s);
	word[n] = 0;
	if (!n)
	    break;
	for (i = 0; flags[i].name; i++)
	    if (!strcmp (word, flags[i].name))
		break;
	if (flags[i].name)
	    bits |= flags[i].bit;
	else
	    D_Warn ("%s %s is not one known here; left out", what, word);
    }
    return bits;
}

#define D_Bits(s)	D_BitsIn (s, (const dehflag_t *) thingflags, "thing flag")

// A frame, sound, sprite or thing that a field names is made if it is past
// the table's end (DSDHacked): the field then points at something.
static int D_StateRef (int x)
{
    if (D_GrowStates (x))
	return x;
    D_Warn ("no frame %d can be; frame 0 instead", x);
    return 0;
}

static int D_SoundRef (int x)
{
    if (x == 0 || D_GrowSounds (x))
	return x;
    D_Warn ("no sound %d can be; none instead", x);
    return 0;
}

static int D_SpriteRef (int x)
{
    if (D_GrowSprites (x))
	return x;
    D_Warn ("no sprite %d can be; sprite 0 instead", x);
    return 0;
}

static int D_ThingRef (int x)	// from 1, as patches number things
{
    if (x >= 1 && D_GrowThings (x - 1))
	return x - 1;
    return MT_NULL;
}


//
// Key = value, in each section
//
static void D_Thing (int n, const char* key, const char* v)
{
    mobjinfo_t*	m;
    int		x = D_Number (v);

    // a thing this names may grow the table (as in D_Frame)
    if (!strcasecmp (key, "Dropped item"))
	x = D_ThingRef (x);
    m = &mobjinfo[n];

    if (!strcasecmp (key, "ID #"))			m->doomednum = x;
    else if (!strcasecmp (key, "Initial frame"))	m->spawnstate = D_StateRef (x);
    else if (!strcasecmp (key, "Hit points"))		m->spawnhealth = x;
    else if (!strcasecmp (key, "First moving frame"))	m->seestate = D_StateRef (x);
    else if (!strcasecmp (key, "Alert sound"))		m->seesound = D_SoundRef (x);
    else if (!strcasecmp (key, "Reaction time"))	m->reactiontime = x;
    else if (!strcasecmp (key, "Attack sound"))		m->attacksound = D_SoundRef (x);
    else if (!strcasecmp (key, "Injury frame"))		m->painstate = D_StateRef (x);
    else if (!strcasecmp (key, "Pain chance"))		m->painchance = x;
    else if (!strcasecmp (key, "Pain sound"))		m->painsound = D_SoundRef (x);
    else if (!strcasecmp (key, "Close attack frame"))	m->meleestate = D_StateRef (x);
    else if (!strcasecmp (key, "Far attack frame"))	m->missilestate = D_StateRef (x);
    else if (!strcasecmp (key, "Death frame"))		m->deathstate = D_StateRef (x);
    else if (!strcasecmp (key, "Exploding frame"))	m->xdeathstate = D_StateRef (x);
    else if (!strcasecmp (key, "Death sound"))		m->deathsound = D_SoundRef (x);
    else if (!strcasecmp (key, "Speed"))		m->speed = x;
    else if (!strcasecmp (key, "Width"))		m->radius = x;
    else if (!strcasecmp (key, "Height"))		m->height = x;
    else if (!strcasecmp (key, "Mass"))			m->mass = x;
    else if (!strcasecmp (key, "Missile damage"))	m->damage = x;
    else if (!strcasecmp (key, "Action sound"))		m->activesound = D_SoundRef (x);
    else if (!strcasecmp (key, "Bits"))			m->flags = D_Bits (v);
    else if (!strcasecmp (key, "Respawn frame"))	m->raisestate = D_StateRef (x);
    // DEHEXTRA's
    else if (!strcasecmp (key, "Dropped item"))		m->droppeditem = x;
    // MBF21's
    else if (!strcasecmp (key, "MBF21 Bits"))
	m->flags2 = D_BitsIn (v, (const dehflag_t *) mbf21flags, "MBF21 flag");
    else if (!strcasecmp (key, "Infighting group"))
	m->infighting_group = x < 0 ? IG_DEFAULT : x + IG_END;
    else if (!strcasecmp (key, "Projectile group"))
	m->projectile_group = x < 0 ? PG_GROUPLESS : x + PG_END;
    else if (!strcasecmp (key, "Splash group"))
	m->splash_group = x < 0 ? SG_DEFAULT : x + SG_END;
    else if (!strcasecmp (key, "Rip sound"))		m->ripsound = D_SoundRef (x);
    else if (!strcasecmp (key, "Fast speed"))		m->altspeed = x;
    else if (!strcasecmp (key, "Melee range"))		m->meleerange = x;
    else
    {
	D_Warn ("Thing has no \"%s\"; left out", key);
	return;
    }
    dehchanges++;
}

static void D_Frame (int n, const char* key, const char* v)
{
    state_t*	s;
    int		x = D_Number (v);

    // A frame or sprite this names may grow its table, which can move it:
    // the entry is found once that is done.
    if (!strcasecmp (key, "Next frame"))
	x = D_StateRef (x);
    else if (!strcasecmp (key, "Sprite number"))
	x = D_SpriteRef (x);
    s = &states[n];

    if (!strcasecmp (key, "Sprite number"))		s->sprite = x;
    else if (!strcasecmp (key, "Sprite subnumber"))	s->frame = x;
    else if (!strcasecmp (key, "Duration"))		s->tics = x;
    else if (!strcasecmp (key, "Next frame"))		s->nextstate = x;
    else if (!strcasecmp (key, "Unknown 1"))		s->misc1 = x;
    else if (!strcasecmp (key, "Unknown 2"))		s->misc2 = x;
    else if (!strcasecmp (key, "Codep frame"))
    {
	if (x < 0 || x >= numstates)
	{
	    D_Warn ("no frame %d to take a code pointer from", x);
	    return;
	}
	// a frame past id's had none before any patch
	if (x < numorigaction)
	    s->action = origaction[x];
	else
	    s->action.acv = NULL;
    }
    // MBF21's
    else if (!strncasecmp (key, "Args", 4) && key[4] >= '1' && key[4] <= '8'
	     && !key[5])
    {
	int	k = key[4] - '1';

	s->args[k] = x;
	if (numargsset < numstates)
	{
	    argsset = realloc (argsset, numstates);
	    if (!argsset)
		I_Error ("DEHACKED: out of memory");
	    memset (argsset + numargsset, 0, numstates - numargsset);
	    numargsset = numstates;
	}
	argsset[n] |= 1 << k;
    }
    else if (!strcasecmp (key, "MBF21 Bits"))
	s->flags = D_BitsIn (v, (const dehflag_t *) frameflags, "frame flag");
    else
    {
	D_Warn ("Frame has no \"%s\"; left out", key);
	return;
    }
    dehchanges++;
}

static void D_Weapon (int n, const char* key, const char* v)
{
    weaponinfo_t*	w = &weaponinfo[n];
    int			x = D_Number (v);

    if (!strcasecmp (key, "Ammo type"))			w->ammo = x;
    else if (!strcasecmp (key, "Deselect frame"))	w->upstate = D_StateRef (x);
    else if (!strcasecmp (key, "Select frame"))		w->downstate = D_StateRef (x);
    else if (!strcasecmp (key, "Bobbing frame"))	w->readystate = D_StateRef (x);
    else if (!strcasecmp (key, "Shooting frame"))	w->atkstate = D_StateRef (x);
    else if (!strcasecmp (key, "Firing frame"))		w->flashstate = D_StateRef (x);
    // MBF21's
    else if (!strcasecmp (key, "Ammo per shot"))
    {
	w->ammopershot = x;
	w->intflags |= WIF_ENABLEAPS;
    }
    else if (!strcasecmp (key, "MBF21 Bits"))
	w->flags = D_BitsIn (v, (const dehflag_t *) weaponflags, "weapon flag");
    else
    {
	D_Warn ("Weapon has no \"%s\"; left out", key);
	return;
    }
    dehchanges++;
}

static void D_Ammo (int n, const char* key, const char* v)
{
    int		x = D_Number (v);

    if (!strcasecmp (key, "Max ammo"))			maxammo[n] = x;
    else if (!strcasecmp (key, "Per ammo"))		clipammo[n] = x;
    else
    {
	D_Warn ("Ammo has no \"%s\"; left out", key);
	return;
    }
    dehchanges++;
}

static void D_Sound (int n, const char* key, const char* v)
{
    sfxinfo_t*	s = &S_sfx[n];
    int		x = D_Number (v);

    // DeHackEd's other fields are where the executable kept the name, the
    // data, the link: nothing a port has a use for.
    if (!strcasecmp (key, "Zero/One"))			s->singularity = x;
    else if (!strcasecmp (key, "Value"))		s->priority = x;
    else
	return;
    dehchanges++;
}

static struct { const char* key; int* var; } miscs[] =
{
    {"Initial Health", &deh_initial_health},
    {"Initial Bullets", &deh_initial_bullets},
    {"Max Health", &deh_max_health},
    {"Max Armor", &deh_max_armor},
    {"Green Armor Class", &deh_green_armor_class},
    {"Blue Armor Class", &deh_blue_armor_class},
    {"Max Soulsphere", &deh_max_soulsphere},
    {"Soulsphere Health", &deh_soulsphere_health},
    {"Megasphere Health", &deh_megasphere_health},
    {"God Mode Health", &deh_god_mode_health},
    {"IDFA Armor", &deh_idfa_armor},
    {"IDFA Armor Class", &deh_idfa_armor_class},
    {"IDKFA Armor", &deh_idkfa_armor},
    {"IDKFA Armor Class", &deh_idkfa_armor_class},
    {"BFG Cells/Shot", &deh_bfg_cells_per_shot},
    {NULL, NULL}
};

static void D_Misc (const char* key, const char* v)
{
    int		i;

    if (!strcasecmp (key, "Monsters Infight"))
    {
	// 221 on, 202 off, as DeHackEd wrote them
	deh_species_infighting = D_Number (v) == 221;
	dehchanges++;
	return;
    }
    for (i = 0; miscs[i].key; i++)
	if (!strcasecmp (key, miscs[i].key))
	{
	    *miscs[i].var = D_Number (v);
	    dehchanges++;
	    return;
	}
    D_Warn ("Misc has no \"%s\"; left out", key);
}


//
// Names: a sprite (4 letters), a sound or a piece of music, renamed.
//
static char* D_Upper (const char* s)
{
    char*	c = strdup (s);
    char*	p;

    for (p = c; *p; p++)
	*p = toupper ((unsigned char) *p);
    return c;
}

static char* D_Lower (const char* s)
{
    char*	c = strdup (s);
    char*	p;

    for (p = c; *p; p++)
	*p = tolower ((unsigned char) *p);
    return c;
}

static boolean D_RenameSprite (const char* from, const char* to)
{
    int		i;

    if (strlen (from) != 4 || strlen (to) != 4)
	return false;
    for (i = 0; i < numspritenames; i++)
	if (sprnames[i] && !strcasecmp (sprnames[i], from))
	{
	    sprnames[i] = D_Upper (to);
	    dehchanges++;
	    return true;
	}
    return false;
}

static boolean D_RenameSound (const char* from, const char* to)
{
    int		i;

    if (strlen (to) > 6)
	return false;
    for (i = 1; i < numsfx; i++)
	if (S_sfx[i].name && !strcasecmp (S_sfx[i].name, from))
	{
	    S_sfx[i].name = D_Lower (to);
	    dehchanges++;
	    return true;
	}
    return false;
}

static boolean D_RenameMusic (const char* from, const char* to)
{
    int		i;

    if (strlen (to) > 6)
	return false;
    for (i = 1; i < NUMMUSIC; i++)
	if (S_music[i].name && !strcasecmp (S_music[i].name, from))
	{
	    S_music[i].name = D_Lower (to);
	    dehchanges++;
	    return true;
	}
    return false;
}


//
// BEX's sections, a line at a time
//
static void D_BexString (const char* name, const char* value)
{
    int		i;
    boolean	found = false;

    // A text can be the same under two names; each gets the new one.
    for (i = 0; bexstrings[i].name; i++)
	if (!strcasecmp (name, bexstrings[i].name))
	{
	    D_SetText (bexstrings[i].text, value);
	    found = true;
	}
    if (found)
	dehchanges++;
    else
	D_Warn ("[STRINGS] has no %s; left out", name);
}

static void D_BexCodeptr (const char* key, const char* v)
{
    int		frame, i;
    const char*	name = v;

    if (strncasecmp (key, "Frame", 5) || sscanf (key + 5, "%d", &frame) != 1)
    {
	D_Warn ("[CODEPTR] expects \"FRAME n = name\"");
	return;
    }
    if (!D_GrowStates (frame))
    {
	D_Warn ("no frame %d can be; left out", frame);
	return;
    }
    if (!strncasecmp (name, "A_", 2))
	name += 2;
    if (!strcasecmp (name, "NULL"))
    {
	states[frame].action.acv = NULL;
	dehchanges++;
	return;
    }
    for (i = 0; codeptrs[i].name; i++)
	if (!strcasecmp (name, codeptrs[i].name))
	{
	    states[frame].action.acv = (actionf_v) codeptrs[i].fn;
	    dehchanges++;
	    return;
	}
    D_Warn ("code pointer %s is not one known here; left out", v);
}


//
// A C-style string from BEX: \n and \\ in it
//
static void D_Unescape (char* s)
{
    char*	o = s;

    for (; *s; s++)
    {
	if (*s == '\\' && s[1] == 'n')
	    *o++ = '\n', s++;
	else if (*s == '\\' && s[1] == '\\')
	    *o++ = '\\', s++;
	else
	    *o++ = *s;
    }
    *o = 0;
}


//
// D_ProcessDeh
// One patch, from a file or a lump.
//
typedef enum
{
    SEC_NONE, SEC_THING, SEC_FRAME, SEC_WEAPON, SEC_AMMO, SEC_SOUND,
    SEC_MISC, SEC_POINTER, SEC_IGNORE,
    SEC_STRINGS, SEC_CODEPTR, SEC_PARS, SEC_SPRITES, SEC_SOUNDS, SEC_MUSIC
} dehsec_t;

static void D_ProcessDeh (char* text, const char* name, boolean lump)
{
    char*	p = text;
    dehsec_t	sec = SEC_NONE;
    int		index = 0;
    static boolean	saidcheat, saidhelper, saidinclude;

    dehname = name;
    dehline = 0;
    dehchanges = 0;

    if (!origaction)
    {
	int	i;

	numorigaction = numstates;
	origaction = malloc (numorigaction * sizeof(*origaction));
	if (!origaction)
	    I_Error ("DEHACKED: out of memory");
	for (i = 0; i < numorigaction; i++)
	    origaction[i] = states[i].action;
    }

    while (*p)
    {
	char*	line = p;
	char*	eq;
	char*	key;
	char*	val;
	char	word[16];
	int	a, b;

	// one line, without its end
	while (*p && *p != '\n')
	    p++;
	if (*p)
	    *p++ = 0;
	dehline++;
	if (strchr (line, '\r'))
	    *strchr (line, '\r') = 0;

	key = D_Trim (line);
	if (!*key || *key == '#')
	    continue;

	// "Text old new", then that many characters, lines and all
	if (sscanf (key, "Text %d %d", &a, &b) == 2)
	{
	    char*	from = malloc (a + 1);
	    char*	to = malloc (b + 1);
	    int		n = 0;

	    if (!from || !to)
		I_Error ("DEHACKED: out of memory");
	    while (n < a + b && *p)
	    {
		if (*p == '\r')
		{
		    p++;
		    continue;
		}
		if (*p == '\n')
		    dehline++;
		if (n < a)
		    from[n] = *p;
		else
		    to[n - a] = *p;
		n++;
		p++;
	    }
	    from[a < n ? a : n] = 0;
	    to[n > a ? n - a : 0] = 0;

	    if (!D_RenameSprite (from, to) && !D_RenameSound (from, to)
		&& !D_RenameMusic (from, to))
	    {
		D_SetText (from, to);
		dehchanges++;
	    }
	    free (from);
	    free (to);
	    sec = SEC_NONE;
	    continue;
	}

	// a section's start
	if (*key == '[')
	{
	    if (!strncasecmp (key, "[STRINGS]", 9))	sec = SEC_STRINGS;
	    else if (!strncasecmp (key, "[CODEPTR]", 9))	sec = SEC_CODEPTR;
	    else if (!strncasecmp (key, "[PARS]", 6))	sec = SEC_PARS;
	    else if (!strncasecmp (key, "[SPRITES]", 9))	sec = SEC_SPRITES;
	    else if (!strncasecmp (key, "[SOUNDS]", 8))	sec = SEC_SOUNDS;
	    else if (!strncasecmp (key, "[MUSIC]", 7))	sec = SEC_MUSIC;
	    else
	    {
		if (!strncasecmp (key, "[HELPER]", 8) && !saidhelper)
		{
		    D_Warn ("[HELPER] (MBF's dog) is not supported");
		    saidhelper = true;
		}
		else if (strncasecmp (key, "[HELPER]", 8))
		    D_Warn ("%s is not a section known here; left out", key);
		sec = SEC_IGNORE;
	    }
	    continue;
	}

	if (!strchr (key, '=') && sscanf (key, "%15s %d", word, &a) == 2
	    && (!strcasecmp (word, "Thing") || !strcasecmp (word, "Frame")
		|| !strcasecmp (word, "Pointer") || !strcasecmp (word, "Weapon")
		|| !strcasecmp (word, "Ammo") || !strcasecmp (word, "Sound")
		|| !strcasecmp (word, "Misc") || !strcasecmp (word, "Cheat")
		|| !strcasecmp (word, "Sprite")))
	{
	    // Thing n (name), Frame n, Pointer n (Frame m), ...
	    sec = SEC_IGNORE;
	    // past the tables' ends, the tables grow (DSDHacked)
	    if (!strcasecmp (word, "Thing"))
	    {
		if (a >= 1 && D_GrowThings (a - 1))
		    sec = SEC_THING, index = a - 1;
		else
		    D_Warn ("no Thing %d can be; left out", a);
	    }
	    else if (!strcasecmp (word, "Frame"))
	    {
		if (D_GrowStates (a))
		    sec = SEC_FRAME, index = a;
		else
		    D_Warn ("no Frame %d can be; left out", a);
	    }
	    else if (!strcasecmp (word, "Pointer"))
	    {
		char*	f = strchr (key, '(');

		if (f && sscanf (f, "(Frame %d)", &b) == 1 && D_GrowStates (b))
		    sec = SEC_POINTER, index = b;
		else
		    D_Warn ("Pointer without a frame there can be; left out");
	    }
	    else if (!strcasecmp (word, "Weapon"))
	    {
		if (a >= 0 && a < NUMWEAPONS)
		    sec = SEC_WEAPON, index = a;
	    }
	    else if (!strcasecmp (word, "Ammo"))
	    {
		if (a >= 0 && a < NUMAMMO)
		    sec = SEC_AMMO, index = a;
	    }
	    else if (!strcasecmp (word, "Sound"))
	    {
		if (a >= 1 && D_GrowSounds (a))
		    sec = SEC_SOUND, index = a;
	    }
	    else if (!strcasecmp (word, "Misc"))
		sec = SEC_MISC;
	    else if (!strcasecmp (word, "Cheat"))
	    {
		if (!saidcheat)
		    D_Warn ("new cheat codes are not supported; id's work");
		saidcheat = true;
	    }
	    continue;
	}

	if (!strncasecmp (key, "INCLUDE", 7))
	{
	    if (!saidinclude)
		D_Warn ("INCLUDE is not supported; the file named is not read");
	    saidinclude = true;
	    continue;
	}

	eq = strchr (key, '=');
	if (!eq)
	{
	    // "Patch File for DeHackEd v3.0", "par 5 1 90" in [PARS]
	    continue;
	}
	*eq = 0;
	val = D_Trim (eq + 1);
	key = D_Trim (key);

	if (!strcasecmp (key, "Doom version") || !strcasecmp (key, "Patch format"))
	    continue;

	switch (sec)
	{
	  case SEC_THING:	D_Thing (index, key, val); break;
	  case SEC_FRAME:	D_Frame (index, key, val); break;
	  case SEC_POINTER:	D_Frame (index, key, val); break;
	  case SEC_WEAPON:	D_Weapon (index, key, val); break;
	  case SEC_AMMO:	D_Ammo (index, key, val); break;
	  case SEC_SOUND:	D_Sound (index, key, val); break;
	  case SEC_MISC:	D_Misc (key, val); break;
	  case SEC_CODEPTR:	D_BexCodeptr (key, val); break;
	  case SEC_STRINGS:
	    {
		// a value can go on over lines ending in a backslash
		char	buf[4096];

		snprintf (buf, sizeof(buf), "%s", val);
		while (strlen (buf) && buf[strlen (buf) - 1] == '\\' && *p)
		{
		    char*	more = p;

		    buf[strlen (buf) - 1] = 0;
		    while (*p && *p != '\n')
			p++;
		    if (*p)
			*p++ = 0;
		    dehline++;
		    if (strchr (more, '\r'))
			*strchr (more, '\r') = 0;
		    strncat (buf, D_Trim (more), sizeof(buf) - strlen (buf) - 1);
		}
		D_Unescape (buf);
		D_BexString (key, buf);
	    }
	    break;
	  case SEC_SPRITES:
	    // "SARG = DEMN", or DSDHacked's "245 = GHUL"
	    if (isdigit ((unsigned char) *key))
	    {
		if (strlen (val) == 4 && D_GrowSprites (atoi (key)))
		{
		    sprnames[atoi (key)] = D_Upper (val);
		    dehchanges++;
		}
		else
		    D_Warn ("no sprite %s to name %s", key, val);
	    }
	    else if (!D_RenameSprite (key, val))
		D_Warn ("no sprite %s to rename", key);
	    break;
	  case SEC_SOUNDS:
	    if (isdigit ((unsigned char) *key))
	    {
		if (strlen (val) <= 6 && atoi (key) >= 1
		    && D_GrowSounds (atoi (key)))
		{
		    S_sfx[atoi (key)].name = D_Lower (val);
		    dehchanges++;
		}
		else
		    D_Warn ("no sound %s to name %s", key, val);
	    }
	    else if (!D_RenameSound (key, val))
		D_Warn ("no sound %s to rename", key);
	    break;
	  case SEC_MUSIC:
	    if (!D_RenameMusic (key, val))
		D_Warn ("no music %s to rename", key);
	    break;
	  default:
	    break;
	}
    }

    printf ("DEHACKED: %s: %d change%s\n", name, dehchanges,
	    dehchanges == 1 ? "" : "s");
}


//
// D_LoadDehFile
//
static void D_LoadDehFile (const char* path)
{
    FILE*	f = fopen (path, "rb");
    long	len;
    char*	text;
    const char*	base = strrchr (path, '/');

    if (!f)
    {
	printf ("DEHACKED: cannot read %s\n", path);
	return;
    }
    fseek (f, 0, SEEK_END);
    len = ftell (f);
    fseek (f, 0, SEEK_SET);
    text = malloc (len + 1);
    if (!text)
	I_Error ("DEHACKED: out of memory");
    len = fread (text, 1, len, f);
    text[len] = 0;
    fclose (f);

    // par times go where the DEHACKED lumps' do
    U_ReadBexPars (text);
    D_ProcessDeh (text, base ? base + 1 : path, false);
    free (text);
}


//
// D_LoadDehacked
// After W_Init, before anything reads the tables: every WAD's DEHACKED lump
// in load order, then -deh's files (and -bex's, Boom's name for the same),
// and for each mod file a .deh or .bex of the same name beside it, which
// is how many mods of the 1990s came.
//
// MBF21's code pointers take args from their frame: those a patch left
// unset are the pointer's defaults, and a frame, thing or sound an arg (or
// misc1, for MBF's) names is made if it is past the table's end.
static void D_FinishDehacked (void)
{
    int		i, k, c;
    state_t*	st;
    long	misc1;
    int		args[MAXSTATEARGS];

    for (i = 0; i < numstates; i++)
    {
	st = &states[i];
	if (!st->action.acv)
	    continue;
	for (c = 0; codeptrs[c].name; c++)
	    if ((actionf_v) codeptrs[c].fn == st->action.acv)
		break;
	if (!codeptrs[c].name)
	    continue;
	for (k = 0; k < codeptrs[c].argcount; k++)
	    if (i >= numargsset || !(argsset[i] & (1 << k)))
		st->args[k] = codeptrs[c].args[k];

	// what the pointer names, copied out: growing a table can move it
	misc1 = st->misc1;
	memcpy (args, st->args, sizeof(args));

#define IS(n)	(!strcmp (codeptrs[c].name, n))
	if (IS ("RandomJump"))
	    D_GrowStates (misc1);
	else if (IS ("HealChase") || IS ("JumpIfHealthBelow")
		 || IS ("JumpIfTargetInSight") || IS ("JumpIfTargetCloser")
		 || IS ("JumpIfTracerInSight") || IS ("JumpIfTracerCloser")
		 || IS ("JumpIfFlagsSet") || IS ("WeaponJump")
		 || IS ("CheckAmmo") || IS ("RefireTo") || IS ("GunFlashTo"))
	    D_GrowStates (args[0]);
	if (IS ("Spawn"))
	    D_GrowThings (misc1 - 1);
	else if (IS ("SpawnObject") || IS ("MonsterProjectile")
		 || IS ("WeaponProjectile"))
	    D_GrowThings (args[0] - 1);
	if (IS ("PlaySound"))
	    D_GrowSounds (misc1);
	else if (IS ("MonsterMeleeAttack"))
	    D_GrowSounds (args[2]);
	else if (IS ("HealChase"))
	    D_GrowSounds (args[1]);
	else if (IS ("WeaponMeleeAttack"))
	    D_GrowSounds (args[3]);
	else if (IS ("WeaponSound"))
	    D_GrowSounds (args[0]);
#undef IS
    }
}

void D_LoadDehacked (void)
{
    int		i, p;

    for (i = 0; i < numlumps; i++)
    {
	char*	text;
	int	len;
	char	what[256];

	if (strncasecmp (lumpinfo[i].name, "DEHACKED", 8))
	    continue;
	len = W_LumpLength (i);
	text = malloc (len + 1);
	if (!text)
	    I_Error ("DEHACKED: out of memory");
	W_ReadLump (i, text);
	text[len] = 0;
	// named by the WAD it is in
	{
	    char	fd[32], path[1024];
	    ssize_t	n;

	    snprintf (fd, sizeof(fd), "/proc/self/fd/%d", lumpinfo[i].handle);
	    n = readlink (fd, path, sizeof(path) - 1);
	    if (n > 0)
	    {
		path[n] = 0;
		snprintf (what, sizeof(what), "%.200s",
			  strrchr (path, '/') ? strrchr (path, '/') + 1 : path);
	    }
	    else
		snprintf (what, sizeof(what), "lump %d", i);
	}
	D_ProcessDeh (text, what, true);
	free (text);
    }

    for (i = 1; wadfiles[i]; i++)
    {
	char		path[1024];
	const char*	exts[] = { ".deh", ".bex", ".DEH", ".BEX" };
	int		e;
	char*		dot;

	snprintf (path, sizeof(path), "%s", wadfiles[i]);
	dot = strrchr (path, '.');
	if (!dot || strchr (dot, '/'))
	    continue;
	for (e = 0; e < 4; e++)
	{
	    FILE*	f;

	    strcpy (dot, exts[e]);
	    if ((f = fopen (path, "rb")))
	    {
		fclose (f);
		D_LoadDehFile (path);
		break;
	    }
	}
    }

    for (p = 1; p < myargc; p++)
    {
	if (strcasecmp (myargv[p], "-deh") && strcasecmp (myargv[p], "-bex"))
	    continue;
	while (++p < myargc && myargv[p][0] != '-')
	    D_LoadDehFile (myargv[p]);
	p--;
    }

    D_FinishDehacked ();
}
