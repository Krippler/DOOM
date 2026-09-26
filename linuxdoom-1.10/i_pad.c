// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	A game controller, the same in the container and on a desktop.
//
//	A button is set to one of the game's actions on Options -> Setup ->
//	Controller, and pressing it posts the key that action is bound to, as
//	if it had been typed: the game, the menus and the automap already know
//	what to do with those. The sticks go into the tic command as speeds,
//	which is what a keyboard cannot give.
//
//	Where the controller's state comes from is the only thing that differs:
//
//	  on a desktop    SDL2's game controller API, opened with dlopen when
//	                  the engine starts, so a machine without SDL still runs
//	                  the game. SDL knows the button layout of nearly every
//	                  pad there is.
//
//	  in the          the browser, which has the pad. The page reads it
//	  container       through the Gamepad API and sends its state here over
//	                  the same WebSocket port as the picture and the sound;
//	                  websockify hands it to a TCP port on localhost, which
//	                  is this file's (DOOM_PAD_PORT, set by the entrypoint).
//
//	Both give the same thing: which buttons are down and where the sticks
//	and triggers are, in the standard layout (A at the bottom, B on the
//	right).
//
//	This replaces a browser-side translation into X keysyms, which could
//	only press keys the X server would pass on, could not tell a menu from
//	the game, and had to be told separately about every key rebound in the
//	game. Nothing here goes through X at all.
//
//-----------------------------------------------------------------------------

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_main.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_fixed.h"

#include "i_pad.h"

extern int	key_right, key_left, key_up, key_down;
extern int	key_strafeleft, key_straferight;
extern int	key_fire, key_use, key_strafe, key_speed;
extern fixed_t	forwardmove[2];
extern fixed_t	sidemove[2];
extern boolean	menuactive;

// The settings. The button defaults are the layout the browser's own
// controller panel had, so nobody has to relearn it; Start is always the
// menu, as Escape is, and Guide belongs to the system.
int	usepad = 1;
int	padbind[PB_COUNT] =
{
    PA_USE,		// A
    PA_MENU,		// B
    PA_WEAPON3,		// X
    PA_WEAPON2,		// Y
    PA_WEAPON4,		// LB
    PA_WEAPON5,		// RB
    PA_RUN,		// LT
    PA_FIRE,		// RT
    PA_AUTOMAP,		// View
    PA_MENU,		// Start, fixed
    PA_WEAPON1,		// left stick click
    PA_WEAPON6,		// right stick click
    PA_FORWARD,		// d-pad
    PA_BACK,
    PA_TURNLEFT,
    PA_TURNRIGHT,
    PA_NONE		// Guide
};
int	padturnspeed = 5;
int	padrumble = 5;
int	padswapsticks = 0;
int	padpushrun = 1;

#define DEADZONE	0.18	// of the stick that moves, and the one that turns

typedef struct
{
    boolean	connected;
    unsigned	buttons;			// 1 << PB_*
    float	lx, ly, rx, ry;			// -1 .. 1, y down
    float	lt, rt;				// 0 .. 1
    char	name[64];
} padstate_t;

static padstate_t	pad;
static unsigned		pad_prev;		// buttons, as last turned into keys
static int		pad_sentas[PB_COUNT];	// the key each held button went down as

// A stick held over in a menu, repeating as a held key would.
static int		pad_navkey;
static int		pad_navnext;

static void PAD_Say (const char* what)
{
    fprintf (stderr, "Controller: %s\n", what);
}


//
// FROM THE BROWSER
//
// A TCP listener on localhost, for websockify to connect the page's WebSocket
// to. Two kinds of message, each starting with a letter:
//
//	'P' connected buttons(4) lx ly rx ry (2 each, signed) lt rt (1 each)
//		16 bytes; little-endian; sticks -32767..32767, triggers 0..255
//	'N' length name
//		the pad's name, for the Controller page
//
// and one the other way, for the page to play on the pad (I_PadRumble):
//
//	'R' low high (2 each, 0..65535) milliseconds (2)
//		7 bytes; little-endian
//
// One page at a time: a new connection replaces the old, which is what a
// reload of the page looks like from here.
//

static int		br_listen = -1;
static int		br_conn = -1;
static byte		br_buf[256];
static int		br_len;

