// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	UMAPINFO: the levels an add-on describes for itself.
//
//	Add-ons made since 2017 carry a lump named UMAPINFO that says what
//	each of their maps is called, what sky and music it has, where its
//	exits lead, what ends the game and what the new game menu offers.
//	The 2024 re-release's No Rest for the Living, Master Levels, SIGIL,
//	TNT and Plutonia all do. This reads it, to revision 2.2 of the
//	specification:
//
//	  https://github.com/kraflab/umapinfo/blob/master/docs/spec.md
//
//	Whatever a map's entry does not say, the game decides as it always
//	did.
//
//-----------------------------------------------------------------------------

#ifndef __U_MAPINFO__
#define __U_MAPINFO__

#include "doomtype.h"

// The most episodes the new game menu will list.
#define UM_MAXEPISODES	8

// How the game ends after a map, if it does.
typedef enum
{
    UM_END_UNSET,	// the entry does not say: the game's own rule
    UM_END_NONE,	// endgame = false: play on, even past an ExM8
    UM_END_DEFAULT,	// endgame = true: the game's own ending
    UM_END_PIC,		// endpic: a picture
    UM_END_BUNNY,	// endbunny: the scroller that ends episode 3
    UM_END_CAST		// endcast: DOOM II's cast of characters
} umending_t;

typedef struct
{
    int		type;		// mobjtype_t
    int		special;	// line special
    int		tag;
} umbossaction_t;

typedef struct
{
    int		episode;	// 1 in DOOM II, which has no episodes
    int		map;

    char*	levelname;	// NULL when not given
    char*	label;		// NULL: the map's name
    boolean	labelclear;	// label = clear: the level name alone
    char*	author;

    char	levelpic[9];	// lump names; empty when not given
    char	skytexture[9];
    char	music[9];
    char	exitpic[9];
    char	enterpic[9];
    char	endpic[9];
    char	interbackdrop[9];
    char	intermusic[9];

    int		nextepisode;	// next and nextsecret; 0 when not given
    int		nextmap;
    int		secretepisode;
    int		secretmap;

    int		partime;	// seconds; 0 when not given

    umending_t	ending;
    boolean	nointermission;

    // NULL when not given; "" for clear, which takes a default text away.
    char*	intertext;
    char*	intertextsecret;

    // Given at all, even as clear, these replace the game's own.
    boolean	bossactions_set;
    int		numbossactions;
    umbossaction_t* bossactions;
} umapentry_t;

typedef struct
{
    char	patch[9];
    char*	name;
    char	key;
    int		episode;	// where it starts
    int		map;
} umepisode_t;

// Episodes UMAPINFO adds to the new game menu: after the game's own, or in
// their place once one has said episode = clear.
extern int		um_numepisodes;
extern boolean		um_episodesclear;
extern umepisode_t	um_episodes[UM_MAXEPISODES];

// Reads every UMAPINFO lump, in the order the WADs were given; a later entry
// for a map replaces an earlier one. After W_InitMultipleFiles.
void U_Init (void);

// The entry for a map, or NULL.
umapentry_t* U_FindMap (int episode, int map);

// The entry for the map being played, or NULL.
umapentry_t* U_ThisMap (void);

// "E5M1" or "MAP21" to an episode and a map, in the form this game uses.
boolean U_ParseMapName (const char* name, int* episode, int* map);

// The other way: the map's lump name, into at least 9 bytes.
void U_MapName (char* buf, int episode, int map);

// A par time from a DEHACKED lump's [PARS] section, in seconds, or 0.
int U_BexPar (int episode, int map);

#endif
