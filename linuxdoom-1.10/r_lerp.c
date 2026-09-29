//
// Drawing between tics: smoother motion above 35 frames a second.
//
// From the Quake container's r_lerpmodels and host_maxfps. DOOM's world
// moves 35 times a second and was drawn 35 times a second, so on a screen
// that shows 60 or 144, everything that moves -- the view most of all --
// moves in steps. Here the world still moves 35 times a second, but a frame
// drawn between two tics shows everything part of the way from where it was
// at the one to where it is at the other.
//
// Only what the renderer reads is blended, and only while it reads it: a
// thing's position, the heights of floors and ceilings, the view and the
// weapon. The play code never sees a blended value, so a demo plays the same
// and a net game stays in step whatever anyone's frame rate is.
//
// That costs one tic of delay: the frame shows where things were up to a tic
// ago. The view's turning is where that would be felt, so the mouse's part
// of it is not blended but shown at once, before it has even gone into a tic
// command -- which is what makes turning with the mouse smooth, rather than
// the view merely moving smoothly behind it.
//

#include "doomdef.h"
#include "doomstat.h"
#include "d_net.h"
#include "p_local.h"
#include "r_state.h"
#include "r_lerp.h"

int		max_fps = TICRATE;

boolean		r_lerping;
fixed_t		r_lerpfrac = FRACUNIT;

static boolean	valid;		// a tic has saved where things were
static boolean	moved;		// and the world ran in it
static int	ticturn;	// the console player's turn that tic, less the mouse

static fixed_t	oldviewz[MAXPLAYERS];
static fixed_t	oldpspx[MAXPLAYERS][NUMPSPRITES];
static fixed_t	oldpspy[MAXPLAYERS][NUMPSPRITES];

// g_game.c: what the mouse put into each tic command, and what it would put
// into the next one now
extern short	G_MouseTurn[BACKUPTICS];
int		G_PendingMouseTurn (void);

// p_spec.c: the lines with a special that acts every tic, scrolling walls
// among them
extern short	numlinespecials;
extern line_t*	linespeciallist[];


boolean R_LerpWanted (void)
{
    return (max_fps == 0 || max_fps > TICRATE)
	&& ticdup == 1
	&& gamestate == GS_LEVEL
	&& !paused
	&& !(menuactive && !netgame);
}


void R_LerpReset (void)
{
    valid = false;
    r_lerping = false;
}


void R_LerpSave (void)
{
    thinker_t*	th;
    sector_t*	sec;
    mobj_t*	mo;
    int		i, j;

    for (th = thinkercap.next ; th != &thinkercap ; th = th->next)
    {
	if (th->function.acp1 != (actionf_p1)P_MobjThinker)
	    continue;

	mo = (mobj_t*) th;
	mo->oldx = mo->x;
	mo->oldy = mo->y;
	mo->oldz = mo->z;
	mo->oldangle = mo->angle;
    }

    for (i = 0, sec = sectors ; i < numsectors ; i++, sec++)
    {
	sec->oldfloorheight = sec->floorheight;
	sec->oldceilingheight = sec->ceilingheight;
    }

    for (i = 0 ; i < MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;

	oldviewz[i] = players[i].viewz;

	for (j = 0 ; j < NUMPSPRITES ; j++)
	{
	    oldpspx[i][j] = players[i].psprites[j].sx;
	    oldpspy[i][j] = players[i].psprites[j].sy;
	}
    }

    ticturn = 0;
    moved = false;
    valid = true;
}


void R_LerpMoved (void)
{
    moved = true;
}


//
// Whether the view can turn with the mouse ahead of the tic commands: only
// when this machine's commands are the ones being run, as they are made.
// A demo's are not this machine's, a recorded demo keeps only the top byte
// of each turn, and in a net game they wait on the other machines.
//
static boolean R_LocalTurn (player_t* player)
{
    return player == &players[consoleplayer]
	&& !netgame && !demoplayback && !demorecording;
}


void R_LerpTurn (player_t* player, int angleturn)
{
    angle_t	turn;
    angle_t	mouse;

    if (!R_LocalTurn (player))
	return;

    // Unsigned, so it wraps as the angle it was added to did.
    turn  = (angle_t) angleturn << 16;
    mouse = (angle_t) G_MouseTurn[(gametic/ticdup) % BACKUPTICS] << 16;
    ticturn = (int) (turn - mouse);
}


