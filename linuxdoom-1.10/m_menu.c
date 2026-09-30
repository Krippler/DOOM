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
//	DOOM selection menu, options, episode etc.
//	Sliders and icons. Kinda widget stuff.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_menu.c,v 1.7 1997/02/03 22:45:10 b1 Exp $";

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <strings.h>


#include "doomdef.h"
#include "dstrings.h"

#include "d_main.h"

#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "v_video.h"
#include "w_wad.h"

#include "r_local.h"


#include "hu_stuff.h"

#include "g_game.h"

#include "m_argv.h"
#include "m_swap.h"

#include "s_sound.h"

#include "doomstat.h"

// Data.
#include "sounds.h"

#include "m_menu.h"
#include "m_misc.h"
#include "i_sound.h"
#include "i_pad.h"
#include "u_mapinfo.h"
#include "st_stuff.h"
#include "r_lerp.h"
#include "d_deh.h"



extern patch_t*		hu_font[HU_FONTSIZE];
extern boolean		message_dontfuckwithme;

extern boolean		chat_on;		// in heads-up code

//
// defaulted values
//
int			mouseSensitivity;       // has default

// Show messages has default, 0 = off, 1 = on
int			showMessages;
	

// Blocky mode, has default, 0 = high, 1 = normal
int			detailLevel;		
int			screenblocks;		// has default

// temp for screenblocks (0-9)
int			screenSize;		

// -1 = no quicksave slot picked!
int			quickSaveSlot;          

 // 1 = message to be printed
int			messageToPrint;
// ...and here is the message string!
char*			messageString;		

// message x & y
int			messx;			
int			messy;
int			messageLastMenuActive;

// timed message = no input from user
boolean			messageNeedsInput;     

// Enter has been let go since the message went up: only then does it answer.
static boolean		messageEnterUp;
static boolean		enterHeld;
static double		enterUpMs;

void    (*messageRoutine)(int response);

#define SAVESTRINGSIZE 	24

char gammamsg[5][26] =
{
    GAMMALVL0,
    GAMMALVL1,
    GAMMALVL2,
    GAMMALVL3,
    GAMMALVL4
};

// we are going to be entering a savegame string
int			saveStringEnter;              
int             	saveSlot;	// which slot to save in
int			saveCharIndex;	// which char we're editing
// old save description before edit
char			saveOldString[SAVESTRINGSIZE];  

boolean			inhelpscreens;
boolean			menuactive;

#define SKULLXOFF		-32
#define LINEHEIGHT		16

// Rows for menus drawn in the small font.
//
// The item graphics are 15 pixels tall, so the original menus need 16. Text is
// 7, and the tallest thing that shares a row is the slider at 13 -- so 13 is
// as tight as this can go while everything still fits, and it buys three
// pixels a row over the old spacing.
#define SMALLLINEHEIGHT		13

extern boolean		sendpause;
char			savegamestrings[10][SAVESTRINGSIZE];

char	endstring[160];


//
// MENU TYPEDEFS
//
typedef struct
{
    // 0 = no cursor here, 1 = ok, 2 = arrows ok
    short	status;
    
    char	name[10];
    
    // choice = menu item #.
    // if status = 2,
    //   choice=0:leftarrow,1:rightarrow
    void	(*routine)(int choice);
    
    // hotkey in menu
    char	alphaKey;			

    // Drawn in the small font instead of the graphic above, when set. The
    // graphics are one per phrase, baked into the WAD, so anything the 1997
    // menus did not already say cannot be drawn that way -- which is why the
    // one item added to Options stood out from the rest of the page.
    char*	text;
} menuitem_t;



typedef struct menu_s
{
    short		numitems;	// # of menu items
    struct menu_s*	prevMenu;	// previous menu
    menuitem_t*		menuitems;	// menu items
    void		(*routine)();	// draw routine
    short		x;
    short		y;		// x,y of menu
    short		lastOn;		// last item user was on in menu
    // Rows per line. 0 means LINEHEIGHT, which is what the original menus
    // want; the text pages added later need to pack tighter to fit above
    // the status bar.
    short		lineheight;
} menu_t;

short		itemOn;			// menu item skull is on
short		skullAnimCounter;	// skull animation counter
short		whichSkull;		// which skull to draw

// graphic name of skulls
// warning: initializer-string for array of chars is too long
char    skullName[2][/*8*/9] = {"M_SKULL1","M_SKULL2"};

// current menudef
menu_t*	currentMenu;                          

//
// PROTOTYPES

//
// A patch's replacement for a text used as a format with one %s in it, if
// it has exactly that one and nothing else a format would read; otherwise
// id's, rather than print from wherever a stray %d pointed.
//
static char* M_OneStringFormat (char* fmt)
{
    const char*	r = D_Text (fmt);
    const char*	p;
    int		n = 0;

    for (p = r; (p = strchr (p, '%')); p += 2)
    {
	if (p[1] == '%')
	    continue;
	if (p[1] != 's' || ++n > 1)
	    return fmt;
    }
    return n == 1 ? (char*) r : fmt;
}

//
void M_NewGame(int choice);
void M_Episode(int choice);
void M_ChooseSkill(int choice);
void M_LoadGame(int choice);
void M_SaveGame(int choice);
void M_Options(int choice);
void M_EndGame(int choice);
void M_ReadThis(int choice);
void M_ReadThis2(int choice);
void M_QuitDOOM(int choice);

void M_ChangeMessages(int choice);
void M_ChangeSensitivity(int choice);
void M_SfxVol(int choice);
void M_MusicVol(int choice);
void M_ChangeDetail(int choice);
void M_SizeDisplay(int choice);
void M_StartGame(int choice);
void M_Sound(int choice);

void M_FinishReadThis(int choice);
void M_LoadSelect(int choice);
void M_SaveSelect(int choice);
void M_ReadSaveStrings(void);
void M_QuickSave(void);
void M_QuickLoad(void);

void M_DrawMainMenu(void);
void M_DrawReadThis1(void);
void M_DrawReadThis2(void);
void M_DrawNewGame(void);
void M_DrawEpisode(void);
void M_DrawOptions(void);
void M_DrawSound(void);
void M_DrawLoad(void);
void M_DrawSave(void);

void M_DrawSaveLoadBorder(int x,int y);
void M_SetupNextMenu(menu_t *menudef);
void M_Setup(int choice);
void M_DrawSetup(void);
void M_Controls(int choice);
void M_MouseOptions(int choice);
void M_WadSelect(int choice);
void M_PadOptions(int choice);
void M_PadButtons(int choice);
void M_ChangeBinding(int choice);
void M_ToggleMouse(int choice);
void M_ToggleMouseGrab(int choice);
void M_ToggleMouseMove(int choice);
void M_ChangeMouseFire(int choice);
void M_ChangeMouseStrafe(int choice);
void M_ChangeMouseForward(int choice);
void M_LoadWad(int choice);
static void M_ChooseWadToPlay (void);
void M_DrawControls(void);
void M_DrawMouseOptions(void);
void M_DrawWadSelect(void);
void M_DrawThermo(int x,int y,int thermWidth,int thermDot);
void M_DrawEmptyCell(menu_t *menu,int item);
void M_DrawSelCell(menu_t *menu,int item);
void M_WriteText(int x, int y, char *string);
int  M_StringWidth(char *string);
int  M_StringHeight(char *string);
void M_StartControlPanel(void);
void M_StartMessage(char *string,void *routine,boolean input);
void M_StopMessage(void);
void M_ClearMenus (void);




//
// DOOM MENU
//
enum
{
    newgame = 0,
    options,
    loadgame,
    savegame,
    readthis,
    quitdoom,
    main_end
} main_e;

menuitem_t MainMenu[]=
{
    {1,"",M_NewGame,'n',"NEW GAME"},
    {1,"",M_Options,'o',"OPTIONS"},
    {1,"",M_LoadGame,'l',"LOAD GAME"},
    {1,"",M_SaveGame,'s',"SAVE GAME"},
    // Another hickup with Special edition.
    {1,"",M_ReadThis,'r',"READ THIS!"},
    {1,"",M_QuitDOOM,'q',"QUIT GAME"}
};

menu_t  MainDef =
{
    main_end,
    NULL,
    MainMenu,
    M_DrawMainMenu,
    97,72,
    0,
    SMALLLINEHEIGHT
};


//
// EPISODE SELECT
//
enum
{
    ep1,
    ep2,
    ep3,
    ep4,
    ep_end
} episodes_e;

// The game's own four, then any UMAPINFO adds; see M_InitEpisodes.
menuitem_t EpisodeMenu[UM_MAXEPISODES]=
{
    {1,"", M_Episode,'k',"KNEE-DEEP IN THE DEAD"},
    {1,"", M_Episode,'t',"THE SHORES OF HELL"},
    {1,"", M_Episode,'i',"INFERNO"},
    {1,"", M_Episode,'t',"THY FLESH CONSUMED"}
};

// where each starts
static int	epiepisode[UM_MAXEPISODES] = { 1, 2, 3, 4 };
static int	epimap[UM_MAXEPISODES] = { 1, 1, 1, 1 };

menu_t  EpiDef =
{
    ep_end,		// # of menu items
    &MainDef,		// previous menu
    EpisodeMenu,	// menuitem_t ->
    M_DrawEpisode,	// drawing routine ->
    48,68,              // x,y
    ep1,		// lastOn
    SMALLLINEHEIGHT
};

//
// NEW GAME
//
enum
{
    killthings,
    toorough,
    hurtme,
    violence,
    nightmare,
    newg_end
} newgame_e;

menuitem_t NewGameMenu[]=
{
    {1,"",	M_ChooseSkill, 'i',"I'M TOO YOUNG TO DIE"},
    {1,"",	M_ChooseSkill, 'h',"HEY, NOT TOO ROUGH"},
    {1,"",	M_ChooseSkill, 'h',"HURT ME PLENTY"},
    {1,"",	M_ChooseSkill, 'u',"ULTRA-VIOLENCE"},
    {1,"",	M_ChooseSkill, 'n',"NIGHTMARE!"}
};

menu_t  NewDef =
{
    newg_end,		// # of menu items
    &EpiDef,		// previous menu
    NewGameMenu,	// menuitem_t ->
    M_DrawNewGame,	// drawing routine ->
    48,68,              // x,y
    hurtme,		// lastOn
    SMALLLINEHEIGHT
};



//
// OPTIONS MENU
//
enum
{
    endgame,
    messages,
    detail,
    scrnsize,
    option_empty1,
    mousesens,
    option_empty2,
    soundvol,
    opt_setup,
    opt_end
} options_e;

menuitem_t OptionsMenu[]=
{
    {1,"",	M_EndGame,'e',"END GAME"},
    {1,"",	M_ChangeMessages,'m',"MESSAGES"},
    {1,"",	M_ChangeDetail,'g',"GRAPHIC DETAIL"},
    {2,"",	M_SizeDisplay,'s',"SCREEN SIZE"},
    {-1,"",0,0,""},
    {2,"",	M_ChangeSensitivity,'m',"MOUSE SENSITIVITY"},
    {-1,"",0,0,""},
    {1,"",	M_Sound,'s',"SOUND VOLUME"},
    {1,"",	M_Setup,'t',"SETUP"}
};

menu_t  OptionsDef =
{
    opt_end,
    &MainDef,
    OptionsMenu,
    M_DrawOptions,
    60,40,
    0,
    SMALLLINEHEIGHT
};

//
// Read This! MENU 1 & 2
//
enum
{
    rdthsempty1,
    read1_end
} read_e;

menuitem_t ReadMenu1[] =
{
    {1,"",M_ReadThis2,0}
};

menu_t  ReadDef1 =
{
    read1_end,
    &MainDef,
    ReadMenu1,
    M_DrawReadThis1,
    280,185,
    0
};

enum
{
    rdthsempty2,
    read2_end
} read_e2;

menuitem_t ReadMenu2[]=
{
    {1,"",M_FinishReadThis,0}
};

menu_t  ReadDef2 =
{
    read2_end,
    &ReadDef1,
    ReadMenu2,
    M_DrawReadThis2,
    330,175,
    0
};

//
// SOUND VOLUME MENU
//
enum
{
    sfx_vol,
    sfx_empty1,
    music_vol,
    sfx_empty2,
    sound_end
} sound_e;

menuitem_t SoundMenu[]=
{
    {2,"",M_SfxVol,'s',"SFX VOLUME"},
    {-1,"",0,0,""},
    {2,"",M_MusicVol,'m',"MUSIC VOLUME"},
    {-1,"",0,0,""}
};

menu_t  SoundDef =
{
    sound_end,
    &OptionsDef,
    SoundMenu,
    M_DrawSound,
    80,64,
    0,
    SMALLLINEHEIGHT
};

//
// LOAD GAME MENU
//
enum
{
    load1,
    load2,
    load3,
    load4,
    load5,
    load6,
    load_end
} load_e;

menuitem_t LoadMenu[]=
{
    {1,"", M_LoadSelect,'1'},
    {1,"", M_LoadSelect,'2'},
    {1,"", M_LoadSelect,'3'},
    {1,"", M_LoadSelect,'4'},
    {1,"", M_LoadSelect,'5'},
    {1,"", M_LoadSelect,'6'}
};

menu_t  LoadDef =
{
    load_end,
    &MainDef,
    LoadMenu,
    M_DrawLoad,
    80,54,
    0
};

//
// SAVE GAME MENU
//
menuitem_t SaveMenu[]=
{
    {1,"", M_SaveSelect,'1'},
    {1,"", M_SaveSelect,'2'},
    {1,"", M_SaveSelect,'3'},
    {1,"", M_SaveSelect,'4'},
    {1,"", M_SaveSelect,'5'},
    {1,"", M_SaveSelect,'6'}
};

menu_t  SaveDef =
{
    load_end,
    &MainDef,
    SaveMenu,
    M_DrawSave,
    80,54,
    0
};


//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
void M_ReadSaveStrings(void)
{
    int             handle;
    int             count;
    int             i;
    char    name[256];
	
    for (i = 0;i < load_end;i++)
    {
	if (M_CheckParm("-cdrom"))
	    sprintf(name,"c:\\doomdata\\"SAVEGAMENAME"%d.dsg",i);
	else
	    sprintf(name,SAVEGAMENAME"%d.dsg",i);

	handle = open (name, O_RDONLY | 0, 0666);
	if (handle == -1)
	{
	    strcpy(&savegamestrings[i][0],EMPTYSTRING);
	    LoadMenu[i].status = 0;
	    continue;
	}
	count = read (handle, &savegamestrings[i], SAVESTRINGSIZE);
	close (handle);
	LoadMenu[i].status = 1;
    }
}


