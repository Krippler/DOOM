//
// Drawing between tics: smoother motion above 35 frames a second.
//
// The game still runs at 35 tics a second, as it always did, and nothing in
// it changes. Only the picture does: a frame drawn between two tics shows
// every thing, floor, ceiling, view and weapon that far between where it was
// at the one tic and where it is at the next. Nothing drawn this way is ever
// written back, so demos and net games stay in step.
//

#ifndef __R_LERP__
#define __R_LERP__

#include "d_player.h"

// 35 (id's, and the default) draws once a tic and blends nothing; 0 draws
// as often as the machine can; anything above 35 is a limit.
extern int	max_fps;

// Set for the frame being drawn: whether it is between tics, and how far.
extern boolean	r_lerping;
extern fixed_t	r_lerpfrac;

// Whether frames are wanted between tics at all.
boolean R_LerpWanted (void);

// Level start and a loaded game: nothing to blend from until a tic has run.
void R_LerpReset (void);

// Start of each tic: where everything is now becomes where it was.
void R_LerpSave (void);

// The world ran this tic, rather than stood still for a pause or a menu.
void R_LerpMoved (void);

// The console player turned by cmd this tic; the part that was the mouse's
// is the one to show at once rather than blend.
void R_LerpTurn (player_t* player, int angleturn);

// A thing that jumped rather than moved: a teleport.
void R_LerpJump (mobj_t* mo);

// For the frame about to be drawn: frac is 0 at the last tic, 1 at the next.
void R_LerpFrame (boolean lerp, double frac);

// Around R_RenderPlayerView.
void R_LerpBegin (void);
void R_LerpEnd (void);

// Where to draw a thing, a player's view and weapon.
fixed_t R_LerpX (mobj_t* mo);
fixed_t R_LerpY (mobj_t* mo);
fixed_t R_LerpZ (mobj_t* mo);
angle_t R_LerpViewAngle (player_t* player);
fixed_t R_LerpViewZ (player_t* player);
fixed_t R_LerpPspX (player_t* player, pspdef_t* psp);
fixed_t R_LerpPspY (player_t* player, pspdef_t* psp);

#endif
