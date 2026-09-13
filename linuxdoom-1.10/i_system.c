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
  // UNUSED.
  on = off = total = 0;
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
// Sleeps for about this many milliseconds. Used by the loop that waits out
// the rest of a tic, which otherwise spins.
//
void I_Sleep (int ms)
{
    struct timespec	ts;

    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long) (ms % 1000) * 1000000L;

    // A signal cutting the sleep short is not worth handling: the caller is a
    // polling loop and will come straight back here.
    nanosleep (&ts, NULL);
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
