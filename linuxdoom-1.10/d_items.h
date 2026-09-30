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
//	Items: key cards, artifacts, weapon, ammunition.
//
//-----------------------------------------------------------------------------


#ifndef __D_ITEMS__
#define __D_ITEMS__

#include "doomdef.h"

#ifdef __GNUG__
#pragma interface
#endif


// Weapon info: sprite frames, ammunition use.
typedef struct
{
    ammotype_t	ammo;
    int		upstate;
    int		downstate;
    int		readystate;
    int		atkstate;
    int		flashstate;

    // MBF21
    int		ammopershot;	// what a shot takes, when intflags says so
    int		intflags;	// WIF_*
    int		flags;		// WPF_*

} weaponinfo_t;

// MBF21: ammopershot is used by id's code pointers too, once a patch sets it
#define WIF_ENABLEAPS		1

// MBF21's weapon flags
#define WPF_NOTHRUST		0x01	// its hits do not push
#define WPF_SILENT		0x02	// monsters do not hear it
#define WPF_NOAUTOFIRE		0x04	// not fired on switching to it
#define WPF_FLEEMELEE		0x08	// monsters flee it as a melee weapon
#define WPF_AUTOSWITCHFROM	0x10	// switched from when ammo turns up
#define WPF_NOAUTOSWITCHTO	0x20	// never switched to on picking up ammo

extern  weaponinfo_t    weaponinfo[NUMWEAPONS];

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