//
// M_LoadGame & Cie.
//
void M_DrawLoad(void)
{
    int             i;
	
    V_DrawPatchDirect (72,28,0,W_CacheLumpName("M_LOADG",PU_CACHE));
    for (i = 0;i < load_end; i++)
    {
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
    }
}



//
// Draw border for the savegame description
//
void M_DrawSaveLoadBorder(int x,int y)
{
    int             i;
	
    V_DrawPatchDirect (x-8,y+7,0,W_CacheLumpName("M_LSLEFT",PU_CACHE));
	
    for (i = 0;i < 24;i++)
    {
	V_DrawPatchDirect (x,y+7,0,W_CacheLumpName("M_LSCNTR",PU_CACHE));
	x += 8;
    }

    V_DrawPatchDirect (x,y+7,0,W_CacheLumpName("M_LSRGHT",PU_CACHE));
}



//
// User wants to load this game
//
void M_LoadSelect(int choice)
{
    char    name[256];
	
    if (M_CheckParm("-cdrom"))
	sprintf(name,"c:\\doomdata\\"SAVEGAMENAME"%d.dsg",choice);
    else
	sprintf(name,SAVEGAMENAME"%d.dsg",choice);
    G_LoadGame (name);
    M_ClearMenus ();
}

//
// Selected from DOOM menu
//
void M_LoadGame (int choice)
{
    if (netgame)
    {
	M_StartMessage(LOADNET,NULL,false);
	return;
    }
	
    M_SetupNextMenu(&LoadDef);
    M_ReadSaveStrings();
}


//
//  M_SaveGame & Cie.
//
void M_DrawSave(void)
{
    int             i;
	
    V_DrawPatchDirect (72,28,0,W_CacheLumpName("M_SAVEG",PU_CACHE));
    for (i = 0;i < load_end; i++)
    {
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
    }
	
    if (saveStringEnter)
    {
	i = M_StringWidth(savegamestrings[saveSlot]);
	M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_");
    }
}

//
// M_Responder calls this when user is finished
//
void M_DoSave(int slot)
{
    G_SaveGame (slot,savegamestrings[slot]);
    M_ClearMenus ();

    // PICK QUICKSAVE SLOT YET?
    if (quickSaveSlot == -2)
	quickSaveSlot = slot;
}

//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect(int choice)
{
    // we are going to be intercepting all chars
    saveStringEnter = 1;
    
    saveSlot = choice;
    strcpy(saveOldString,savegamestrings[choice]);
    if (!strcmp(savegamestrings[choice],EMPTYSTRING))
	savegamestrings[choice][0] = 0;
    saveCharIndex = strlen(savegamestrings[choice]);
}

//
// Selected from DOOM menu
//
void M_SaveGame (int choice)
{
    if (!usergame)
    {
	M_StartMessage(SAVEDEAD,NULL,false);
	return;
    }
	
    if (gamestate != GS_LEVEL)
	return;
	
    M_SetupNextMenu(&SaveDef);
    M_ReadSaveStrings();
}



//
//      M_QuickSave
//
char    tempstring[80];

void M_QuickSaveResponse(int ch)
{
    if (ch == 'y')
    {
	M_DoSave(quickSaveSlot);
	S_StartSound(NULL,sfx_swtchx);
    }
}

void M_QuickSave(void)
{
    if (!usergame)
    {
	S_StartSound(NULL,sfx_oof);
	return;
    }

    if (gamestate != GS_LEVEL)
	return;
	
    if (quickSaveSlot < 0)
    {
	M_StartControlPanel();
	M_ReadSaveStrings();
	M_SetupNextMenu(&SaveDef);
	quickSaveSlot = -2;	// means to pick a slot now
	return;
    }
    sprintf(tempstring,M_OneStringFormat (QSPROMPT),savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring,M_QuickSaveResponse,true);
}



//
// M_QuickLoad
//
void M_QuickLoadResponse(int ch)
{
    if (ch == 'y')
    {
	M_LoadSelect(quickSaveSlot);
	S_StartSound(NULL,sfx_swtchx);
    }
}


void M_QuickLoad(void)
{
    if (netgame)
    {
	M_StartMessage(QLOADNET,NULL,false);
	return;
    }
	
    if (quickSaveSlot < 0)
    {
	M_StartMessage(QSAVESPOT,NULL,false);
	return;
    }
    sprintf(tempstring,M_OneStringFormat (QLPROMPT),savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring,M_QuickLoadResponse,true);
}




//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void M_DrawReadThis1(void)
{
    inhelpscreens = true;
    switch ( gamemode )
    {
      case commercial:
	V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP",PU_CACHE));
	break;
      case shareware:
      case registered:
      case retail:
	V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP1",PU_CACHE));
	break;
      default:
	break;
    }
    return;
}



//
// Read This Menus - optional second page.
//
void M_DrawReadThis2(void)
{
    inhelpscreens = true;
    switch ( gamemode )
    {
      case retail:
      case commercial:
	// This hack keeps us from having to change menus.
	V_DrawPatchDirect (0,0,0,W_CacheLumpName("CREDIT",PU_CACHE));
	break;
      case shareware:
      case registered:
	V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP2",PU_CACHE));
	break;
      default:
	break;
    }
    return;
}


//
// Change Sfx & Music volumes
//
void M_DrawSound(void)
{
    V_DrawPatchDirect (60,38,0,W_CacheLumpName("M_SVOL",PU_CACHE));

    M_DrawThermo(SoundDef.x,SoundDef.y+SoundDef.lineheight*(sfx_vol+1),
		 16,snd_SfxVolume);

    M_DrawThermo(SoundDef.x,SoundDef.y+SoundDef.lineheight*(music_vol+1),
		 16,snd_MusicVolume);
}

void M_Sound(int choice)
{
    M_SetupNextMenu(&SoundDef);
}

void M_SfxVol(int choice)
{
    switch(choice)
    {
      case 0:
	if (snd_SfxVolume)
	    snd_SfxVolume--;
	break;
      case 1:
	if (snd_SfxVolume < 15)
	    snd_SfxVolume++;
	break;
    }
	
    S_SetSfxVolume(snd_SfxVolume /* *8 */);
}

void M_MusicVol(int choice)
{
    switch(choice)
    {
      case 0:
	if (snd_MusicVolume)
	    snd_MusicVolume--;
	break;
      case 1:
	if (snd_MusicVolume < 15)
	    snd_MusicVolume++;
	break;
    }
	
    S_SetMusicVolume(snd_MusicVolume /* *8 */);
}




//
// M_DrawMainMenu
//
void M_DrawMainMenu(void)
{
    V_DrawPatchDirect (94,2,0,W_CacheLumpName("M_DOOM",PU_CACHE));
}




//
// M_NewGame
//
int     epi;

void M_DrawNewGame(void)
{
    V_DrawPatchDirect (96,14,0,W_CacheLumpName("M_NEWG",PU_CACHE));
    V_DrawPatchDirect (54,38,0,W_CacheLumpName("M_SKILL",PU_CACHE));
}

void M_NewGame(int choice)
{
    if (netgame && !demoplayback)
    {
	M_StartMessage(NEWGAME,NULL,false);
	return;
    }

    // What to play first, from the WAD folder; then how hard. With nothing
    // in the folder to choose from, straight on with the game running.
    M_ChooseWadToPlay ();
}


//
//      M_Episode
//

void M_DrawEpisode(void)
{
    V_DrawPatchDirect (54,38,0,W_CacheLumpName("M_EPISOD",PU_CACHE));
}

// A mod's own first map, when New Game went straight to it (M_NewGameFor).
static int	modepisode, modmap;

// The chosen episode, or DOOM II's MAP01 when there is none to choose.
static void M_StartEpisode (int skill)
{
    if (epi < 0 && modepisode)
	G_DeferedInitNew (skill, modepisode, modmap);
    else if (epi < EpiDef.numitems)
	G_DeferedInitNew (skill, epiepisode[epi], epimap[epi]);
    else
	G_DeferedInitNew (skill, 1, 1);
}

void M_VerifyNightmare(int ch)
{
    if (ch != 'y')
	return;
		
    M_StartEpisode (nightmare);
    M_ClearMenus ();
}

void M_ChooseSkill(int choice)
{
    if (choice == nightmare)
    {
	M_StartMessage(NIGHTMARE,M_VerifyNightmare,true);
	return;
    }
	
    M_StartEpisode (choice);
    M_ClearMenus ();
}

void M_Episode(int choice)
{
    if ( (gamemode == shareware)
	 && epiepisode[choice] > 1)
    {
	M_StartMessage(SWSTRING,NULL,false);
	M_SetupNextMenu(&ReadDef1);
	return;
    }

    // The original also turned a registered game's choice of a fourth
    // episode into the first, here; M_Init leaves it out of the list.
    epi = choice;
    M_SetupNextMenu(&NewDef);
}



//
// CONTROLS, MOUSE AND WAD SELECTION
//
// The original release had no way to rebind a key or pick game data from
// inside the game: bindings lived only in the config file, and the IWAD was
// whichever one happened to be found at startup. These three pages add that.
//
// None of them have graphic lumps to draw with, so each menu is built from
// items with empty names -- M_Drawer skips drawing those but still lets the
// cursor sit on them -- and the menu's own draw routine writes the text.
//

extern int	key_right;
extern int	key_left;
extern int	key_up;
extern int	key_down;
extern int	key_strafeleft;
extern int	key_straferight;
extern int	key_fire;
extern int	key_use;
extern int	key_strafe;
extern int	key_speed;
extern int	key_menu;
extern int	key_nextweapon;
extern int	key_prevweapon;
extern int	novert;
extern int	weaponpickup;

extern int	usemouse;
extern int	mousebfire;
extern int	mousebstrafe;
extern int	mousebforward;
extern int	grabMouse;


// Quick save and quick load: F6 and F9, as id had them, until the Controls
// page says otherwise. A browser keeps some function keys for itself, and a
// laptop needs Fn to reach them at all.
int	key_quicksave = KEY_F6;
int	key_quickload = KEY_F9;

typedef struct
{
    char*	label;
    int*	key;
} binding_t;

static binding_t bindings[] =
{
    {"FIRE",		&key_fire},
    {"USE / OPEN",	&key_use},
    {"FORWARD",		&key_up},
    {"BACK",		&key_down},
    {"TURN LEFT",	&key_left},
    {"TURN RIGHT",	&key_right},
    {"STRAFE LEFT",	&key_strafeleft},
    {"STRAFE RIGHT",	&key_straferight},
    {"STRAFE ON",	&key_strafe},
    {"RUN",		&key_speed},
    {"NEXT WEAPON",	&key_nextweapon},
    {"PREV WEAPON",	&key_prevweapon},
    {"MENU",		&key_menu},
    {"QUICK SAVE",	&key_quicksave},
    {"QUICK LOAD",	&key_quickload}
};

#define NUM_BINDINGS	(sizeof(bindings)/sizeof(bindings[0]))

// Set while the next keypress is being captured for a binding.
static boolean	bindingWait = false;

// The size of G_Responder's gamekeydown array: the largest key value the game
// can hold down, and so the largest one worth storing in a binding.
#define MAXKEYVALUE	256


typedef struct
{
    int		key;
    char*	name;
} keyname_t;

static keyname_t keynames[] =
{
    {KEY_RIGHTARROW,	"RIGHT"},
    {KEY_LEFTARROW,	"LEFT"},
    {KEY_UPARROW,	"UP"},
    {KEY_DOWNARROW,	"DOWN"},
    {KEY_ENTER,		"ENTER"},
    {KEY_TAB,		"TAB"},
    {KEY_BACKSPACE,	"BACKSP"},
    {KEY_DEL,		"DEL"},
    {KEY_INS,		"INS"},
    {KEY_HOME,		"HOME"},
    {KEY_END,		"END"},
    {KEY_PGUP,		"PGUP"},
    {KEY_PGDN,		"PGDN"},
    {KEY_PAUSE,		"PAUSE"},
    {KEY_ESCAPE,	"ESC"},
    {KEY_EQUALS,	"="},
    {KEY_MINUS,		"-"},
    {KEY_RSHIFT,	"SHIFT"},
    {KEY_RCTRL,		"CTRL"},
    {KEY_RALT,		"ALT"},
    {KEY_MWHEELUP,	"WHEEL UP"},
    {KEY_MWHEELDOWN,	"WHEEL DOWN"},
    {' ',		"SPACE"},
    {',',		"COMMA"},
    {'.',		"PERIOD"},
    {'/',		"SLASH"},
    {';',		"SEMICOL"},
    {'\'',		"QUOTE"},
    {'[',		"LBRACK"},
    {']',		"RBRACK"},
    {'\\',		"BSLASH"},
    {'`',		"TILDE"}
};


static char* M_KeyName (int key)
{
    static char	buf[16];
    unsigned	i;

    // Cleared with Delete on the Controls page. 0 is not a key anything
    // sends either, so an old config holding it reads the same way.
    if (key <= 0)
	return "---";

    for (i = 0; i < sizeof(keynames)/sizeof(keynames[0]); i++)
	if (keynames[i].key == key)
	    return keynames[i].name;

    if (key >= KEY_F1 && key <= KEY_F10)
    {
	snprintf (buf, sizeof(buf), "F%d", key - KEY_F1 + 1);
	return buf;
    }

    if (key > 32 && key < 127)
    {
	buf[0] = toupper(key);
	buf[1] = 0;
	return buf;
    }

    snprintf (buf, sizeof(buf), "KEY%d", key);
    return buf;
}


enum
{
    setup_controls,
    setup_mouse,
    setup_pad,
    setup_gameplay,
    setup_wads,
    setup_end
} setup_e;

void M_Gameplay (int choice);

menuitem_t SetupMenu[] =
{
    {1,"",M_Controls,'c',"CONTROLS"},
    {1,"",M_MouseOptions,'m',"MOUSE"},
    {1,"",M_PadOptions,'p',"CONTROLLER"},
    {1,"",M_Gameplay,'g',"GAMEPLAY"},
    {1,"",M_WadSelect,'w',"LOAD WAD"}
};

