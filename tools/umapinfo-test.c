//
// umapinfo-test -- the UMAPINFO reader, linuxdoom-1.10/u_mapinfo.c, on its
// own: lumps of text in, map entries out, checked against what they say.
// The smoke test's WAD is the shareware episode, which cannot load add-ons,
// so this is where UMAPINFO is tested in CI.
//
//   cc -Ilinuxdoom-1.10 -DNORMALUNIX -DLINUX -o umapinfo-test
//      tools/umapinfo-test.c linuxdoom-1.10/u_mapinfo.c
//   ./umapinfo-test doom       ExMy names, and what goes wrong in a lump
//   ./umapinfo-test doom2      MAPxx names, and No Rest for the Living's
//                              entries, built in for the BFG Edition's WAD
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "info.h"
#include "w_wad.h"
#include "u_mapinfo.h"

// What u_mapinfo.c takes from the rest of the engine.
GameMode_t	gamemode;
int		gameepisode;
int		gamemap;
boolean		nervepack;
lumpinfo_t*	lumpinfo;
int		numlumps;

static const char*	lumptext[8];

int W_LumpLength (int lump)
{
    return strlen (lumptext[lump]);
}

void W_ReadLump (int lump, void* dest)
{
    memcpy (dest, lumptext[lump], strlen (lumptext[lump]));
}

void I_Error (char* error, ...)
{
    va_list	ap;

    va_start (ap, error);
    vfprintf (stderr, error, ap);
    va_end (ap);
    fputc ('\n', stderr);
    exit (2);
}

static int	failures;

