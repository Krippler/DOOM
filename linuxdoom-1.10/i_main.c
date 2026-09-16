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
//	Main program, simply calls D_DoomMain high level loop.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_main.c,v 1.4 1997/02/03 22:45:10 b1 Exp $";



#include <execinfo.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "doomdef.h"

#include "m_argv.h"
#include "d_main.h"


//
// Say where it died.
//
// A crash used to reach the container's log as the single word "Segmentation
// fault", which is not enough to act on and not enough to ask about either:
// the one report of one said nothing about what the game had been doing, and
// there is no core file in a container that nobody is going to run gdb in.
//
// So the stack is printed where the log will pick it up. backtrace_symbols_fd
// writes with write(2) and allocates nothing, which is what makes it usable
// from a signal handler; backtrace_symbols, the obvious-looking one, calls
// malloc and would deadlock exactly when it is needed. -rdynamic in the
// Makefile is what puts the names in, and it survives the strip in the
// Dockerfile because the names go in .dynsym rather than .symtab.
//
// Then the default handler gets the signal back, so the exit status still says
// what happened and a core file is still written where one is wanted.
//
// write(2) rather than fprintf, which allocates. Nothing useful can be done
// about a short write from in here, so the count is dropped deliberately.
static void I_CrashSay (const char* text)
{
    ssize_t written = write (STDERR_FILENO, text, strlen(text));
    (void) written;
}


static void I_Crashed (int sig)
{
    void*	frames[32];
    int		n;
    const char*	name;

    switch (sig)
    {
      case SIGSEGV: name = "SIGSEGV (bad memory access)"; break;
      case SIGBUS:  name = "SIGBUS (bad address)";        break;
      case SIGFPE:  name = "SIGFPE (arithmetic error)";   break;
      case SIGILL:  name = "SIGILL (illegal instruction)"; break;
      case SIGABRT: name = "SIGABRT (aborted)";           break;
      default:      name = "a fatal signal";              break;
    }

    I_CrashSay ("\nDOOM died on ");
    I_CrashSay (name);
    I_CrashSay (". Innermost frame first:\n");

    n = backtrace (frames, sizeof(frames)/sizeof(frames[0]));
    backtrace_symbols_fd (frames, n, STDERR_FILENO);

    signal (sig, SIG_DFL);
    raise (sig);
}


int
main
( int		argc,
  char**	argv ) 
{ 
    signal (SIGSEGV, I_Crashed);
    signal (SIGBUS,  I_Crashed);
    signal (SIGFPE,  I_Crashed);
    signal (SIGILL,  I_Crashed);
    signal (SIGABRT, I_Crashed);

    myargc = argc; 
    myargv = argv; 
 
    D_DoomMain (); 

    return 0;
} 