menu_t SetupDef =
{
    setup_end,
    &OptionsDef,
    SetupMenu,
    M_DrawSetup,
    60,64,
    0,
    SMALLLINEHEIGHT
};


void M_Setup (int choice)
{
    choice = 0;
    M_SetupNextMenu (&SetupDef);
}


void M_DrawSetup (void)
{
    M_WriteText (60, 40, "SETUP");
}


menuitem_t ControlsMenu[] =
{
    {1,"",M_ChangeBinding,0}, {1,"",M_ChangeBinding,0},
    {1,"",M_ChangeBinding,0}, {1,"",M_ChangeBinding,0},
    {1,"",M_ChangeBinding,0}, {1,"",M_ChangeBinding,0},
    {1,"",M_ChangeBinding,0}, {1,"",M_ChangeBinding,0},
    {1,"",M_ChangeBinding,0}, {1,"",M_ChangeBinding,0},
    {1,"",M_ChangeBinding,0}, {1,"",M_ChangeBinding,0},
    {1,"",M_ChangeBinding,0}, {1,"",M_ChangeBinding,0},
    {1,"",M_ChangeBinding,0}
};

// Fifteen rows at the small font's 13 pixels reach well into the status bar,
// so these are packed at 10 under a title moved up, as the controller's
// buttons are.
menu_t ControlsDef =
{
    NUM_BINDINGS,
    &SetupDef,
    ControlsMenu,
    M_DrawControls,
    56,16,
    0,
    10
};


void M_Controls (int choice)
{
    choice = 0;
    bindingWait = false;
    M_SetupNextMenu (&ControlsDef);
}


void M_ChangeBinding (int choice)
{
    choice = 0;
    // The next keypress is taken as the new binding; M_Responder watches
    // this flag before it does anything else with the key.
    bindingWait = true;
    S_StartSound (NULL, sfx_swtchn);
}


void M_DrawControls (void)
{
    unsigned	i;
    int		y;

    M_WriteText (56, 4, "CONTROLS");

    y = ControlsDef.y;

    // A line down from the row, as on the controller's page, so the text
    // sits in the middle of the skull beside it at this spacing.
    for (i = 0; i < NUM_BINDINGS; i++)
    {
	M_WriteText (ControlsDef.x, y + 1, bindings[i].label);

	if (bindingWait && i == (unsigned)itemOn)
	    M_WriteText (ControlsDef.x + 148, y + 1, "???");
	else
	    M_WriteText (ControlsDef.x + 148, y + 1,
			 M_KeyName(*bindings[i].key));

	y += ControlsDef.lineheight;
    }
}


//
// Mouse.
//
enum
{
    mouse_on,
    mouse_grab,
    mouse_move,
    mouse_firebtn,
    mouse_strafebtn,
    mouse_fwdbtn,
    mouse_end
} mouse_e;

menuitem_t MouseMenu[] =
{
    {1,"",M_ToggleMouse,'m'},
    {1,"",M_ToggleMouseGrab,'g'},
    {1,"",M_ToggleMouseMove,'v'},
    {2,"",M_ChangeMouseFire,'f'},
    {2,"",M_ChangeMouseStrafe,'s'},
    {2,"",M_ChangeMouseForward,'w'}
};

menu_t MouseDef =
{
    mouse_end,
    &SetupDef,
    MouseMenu,
    M_DrawMouseOptions,
    56,48,
    0,
    SMALLLINEHEIGHT
};


void M_MouseOptions (int choice)
{
    choice = 0;
    M_SetupNextMenu (&MouseDef);
}


void M_ToggleMouse (int choice)
{
    choice = 0;
    usemouse = !usemouse;
    S_StartSound (NULL, sfx_pistol);
}


void M_ToggleMouseMove (int choice)
{
    choice = 0;
    // Stored the other way round: the config key is novert, as everywhere
    // else, but a menu reading "MOVE WITH MOUSE: OFF" is easier to follow
    // than one reading "NO VERTICAL: ON".
    novert = !novert;
    S_StartSound (NULL, sfx_pistol);
}


void M_ToggleMouseGrab (int choice)
{
    choice = 0;
    // Grabbing confines the pointer and warps it back to the centre every
    // frame, which is what lets you keep turning instead of running out of
    // window. Worth being able to switch off over a remote desktop.
    grabMouse = !grabMouse;
    I_SetMouseGrab (grabMouse);
    S_StartSound (NULL, sfx_pistol);
}


static void M_CycleButton (int* button, int choice)
{
    if (choice)
	*button = (*button + 1) % 3;
    else
	*button = (*button + 2) % 3;

    S_StartSound (NULL, sfx_stnmov);
}

void M_ChangeMouseFire (int choice)	{ M_CycleButton (&mousebfire, choice); }
void M_ChangeMouseStrafe (int choice)	{ M_CycleButton (&mousebstrafe, choice); }
void M_ChangeMouseForward (int choice)	{ M_CycleButton (&mousebforward, choice); }


void M_DrawMouseOptions (void)
{
    int		y = MouseDef.y;
    char	buf[32];

    M_WriteText (56, 14, "MOUSE");

    M_WriteText (MouseDef.x, y, "ENABLE MOUSE");
    M_WriteText (MouseDef.x + 148, y, usemouse ? "ON" : "OFF");
    y += MouseDef.lineheight;

    M_WriteText (MouseDef.x, y, "GRAB POINTER");
    M_WriteText (MouseDef.x + 148, y, grabMouse ? "ON" : "OFF");
    y += MouseDef.lineheight;

    M_WriteText (MouseDef.x, y, "MOVE WITH MOUSE");
    M_WriteText (MouseDef.x + 148, y, novert ? "OFF" : "ON");
    y += MouseDef.lineheight;

    snprintf (buf, sizeof(buf), "BUTTON %d", mousebfire + 1);
    M_WriteText (MouseDef.x, y, "FIRE");
    M_WriteText (MouseDef.x + 148, y, buf);
    y += MouseDef.lineheight;

    snprintf (buf, sizeof(buf), "BUTTON %d", mousebstrafe + 1);
    M_WriteText (MouseDef.x, y, "STRAFE");
    M_WriteText (MouseDef.x + 148, y, buf);
    y += MouseDef.lineheight;

    snprintf (buf, sizeof(buf), "BUTTON %d", mousebforward + 1);
    M_WriteText (MouseDef.x, y, "FORWARD");
    M_WriteText (MouseDef.x + 148, y, buf);
    y += MouseDef.lineheight;

    M_WriteText (56, 140, "SENSITIVITY IS UNDER OPTIONS");
}


//
// The controller.
//
// Its settings, and a page of its own for what each button does. A button is
// set to one of the game's actions rather than to a key: pressing it presses
// whatever that action is bound to on the Controls page, so rebinding a key
// there needs nothing changing here. See i_pad.c.
//
enum
{
    pad_on,
    pad_buttons,
    pad_turn,
    pad_curve,
    pad_movedz,
    pad_turndz,
    pad_vibration,
    pad_swap,
    pad_pushrun,
    pad_end
} pad_e;

void M_TogglePad (int choice);
void M_ChangePadTurn (int choice);
void M_ChangePadCurve (int choice);
void M_ChangePadMoveDZ (int choice);
void M_ChangePadTurnDZ (int choice);
void M_ChangePadRumble (int choice);
void M_TogglePadSwap (int choice);
void M_TogglePadPushRun (int choice);
void M_DrawPadOptions (void);
void M_CyclePadButton (int choice);
void M_DrawPadButtons (void);

menuitem_t PadMenu[] =
{
    {1,"",M_TogglePad,'u'},
    {1,"",M_PadButtons,'b'},
    {2,"",M_ChangePadTurn,'t'},
    {2,"",M_ChangePadCurve,'c'},
    {2,"",M_ChangePadMoveDZ,'m'},
    {2,"",M_ChangePadTurnDZ,'d'},
    {2,"",M_ChangePadRumble,'v'},
    {1,"",M_TogglePadSwap,'s'},
    {1,"",M_TogglePadPushRun,'p'}
};

// Nine rows and two lines of help below them fit above the status bar at
// 11 pixels a row, not the small font's 13.
menu_t PadDef =
{
    pad_end,
    &SetupDef,
    PadMenu,
    M_DrawPadOptions,
    56,42,
    0,
    11
};

static char* padactionnames[PA_COUNT] =
{
    "NOTHING",
    "FIRE", "USE / OPEN", "RUN", "STRAFE ON",
    "FORWARD", "BACK", "TURN LEFT", "TURN RIGHT",
    "STRAFE LEFT", "STRAFE RIGHT",
    "WEAPON 1", "WEAPON 2", "WEAPON 3", "WEAPON 4",
    "WEAPON 5", "WEAPON 6", "WEAPON 7",
    "AUTOMAP", "MENU",
    "NEXT WEAPON", "PREV WEAPON",
    "QUICK SAVE", "QUICK LOAD"
};

// The buttons that can be set, in the order the page lists them. Start is
// always the menu, as Escape is, and Guide belongs to the system.
static struct { int button; char* label; } padrows[] =
{
    {PB_A,	"A"},
    {PB_B,	"B"},
    {PB_X,	"X"},
    {PB_Y,	"Y"},
    {PB_LB,	"LB"},
    {PB_RB,	"RB"},
    {PB_LT,	"LT"},
    {PB_RT,	"RT"},
    {PB_BACK,	"VIEW"},
    {PB_LS,	"LEFT STICK"},
    {PB_RS,	"RIGHT STICK"},
    {PB_UP,	"D-PAD UP"},
    {PB_DOWN,	"D-PAD DOWN"},
    {PB_LEFT,	"D-PAD LEFT"},
    {PB_RIGHT,	"D-PAD RIGHT"}
};

#define NUM_PADROWS	(sizeof(padrows)/sizeof(padrows[0]))

// Fifteen rows is more than the small font's 13 pixel spacing fits between
// the title and the status bar, so this page packs them at 10: seven pixels
// of text and three between. The skull hangs over its neighbours' margin,
// which has nothing in it.
#define PADROWHEIGHT	10

menuitem_t PadButtonsMenu[] =
{
    {2,"",M_CyclePadButton,0}, {2,"",M_CyclePadButton,0},
    {2,"",M_CyclePadButton,0}, {2,"",M_CyclePadButton,0},
    {2,"",M_CyclePadButton,0}, {2,"",M_CyclePadButton,0},
    {2,"",M_CyclePadButton,0}, {2,"",M_CyclePadButton,0},
    {2,"",M_CyclePadButton,0}, {2,"",M_CyclePadButton,0},
    {2,"",M_CyclePadButton,0}, {2,"",M_CyclePadButton,0},
    {2,"",M_CyclePadButton,0}, {2,"",M_CyclePadButton,0},
    {2,"",M_CyclePadButton,0}
};

menu_t PadButtonsDef =
{
    NUM_PADROWS,
    &PadDef,
    PadButtonsMenu,
    M_DrawPadButtons,
    48,16,
    0,
    PADROWHEIGHT
};


void M_PadOptions (int choice)
{
    choice = 0;
    M_SetupNextMenu (&PadDef);
}

void M_PadButtons (int choice)
{
    choice = 0;
    M_SetupNextMenu (&PadButtonsDef);
}

void M_TogglePad (int choice)
{
    choice = 0;
    usepad = !usepad;
    S_StartSound (NULL, sfx_pistol);
}

void M_TogglePadSwap (int choice)
{
    choice = 0;
    padswapsticks = !padswapsticks;
    S_StartSound (NULL, sfx_pistol);
}

void M_TogglePadPushRun (int choice)
{
    choice = 0;
    padpushrun = !padpushrun;
    S_StartSound (NULL, sfx_pistol);
}

static void M_Step (int* value, int choice, int max)
{
    if (choice && *value < max)
	(*value)++;
    else if (!choice && *value > 0)
	(*value)--;
    S_StartSound (NULL, sfx_stnmov);
}

void M_ChangePadTurn (int choice)	{ M_Step (&padturnspeed, choice, 9); }
void M_ChangePadCurve (int choice)	{ M_Step (&padturncurve, choice, 9); }
void M_ChangePadMoveDZ (int choice)	{ M_Step (&padmovedeadzone, choice, 9); }
void M_ChangePadTurnDZ (int choice)	{ M_Step (&padturndeadzone, choice, 9); }

void M_ChangePadRumble (int choice)
{
    M_Step (&padrumble, choice, 9);
    // Felt while it is being set, if there is a pad to feel it on.
    I_PadRumbleTest ();
}

void M_CyclePadButton (int choice)
{
    int*	a;

    if (itemOn < 0 || itemOn >= (short)NUM_PADROWS)
	return;
    a = &padbind[padrows[itemOn].button];
    if (*a < 0 || *a >= PA_COUNT)
	*a = PA_NONE;
    *a = choice ? (*a + 1) % PA_COUNT : (*a + PA_COUNT - 1) % PA_COUNT;
    S_StartSound (NULL, sfx_stnmov);
}

// A pad's name, fitted to the screen: the small font is 320 pixels of
// about eight each, and the start of a name is the useful part.
static void M_WritePadName (int y)
{
    char	buf[40];
    char*	name = I_PadName ();
    char*	s;

    if (!name)
    {
	M_WriteText (56, y, "NO CONTROLLER CONNECTED");
	return;
    }
    snprintf (buf, sizeof(buf), "%.34s", name);
    for (s = buf; *s; s++)
    {
	*s = toupper (*s);
	if (*s < HU_FONTSTART || *s > HU_FONTEND)
	    *s = ' ';
    }
    M_WriteText (56, y, buf);
}

