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
// DESCRIPTION:
//	Savegame I/O, archiving, persistence.
//
//-----------------------------------------------------------------------------


#ifndef __P_SAVEG__
#define __P_SAVEG__


#ifdef __GNUG__
#pragma interface
#endif


// Persistent storage/archiving.
// These are the load / save game routines.
void P_ArchivePlayers (void);
void P_UnArchivePlayers (void);
void P_ArchiveWorld (void);
void P_UnArchiveWorld (void);
void P_ArchiveThinkers (void);
void P_UnArchiveThinkers (void);
void P_ArchiveSpecials (void);
void P_UnArchiveSpecials (void);

// The most the four archive routines can write for the level as it is now.
int P_ArchiveSize (void);

// Whether the save being loaded holds things' targets and tracers as
// numbers (saves that list their WADs); others' are pointers, cleared.
extern boolean	savegamerefs;

// Whether a savegame's archive fits the map it names, before loading it.
boolean P_SaveGameFits (byte* p, byte* end, int maplump, boolean* ingame);
// ... and the map in a WAD file that is not loaded.
boolean P_SaveFitsFile (byte* p, byte* end, char* path, int episode, int map,
			boolean* ingame);

extern byte*		save_p; 


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
