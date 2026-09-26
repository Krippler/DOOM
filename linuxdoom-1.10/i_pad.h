// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	A game controller, read by the engine itself: from the browser in the
//	container, through SDL2 on a desktop. See i_pad.c.
//
//-----------------------------------------------------------------------------

#ifndef __I_PAD__
#define __I_PAD__

#include "doomtype.h"
#include "d_ticcmd.h"

// The buttons, in the order of the browser's standard layout.
enum
{
    PB_A, PB_B, PB_X, PB_Y, PB_LB, PB_RB, PB_LT, PB_RT, PB_BACK, PB_START,
    PB_LS, PB_RS, PB_UP, PB_DOWN, PB_LEFT, PB_RIGHT, PB_GUIDE,
    PB_COUNT
};

// What a button can be set to do in the game.
enum
{
    PA_NONE,
    PA_FIRE, PA_USE, PA_RUN, PA_STRAFE,
    PA_FORWARD, PA_BACK, PA_TURNLEFT, PA_TURNRIGHT,
    PA_STRAFELEFT, PA_STRAFERIGHT,
    PA_WEAPON1, PA_WEAPON2, PA_WEAPON3, PA_WEAPON4,
    PA_WEAPON5, PA_WEAPON6, PA_WEAPON7,
    PA_AUTOMAP, PA_MENU,
    PA_COUNT
};

// Settings, kept in .doomrc (m_misc.c) and set on Options -> Setup ->
// Controller.
extern int	usepad;
extern int	padbind[PB_COUNT];	// PA_* for each button
extern int	padturnspeed;		// 0..9
extern int	padrumble;		// 0 (off) .. 9
extern int	padswapsticks;
extern int	padpushrun;

void	I_PadInit (void);
void	I_PadShutdown (void);

// Once a tic or more, from I_StartTic: read the pad, and turn its buttons
// into the key events the game and the menus already answer to.
void	I_PadPoll (void);

// From G_BuildTiccmd: the sticks, as speeds rather than keys.
void	I_PadTiccmd (ticcmd_t* cmd, int* forward, int* side, int speed);

// A pulse on the pad: low the heavy motor and high the light one, 0 to 1 each
// before the Vibration setting scales them.
void	I_PadRumble (float low, float high, int ms);
void	I_PadRumbleTest (void);

// For the Controller page: the pad's name, or NULL when there is none.
char*	I_PadName (void);

// The key a button stands for in the menus (m_menu.c decides, as it knows
// what is on screen), or 0 for nothing.
int	M_PadKey (int button);

#endif