static void PAD_BridgeInit (int port)
{
    struct sockaddr_in	addr;
    int			one = 1;
    char		msg[80];

    br_listen = socket (AF_INET, SOCK_STREAM, 0);
    if (br_listen < 0)
	return;
    setsockopt (br_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    memset (&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons (port);
    addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

    if (bind (br_listen, (struct sockaddr *)&addr, sizeof(addr)) < 0
	|| listen (br_listen, 2) < 0)
    {
	snprintf (msg, sizeof(msg), "cannot listen on port %d (%s)", port,
		  strerror (errno));
	PAD_Say (msg);
	close (br_listen);
	br_listen = -1;
	return;
    }
    fcntl (br_listen, F_SETFL, O_NONBLOCK);
    snprintf (msg, sizeof(msg), "from the browser, port %d", port);
    PAD_Say (msg);
}

static void PAD_BridgeDrop (void)
{
    if (pad.connected)
	PAD_Say ("disconnected");
    if (br_conn >= 0)
	close (br_conn);
    br_conn = -1;
    br_len = 0;
    pad.connected = false;
}

static short PAD_Short (byte* p)
{
    return (short)(p[0] | (p[1] << 8));
}

static void PAD_BridgeRead (void)
{
    int		c, n, used, one = 1;

    if (br_listen < 0)
	return;

    c = accept (br_listen, NULL, NULL);
    if (c >= 0)
    {
	PAD_BridgeDrop ();
	br_conn = c;
	fcntl (br_conn, F_SETFL, O_NONBLOCK);
	setsockopt (br_conn, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    }

    if (br_conn < 0)
	return;

    for (;;)
    {
	n = recv (br_conn, br_buf + br_len, sizeof(br_buf) - br_len, 0);
	if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR))
	{
	    PAD_BridgeDrop ();		// the page went away
	    return;
	}
	if (n < 0)
	    break;
	br_len += n;

	used = 0;
	while (used < br_len)
	{
	    byte*	m = br_buf + used;
	    int		left = br_len - used;

	    if (m[0] == 'P')
	    {
		if (left < 16)
		    break;
		if (pad.connected != (m[1] != 0))
		    PAD_Say (m[1] ? (pad.name[0] ? pad.name : "connected")
			     : "disconnected");
		pad.connected = m[1] != 0;
		pad.buttons = m[2] | (m[3] << 8) | (m[4] << 16)
		    | ((unsigned)m[5] << 24);
		pad.lx = PAD_Short (m + 6) / 32767.0;
		pad.ly = PAD_Short (m + 8) / 32767.0;
		pad.rx = PAD_Short (m + 10) / 32767.0;
		pad.ry = PAD_Short (m + 12) / 32767.0;
		pad.lt = m[14] / 255.0;
		pad.rt = m[15] / 255.0;
		used += 16;
	    }
	    else if (m[0] == 'N')
	    {
		if (left < 2 || left < 2 + m[1])
		    break;
		n = m[1] < sizeof(pad.name) - 1 ? m[1] : sizeof(pad.name) - 1;
		memcpy (pad.name, m + 2, n);
		pad.name[n] = 0;
		used += 2 + m[1];
	    }
	    else
	    {
		used = br_len;		// lost the thread; start again
		break;
	    }
	}
	memmove (br_buf, br_buf + used, br_len - used);
	br_len -= used;
	if (br_len == sizeof(br_buf))
	    br_len = 0;
    }
}


//
// ON A DESKTOP
//
// SDL2, through dlopen. Only the game controller calls are used, and they are
// declared here rather than taken from SDL's headers, so building needs
// nothing installed; that part of the ABI has been stable since SDL 2.0.
//

typedef struct _SDL_GameController	SDL_GameController;

#define SDL_INIT_GAMECONTROLLER	0x00002000u

static int			(*pSDL_Init) (unsigned);
static int			(*pSDL_NumJoysticks) (void);
static int			(*pSDL_IsGameController) (int);
static SDL_GameController*	(*pSDL_GameControllerOpen) (int);
static void			(*pSDL_GameControllerClose) (SDL_GameController*);
static int			(*pSDL_GameControllerGetAttached) (SDL_GameController*);
static const char*		(*pSDL_GameControllerName) (SDL_GameController*);
static unsigned char		(*pSDL_GameControllerGetButton) (SDL_GameController*, int);
static short			(*pSDL_GameControllerGetAxis) (SDL_GameController*, int);
static void			(*pSDL_GameControllerUpdate) (void);
static void			(*pSDL_PumpEvents) (void);
static void			(*pSDL_FlushEvents) (unsigned, unsigned);
// SDL 2.0.9 and later; without it the pad works and does not vibrate
static int			(*pSDL_GameControllerRumble) (SDL_GameController*,
					unsigned short, unsigned short, unsigned);

static boolean			sdl_ok;
static SDL_GameController*	sdl_pad;
static int			sdl_nextscan;

static void PAD_SDLInit (void)
{
    static const char*	names[] = { "libSDL2-2.0.so.0", "libSDL2-2.0.so",
				    "libSDL2.so" };
    void*	lib = NULL;
    int		i;

    for (i = 0 ; i < 3 && !lib ; i++)
	lib = dlopen (names[i], RTLD_NOW | RTLD_LOCAL);
    if (!lib)
    {
	PAD_Say ("SDL2 is not installed, so no controllers");
	return;
    }

#define SYM(n)	if (!(*(void **)&p##n = dlsym (lib, #n))) goto missing
    SYM(SDL_Init);
    SYM(SDL_NumJoysticks);
    SYM(SDL_IsGameController);
    SYM(SDL_GameControllerOpen);
    SYM(SDL_GameControllerClose);
    SYM(SDL_GameControllerGetAttached);
    SYM(SDL_GameControllerName);
    SYM(SDL_GameControllerGetButton);
    SYM(SDL_GameControllerGetAxis);
    SYM(SDL_GameControllerUpdate);
    SYM(SDL_PumpEvents);
    SYM(SDL_FlushEvents);
#undef SYM
    *(void **)&pSDL_GameControllerRumble =
	dlsym (lib, "SDL_GameControllerRumble");

    if (pSDL_Init (SDL_INIT_GAMECONTROLLER) < 0)
    {
	PAD_Say ("SDL2 would not start its controller support");
	return;
    }
    sdl_ok = true;
    PAD_Say ("through SDL2");
    return;

  missing:
    PAD_Say ("this SDL2 is missing a function it needs");
}

static void PAD_SDLRead (void)
{
    static const int	sdlbutton[PB_COUNT] =
    {
	0, 1, 2, 3,		// A B X Y
	9, 10,			// LB RB
	-1, -1,			// LT RT, which SDL reports as axes
	4, 6,			// Back, Start
	7, 8,			// stick clicks
	11, 12, 13, 14,		// d-pad
	5			// Guide
    };
    int		i;

    if (!sdl_ok)
	return;

    pSDL_PumpEvents ();
    pSDL_GameControllerUpdate ();
    pSDL_FlushEvents (0, 0xFFFF);	// nobody reads SDL's queue; keep it empty

    if (sdl_pad && !pSDL_GameControllerGetAttached (sdl_pad))
    {
	pSDL_GameControllerClose (sdl_pad);
	sdl_pad = NULL;
	pad.connected = false;
	PAD_Say ("disconnected");
    }

    // Look for one to open, once a second while there is none.
    if (!sdl_pad && I_GetTime () >= sdl_nextscan)
    {
	sdl_nextscan = I_GetTime () + TICRATE;
	for (i = 0 ; i < pSDL_NumJoysticks () ; i++)
	    if (pSDL_IsGameController (i)
		&& (sdl_pad = pSDL_GameControllerOpen (i)) != NULL)
	    {
		const char*	n = pSDL_GameControllerName (sdl_pad);

		snprintf (pad.name, sizeof(pad.name), "%s", n ? n : "Controller");
		PAD_Say (pad.name);
		break;
	    }
    }

    if (!sdl_pad)
	return;

    pad.connected = true;
    pad.buttons = 0;
    for (i = 0 ; i < PB_COUNT ; i++)
	if (sdlbutton[i] >= 0
	    && pSDL_GameControllerGetButton (sdl_pad, sdlbutton[i]))
	    pad.buttons |= 1u << i;
    pad.lx = pSDL_GameControllerGetAxis (sdl_pad, 0) / 32767.0;
    pad.ly = pSDL_GameControllerGetAxis (sdl_pad, 1) / 32767.0;
    pad.rx = pSDL_GameControllerGetAxis (sdl_pad, 2) / 32767.0;
    pad.ry = pSDL_GameControllerGetAxis (sdl_pad, 3) / 32767.0;
    pad.lt = pSDL_GameControllerGetAxis (sdl_pad, 4) / 32767.0;
    pad.rt = pSDL_GameControllerGetAxis (sdl_pad, 5) / 32767.0;
}


//
// INTO THE GAME
//

char* I_PadName (void)
{
    if (!pad.connected)
	return NULL;
    return pad.name[0] ? pad.name : "Controller";
}

static void PAD_Post (evtype_t type, int key)
{
    event_t	ev;

    ev.type = type;
    ev.data1 = key;
    ev.data2 = ev.data3 = 0;
    D_PostEvent (&ev);
}

//
// The key a button's action stands for in the game: whatever that action is
// bound to right now, so a key rebound on the Controls page is followed here
// with nothing else to change.
//
static int PAD_GameKey (int b)
{
    int		a = b == PB_START ? PA_MENU : padbind[b];

    switch (a)
    {
      case PA_FIRE:		return key_fire;
      case PA_USE:		return key_use;
      case PA_RUN:		return key_speed;
      case PA_STRAFE:		return key_strafe;
      case PA_FORWARD:		return key_up;
      case PA_BACK:		return key_down;
      case PA_TURNLEFT:		return key_left;
      case PA_TURNRIGHT:	return key_right;
      case PA_STRAFELEFT:	return key_strafeleft;
      case PA_STRAFERIGHT:	return key_straferight;
      case PA_AUTOMAP:		return KEY_TAB;
      case PA_MENU:		return KEY_ESCAPE;
    }
    if (a >= PA_WEAPON1 && a <= PA_WEAPON7)
	return '1' + a - PA_WEAPON1;
    return 0;
}

//
// A button went down or up. In a menu it is a menu key (M_PadKey), and
// everywhere else it is its action's key.
//
// The key a button went down as is the key it comes up as, whatever has
// opened or closed in between -- otherwise a trigger held to fire and let go
// in the menu would leave the player firing.
//
static void PAD_Button (int b, boolean down)
{
    int		k;

    if (!down)
    {
	if (pad_sentas[b])
	    PAD_Post (ev_keyup, pad_sentas[b]);
	pad_sentas[b] = 0;
	return;
    }

    k = menuactive ? M_PadKey (b) : PAD_GameKey (b);

    if (k <= 0 || k >= 256)
	k = 0;			// unbound, or not a key the game can hold
    pad_sentas[b] = k;
    if (k)
	PAD_Post (ev_keydown, k);
}

//
// The left stick as arrow keys in a menu, repeating while it is held over,
// the way the d-pad does not have to (each press of that is its own step).
//
static void PAD_Nav (void)
{
    int		k = 0;
    int		now = I_GetTime ();

    if (menuactive)
    {
	if (pad.ly < -0.6)
	    k = KEY_UPARROW;
	else if (pad.ly > 0.6)
	    k = KEY_DOWNARROW;
	else if (pad.lx < -0.6)
	    k = KEY_LEFTARROW;
	else if (pad.lx > 0.6)
	    k = KEY_RIGHTARROW;
    }

    if (k != pad_navkey)
    {
	pad_navkey = k;
	pad_navnext = now + TICRATE*2/5;
	if (k)
	{
	    PAD_Post (ev_keydown, k);
	    PAD_Post (ev_keyup, k);
	}
	return;
    }

    if (k && now >= pad_navnext)
    {
	pad_navnext = now + TICRATE/8;
	PAD_Post (ev_keydown, k);
	PAD_Post (ev_keyup, k);
    }
}

void I_PadPoll (void)
{
    unsigned	now, change;
    int		b;

    PAD_BridgeRead ();
    PAD_SDLRead ();

    now = 0;
    if (pad.connected && usepad)
    {
	now = pad.buttons & ~((1u << PB_LT) | (1u << PB_RT) | (1u << PB_GUIDE));

	// Triggers are analog; a third of the way is a press, and it has to
	// come back past a fifth to let go, so one resting near the mark does
	// not chatter.
	if (pad.lt > 0.33 || ((pad_prev & (1u << PB_LT)) && pad.lt > 0.2))
	    now |= 1u << PB_LT;
	if (pad.rt > 0.33 || ((pad_prev & (1u << PB_RT)) && pad.rt > 0.2))
	    now |= 1u << PB_RT;
    }

    change = now ^ pad_prev;
    for (b = 0 ; b < PB_COUNT ; b++)
	if (change & (1u << b))
	    PAD_Button (b, (now & (1u << b)) != 0);
    pad_prev = now;

    if (pad.connected && usepad)
	PAD_Nav ();
    else
	pad_navkey = 0;
}

//
// A stick past its deadzone, taken as a stick rather than two axes so a
// diagonal is not held to a higher bar, and rescaled so the first movement
// past it is a small one. Returns how far over it is, 0 to 1.
//
static float PAD_Stick (float x, float y, float* ox, float* oy)
{
    float	m, s;

    m = sqrt (x*x + y*y);
    if (m <= DEADZONE || m <= 0)
    {
	*ox = *oy = 0;
	return 0;
    }
    s = (m - DEADZONE) / (1 - DEADZONE);
    if (s > 1)
	s = 1;
    *ox = x / m * s;
    *oy = y / m * s;
    return s;
}

//
// The left stick walks and sidesteps, the right one turns, both in
// proportion to how far they are pushed. Pushed all the way the walking
// stick runs, unless that is switched off or the run key is down anyway.
//
// Turning is degrees a second, 90 to 360 on the Turn Speed slider -- 240 at
// the default, which is the keyboard's own fast turn -- on a curve so a small
// push aims finely.
//
void I_PadTiccmd (ticcmd_t* cmd, int* forward, int* side, int speed)
{
    float	mx, my, tx, ty, m, deg;
    int		run;

    if (!pad.connected || !usepad || menuactive || gamestate != GS_LEVEL)
	return;

    if (padswapsticks)
    {
	m = PAD_Stick (pad.rx, pad.ry, &mx, &my);
	PAD_Stick (pad.lx, pad.ly, &tx, &ty);
    }
    else
    {
	m = PAD_Stick (pad.lx, pad.ly, &mx, &my);
	PAD_Stick (pad.rx, pad.ry, &tx, &ty);
    }

    run = speed || (padpushrun && m > 0.9);
    *forward -= (int)(my * forwardmove[run]);
    *side += (int)(mx * sidemove[run]);

    if (tx)
    {
	deg = (90 + padturnspeed * 30) * (tx * tx) / TICRATE;
	cmd->angleturn -= (tx < 0 ? -1 : 1) * (short)(deg * 65536 / 360);
    }
}

//
// Vibration, when a shot goes off or the player is hurt. On a desktop SDL
// plays it; in the container it goes to the page, which has the pad.
//
static void PAD_Rumble (float low, float high, int ms)
{
    float	scale;
    int		lo, hi;
    byte	m[7];

    if (!pad.connected || !usepad || !padrumble)
	return;

    scale = padrumble / 5.0;
    lo = low * scale * 65535;
    hi = high * scale * 65535;
    lo = lo < 0 ? 0 : (lo > 65535 ? 65535 : lo);
    hi = hi < 0 ? 0 : (hi > 65535 ? 65535 : hi);
    ms = ms < 1 ? 1 : (ms > 2000 ? 2000 : ms);
    if (!lo && !hi)
	return;

    if (sdl_pad && pSDL_GameControllerRumble)
    {
	pSDL_GameControllerRumble (sdl_pad, lo, hi, ms);
	return;
    }

    if (br_conn >= 0)
    {
	m[0] = 'R';
	m[1] = lo & 255;	m[2] = lo >> 8;
	m[3] = hi & 255;	m[4] = hi >> 8;
	m[5] = ms & 255;	m[6] = ms >> 8;
	send (br_conn, m, sizeof(m), MSG_DONTWAIT | MSG_NOSIGNAL);
    }
}

// Only for the player's own shots and wounds, in a level being played: not a
// demo running behind the title screen, and not while a menu is in front.
void I_PadRumble (float low, float high, int ms)
{
    if (menuactive || demoplayback || gamestate != GS_LEVEL)
	return;
    PAD_Rumble (low, high, ms);
}

// A pulse at the strength just chosen, from the Vibration setting, so it can
// be felt while it is being set.
void I_PadRumbleTest (void)
{
    PAD_Rumble (0.5, 0.5, 150);
}

void I_PadInit (void)
{
    int		i, port = 0;
    char*	env;

    if (M_CheckParm ("-nojoy"))
	return;

    // The browser, when the container has said where to listen for it; a
    // desktop otherwise.
    i = M_CheckParm ("-padport");
    if (i && i < myargc - 1)
	port = atoi (myargv[i+1]);
    else if ((env = getenv ("DOOM_PAD_PORT")) && *env)
	port = atoi (env);

    if (port > 0)
	PAD_BridgeInit (port);
    else
	PAD_SDLInit ();
}

void I_PadShutdown (void)
{
    PAD_BridgeDrop ();
    if (br_listen >= 0)
	close (br_listen);
    br_listen = -1;
    if (sdl_pad)
	pSDL_GameControllerClose (sdl_pad);
    sdl_pad = NULL;
}