void M_DrawPadOptions (void)
{
    int		y = PadDef.y;
    char	buf[16];

    M_WriteText (56, 14, "CONTROLLER");
    M_WritePadName (28);

    M_WriteText (PadDef.x, y, "USE CONTROLLER");
    M_WriteText (PadDef.x + 148, y, usepad ? "ON" : "OFF");
    y += PadDef.lineheight;

    M_WriteText (PadDef.x, y, "BUTTONS...");
    y += PadDef.lineheight;

    snprintf (buf, sizeof(buf), "%d", padturnspeed);
    M_WriteText (PadDef.x, y, "TURN SPEED");
    M_WriteText (PadDef.x + 148, y, buf);
    y += PadDef.lineheight;

    snprintf (buf, sizeof(buf), "%d", padturncurve);
    M_WriteText (PadDef.x, y, "TURN CURVE");
    M_WriteText (PadDef.x + 148, y, buf);
    y += PadDef.lineheight;

    snprintf (buf, sizeof(buf), "%d", padmovedeadzone);
    M_WriteText (PadDef.x, y, "MOVE DEADZONE");
    M_WriteText (PadDef.x + 148, y, buf);
    y += PadDef.lineheight;

    snprintf (buf, sizeof(buf), "%d", padturndeadzone);
    M_WriteText (PadDef.x, y, "TURN DEADZONE");
    M_WriteText (PadDef.x + 148, y, buf);
    y += PadDef.lineheight;

    snprintf (buf, sizeof(buf), "%d", padrumble);
    M_WriteText (PadDef.x, y, "VIBRATION");
    M_WriteText (PadDef.x + 148, y, padrumble ? buf : "OFF");
    y += PadDef.lineheight;

    M_WriteText (PadDef.x, y, "SWAP STICKS");
    M_WriteText (PadDef.x + 148, y, padswapsticks ? "ON" : "OFF");
    y += PadDef.lineheight;

    M_WriteText (PadDef.x, y, "PUSH STICK TO RUN");
    M_WriteText (PadDef.x + 148, y, padpushrun ? "ON" : "OFF");

    M_WriteText (56, 146, "LEFT STICK MOVES, RIGHT STICK TURNS");
    M_WriteText (56, 156, "START IS ALWAYS THE MENU");
}

void M_DrawPadButtons (void)
{
    unsigned	i;
    int		y = PadButtonsDef.y;
    int		a;

    M_WriteText (48, 4, "CONTROLLER BUTTONS");

    for (i = 0; i < NUM_PADROWS; i++)
    {
	a = padbind[padrows[i].button];
	M_WriteText (PadButtonsDef.x, y + 1, padrows[i].label);
	M_WriteText (PadButtonsDef.x + 112, y + 1,
		     a >= 0 && a < PA_COUNT ? padactionnames[a] : "?");
	y += PadButtonsDef.lineheight;
    }
}


//
// Gameplay: the crosshair, the HUD and changing weapon on a pickup, from
// the Quake container's Options -> Gameplay.
//
enum
{
    gp_crosshair,
    gp_red,
    gp_green,
    gp_blue,
    gp_hud,
    gp_pickup,
    gp_maxfps,
    gp_end
} gameplay_e;

void M_ChangeCrosshair (int choice);
void M_ChangeCrosshairRed (int choice);
void M_ChangeCrosshairGreen (int choice);
void M_ChangeCrosshairBlue (int choice);
void M_ChangeHudStyle (int choice);
void M_ChangeWeaponPickup (int choice);
void M_ChangeMaxFps (int choice);
void M_DrawGameplay (void);

menuitem_t GameplayMenu[] =
{
    {2,"",M_ChangeCrosshair,'c'},
    {2,"",M_ChangeCrosshairRed,'r'},
    {2,"",M_ChangeCrosshairGreen,'g'},
    {2,"",M_ChangeCrosshairBlue,'b'},
    {2,"",M_ChangeHudStyle,'h'},
    {2,"",M_ChangeWeaponPickup,'w'},
    {2,"",M_ChangeMaxFps,'f'}
};

menu_t GameplayDef =
{
    gp_end,
    &SetupDef,
    GameplayMenu,
    M_DrawGameplay,
    32,48,
    0,
    SMALLLINEHEIGHT
};

static char* crosshairnames[] =
    { "OFF", "CROSS", "DOT", "CIRCLE", "CROSS WITH GAP", "CIRCLE WITH DOT" };
static char* pickupnames[] = { "ALWAYS", "ONLY IF NEW", "NEVER" };

void M_Gameplay (int choice)
{
    choice = 0;
    M_SetupNextMenu (&GameplayDef);
}

// A setting that goes round: right and Enter forward, left back.
static void M_Cycle (int* value, int choice, int count)
{
    if (*value < 0 || *value >= count)
	*value = 0;
    *value = choice ? (*value + 1) % count : (*value + count - 1) % count;
    S_StartSound (NULL, sfx_stnmov);
}

void M_ChangeCrosshair (int choice)	{ M_Cycle (&crosshair, choice, 6); }
void M_ChangeCrosshairRed (int choice)	{ M_Step (&crosshair_r, choice, 8); }
void M_ChangeCrosshairGreen (int choice) { M_Step (&crosshair_g, choice, 8); }
void M_ChangeCrosshairBlue (int choice)	{ M_Step (&crosshair_b, choice, 8); }
void M_ChangeWeaponPickup (int choice)	{ M_Cycle (&weaponpickup, choice, 3); }

// 35 is id's: drawn once a tic, nothing blended. 0 is no limit at all.
static int maxfpsvalues[] = { 35, 60, 72, 90, 120, 144, 165, 240, 0 };
#define NUMMAXFPS	(int) (sizeof(maxfpsvalues) / sizeof(maxfpsvalues[0]))

void M_ChangeMaxFps (int choice)
{
    int		i;

    // where it is in the list, or where a number typed into .doomrc would be
    for (i = 0 ; i < NUMMAXFPS - 1 ; i++)
	if (max_fps != 0 && max_fps <= maxfpsvalues[i])
	    break;

    if (max_fps == 0)
	i = NUMMAXFPS - 1;
    else if (max_fps != maxfpsvalues[i] && choice)
	i--;		// between two: on to the higher, back to the lower

    i = choice ? (i + 1) % NUMMAXFPS : (i + NUMMAXFPS - 1) % NUMMAXFPS;
    max_fps = maxfpsvalues[i];
    S_StartSound (NULL, sfx_stnmov);
}

void M_ChangeHudStyle (int choice)
{
    M_Cycle (&hud_style, choice, 2);
    // the view fills the screen with the minimal HUD, and goes back to the
    // screen size setting without it
    R_SetViewSize (screenblocks, detailLevel);
}

void M_DrawGameplay (void)
{
    int		x = GameplayDef.x;
    int		y = GameplayDef.y;
    int		vx = x + 184;
    char	buf[16];

    M_WriteText (x, 14, "GAMEPLAY");

    M_WriteText (x, y, "CROSSHAIR");
    M_WriteText (vx, y, crosshairnames[crosshair >= 0 && crosshair < 6
					? crosshair : 0]);
    y += GameplayDef.lineheight;

    snprintf (buf, sizeof(buf), "%d", crosshair_r);
    M_WriteText (x, y, "CROSSHAIR RED");
    M_WriteText (vx, y, buf);
    y += GameplayDef.lineheight;

    snprintf (buf, sizeof(buf), "%d", crosshair_g);
    M_WriteText (x, y, "CROSSHAIR GREEN");
    M_WriteText (vx, y, buf);
    y += GameplayDef.lineheight;

    snprintf (buf, sizeof(buf), "%d", crosshair_b);
    M_WriteText (x, y, "CROSSHAIR BLUE");
    M_WriteText (vx, y, buf);
    y += GameplayDef.lineheight;

    M_WriteText (x, y, "HUD STYLE");
    M_WriteText (vx, y, hud_style ? "MINIMAL" : "STATUS BAR");
    y += GameplayDef.lineheight;

    M_WriteText (x, y, "WEAPON ON PICKUP");
    M_WriteText (vx, y, pickupnames[weaponpickup >= 0 && weaponpickup < 3
				    ? weaponpickup : 1]);
    y += GameplayDef.lineheight;

    if (max_fps == 0)
	snprintf (buf, sizeof(buf), "NO LIMIT");
    else if (max_fps <= TICRATE)
	snprintf (buf, sizeof(buf), "35 (ID'S)");
    else
	snprintf (buf, sizeof(buf), "%d", max_fps);
    M_WriteText (x, y, "MAX FPS");
    M_WriteText (vx, y, buf);

    // ONLY IF NEW is id's, and all a demo or a net game will use
    M_WriteText (x, 146, "ONLY IF NEW IS ID'S, AND WHAT DEMOS");
    M_WriteText (x, 156, "AND NET GAMES ALWAYS USE");
}


//
// WAD selection.
//
// As many files as the folder has, up to this; ten rows at a time show,
// and the list scrolls under the cursor.
#define MAX_WADS	256
#define WAD_NAMELEN	64
#define WAD_ROWS	10

static char	wadNames[MAX_WADS][WAD_NAMELEN];	// the file's name
static char	wadTitles[MAX_WADS][WAD_NAMELEN];	// the game's (M_WadTitle)
static char	wadPaths[MAX_WADS][256];
static boolean	wadIsIwad[MAX_WADS];
static int	numWads = 0;
static int	wadTop = 0;		// the first row shown
static char	wadMessage[48] = "";

menuitem_t	WadMenu[MAX_WADS];		// filled in by M_ScanWads

// New Game shows the same list, to choose what to play before how hard;
// Load WAD, under Options, only loads.
static boolean	wadNewGame = false;


//
// Where to look for game data. The container points this at the folder the
// user mounted, which is not the same place the engine loads its IWAD from.
//
static char* M_WadDir (void)
{
    char*	dir;

    dir = getenv ("DOOM_WADPATH");
    if (dir && *dir)
	return dir;

    dir = getenv ("DOOMWADDIR");
    if (dir && *dir)
	return dir;

    return ".";
}


//
// An IWAD replaces the game; a PWAD is loaded on top of the current one.
// The four byte signature says which, and is more reliable than the name.
//
static boolean M_IsIwad (char* path)
{
    FILE*	f;
    char	id[4];
    boolean	result = false;

    f = fopen (path, "rb");

    if (!f)
	return false;

    if (fread (id, 1, 4, f) == 4 && !memcmp (id, "IWAD", 4))
	result = true;

    fclose (f);
    return result;
}


//
// Which maps a WAD holds, from its directory: ExMy maps are DOOM's, MAPxx
// maps DOOM II's. *episodes is the highest episode among the ExMy ones.
// Neither, for a WAD that only changes graphics or sounds.
//
#define WADMAPS_DOOM	1
#define WADMAPS_DOOM2	2

static int M_WadMaps (char* path, int* episodes)
{
    FILE*		f;
    unsigned char	head[12];
    unsigned char*	dir;
    char*		name;
    int			numlumps, infotableofs;
    int			i;
    int			maps = 0;

    *episodes = 0;

    f = fopen (path, "rb");
    if (!f)
	return 0;

    if (fread (head, 1, 12, f) != 12
	|| (memcmp (head, "IWAD", 4) && memcmp (head, "PWAD", 4)))
    {
	fclose (f);
	return 0;
    }

    numlumps = head[4] | head[5] << 8 | head[6] << 16 | head[7] << 24;
    infotableofs = head[8] | head[9] << 8 | head[10] << 16 | head[11] << 24;

    if (numlumps <= 0 || numlumps > (1 << 20) || infotableofs < 12
	|| fseek (f, infotableofs, SEEK_SET)
	|| !(dir = malloc (numlumps * 16)))
    {
	fclose (f);
	return 0;
    }

    if (fread (dir, 16, numlumps, f) != (size_t) numlumps)
	numlumps = 0;
    fclose (f);

    for (i = 0; i < numlumps; i++)
    {
	name = (char*) dir + i * 16 + 8;

	if (toupper (name[0]) == 'E' && name[1] >= '1' && name[1] <= '9'
	    && toupper (name[2]) == 'M' && name[3] >= '0' && name[3] <= '9'
	    && (name[4] == 0 || (name[4] >= '0' && name[4] <= '9' && !name[5])))
	{
	    maps |= WADMAPS_DOOM;
	    if (name[1] - '0' > *episodes)
		*episodes = name[1] - '0';
	}
	else if (!strncasecmp (name, "MAP", 3)
		 && name[3] >= '0' && name[3] <= '9'
		 && name[4] >= '0' && name[4] <= '9' && !name[5])
	    maps |= WADMAPS_DOOM2;
    }

    free (dir);
    return maps;
}


//
// The game a mod is to be loaded on.
//
// A PWAD goes on top of an IWAD, and has to go on top of the right one:
// SIGIL II's maps are E6M1 to E6M9 and its walls are built from DOOM's
// graphics, so on DOOM II it is a missing texture and a fatal error. Load WAD
// used to drop the running game's -iwad and let the engine pick, which it
// does in id's order, DOOM II first -- so SIGIL II, chosen while playing
// DOOM, started on DOOM II.
//
// Now: a mod with no maps, or with maps for the game already running, keeps
// that game, and so does one with maps for both. Otherwise the installed game
// that fits is found -- for DOOM maps the one with the most episodes, never
// the shareware one, which will not load a mod at all; for DOOM II maps DOOM
// II itself if it is there, even from TNT or Plutonia, since nearly every
// MAPxx mod was made for it. TNT or Plutonia only when there is no DOOM II,
// and then the one running if it is one of them. NULL if there is none.
//
// The engine runs all three as the same game, commercial -- gamemission is
// never set -- so DOOM II is told from the other two by its file name, doom2
// or doom2f, as the container's links and id's own names have it.
//
static boolean M_IsDoom2Name (char* path)
{
    char*	base = strrchr (path, '/');

    return !strncasecmp (base ? base + 1 : path, "doom2", 5);
}

static char* M_IwadFor (char* pwad)
{
    static char	best[256];
    char	cand[256];
    char*	dirs[2];
    int		pwadeps, eps;
    int		want;
    int		score, bestscore = 0;
    int		d, len;
    DIR*	dp;
    struct dirent* e;

    want = M_WadMaps (pwad, &pwadeps);

    // The game running now, if it will do.
    if ((!want || want == (WADMAPS_DOOM | WADMAPS_DOOM2))
	&& gamemode != shareware)
	return wadfiles[0];
    if (want == WADMAPS_DOOM2 && gamemode == commercial
	&& M_IsDoom2Name (wadfiles[0]))
	return wadfiles[0];
    if (want == WADMAPS_DOOM
	&& (gamemode == retail || (gamemode == registered && pwadeps <= 3)))
	return wadfiles[0];

    dirs[0] = M_WadDir ();
    dirs[1] = getenv ("DOOMWADDIR");

    for (d = 0; d < 2; d++)
    {
	if (!dirs[d] || !*dirs[d] || !(dp = opendir (dirs[d])))
	    continue;

	while ((e = readdir (dp)))
	{
	    len = strlen (e->d_name);
	    if (len < 5 || strcasecmp (e->d_name + len - 4, ".wad"))
		continue;

	    snprintf (cand, sizeof(cand), "%s/%s", dirs[d], e->d_name);
	    if (!M_IsIwad (cand))
		continue;

	    score = 0;
	    switch (M_WadMaps (cand, &eps))
	    {
	      case WADMAPS_DOOM:
		// shareware has one episode, and loads no mods
		if ((want & WADMAPS_DOOM || !want) && eps > 1)
		    score = 10 + eps;
		break;
	      case WADMAPS_DOOM2:
		if (want & WADMAPS_DOOM2)
		    score = M_IsDoom2Name (e->d_name) ? 20 : 10;
		break;
	    }

	    if (score > bestscore)
	    {
		bestscore = score;
		snprintf (best, sizeof(best), "%s", cand);
	    }
	}
	closedir (dp);
    }

    // No DOOM II: TNT or Plutonia will do, and the one running first.
    if (bestscore < 20 && want & WADMAPS_DOOM2 && gamemode == commercial)
	return wadfiles[0];

    return bestscore ? best : NULL;
}


