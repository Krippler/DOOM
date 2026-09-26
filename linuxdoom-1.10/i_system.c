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
// DESCRIPTION:
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";


#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>

#include "doomdef.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_pad.h"

#include "d_net.h"
#include "g_game.h"

#ifdef __GNUG__
#pragma implementation "i_system.h"
#endif
#include "i_system.h"




//
// How much memory the zone allocator gets, in megabytes.
//
// This is the cache for every wall texture, every sprite and every level
// structure in play. The 1997 default was 6 MB, and the config file's default
// -- which M_LoadDefaults applies before Z_Init runs, so it is the one that
// counts -- was 2. Two megabytes does not hold a level's textures and the
// sprites of the things standing in it, so the zone spends the game purging
// what it is about to want again and reading it back from the WAD: measured
// on the shareware demos, 230 lumps and 825 KB re-read in three minutes of
// play, and that is the smallest WAD and the smallest levels there are. Those
// re-reads land exactly when a door reveals a texture nothing was using or a
// monster comes into view for the first time.
//
// 16 MB was enough to take that to 41 reads -- first-time loads and no more.
// 32 leaves room for a retail IWAD's larger levels and the PWADs people play,
// and 32 MB of a machine's memory is no longer worth saving.
//
#define ZONE_MB_MIN	32

int	mb_used = ZONE_MB_MIN;


void
I_Tactile
( int	on,
  int	off,
  int	total )
{
    // id's hook for force feedback, called when the player is hurt, and
    // left empty. P_DamageMobj asks for 40 ms plus 2 a point of damage, up
    // to 100 points; a controller gets that, harder for more damage, and a
    // little longer, because a pulse under a tenth of a second is barely
    // felt through a pad.
    int		damage = (total - 40) / 2;

    on = off = 0;
    I_PadRumble (damage / 40.0, damage / 60.0, total + 80);
}

ticcmd_t	emptycmd;
ticcmd_t*	I_BaseTiccmd(void)
{
    return &emptycmd;
}


//
// A floor rather than a setting: anyone who has played this container before
// has 'mb_used 2' written in their config file, and honouring it would keep
// the thrashing that number causes. Asking for more still works; asking for
// less is a 1997 answer to a problem nobody has.
//
int  I_GetHeapSize (void)
{
    if (mb_used < ZONE_MB_MIN)
	mb_used = ZONE_MB_MIN;

    return mb_used*1024*1024;
}

byte* I_ZoneBase (int*	size)
{
    *size = I_GetHeapSize ();
    return (byte *) malloc (*size);
}



//
// I_Sleep
//
// Sleeps for about this many milliseconds.
//
void I_Sleep (int ms)
{
    struct timespec	ts;

    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long) (ms % 1000) * 1000000L;

    // A signal cutting the sleep short is not worth handling: every caller is
    // a polling loop and will come straight back here.
    nanosleep (&ts, NULL);
}


//
// Waiting for the next tic.
//
// The engine spends five sixths of every tic waiting for it, and how it waits
// decides two things that pull against each other: how much CPU is left for
// everything else getting the picture out, and how promptly the engine notices
// the tic has arrived.
//
// Spinning notices instantly and costs a whole core -- that was the 1997
// behaviour, and it starved the X server, the VNC server and the proxy badly
// enough to drop a quarter of the frames. Sleeping a millisecond at a time
// costs nothing and notices within a millisecond, PROVIDED the kernel hands
// the CPU back when asked. On a busy host it does not: measured in the field
// at 43 ms late, and a machine that is consistently a few milliseconds late
// runs every frame late, which is 28 frames a second instead of 35 and looks
// like a shudder.
//
// So: sleep while there is time to spare, and hold the CPU through the last
// stretch, where being handed it late is what costs a frame. The stretch is
// as long as this machine has actually been late, and no longer -- a punctual
// one spins a millisecond in twenty-eight and uses three per cent of a core,
// the same as before. DOOM_TIC_SPIN_MS overrides it; 0 turns it off.
//
#define SPIN_MIN_MS	1.0
#define SPIN_MAX_MS	8.0

double	I_SleepLate;		// worst overshoot since last read, milliseconds
double	I_SpinMs = SPIN_MIN_MS;	// how much of the tic end is held, not slept

