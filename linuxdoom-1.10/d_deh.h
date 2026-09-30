// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	DEHACKED patches: see d_deh.c.
//
//-----------------------------------------------------------------------------

#ifndef __D_DEH__
#define __D_DEH__

// Read every patch: the WADs' DEHACKED lumps, -deh files, and .deh files
// beside mods. After W_Init, before the tables are used.
void D_LoadDehacked (void);

// A text as a patch has it, or s itself.
const char* D_Text (const char* s);

// id's numbers, as a patch's Misc section may have changed them
extern int	deh_initial_health;
extern int	deh_initial_bullets;
extern int	deh_max_health;
extern int	deh_max_armor;
extern int	deh_green_armor_class;
extern int	deh_blue_armor_class;
extern int	deh_max_soulsphere;
extern int	deh_soulsphere_health;
extern int	deh_megasphere_health;
extern int	deh_god_mode_health;
extern int	deh_idfa_armor;
extern int	deh_idfa_armor_class;
extern int	deh_idkfa_armor;
extern int	deh_idkfa_armor_class;
extern int	deh_bfg_cells_per_shot;
extern int	deh_species_infighting;

#endif