//
// The name a WAD goes by, for the list: the game's or the mod's, rather
// than its file's. From the first of:
//
//   - its GAMEINFO lump's STARTUPTITLE (ZDoom's; SIGIL and SIGIL II have one)
//   - its UMAPINFO episodes' name, when they all have the one: how the 2024
//     re-release's add-ons, TNT and Plutonia name themselves
//   - the "Title :" line of a .txt of the same name beside it, the form
//     every mod on the idgames archive comes with
//   - for an IWAD, what its maps say it is, as the engine itself tells
//   - the file's name, without ".wad"
//
static char* M_WadLump (FILE* f, unsigned char* dir, int numlumps,
			const char* name, int* len)
{
    int		i;
    char*	data;

    for (i = numlumps - 1; i >= 0; i--)
    {
	unsigned char*	d = dir + i * 16;
	int		pos = d[0] | d[1] << 8 | d[2] << 16 | d[3] << 24;
	int		size = d[4] | d[5] << 8 | d[6] << 16 | d[7] << 24;

	if (strncasecmp ((char*) d + 8, name, 8))
	    continue;
	if (size <= 0 || size > (1 << 20) || fseek (f, pos, SEEK_SET)
	    || !(data = malloc (size + 1)))
	    return NULL;
	if (fread (data, 1, size, f) != (size_t) size)
	{
	    free (data);
	    return NULL;
	}
	data[size] = 0;
	*len = size;
	return data;
    }
    return NULL;
}

// the quoted string after 'key =' in text, or NULL
static boolean M_QuotedAfter (const char* text, const char* key,
			      char* out, int outlen)
{
    const char*	p = text;
    int		klen = strlen (key);

    while ((p = strcasestr (p, key)))
    {
	const char*	q = p + klen;

	p = q;
	while (*q == ' ' || *q == '\t')
	    q++;
	if (*q != '=')
	    continue;
	q++;
	while (*q == ' ' || *q == '\t')
	    q++;
	if (*q != '"')
	    continue;
	q++;
	snprintf (out, outlen, "%.*s", (int) (strchr (q, '"') ? strchr (q, '"') - q
					       : (int) strlen (q)), q);
	return *out != 0;
    }
    return false;
}

static void M_WadTitle (char* path, char* file, char* out, int outlen)
{
    FILE*		f = fopen (path, "rb");
    unsigned char	head[12];
    unsigned char*	dir = NULL;
    int			numlumps = 0, ofs, len, i;
    char*		text;
    boolean		iwad = false;
    boolean		e1 = false, e2 = false, e4 = false, map01 = false;
    boolean		freedoom = false, freedm = false;

    out[0] = 0;

    if (f && fread (head, 1, 12, f) == 12
	&& (!memcmp (head, "IWAD", 4) || !memcmp (head, "PWAD", 4)))
    {
	iwad = !memcmp (head, "IWAD", 4);
	numlumps = head[4] | head[5] << 8 | head[6] << 16 | head[7] << 24;
	ofs = head[8] | head[9] << 8 | head[10] << 16 | head[11] << 24;
	if (numlumps > 0 && numlumps < (1 << 20)
	    && !fseek (f, ofs, SEEK_SET)
	    && (dir = malloc (numlumps * 16))
	    && fread (dir, 16, numlumps, f) != (size_t) numlumps)
	    numlumps = 0;
    }

    if (dir && numlumps)
    {
	// GAMEINFO
	if ((text = M_WadLump (f, dir, numlumps, "GAMEINFO", &len)))
	{
	    M_QuotedAfter (text, "STARTUPTITLE", out, outlen);
	    free (text);
	}

	// UMAPINFO: episode = "M_EPIx", "Name", "k" -- one name for them all
	if (!out[0] && (text = M_WadLump (f, dir, numlumps, "UMAPINFO", &len)))
	{
	    char	name[WAD_NAMELEN] = "";
	    char*	p = text;
	    boolean	one = true;

	    while ((p = strcasestr (p, "episode")))
	    {
		char*	q = p + 7;
		char*	a;
		char*	b;

		p = q;
		while (*q == ' ' || *q == '\t')
		    q++;
		if (*q != '=' || !(a = strchr (q, '"')) || !(a = strchr (a + 1, '"'))
		    || !(a = strchr (a + 1, '"')) || !(b = strchr (a + 1, '"'))
		    || memchr (q, '\n', a - q))
		    continue;		// "episode = clear", or not the key
		if (!name[0])
		    snprintf (name, sizeof(name), "%.*s", (int) (b - a - 1), a + 1);
		else if ((int) strlen (name) != b - a - 1
			 || strncmp (name, a + 1, b - a - 1))
		    one = false;
	    }
	    if (name[0] && one)
		snprintf (out, outlen, "%s", name);
	    free (text);
	}

	for (i = 0; i < numlumps; i++)
	{
	    char*	n = (char*) dir + i * 16 + 8;

	    if (!strncasecmp (n, "E1M1", 5))		e1 = true;
	    else if (!strncasecmp (n, "E2M1", 5))	e2 = true;
	    else if (!strncasecmp (n, "E4M1", 5))	e4 = true;
	    else if (!strncasecmp (n, "MAP01", 6))	map01 = true;
	    else if (!strncasecmp (n, "FREEDOOM", 8))	freedoom = true;
	    else if (!strncasecmp (n, "FREEDM", 7))	freedm = true;
	}
    }
    if (f)
	fclose (f);
    free (dir);

    // the idgames text file beside it
    if (!out[0])
    {
	char	txt[300];
	char	line[256];
	char*	dot;
	FILE*	t = NULL;

	snprintf (txt, sizeof(txt), "%s", path);
	if ((dot = strrchr (txt, '.')) && !strchr (dot, '/'))
	{
	    strcpy (dot, ".txt");
	    if (!(t = fopen (txt, "r")))
	    {
		strcpy (dot, ".TXT");
		t = fopen (txt, "r");
	    }
	}
	while (t && fgets (line, sizeof(line), t))
	{
	    char*	c = line;

	    while (*c == ' ' || *c == '\t')
		c++;
	    if (strncasecmp (c, "Title", 5))
		continue;
	    c += 5;
	    while (*c == ' ' || *c == '\t')
		c++;
	    if (*c != ':')
		continue;
	    c++;
	    while (*c == ' ' || *c == '\t')
		c++;
	    c[strcspn (c, "\r\n")] = 0;
	    if (*c)
		snprintf (out, outlen, "%s", c);
	    break;
	}
	if (t)
	    fclose (t);
    }

    // an IWAD with nothing to say: what it is
    if (!out[0] && iwad)
    {
	if (freedm)
	    snprintf (out, outlen, "FreeDM");
	else if (freedoom)
	    snprintf (out, outlen, map01 ? "Freedoom: Phase 2"
		      : "Freedoom: Phase 1");
	else if (map01)
	    snprintf (out, outlen, "%s",
		      !strncasecmp (file, "tnt", 3) ? "TNT: Evilution"
		      : !strncasecmp (file, "plutonia", 8)
		      ? "The Plutonia Experiment" : "DOOM II: Hell on Earth");
	else if (e4)
	    snprintf (out, outlen, "The Ultimate DOOM");
	else if (e2)
	    snprintf (out, outlen, "DOOM");
	else if (e1)
	    snprintf (out, outlen, "DOOM Shareware");
    }

    if (!out[0])
    {
	snprintf (out, outlen, "%s", file);
	if (strlen (out) > 4 && !strcasecmp (out + strlen (out) - 4, ".wad"))
	    out[strlen (out) - 4] = 0;
    }
}


static void M_ScanWads (void)
{
    DIR*		d;
    struct dirent*	e;
    char*		dir = M_WadDir();
    int			len;

    numWads = 0;
    d = opendir (dir);

    if (!d)
    {
	snprintf (wadMessage, sizeof(wadMessage), "CANNOT READ %s", dir);
	return;
    }

    while ((e = readdir(d)) && numWads < MAX_WADS)
    {
	len = strlen (e->d_name);

	if (len < 5 || strcasecmp (e->d_name + len - 4, ".wad"))
	    continue;

	snprintf (wadPaths[numWads], sizeof(wadPaths[0]), "%s/%s", dir, e->d_name);

	if (access (wadPaths[numWads], R_OK))
	    continue;

	snprintf (wadNames[numWads], WAD_NAMELEN, "%s", e->d_name);
	wadIsIwad[numWads] = M_IsIwad (wadPaths[numWads]);
	M_WadTitle (wadPaths[numWads], e->d_name, wadTitles[numWads],
		    WAD_NAMELEN);
	numWads++;
    }

    closedir (d);

    // In alphabetical order of the names shown, whatever order the folder
    // keeps them in; two of the same name by file name.
    {
	int	i, j;

	for (i = 1; i < numWads; i++)
	    for (j = i; j > 0
		 && (strcasecmp (wadTitles[j-1], wadTitles[j]) > 0
		     || (!strcasecmp (wadTitles[j-1], wadTitles[j])
			 && strcasecmp (wadNames[j-1], wadNames[j]) > 0)); j--)
	    {
		char	name[WAD_NAMELEN];
		char	title[WAD_NAMELEN];
		char	path[256];
		boolean	iwad = wadIsIwad[j];

		memcpy (name, wadNames[j], WAD_NAMELEN);
		memcpy (title, wadTitles[j], WAD_NAMELEN);
		memcpy (path, wadPaths[j], 256);
		memcpy (wadNames[j], wadNames[j-1], WAD_NAMELEN);
		memcpy (wadTitles[j], wadTitles[j-1], WAD_NAMELEN);
		memcpy (wadPaths[j], wadPaths[j-1], 256);
		wadIsIwad[j] = wadIsIwad[j-1];
		memcpy (wadNames[j-1], name, WAD_NAMELEN);
		memcpy (wadTitles[j-1], title, WAD_NAMELEN);
		memcpy (wadPaths[j-1], path, 256);
		wadIsIwad[j-1] = iwad;
	    }
    }

    for (len = 0; len < MAX_WADS; len++)
    {
	WadMenu[len].status = 1;
	WadMenu[len].name[0] = 0;
	WadMenu[len].routine = M_LoadWad;
	WadMenu[len].alphaKey = 0;
	WadMenu[len].text = NULL;
    }
    wadTop = 0;

    if (!numWads)
	snprintf (wadMessage, sizeof(wadMessage), "NO WAD FILES IN %s", dir);
    else
	wadMessage[0] = 0;
}


//
// Restart into the chosen file.
//
// Nothing here can be swapped while the game is running: textures, sprites,
// the sound cache and every zone allocation are built once around the WADs
// loaded at startup. So the engine hands itself the new arguments and starts
// again, which takes a couple of seconds and lands back on the title screen.
//
static void M_RelaunchWithFiles (char** files, int n)
{
    char*	newargv[2*MAX_WADS + 16];
    char*	path = files[n-1];
    int		argc = 0;
    int		i;

    newargv[argc++] = myargv[0];

    // Carry the original arguments across, dropping any WAD selection made
    // last time so the choices do not accumulate.
    for (i = 1; i < myargc && argc < MAX_WADS + 5; i++)
    {
	if (!strcasecmp (myargv[i], "-iwad") || !strcasecmp (myargv[i], "-file"))
	{
	    // Skip the option and the filenames that follow it.
	    while (i + 1 < myargc && myargv[i+1][0] != '-')
		i++;
	    continue;
	}

	newargv[argc++] = myargv[i];
    }

    // The game, and the mods on it.
    newargv[argc++] = "-iwad";
    newargv[argc++] = files[0];
    if (n > 1)
	newargv[argc++] = "-file";
    for (i = 1; i < n && argc < 2*MAX_WADS + 15; i++)
	newargv[argc++] = files[i];
    newargv[argc] = NULL;

    // Where to come back to if the engine cannot start on the new file:
    // these arguments, which it is running on now. See I_ErrorGoBack.
    {
	static char	prev[4096];
	size_t		n = 0;
	char*		base = strrchr (path, '/');

	for (i = 0; i < myargc && n < sizeof(prev); i++)
	    n += snprintf (prev + n, sizeof(prev) - n, "%s%s",
			   i ? "\x1f" : "", myargv[i]);
	if (n < sizeof(prev))
	    setenv ("DOOM_PREVIOUS_ARGS", prev, 1);
	setenv ("DOOM_LOADING", base ? base + 1 : path, 1);
    }

    M_SaveDefaults ();
    I_ShutdownSound ();
    I_ShutdownMusic ();
    I_ShutdownGraphics ();

    execv ("/proc/self/exe", newargv);

    // /proc is not always there; fall back to however we were invoked.
    execv (myargv[0], newargv);

    I_Error ("Could not restart to load %s", path);
}

// A game on its own, or a mod on the game M_IwadFor chose for it.
static void M_RelaunchWith (char* path, char* iwad)
{
    char*	files[2];

    if (iwad)
    {
	files[0] = iwad;
	files[1] = path;
	M_RelaunchWithFiles (files, 2);
    }
    else
	M_RelaunchWithFiles (&path, 1);
}


//
// M_LoadSaveOn
// A savegame made on other WADs than these: a level is only its WADs'
// (the save holds its sectors and lines by number), so the engine restarts
// on them and loads it there -- DOOM_LOADGAME says which save. False, and
// nothing done, when one of them is not here to restart on; *missing is it.
//
boolean M_LoadSaveOn (char** files, int n, char* savename, char** missing)
{
    FILE*	f;
    int		i;

    for (i = 0; i < n; i++)
    {
	if (!(f = fopen (files[i], "rb")))
	{
	    *missing = files[i];
	    return false;
	}
	fclose (f);
    }
    setenv ("DOOM_LOADGAME", savename, 1);
    M_RelaunchWithFiles (files, n);
    return true;		// not reached
}