static double
I_NowMs (void)
{
    struct timespec	ts;

    clock_gettime (CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}


//
// Microseconds until I_GetTime would return something new.
//
static long
I_UsecToTic (void)
{
    struct timeval	tp;
    long		tic, next;

    gettimeofday (&tp, NULL);

    // The same arithmetic I_GetTime uses, solved for the earliest microsecond
    // that reads as the following tic.
    tic  = (long) tp.tv_usec * TICRATE / 1000000;
    next = ((tic + 1) * 1000000L + TICRATE - 1) / TICRATE;

    return next - tp.tv_usec;
}


void I_WaitForTic (void)
{
    static int	configured = 0;
    static double spin_max = SPIN_MAX_MS;
    long	left;

    if (!configured)
    {
	char*	e = getenv ("DOOM_TIC_SPIN_MS");

	configured = 1;

	if (e && *e)
	{
	    spin_max = atof (e);
	    if (spin_max < 0)
		spin_max = 0;
	    I_SpinMs = spin_max < SPIN_MIN_MS ? spin_max : SPIN_MIN_MS;
	}
    }

    left = I_UsecToTic ();

    if (left <= 0)
	return;

    if (left > (long) (I_SpinMs * 1000.0))
    {
	double	before, late;

	before = I_NowMs ();
	I_Sleep (1);
	late = I_NowMs () - before - 1.0;

	// How late the CPU came back. The spin window grows to cover it, so
	// the next tic end is held rather than slept through, and decays so a
	// machine that settles down stops paying for a bad minute.
	if (late > I_SleepLate)
	    I_SleepLate = late;

	if (late > I_SpinMs)
	    I_SpinMs = late < spin_max ? late : spin_max;
	else
	    I_SpinMs -= (I_SpinMs - SPIN_MIN_MS) * 0.0005;

	if (I_SpinMs < SPIN_MIN_MS)
	    I_SpinMs = SPIN_MIN_MS;

	return;
    }

    // Inside the window: hold on to the CPU rather than hand it back and hope.
    //
    // Spin on the tic itself, not on the microseconds left to it -- the latter
    // is a countdown that resets to a full tic the moment the boundary passes,
    // so waiting for it to reach zero waits for ever. It did, once.
    {
	int	start = I_GetTime ();

	while (I_GetTime () == start)
	    ;
    }
}


//
// I_GetTime
// returns time in 1/70th second tics
//
int  I_GetTime (void)
{
    struct timeval	tp;
    struct timezone	tzp;
    int			newtics;
    static int		basetime=0;
  
    gettimeofday(&tp, &tzp);
    if (!basetime)
	basetime = tp.tv_sec;
    newtics = (tp.tv_sec-basetime)*TICRATE + tp.tv_usec*TICRATE/1000000;
    return newtics;
}



//
// I_Init
//
void I_Init (void)
{
    I_InitSound();
    I_PadInit();
    //  I_InitGraphics();
}

//
// I_Quit
//
void I_Quit (void)
{
    D_QuitNetGame ();
    I_ShutdownSound();
    I_ShutdownMusic();
    M_SaveDefaults ();
    I_PadShutdown();
    I_ShutdownGraphics();
    exit(0);
}

void I_WaitVBL(int count)
{
#ifdef SGI
    sginap(1);                                           
#else
#ifdef SUN
    sleep(0);
#else
    usleep (count * (1000000/70) );                                
#endif
#endif
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

byte*	I_AllocLow(int length)
{
    byte*	mem;
        
    mem = (byte *)malloc (length);
    memset (mem,0,length);
    return mem;
}


//
// I_Error
//
extern boolean demorecording;

void I_Error (char *error, ...)
{
    va_list	argptr;

    // Message first.
    va_start (argptr,error);
    fprintf (stderr, "Error: ");
    vfprintf (stderr,error,argptr);
    fprintf (stderr, "\n");
    va_end (argptr);

    fflush( stderr );

    // Shutdown. Here might be other errors.
    if (demorecording)
	G_CheckDemoStatus();

    D_QuitNetGame ();
    I_ShutdownGraphics();
    
    exit(-1);
}