#define CHECK(cond) \
    do { if (!(cond)) { printf ("FAILED line %d: %s\n", __LINE__, #cond); \
			failures++; } } while (0)

static void AddLump (const char* name, const char* text)
{
    lumpinfo = realloc (lumpinfo, (numlumps + 1) * sizeof(*lumpinfo));
    memset (&lumpinfo[numlumps], 0, sizeof(*lumpinfo));
    strncpy (lumpinfo[numlumps].name, name, 8);
    lumptext[numlumps++] = text;
}

static void TestDoom (void)
{
    umapentry_t*	m;
    int			e, n;

    gamemode = retail;

    AddLump ("PLAYPAL", "not text");
    AddLump ("UMAPINFO",
	"// a comment\n"
	"/* and one\n   over lines */\n"
	"MAP E1M1\n"
	"{\n"
	"    levelname = \"Hangar\"\n"
	"    label = clear\n"
	"    Next = \"E5M2\"\n"
	"    nextsecret = \"E1M9\"\n"
	"    skytexture = \"sky2\"\n"
	"    music = \"D_E1M5\"\n"
	"    partime = 30\n"
	"    intertext = \"line one\",\n"
	"                \"line two\"\n"
	"    endgame = false\n"
	"    bossaction = BaronOfHell, 23, 666\n"
	"    bossaction = Cyberdemon, 97, 1\n"	// a teleporter: refused
	"    bossaction = Imp, 23, 5\n"		// DoomImp: refused
	"    bossaction = Fatso, 23, 0\n"	// tag 0, not an exit: refused
	"    bossaction = Fatso, 52, 0\n"	// an exit: kept
	"    kex_finishedtaskid = \"x\", 3\n"	// someone else's: passed over
	"    episode = clear\n"
	"    episode = \"M_EPI1\", \"First\", \"F\"\n"
	"}\n"
	"map e5m2 { endpic = \"CREDIT\" nointermission = true\n"
	"           intertextsecret = clear author = \"Someone\" }\n"
	"MAP MAP01 { levelname = \"a DOOM II map\" }\n");
    AddLump ("UMAPINFO",
	"MAP E5M2 { levelname = \"Replaced\" }\n"
	"MAP E5M1 { episode = \"M_EPI5\", \"Fifth\", \"5\" endbunny = true }\n");
    AddLump ("DEHACKED",
	"Patch File for DeHackEd v3.0\n"
	"Doom version = 21\n"
	"[PARS]\n"
	"par 5 1 90\n"
	"  par 5 2 150\n"
	"par 5 1 95\n"			// a later word on the same map
	"[STRINGS]\n"
	"par 5 3 360\n");		// not in [PARS]
    AddLump ("UMAPINFO",
	"MAP E6M1 { levelname = \"Kept\" }\n"
	"MAP E6M2 { levelname \"no equals\" }\n"
	"MAP E6M3 { levelname = \"after the mistake\" }\n");

    U_Init ();

    m = U_FindMap (1, 1);
    CHECK (m != NULL);
    if (m)
    {
	CHECK (!strcmp (m->levelname, "Hangar"));
	CHECK (m->labelclear && !m->label);
	CHECK (m->nextepisode == 5 && m->nextmap == 2);
	CHECK (m->secretepisode == 1 && m->secretmap == 9);
	CHECK (!strcmp (m->skytexture, "SKY2"));
	CHECK (!strcmp (m->music, "D_E1M5"));
	CHECK (m->partime == 30);
	CHECK (m->intertext && !strcmp (m->intertext, "line one\nline two"));
	CHECK (!m->intertextsecret);
	CHECK (m->ending == UM_END_NONE);
	CHECK (m->bossactions_set && m->numbossactions == 2);
	if (m->numbossactions == 2)
	{
	    CHECK (m->bossactions[0].type == MT_BRUISER);
	    CHECK (m->bossactions[0].special == 23);
	    CHECK (m->bossactions[0].tag == 666);
	    CHECK (m->bossactions[1].type == MT_FATSO);
	    CHECK (m->bossactions[1].special == 52);
	}
    }

    // replaced whole by the later lump: no end picture left
    m = U_FindMap (5, 2);
    CHECK (m && m->levelname && !strcmp (m->levelname, "Replaced"));
    CHECK (m && m->ending == UM_END_UNSET && !m->endpic[0] && !m->author);

    m = U_FindMap (5, 1);
    CHECK (m && m->ending == UM_END_BUNNY);

    CHECK (U_FindMap (6, 1) != NULL);
    CHECK (U_FindMap (6, 2) == NULL);	// where the reading stopped
    CHECK (U_FindMap (6, 3) == NULL);

    CHECK (um_episodesclear);
    CHECK (um_numepisodes == 2);
    CHECK (!strcmp (um_episodes[0].name, "First"));
    CHECK (um_episodes[0].key == 'f');
    CHECK (um_episodes[0].episode == 1 && um_episodes[0].map == 1);
    CHECK (!strcmp (um_episodes[1].patch, "M_EPI5"));
    CHECK (um_episodes[1].episode == 5 && um_episodes[1].map == 1);

    CHECK (U_ParseMapName ("E1M10", &e, &n) && e == 1 && n == 10);
    CHECK (U_ParseMapName ("e4m1", &e, &n) && e == 4 && n == 1);
    CHECK (!U_ParseMapName ("MAP01", &e, &n));
    CHECK (!U_ParseMapName ("E1M", &e, &n));
    CHECK (!U_ParseMapName ("E1M1x", &e, &n));

    gameepisode = 5;
    gamemap = 1;
    CHECK (U_ThisMap () == U_FindMap (5, 1));

    // DEHACKED's [PARS], as SIGIL has its par times
    CHECK (U_BexPar (5, 1) == 95);
    CHECK (U_BexPar (5, 2) == 150);
    CHECK (U_BexPar (5, 3) == 0);
    CHECK (U_BexPar (1, 1) == 0);
}

static void TestDoom2 (void)
{
    umapentry_t*	m;
    int			e, n;
    char		name[9];

    gamemode = commercial;
    nervepack = true;

    U_Init ();

    m = U_FindMap (1, 4);
    CHECK (m && !strcmp (m->skytexture, "SKY3"));
    CHECK (m && m->secretmap == 9 && m->nextmap == 0);
    CHECK (m && !strcmp (m->label, "Level 4"));
    CHECK (m && !strcmp (m->levelname, "Hell Mountain"));
    CHECK (m && m->partime == 105);

    m = U_FindMap (1, 9);
    CHECK (m && m->nextmap == 5);
    CHECK (m && !strcmp (m->music, "D_DDTBLU"));

    m = U_FindMap (1, 6);
    CHECK (m && m->intertext && !*m->intertext);	// cleared

    m = U_FindMap (1, 7);
    CHECK (m && m->bossactions_set && m->numbossactions == 0);

    m = U_FindMap (1, 8);
    CHECK (m && m->ending == UM_END_CAST);
    CHECK (m && !strcmp (m->interbackdrop, "SLIME16"));
    CHECK (m && m->intertext
	   && !strncmp (m->intertext, "Trouble was brewing", 19)
	   && strstr (m->intertext, "\n\nThis ride is closed."));

    CHECK (U_FindMap (1, 10) == NULL);
    CHECK (um_episodesclear && um_numepisodes == 1);

    // DOOM II has one set of maps, whatever the episode
    CHECK (U_FindMap (3, 4) == U_FindMap (1, 4));

    CHECK (U_ParseMapName ("MAP21", &e, &n) && e == 1 && n == 21);
    CHECK (U_ParseMapName ("map07", &e, &n) && n == 7);
    CHECK (!U_ParseMapName ("E1M1", &e, &n));
    U_MapName (name, 1, 4);
    CHECK (!strcmp (name, "MAP04"));
}

int main (int argc, char** argv)
{
    if (argc == 2 && !strcmp (argv[1], "doom"))
	TestDoom ();
    else if (argc == 2 && !strcmp (argv[1], "doom2"))
	TestDoom2 ();
    else
    {
	fprintf (stderr, "usage: umapinfo-test doom|doom2\n");
	return 2;
    }

    if (failures)
    {
	printf ("umapinfo-test %s: %d FAILED\n", argv[1], failures);
	return 1;
    }
    printf ("umapinfo-test %s: all passed\n", argv[1]);
    return 0;
}