//
// M_LoadGameFailed
// Why a savegame was not loaded, until a key is pressed. file: the WAD it
// was saved on that is not here; NULL when the save does not fit the level
// it names in the game running, which is a save from another game or mod
// that did not record which.
//
void M_LoadGameFailed (char* file)
{
    static char	text[256];

    if (file)
    {
	char	title[WAD_NAMELEN];
	char*	base = strrchr (file, '/');

	base = base ? base + 1 : file;
	M_WadTitle (file, base, title, sizeof(title));
	snprintf (text, sizeof(text),
		  "THIS GAME WAS SAVED IN\n%.30s,\n"
		  "WHICH IS NOT IN THE WAD FOLDER.\n\n"PRESSKEY,
		  title[0] ? title : base);
    }
    else
	snprintf (text, sizeof(text),
		  "THIS GAME WAS SAVED IN ANOTHER\n"
		  "GAME OR MOD. LOAD THAT FIRST,\n"
		  "FROM OPTIONS, SETUP, LOAD WAD.\n\n"PRESSKEY);
    M_StartMessage (text, NULL, false);
}



menu_t WadDef =
{
    1,
    &SetupDef,
    WadMenu,
    M_DrawWadSelect,
    40,38,
    0,
    SMALLLINEHEIGHT
};


//
// New Game: the game or mod first, then the difficulty.
//
// Choosing a mod used to mean Options, Setup, Load WAD, the restart, and
// then New Game and an episode menu that listed the game's episodes as well
// as the mod's. New Game now opens the WAD list; what is already running
// goes straight on, anything else restarts the engine (the only way to
// change WADs, see M_RelaunchWith) and the new engine opens the difficulty
// menu itself -- DOOM_NEWGAME says to.
//
// A mod whose maps are all one episode -- SIGIL, SIGIL II, any MAPxx mod --
// needs no episode menu: it starts on its own first map. A game with
// episodes to choose between still offers them.
//

static boolean M_SameFile (char* a, char* b)
{
    struct stat	sa, sb;

    if (!a || !b || stat (a, &sa) || stat (b, &sb))
	return false;
    return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

// whether this row of the list is what is running now: the game on its own,
// or the one mod on top of it
static boolean M_WadIsRunning (int i)
{
    if (wadIsIwad[i])
	return M_SameFile (wadPaths[i], wadfiles[0]) && !wadfiles[1];
    return wadfiles[1] && !wadfiles[2] && M_SameFile (wadPaths[i], wadfiles[1]);
}

// A map lump's episode and map, from its name; 0 when it is not one.
static int M_MapName (char* name, int* episode, int* map)
{
    if (toupper (name[0]) == 'E' && name[1] >= '1' && name[1] <= '9'
	&& toupper (name[2]) == 'M' && name[3] >= '0' && name[3] <= '9'
	&& (name[4] == 0 || (name[4] >= '0' && name[4] <= '9' && !name[5])))
    {
	*episode = name[1] - '0';
	*map = atoi (name + 3);
	return 1;
    }
    if (!strncasecmp (name, "MAP", 3) && name[3] >= '0' && name[3] <= '9'
	&& name[4] >= '0' && name[4] <= '9' && !name[5])
    {
	*episode = 1;
	*map = atoi (name + 3);
	return 1;
    }
    return 0;
}

//
// The first map of the mods loaded (everything after the IWAD), and how
// many episodes their maps are in; 0 when they have none.
//
static int M_ModMaps (int* firstepisode, int* firstmap)
{
    int		i, e, m;
    int		episodes = 0;
    int		seen[10] = { 0 };
    char	name[9];

    *firstepisode = *firstmap = 0;
    for (i = 0; i < numlumps; i++)
    {
	if (lumpinfo[i].handle == lumpinfo[0].handle)
	    continue;
	memcpy (name, lumpinfo[i].name, 8);
	name[8] = 0;
	if (!M_MapName (name, &e, &m))
	    continue;
	if (!seen[e])
	    seen[e] = 1, episodes++;
	if (!*firstepisode || e < *firstepisode
	    || (e == *firstepisode && m < *firstmap))
	    *firstepisode = e, *firstmap = m;
    }
    return episodes;
}

//
// On from the choice of what to play, with it running: the difficulty for
// a mod of one episode or a game of none, else the episodes. back is where
// Escape goes from there.
//
static void M_NewGameFor (menu_t* back)
{
    int		e, m;

    if (M_ModMaps (&e, &m) == 1)
    {
	epi = -1;
	modepisode = e;
	modmap = m;
	NewDef.prevMenu = back;
	M_SetupNextMenu (&NewDef);
    }
    else if (EpiDef.numitems <= 1)
    {
	// No choice to make with one episode, or with DOOM II's none.
	epi = 0;
	modepisode = 0;
	NewDef.prevMenu = back;
	M_SetupNextMenu (&NewDef);
    }
    else
    {
	modepisode = 0;
	EpiDef.prevMenu = back;
	NewDef.prevMenu = &EpiDef;
	M_SetupNextMenu (&EpiDef);
    }
}

static void M_ChooseWadToPlay (void)
{
    int		i;

    M_ScanWads ();
    if (!numWads)
    {
	M_NewGameFor (&MainDef);
	return;
    }

    wadNewGame = true;
    WadDef.prevMenu = &MainDef;
    WadDef.numitems = numWads;
    WadDef.lastOn = 0;
    for (i = 0; i < numWads; i++)
	if (M_WadIsRunning (i))
	    WadDef.lastOn = i;
    M_SetupNextMenu (&WadDef);
}

//
// Just restarted from New Game's list: menus open, on the difficulty (or the
// episodes). From D_DoomMain.
//
void M_NewGameAfterRestart (void)
{
    M_StartControlPanel ();
    M_NewGameFor (&MainDef);
    fprintf (stderr, "New game: on to %s\n",
	     currentMenu == &EpiDef ? "the episodes" : "the difficulty");
}


void M_WadSelect (int choice)
{
    choice = 0;
    wadNewGame = false;
    WadDef.prevMenu = &SetupDef;
    M_ScanWads ();
    // Only offer as many rows as there are files.
    WadDef.numitems = numWads ? numWads : 1;
    WadDef.lastOn = 0;
    M_SetupNextMenu (&WadDef);
}


void M_LoadWad (int choice)
{
    char*	iwad;

    if (choice < 0 || choice >= numWads)
	return;

    if (wadNewGame)
    {
	// what is running already: no restart, on to the difficulty
	if (M_WadIsRunning (choice))
	{
	    M_NewGameFor (&WadDef);
	    return;
	}
	setenv ("DOOM_NEWGAME", "1", 1);
    }

    if (wadIsIwad[choice])
    {
	M_RelaunchWith (wadPaths[choice], NULL);
	return;
    }

    // A mod needs a game under it, and the right one. When there is none,
    // say so here: restarting would only end in an error. That includes
    // shareware, which the engine refuses to load any mod on.
    iwad = M_IwadFor (wadPaths[choice]);
    if (!iwad)
    {
	int	eps;
	int	maps = M_WadMaps (wadPaths[choice], &eps);

	if (maps & WADMAPS_DOOM2)
	    snprintf (wadMessage, sizeof(wadMessage), "NEEDS DOOM II, NOT FOUND");
	else if (maps)
	    snprintf (wadMessage, sizeof(wadMessage), "NEEDS DOOM, NOT FOUND");
	else
	    snprintf (wadMessage, sizeof(wadMessage),
		      "SHAREWARE CANNOT LOAD MODS");
	S_StartSound (NULL, sfx_oof);
	unsetenv ("DOOM_NEWGAME");
	return;
    }

    M_RelaunchWith (wadPaths[choice], iwad);
}


void M_DrawWadSelect (void)
{
    int		i;
    int		y;
    int		top = 38;

    M_WriteText (40, 14, wadNewGame ? "NEW GAME: WHICH GAME OR MOD?"
		 : "LOAD WAD: THE GAME RESTARTS");
    // Above the list: with ten files the rows reach y=152, and anything
    // below 168 would be drawn onto the status bar and stay there. The
    // file the cursor is on, as the rows give the game's name.
    if (wadMessage[0])
	M_WriteText (40, 28, wadMessage);
    else if (numWads && itemOn >= 0 && itemOn < numWads)
    {
	char	file[WAD_NAMELEN + 8];

	snprintf (file, sizeof(file), "FILE: %.60s", wadNames[itemOn]);
	M_WriteText (40, 28, file);
    }

    if (!numWads)
    {
	WadDef.y = top;
	M_WriteText (40, WadDef.y, "NO WAD FILES FOUND");
	return;
    }

    // Ten rows at a time, following the cursor. M_Drawer puts the skull at
    // WadDef.y + itemOn rows, so WadDef.y moves up with the list.
    if (itemOn < wadTop)
	wadTop = itemOn;
    if (itemOn >= wadTop + WAD_ROWS)
	wadTop = itemOn - WAD_ROWS + 1;
    WadDef.y = top - wadTop * WadDef.lineheight;

    y = top;
    for (i = wadTop; i < numWads && i < wadTop + WAD_ROWS; i++)
    {
	// the game's name, cut short of the GAME / MOD column
	{
	    char	name[WAD_NAMELEN + 4];
	    int		n = strlen (wadTitles[i]);

	    snprintf (name, sizeof(name), "%.63s", wadTitles[i]);
	    while (n > 1 && M_StringWidth (name) > 172)
	    {
		n--;
		snprintf (name, sizeof(name), "%.*s...", n, wadTitles[i]);
	    }
	    M_WriteText (WadDef.x, y, name);
	}
	M_WriteText (WadDef.x + 180, y, M_WadIsRunning (i) ? "PLAYING"
		     : wadIsIwad[i] ? "GAME" : "MOD");
	y += WadDef.lineheight;
    }

    // more above or below, against the screen's right edge
    if (wadTop > 0)
	M_WriteText (318 - M_StringWidth ("UP"), top, "UP");
    if (wadTop + WAD_ROWS < numWads)
	M_WriteText (318 - M_StringWidth ("MORE"),
		     top + (WAD_ROWS - 1) * WadDef.lineheight, "MORE");
}


//
// M_Options
//
char    detailNames[2][9]	= {"M_GDHIGH","M_GDLOW"};
char	msgNames[2][9]		= {"M_MSGOFF","M_MSGON"};


void M_DrawOptions(void)
{
    int		lh = OptionsDef.lineheight;
    int		ty = (lh - SHORT(hu_font[0]->height))/2;

    V_DrawPatchDirect (108,15,0,W_CacheLumpName("M_OPTTTL",PU_CACHE));

    // The values as text too. They were graphics -- M_GDHIGH, M_MSGON and so
    // on -- which stood a head taller than the small labels beside them now.
    M_WriteText (OptionsDef.x + 148, OptionsDef.y + lh*detail + ty,
		 detailLevel ? "LOW" : "HIGH");

    M_WriteText (OptionsDef.x + 148, OptionsDef.y + lh*messages + ty,
		 showMessages ? "ON" : "OFF");

    M_DrawThermo(OptionsDef.x,OptionsDef.y+lh*(mousesens+1),
		 10,mouseSensitivity);
	
    M_DrawThermo(OptionsDef.x,OptionsDef.y+lh*(scrnsize+1),
		 9,screenSize);
}

void M_Options(int choice)
{
    M_SetupNextMenu(&OptionsDef);
}



//
//      Toggle messages on/off
//
void M_ChangeMessages(int choice)
{
    // warning: unused parameter `int choice'
    choice = 0;
    showMessages = 1 - showMessages;
	
    if (!showMessages)
	players[consoleplayer].message = MSGOFF;
    else
	players[consoleplayer].message = MSGON ;

    message_dontfuckwithme = true;
}


//
// M_EndGame
//
void M_EndGameResponse(int ch)
{
    if (ch != 'y')
	return;
		
    currentMenu->lastOn = itemOn;
    M_ClearMenus ();
    D_StartTitle ();
}

void M_EndGame(int choice)
{
    choice = 0;
    if (!usergame)
    {
	S_StartSound(NULL,sfx_oof);
	return;
    }
	
    if (netgame)
    {
	M_StartMessage(NETEND,NULL,false);
	return;
    }
	
    M_StartMessage(ENDGAME,M_EndGameResponse,true);
}




//
// M_ReadThis
//
void M_ReadThis(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef1);
}

void M_ReadThis2(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef2);
}

void M_FinishReadThis(int choice)
{
    choice = 0;
    M_SetupNextMenu(&MainDef);
}




//
// M_QuitDOOM
//
int     quitsounds[8] =
{
    sfx_pldeth,
    sfx_dmpain,
    sfx_popain,
    sfx_slop,
    sfx_telept,
    sfx_posit1,
    sfx_posit3,
    sfx_sgtatk
};

int     quitsounds2[8] =
{
    sfx_vilact,
    sfx_getpow,
    sfx_boscub,
    sfx_slop,
    sfx_skeswg,
    sfx_kntdth,
    sfx_bspact,
    sfx_sgtatk
};



void M_QuitResponse(int ch)
{
    if (ch != 'y')
	return;
    if (!netgame)
    {
	if (gamemode == commercial)
	    S_StartSound(NULL,quitsounds2[(gametic>>2)&7]);
	else
	    S_StartSound(NULL,quitsounds[(gametic>>2)&7]);
	I_WaitVBL(105);
    }
    I_Quit ();
}




void M_QuitDOOM(int choice)
{
  // We pick index 0 which is language sensitive,
  //  or one at random, between 1 and maximum number.
  if (language != english )
    sprintf(endstring,"%s\n\n"DOSY, D_Text (endmsg[0]) );
  else
    sprintf(endstring,"%s\n\n"DOSY,
	    D_Text (endmsg[ (gametic%(NUM_QUITMESSAGES-2))+1 ]));
  
  M_StartMessage(endstring,M_QuitResponse,true);
}




void M_ChangeSensitivity(int choice)
{
    switch(choice)
    {
      case 0:
	if (mouseSensitivity)
	    mouseSensitivity--;
	break;
      case 1:
	if (mouseSensitivity < 9)
	    mouseSensitivity++;
	break;
    }
}




