// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//  ID24's INTERLEVEL lumps: an intermission's background, music, and
//  layers of animations, each shown when its conditions hold.
//
//-----------------------------------------------------------------------------

#ifndef __WI_INTERLVL__
#define __WI_INTERLVL__

typedef enum
{
    IC_NONE,
    IC_MAPGREATER,	// the level's number is greater than param
    IC_MAPEQUAL,	// it is param
    IC_MAPVISITED,	// level param has been played
    IC_MAPNOTSECRET,	// the level is not a secret one
    IC_SECRETVISITED,	// a secret level has been played
    IC_TALLY,		// on the tally, leaving a level
    IC_ENTERING		// on the screen that shows the next
} ilcondtype_t;

// frame types
#define IF_INFINITE	0x0001
#define IF_FIXED	0x0002
#define IF_RANDOM	0x0004
#define IF_RANDOMSTART	0x1000

typedef struct
{
    int		condition;
    int		param;
} ilcond_t;

typedef struct
{
    char	image[9];
    int		type;
    int		duration;	// tics
    int		maxduration;
} ilframe_t;

typedef struct
{
    int		x;
    int		y;
    ilframe_t*	frames;
    int		numframes;
    ilcond_t*	conds;
    int		numconds;
} ilanim_t;

typedef struct
{
    ilanim_t*	anims;
    int		numanims;
    ilcond_t*	conds;
    int		numconds;
} illayer_t;

typedef struct
{
    char	music[9];
    char	background[9];
    illayer_t*	layers;
    int		numlayers;
} interlevel_t;

// The lump's intermission, or NULL when it has none that can be read.
interlevel_t* WI_ParseInterlevel (const char* lump);
void WI_FreeInterlevel (interlevel_t* il);

#endif