void R_LerpJump (mobj_t* mo)
{
    mo->oldx = mo->x;
    mo->oldy = mo->y;
    mo->oldz = mo->z;
    mo->oldangle = mo->angle;

    if (mo->player)
	oldviewz[mo->player - players] = mo->player->viewz;
}


void R_LerpFrame (boolean lerp, double frac)
{
    if (frac < 0)
	frac = 0;
    if (frac > 1)
	frac = 1;

    r_lerping = lerp && valid;
    r_lerpfrac = r_lerping ? (fixed_t) (frac * FRACUNIT) : FRACUNIT;
}


static fixed_t R_Lerp (fixed_t from, fixed_t to)
{
    return from + FixedMul (to - from, r_lerpfrac);
}


//
// The renderer reads floor and ceiling heights in too many places to change
// each one, so for the time it draws they are what they would be now, and
// they are put back the moment it is done. The same goes for walls that
// scroll, which move a whole unit a tic.
//
void R_LerpBegin (void)
{
    sector_t*	sec;
    int		i;

    if (!r_lerping)
	return;

    for (i = 0, sec = sectors ; i < numsectors ; i++, sec++)
    {
	sec->savefloorheight = sec->floorheight;
	sec->saveceilingheight = sec->ceilingheight;
	sec->floorheight = R_Lerp (sec->oldfloorheight, sec->floorheight);
	sec->ceilingheight = R_Lerp (sec->oldceilingheight, sec->ceilingheight);
    }

    if (moved)
	for (i = 0 ; i < numlinespecials ; i++)
	    if (linespeciallist[i]->special == 48)
		sides[linespeciallist[i]->sidenum[0]].textureoffset
		    -= FRACUNIT - r_lerpfrac;
}


void R_LerpEnd (void)
{
    sector_t*	sec;
    int		i;

    if (!r_lerping)
	return;

    for (i = 0, sec = sectors ; i < numsectors ; i++, sec++)
    {
	sec->floorheight = sec->savefloorheight;
	sec->ceilingheight = sec->saveceilingheight;
    }

    if (moved)
	for (i = 0 ; i < numlinespecials ; i++)
	    if (linespeciallist[i]->special == 48)
		sides[linespeciallist[i]->sidenum[0]].textureoffset
		    += FRACUNIT - r_lerpfrac;
}


fixed_t R_LerpX (mobj_t* mo)
{
    return r_lerping ? R_Lerp (mo->oldx, mo->x) : mo->x;
}

fixed_t R_LerpY (mobj_t* mo)
{
    return r_lerping ? R_Lerp (mo->oldy, mo->y) : mo->y;
}

fixed_t R_LerpZ (mobj_t* mo)
{
    return r_lerping ? R_Lerp (mo->oldz, mo->z) : mo->z;
}


//
// The view's angle. Turning with the keys or a stick is at a rate, and is
// blended like any movement. Turning with the mouse is not: the part of the
// last tic's turn that was the mouse's is shown whole, and so is what the
// mouse has done since, which the next tic command will carry.
//
angle_t R_LerpViewAngle (player_t* player)
{
    angle_t	angle = player->mo->angle;
    int		d;

    if (!r_lerping)
	return angle;

    if (R_LocalTurn (player))
    {
	angle -= (angle_t) (int) (((long long) ticturn
				   * (FRACUNIT - r_lerpfrac)) >> FRACBITS);

	// Only while the next command's turn will actually be made: not
	// while dead, and not in the moment a teleport holds the player.
	if (player->playerstate == PST_LIVE && !player->mo->reactiontime)
	    angle += (angle_t) G_PendingMouseTurn () << 16;

	return angle;
    }

    d = (int) (angle - player->mo->oldangle);
    return player->mo->oldangle
	+ (angle_t) (int) (((long long) d * r_lerpfrac) >> FRACBITS);
}


fixed_t R_LerpViewZ (player_t* player)
{
    if (!r_lerping)
	return player->viewz;

    return R_Lerp (oldviewz[player - players], player->viewz);
}


fixed_t R_LerpPspX (player_t* player, pspdef_t* psp)
{
    if (!r_lerping)
	return psp->sx;

    return R_Lerp (oldpspx[player - players][psp - player->psprites], psp->sx);
}


fixed_t R_LerpPspY (player_t* player, pspdef_t* psp)
{
    if (!r_lerping)
	return psp->sy;

    return R_Lerp (oldpspy[player - players][psp - player->psprites], psp->sy);
}