void M_ChangeDetail(int choice)
{
    choice = 0;

    // Low detail does not work in this port. The 1997 source says so itself,
    // in the comment just below, and the routine it selects indexes past the
    // end of its column table.
    //
    // It used to flip detailLevel anyway. That did nothing visible, because
    // the call that would apply it is commented out -- but the changed value
    // was still written to .doomrc, and the next start applied it for real,
    // at which point the renderer drew outside the framebuffer and the game
    // died. Pressing this one menu item broke the game on the following
    // launch. Leave the setting alone and say why.
    players[consoleplayer].message = "LOW DETAIL IS NOT AVAILABLE";

    return;
    
    /*R_SetViewSize (screenblocks, detailLevel);

    if (!detailLevel)
	players[consoleplayer].message = DETAILHI;
    else
	players[consoleplayer].message = DETAILLO;*/
}




void M_SizeDisplay(int choice)
{
    switch(choice)
    {
      case 0:
	if (screenSize > 0)
	{
	    screenblocks--;
	    screenSize--;
	}
	break;
      case 1:
	if (screenSize < 8)
	{
	    screenblocks++;
	    screenSize++;
	}
	break;
    }
	

    R_SetViewSize (screenblocks, detailLevel);
}




//
//      Menu Functions
//
void
M_DrawThermo
( int	x,
  int	y,
  int	thermWidth,
  int	thermDot )
{
    int		xx;
    int		i;

    xx = x;
    V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERML",PU_CACHE));
    xx += 8;
    for (i=0;i<thermWidth;i++)
    {
	V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERMM",PU_CACHE));
	xx += 8;
    }
    V_DrawPatchDirect (xx,y,0,W_CacheLumpName("M_THERMR",PU_CACHE));

    V_DrawPatchDirect ((x+8) + thermDot*8,y,
		       0,W_CacheLumpName("M_THERMO",PU_CACHE));

    // And the value, after it. Counting the notches was the only way to know
    // what a volume was set to -- or, with sixteen of them, to be sure.
    {
	char	num[8];

	snprintf (num, sizeof(num), "%d", thermDot);
	M_WriteText (xx + 12, y + 3, num);
    }
}



void
M_DrawEmptyCell
( menu_t*	menu,
  int		item )
{
    V_DrawPatchDirect (menu->x - 10,        menu->y+item*LINEHEIGHT - 1, 0,
		       W_CacheLumpName("M_CELL1",PU_CACHE));
}

void
M_DrawSelCell
( menu_t*	menu,
  int		item )
{
    V_DrawPatchDirect (menu->x - 10,        menu->y+item*LINEHEIGHT - 1, 0,
		       W_CacheLumpName("M_CELL2",PU_CACHE));
}


void
M_StartMessage
( char*		string,
  void*		routine,
  boolean	input )
{
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = (char*) D_Text (string);	// or a patch's
    messageRoutine = routine;
    messageNeedsInput = input;
    messageEnterUp = !enterHeld;
    menuactive = true;
    return;
}



void M_StopMessage(void)
{
    menuactive = messageLastMenuActive;
    messageToPrint = 0;
}



//
// Find string width from hu_font chars
//
int M_StringWidth(char* string)
{
    int             i;
    int             w = 0;
    int             c;
	
    for (i = 0;i < strlen(string);i++)
    {
	c = toupper(string[i]) - HU_FONTSTART;
	if (c < 0 || c >= HU_FONTSIZE)
	    w += 4;
	else
	    w += SHORT (hu_font[c]->width);
    }
		
    return w;
}



//
//      Find string height from hu_font chars
//
int M_StringHeight(char* string)
{
    int             i;
    int             h;
    int             height = SHORT(hu_font[0]->height);
	
    h = height;
    for (i = 0;i < strlen(string);i++)
	if (string[i] == '\n')
	    h += height;
		
    return h;
}


//
//      Write a string using the hu_font
//
void
M_WriteText
( int		x,
  int		y,
  char*		string)
{
    int		w;
    char*	ch;
    int		c;
    int		cx;
    int		cy;
		

    ch = string;
    cx = x;
    cy = y;
	
    while(1)
    {
	c = *ch++;
	if (!c)
	    break;
	if (c == '\n')
	{
	    cx = x;
	    cy += 12;
	    continue;
	}
		
	c = toupper(c) - HU_FONTSTART;
	if (c < 0 || c>= HU_FONTSIZE)
	{
	    cx += 4;
	    continue;
	}
		
	w = SHORT (hu_font[c]->width);
	if (cx+w > SCREENWIDTH)
	    break;
	V_DrawPatchDirect(cx, cy, 0, hu_font[c]);
	cx+=w;
    }
}



//
// CONTROL PANEL
//

//
// M_PadKey
//
// What a controller button means while the menu is up, as a key the menu
// already answers to. Start is Escape everywhere. A chooses and B goes back a
// page, closing the menu from the top one -- except on a question, where they
// are yes and no, and while a savegame is being named, where B abandons it
// as Escape does rather than rubbing out a letter. While the Controls page is
// waiting for a key, B and Start cancel and nothing else is taken, so a pad
// button cannot end up bound to a keyboard action.
//
int M_PadKey (int b)
{
    if (b == PB_START)
	return KEY_ESCAPE;

    if (messageToPrint)
    {
	if (messageNeedsInput)
	    return b == PB_A ? 'y' : b == PB_B ? 'n' : 0;
	return b == PB_A || b == PB_B ? KEY_ENTER : 0;
    }

    if (bindingWait)
	return b == PB_B ? KEY_ESCAPE : 0;

    if (saveStringEnter)
	return b == PB_A ? KEY_ENTER : b == PB_B ? KEY_ESCAPE : 0;

    switch (b)
    {
      case PB_A:	return KEY_ENTER;
      case PB_B:	return currentMenu->prevMenu ? KEY_BACKSPACE : KEY_ESCAPE;
      case PB_Y:	return KEY_DEL;
      case PB_UP:	return KEY_UPARROW;
      case PB_DOWN:	return KEY_DOWNARROW;
      case PB_LEFT:	return KEY_LEFTARROW;
      case PB_RIGHT:	return KEY_RIGHTARROW;
      case PB_LB:	return KEY_LEFTARROW;
      case PB_RB:	return KEY_RIGHTARROW;
    }
    return 0;
}


//
// M_Responder
//
boolean M_Responder (event_t* ev)
{
    int             ch;
    int             i;
    static  int     joywait = 0;
    static  int     mousewait = 0;
    static  int     mousey = 0;
    static  int     lasty = 0;
    static  int     mousex = 0;
    static  int     lastx = 0;
	
    ch = -1;

    // Where Enter is, for answering a question with it (below).
    if (ev->type == ev_keydown && ev->data1 == KEY_ENTER)
	enterHeld = true;
    if (ev->type == ev_keyup && ev->data1 == KEY_ENTER)
    {
	enterHeld = false;
	enterUpMs = I_NowMs ();
	messageEnterUp = true;
    }
	
    if (ev->type == ev_joystick && joywait < I_GetTime())
    {
	if (ev->data3 == -1)
	{
	    ch = KEY_UPARROW;
	    joywait = I_GetTime() + 5;
	}
	else if (ev->data3 == 1)
	{
	    ch = KEY_DOWNARROW;
	    joywait = I_GetTime() + 5;
	}
		
	if (ev->data2 == -1)
	{
	    ch = KEY_LEFTARROW;
	    joywait = I_GetTime() + 2;
	}
	else if (ev->data2 == 1)
	{
	    ch = KEY_RIGHTARROW;
	    joywait = I_GetTime() + 2;
	}
		
	if (ev->data1&1)
	{
	    ch = KEY_ENTER;
	    joywait = I_GetTime() + 5;
	}
	if (ev->data1&2)
	{
	    ch = KEY_BACKSPACE;
	    joywait = I_GetTime() + 5;
	}
    }
    else
    {
	if (ev->type == ev_mouse && mousewait < I_GetTime())
	{
	    mousey += ev->data3;
	    if (mousey < lasty-30)
	    {
		ch = KEY_DOWNARROW;
		mousewait = I_GetTime() + 5;
		mousey = lasty -= 30;
	    }
	    else if (mousey > lasty+30)
	    {
		ch = KEY_UPARROW;
		mousewait = I_GetTime() + 5;
		mousey = lasty += 30;
	    }
		
	    mousex += ev->data2;
	    if (mousex < lastx-30)
	    {
		ch = KEY_LEFTARROW;
		mousewait = I_GetTime() + 5;
		mousex = lastx -= 30;
	    }
	    else if (mousex > lastx+30)
	    {
		ch = KEY_RIGHTARROW;
		mousewait = I_GetTime() + 5;
		mousex = lastx += 30;
	    }
		
	    if (ev->data1&1)
	    {
		ch = KEY_ENTER;
		mousewait = I_GetTime() + 15;
	    }
			
	    if (ev->data1&2)
	    {
		ch = KEY_BACKSPACE;
		mousewait = I_GetTime() + 15;
	    }
	}
	else
	    if (ev->type == ev_keydown)
	    {
		ch = ev->data1;
	    }
    }
    
    if (ch == -1)
	return false;

    // Waiting for a key to bind. Take whatever was pressed, unless it is
    // escape, which backs out and leaves the binding alone.
    //
    // Enter is refused and the prompt left open. It is the key that opened the
    // prompt, so pressing it again is what anybody does when they are not sure
    // the first press registered -- and it would bind the menu's own confirm
    // key to a game control, which makes that control confirm menu items from
    // then on. A real config arrived with key_fire 13 by exactly this route.
    // Escape still cancels, so nothing is stuck.
    if (bindingWait)
    {
	if (ch == KEY_ENTER)
	    return true;

	bindingWait = false;

	// Backspace is the menu's, as Escape is: it cancels, and is never a
	// binding (see the pop-up below).
	if (ch == KEY_BACKSPACE)
	    ch = KEY_ESCAPE;

	// Out of range is refused as well. xlatekey passes a keysym it does not
	// recognise straight through, so a Super key or a media key arrives here
	// as 65515 or thereabouts -- and G_Responder only ever records 0..255, so
	// binding one produces a control that can never fire and a config file
	// the game then reads that value back out of for ever.
	if (ch != KEY_ESCAPE
	    && ch >= 0 && ch < MAXKEYVALUE
	    && currentMenu == &ControlsDef
	    && itemOn >= 0 && itemOn < (short)NUM_BINDINGS)
	{
	    *bindings[itemOn].key = ch;
	    S_StartSound (NULL, sfx_pistol);
	}

	return true;
    }

    
    // Save Game string input
    if (saveStringEnter)
    {
	switch(ch)
	{
	  case KEY_DEL:			// what it always did here, as Backspace
	  case KEY_BACKSPACE:
	    if (saveCharIndex > 0)
	    {
		saveCharIndex--;
		savegamestrings[saveSlot][saveCharIndex] = 0;
	    }
	    break;
				
	  case KEY_ESCAPE:
	    saveStringEnter = 0;
	    strcpy(&savegamestrings[saveSlot][0],saveOldString);
	    break;
				
	  case KEY_ENTER:
	    saveStringEnter = 0;
	    if (savegamestrings[saveSlot][0])
		M_DoSave(saveSlot);
	    break;
				
	  default:
	    ch = toupper(ch);
	    if (ch != 32)
		if (ch-HU_FONTSTART < 0 || ch-HU_FONTSTART >= HU_FONTSIZE)
		    break;
	    if (ch >= 32 && ch <= 127 &&
		saveCharIndex < SAVESTRINGSIZE-1 &&
		M_StringWidth(savegamestrings[saveSlot]) <
		(SAVESTRINGSIZE-2)*8)
	    {
		savegamestrings[saveSlot][saveCharIndex++] = ch;
		savegamestrings[saveSlot][saveCharIndex] = 0;
	    }
	    break;
	}
	return true;
    }
    
    // Take care of any messages that need input
    if (messageToPrint)
    {
	// Backspace answers no, as it goes back everywhere else.
	if (ch == KEY_BACKSPACE)
	    ch = KEY_ESCAPE;

	// And Enter yes, as in the Quake container -- but only a press that
	// began after the question was asked. Enter is what chose QUIT GAME,
	// and a key held a moment too long repeats, which would answer for a
	// slow finger. Behind VNC the browser repeats the press alone, with no
	// release between; X repeats a release and a press together, so a
	// release in the last 50 ms does not count either -- no finger lets go
	// and presses again that fast.
	if (ch == KEY_ENTER && messageNeedsInput)
	{
	    if (!messageEnterUp || I_NowMs () - enterUpMs < 50)
		return true;
	    ch = 'y';
	}

	if (messageNeedsInput == true &&
	    !(ch == ' ' || ch == 'n' || ch == 'y' || ch == KEY_ESCAPE))
	    return false;
		
	menuactive = messageLastMenuActive;
	messageToPrint = 0;
	if (messageRoutine)
	    messageRoutine(ch);
			
	menuactive = false;
	S_StartSound(NULL,sfx_swtchx);
	return true;
    }
	
    // Delete clears the highlighted binding, on the Controls page and on the
    // controller's Buttons page: a control can be left with no key, and a
    // button with nothing to do, without binding something else to it first.
    // (On a controller Y is Delete, in the menus.)
    if (ch == KEY_DEL && menuactive)
    {
	if (currentMenu == &ControlsDef
	    && itemOn >= 0 && itemOn < (short)NUM_BINDINGS)
	{
	    *bindings[itemOn].key = -1;
	    S_StartSound (NULL, sfx_pistol);
	    return true;
	}
	if (currentMenu == &PadButtonsDef
	    && itemOn >= 0 && itemOn < (short)NUM_PADROWS)
	{
	    padbind[padrows[itemOn].button] = PA_NONE;
	    S_StartSound (NULL, sfx_pistol);
	    return true;
	}
    }

    // A second key for the menu, treated as Escape from here down so every
    // place that tests for Escape keeps working unchanged.
    //
    // Deliberately below the save-name and message handling above: those read
    // ordinary characters, and translating one of them into Escape would
    // cancel whatever the player was typing.
    if (key_menu != KEY_ESCAPE && ch == key_menu)
	ch = KEY_ESCAPE;

    if (devparm && ch == KEY_F1)
    {
	G_ScreenShot ();
	return true;
    }

    // Quick save and quick load, on whatever keys the Controls page gave
    // them. Not while a chat message is being typed, in case either is a
    // letter.
    if (!menuactive && !chat_on && ch > 0)
    {
	if (ch == key_quicksave)
	{
	    S_StartSound(NULL,sfx_swtchn);
	    M_QuickSave();
	    return true;
	}
	if (ch == key_quickload)
	{
	    S_StartSound(NULL,sfx_swtchn);
	    M_QuickLoad();
	    return true;
	}
    }
		
    
    // F-Keys
    if (!menuactive)
	switch(ch)
	{
	  case KEY_MINUS:         // Screen size down
	    if (automapactive || chat_on)
		return false;
	    M_SizeDisplay(0);
	    S_StartSound(NULL,sfx_stnmov);
	    return true;
				
	  case KEY_EQUALS:        // Screen size up
	    if (automapactive || chat_on)
		return false;
	    M_SizeDisplay(1);
	    S_StartSound(NULL,sfx_stnmov);
	    return true;
				
	  case KEY_F1:            // Help key
	    M_StartControlPanel ();

	    if ( gamemode == retail )
	      currentMenu = &ReadDef2;
	    else
	      currentMenu = &ReadDef1;
	    
	    itemOn = 0;
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
				
	  case KEY_F2:            // Save
	    M_StartControlPanel();
	    S_StartSound(NULL,sfx_swtchn);
	    M_SaveGame(0);
	    return true;
				
	  case KEY_F3:            // Load
	    M_StartControlPanel();
	    S_StartSound(NULL,sfx_swtchn);
	    M_LoadGame(0);
	    return true;
				
	  case KEY_F4:            // Sound Volume
	    M_StartControlPanel ();
	    currentMenu = &SoundDef;
	    itemOn = sfx_vol;
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
				
	  case KEY_F5:            // Detail toggle
	    M_ChangeDetail(0);
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
				
	  // F6, quick save, and F9, quick load, are bindings now: above.

	  case KEY_F7:            // End game
	    S_StartSound(NULL,sfx_swtchn);
	    M_EndGame(0);
	    return true;
				
	  case KEY_F8:            // Toggle messages
	    M_ChangeMessages(0);
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
				
	  case KEY_F10:           // Quit DOOM
	    S_StartSound(NULL,sfx_swtchn);
	    M_QuitDOOM(0);
	    return true;
				
	  case KEY_F11:           // gamma toggle
	    usegamma++;
	    if (usegamma > 4)
		usegamma = 0;
	    players[consoleplayer].message = gammamsg[usegamma];
	    I_SetPalette (W_CacheLumpName ("PLAYPAL",PU_CACHE));
	    return true;
				
	}

    
    // Pop-up menu?
    //
    // Backspace too, as in the Quake container: inside the menus it already
    // goes back a level, and in the browser, which keeps Escape for itself,
    // it is a key that always reaches the game. Not while typing a chat
    // message, where it rubs out.
    if (!menuactive)
    {
	if (ch == KEY_ESCAPE || (ch == KEY_BACKSPACE && !chat_on))
	{
	    M_StartControlPanel ();
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
	}
	return false;
    }

    
    //
    // A control bound to an ordinary character has to work the menu too.
    //
    // This switch reads the arrows and Return literally and sends everything
    // else to the hotkey search below, which jumps to the item beginning with
    // that letter. So a player who moves with WASD -- or a controller pressing
    // their bindings -- pressed 's' here and landed on SAVE GAME instead of
    // moving down a line: from NEW GAME that is three items along.
    //
    // Mapping the movement bindings onto the navigation keys first settles it,
    // and means the keys you walk with are the keys you navigate with. Only
    // where they differ, so nothing changes for the defaults, and only with a
    // menu open, since everything above has already returned.
    //
    if (ch == key_up && key_up != KEY_UPARROW)
	ch = KEY_UPARROW;
    else if (ch == key_down && key_down != KEY_DOWNARROW)
	ch = KEY_DOWNARROW;
    else if (ch == key_left && key_left != KEY_LEFTARROW)
	ch = KEY_LEFTARROW;
    else if (ch == key_right && key_right != KEY_RIGHTARROW)
	ch = KEY_RIGHTARROW;
    else if (ch == key_use && key_use != KEY_ENTER)
	ch = KEY_ENTER;

    // Keys usable within menu
    switch (ch)
    {
      case KEY_DOWNARROW:
	do
	{
	    if (itemOn+1 > currentMenu->numitems-1)
		itemOn = 0;
	    else itemOn++;
	    S_StartSound(NULL,sfx_pstop);
	} while(currentMenu->menuitems[itemOn].status==-1);
	return true;
		
      case KEY_UPARROW:
	do
	{
	    if (!itemOn)
		itemOn = currentMenu->numitems-1;
	    else itemOn--;
	    S_StartSound(NULL,sfx_pstop);
	} while(currentMenu->menuitems[itemOn].status==-1);
	return true;

      case KEY_LEFTARROW:
	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	{
	    S_StartSound(NULL,sfx_stnmov);
	    currentMenu->menuitems[itemOn].routine(0);
	}
	return true;
		
      case KEY_RIGHTARROW:
	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	{
	    S_StartSound(NULL,sfx_stnmov);
	    currentMenu->menuitems[itemOn].routine(1);
	}
	return true;

      case KEY_ENTER:
	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status)
	{
	    currentMenu->lastOn = itemOn;
	    if (currentMenu->menuitems[itemOn].status == 2)
	    {
		currentMenu->menuitems[itemOn].routine(1);      // right arrow
		S_StartSound(NULL,sfx_stnmov);
	    }
	    else
	    {
		currentMenu->menuitems[itemOn].routine(itemOn);
		S_StartSound(NULL,sfx_pistol);
	    }
	}
	return true;
		
      case KEY_ESCAPE:
	currentMenu->lastOn = itemOn;
	M_ClearMenus ();
	S_StartSound(NULL,sfx_swtchx);
	return true;
		
      case KEY_BACKSPACE:
	currentMenu->lastOn = itemOn;
	if (currentMenu->prevMenu)
	{
	    currentMenu = currentMenu->prevMenu;
	    itemOn = currentMenu->lastOn;
	    S_StartSound(NULL,sfx_swtchn);
	}
	else
	{
	    // and out of the menus from the top, as it came in
	    M_ClearMenus ();
	    S_StartSound(NULL,sfx_swtchx);
	}
	return true;
	
      default:
	for (i = itemOn+1;i < currentMenu->numitems;i++)
	    if (currentMenu->menuitems[i].alphaKey == ch)
	    {
		itemOn = i;
		S_StartSound(NULL,sfx_pstop);
		return true;
	    }
	for (i = 0;i <= itemOn;i++)
	    if (currentMenu->menuitems[i].alphaKey == ch)
	    {
		itemOn = i;
		S_StartSound(NULL,sfx_pstop);
		return true;
	    }
	break;
	
    }

    return false;
}



//
// M_StartControlPanel
//
void M_StartControlPanel (void)
{
    // intro might call this repeatedly
    if (menuactive)
	return;
    
    menuactive = 1;
    currentMenu = &MainDef;         // JDC
    itemOn = currentMenu->lastOn;   // JDC
}


//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
    static short	x;
    static short	y;
    short		i;
    short		max;
    short		lh;
    boolean		textmenu;
    char		string[40];
    int			start;

    inhelpscreens = false;

    
    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
	start = 0;
	y = 100 - M_StringHeight(messageString)/2;
	while(*(messageString+start))
	{
	    char*	line = messageString + start;
	    char*	nl = strchr(line, '\n');
	    size_t	len = nl ? (size_t)(nl - line) : strlen(line);

	    // Truncate rather than overrun. The original copied a whole line
	    // into this buffer with no length check, and two missing commas
	    // in the quit message table joined messages into lines of 47
	    // characters, which aborted the game on the way out.
	    start += nl ? len + 1 : len;

	    if (len > sizeof(string) - 1)
		len = sizeof(string) - 1;

	    memcpy (string, line, len);
	    string[len] = 0;

	    x = 160 - M_StringWidth(string)/2;
	    M_WriteText(x,y,string);
	    y += SHORT(hu_font[0]->height);
	}
	return;
    }

    if (!menuactive)
	return;

    if (currentMenu->routine)
	currentMenu->routine();         // call Draw routine
    
    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->numitems;
    lh = currentMenu->lineheight ? currentMenu->lineheight : LINEHEIGHT;

    // A menu is drawn one way or the other, not both; item zero decides.
    textmenu = max > 0 && currentMenu->menuitems[0].text != NULL;

    for (i=0;i<max;i++)
    {
	if (currentMenu->menuitems[i].text)
	    M_WriteText (x, y + (lh - SHORT(hu_font[0]->height))/2,
			 currentMenu->menuitems[i].text);
	else if (currentMenu->menuitems[i].name[0])
	    V_DrawPatchDirect (x,y,0,
			       W_CacheLumpName(currentMenu->menuitems[i].name ,PU_CACHE));
	y += lh;
    }

    
    // DRAW SKULL
    //
    // Centred on the row for a text menu, where the rows are shorter than the
    // 19 pixel skull. The original -5 is tuned for 16 pixel rows of graphics
    // and is kept for those.
    V_DrawPatchDirect(x + SKULLXOFF,
		      currentMenu->y + (textmenu ? (lh - 19)/2 : -5) + itemOn*lh,
		      0,
		      W_CacheLumpName(skullName[whichSkull],PU_CACHE));

}


//
// M_ClearMenus
//
void M_ClearMenus (void)
{
    menuactive = 0;
    // if (!netgame && usergame && paused)
    //       sendpause = true;
}




//
// M_SetupNextMenu
//
void M_SetupNextMenu(menu_t *menudef)
{
    currentMenu = menudef;
    itemOn = currentMenu->lastOn;
}


//
// M_Ticker
//
void M_Ticker (void)
{
    if (--skullAnimCounter <= 0)
    {
	whichSkull ^= 1;
	skullAnimCounter = 8;
    }
}


//
// M_InitEpisodes
// The new game menu's episodes: the game's own -- none in DOOM II -- then
// any UMAPINFO adds, or those alone once it has said episode = clear, which
// the 2024 re-release's add-ons all do. SIGIL loaded over The Ultimate DOOM
// has the one episode, then, and New Game goes straight to it.
//
static void M_InitEpisodes (void)
{
    int		n;
    int		i;

    n = gamemode == commercial || um_episodesclear ? 0 : EpiDef.numitems;

    for (i = 0; i < um_numepisodes && n < UM_MAXEPISODES; i++, n++)
    {
	EpisodeMenu[n].status = 1;
	EpisodeMenu[n].name[0] = 0;
	EpisodeMenu[n].routine = M_Episode;
	EpisodeMenu[n].alphaKey = um_episodes[i].key;
	EpisodeMenu[n].text = um_episodes[i].name;
	epiepisode[n] = um_episodes[i].episode;
	epimap[n] = um_episodes[i].map;
    }

    EpiDef.numitems = n;
    if (EpiDef.lastOn >= n)
	EpiDef.lastOn = 0;
    NewDef.prevMenu = n > 1 ? &EpiDef : &MainDef;
}


//
// M_LoadFailed
// Back on the game from before, after the engine could not start on the
// one chosen from Load WAD (I_ErrorGoBack): what it was and why, on the
// title screen until a key is pressed. The error is folded to fit.
//
void M_LoadFailed (char* wad, char* why)
{
    static char	text[512];
    int		n;
    int		col = 0;
    char*	w;

    n = snprintf (text, sizeof(text), "COULD NOT START %.24s\n\n",
		  wad && *wad ? wad : "THAT WAD");
    for (w = why; *w && n < (int)sizeof(text) - 32; w++)
    {
	// break at the space before a word that would pass 32 characters
	if (*w == ' ')
	{
	    char*	end = strchr (w + 1, ' ');
	    int		len = end ? end - w - 1 : (int)strlen (w + 1);

	    if (col + 1 + len > 32)
	    {
		text[n++] = '\n';
		col = 0;
		continue;
	    }
	}
	if (col == 32)
	{
	    text[n++] = '\n';
	    col = 0;
	}
	text[n++] = *w;
	col++;
    }
    snprintf (text + n, sizeof(text) - n, "\n\nTHE GAME BEFORE IT IS BACK.\n"
	      "PRESS A KEY.");
    M_StartMessage (text, NULL, false);
}


//
// M_Init
//
void M_Init (void)
{
    currentMenu = &MainDef;
    menuactive = 0;
    itemOn = currentMenu->lastOn;
    whichSkull = 0;
    skullAnimCounter = 10;
    // Keep the saved value inside what the menu can express, so a bad one is
    // corrected rather than carried around for ever, and so the slider and
    // the view size cannot drift apart.
    if (screenblocks < 3)
	screenblocks = 3;
    if (screenblocks > 11)
	screenblocks = 11;

    // Older builds could save this as 1; see M_ChangeDetail.
    detailLevel = 0;

    // Configs written before backquote became the default carry the old
    // no-op value. Escape is read directly either way, so promoting it
    // costs nothing and gives those players the browser-safe key too.
    if (key_menu == KEY_ESCAPE)
	key_menu = '`';

    // Backspace opens the menu now, so it cannot be a control as well; an
    // older config that bound it gets the control back unbound.
    {
	unsigned	i;

	for (i = 0; i < NUM_BINDINGS; i++)
	    if (*bindings[i].key == KEY_BACKSPACE)
		*bindings[i].key = -1;
    }

    screenSize = screenblocks - 3;
    messageToPrint = 0;
    messageString = NULL;
    messageLastMenuActive = menuactive;
    quickSaveSlot = -1;

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.

  
    switch ( gamemode )
    {
      case commercial:
	// This is used because DOOM 2 had only one HELP
        //  page. I use CREDIT as second page now, but
	//  kept this hack for educational purposes.
	MainMenu[readthis] = MainMenu[quitdoom];
	MainDef.numitems--;
	MainDef.y += 8;
	NewDef.prevMenu = &MainDef;
	ReadDef1.routine = M_DrawReadThis1;
	ReadDef1.x = 330;
	ReadDef1.y = 165;
	ReadMenu1[0].routine = M_FinishReadThis;
	break;
      case shareware:
	// Episode 2 and 3 are handled,
	//  branching to an ad screen.
      case registered:
	// We need to remove the fourth episode.
	EpiDef.numitems--;
	break;
      case retail:
	// We are fine.
      default:
	break;
    }

    M_InitEpisodes ();
    
}

