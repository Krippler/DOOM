// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	UMAPINFO: the levels an add-on describes for itself. See u_mapinfo.h.
//
//	A lump is a list of entries, one to a map:
//
//	  MAP E5M1
//	  {
//	      levelname = "Baphomet's Demesne"
//	      next = "E5M2"
//	      intertext = "a line", "another line"
//	  }
//
//	Values are strings in double quotes, numbers, or words (true, false,
//	clear, and monster names for bossaction). Keys this does not know --
//	the 2024 re-release's own kex_ ones, say -- are passed over, as the
//	specification asks. Something that is not UMAPINFO at all stops the
//	reading of that lump with a message saying where, and the maps read
//	before it stand.
//
//-----------------------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "info.h"
#include "w_wad.h"

#include "u_mapinfo.h"

int		um_numepisodes;
boolean		um_episodesclear;
umepisode_t	um_episodes[UM_MAXEPISODES];

static umapentry_t*	maps;
static int		nummaps;

typedef struct
{
    int		episode;
    int		map;
    int		seconds;
} bexpar_t;

static bexpar_t*	bexpars;
static int		numbexpars;


//
// The monster names bossaction takes: ZDoom's, as the specification lists
// them, in the order of mobjtype_t.
//
static const char* const thingnames[] =
{
    "DoomPlayer", "ZombieMan", "ShotgunGuy", "Archvile", "ArchvileFire",
    "Revenant", "RevenantTracer", "RevenantTracerSmoke", "Fatso", "FatShot",
    "ChaingunGuy", "DoomImp", "Demon", "Spectre", "Cacodemon", "BaronOfHell",
    "BaronBall", "HellKnight", "LostSoul", "SpiderMastermind", "Arachnotron",
    "Cyberdemon", "PainElemental", "WolfensteinSS", "CommanderKeen",
    "BossBrain", "BossEye", "BossTarget", "SpawnShot", "SpawnFire",
    "ExplosiveBarrel", "DoomImpBall", "CacodemonBall", "Rocket", "PlasmaBall",
    "BFGBall", "ArachnotronPlasma", "BulletPuff", "Blood", "TeleportFog",
    "ItemFog", "TeleportDest", "BFGExtra", "GreenArmor", "BlueArmor",
    "HealthBonus", "ArmorBonus", "BlueCard", "RedCard", "YellowCard",
    "YellowSkull", "RedSkull", "BlueSkull", "Stimpack", "Medikit",
    "Soulsphere", "InvulnerabilitySphere", "Berserk", "BlurSphere", "RadSuit",
    "Allmap", "Infrared", "Megasphere", "Clip", "ClipBox", "RocketAmmo",
    "RocketBox", "Cell", "CellPack", "Shell", "ShellBox", "Backpack",
    "BFG9000", "Chaingun", "Chainsaw", "RocketLauncher", "PlasmaRifle",
    "Shotgun", "SuperShotgun", "TechLamp", "TechLamp2", "Column",
    "TallGreenColumn", "ShortGreenColumn", "TallRedColumn", "ShortRedColumn",
    "SkullColumn", "HeartColumn", "EvilEye", "FloatingSkull", "TorchTree",
    "BlueTorch", "GreenTorch", "RedTorch", "ShortBlueTorch",
    "ShortGreenTorch", "ShortRedTorch", "Stalagtite", "TechPillar",
    "CandleStick", "Candelabra", "BloodyTwitch", "Meat2", "Meat3", "Meat4",
    "Meat5", "NonsolidMeat2", "NonsolidMeat4", "NonsolidMeat3",
    "NonsolidMeat5", "NonsolidTwitch", "DeadCacodemon", "DeadMarine",
    "DeadZombieMan", "DeadDemon", "DeadLostSoul", "DeadDoomImp",
    "DeadShotgunGuy", "GibbedMarine", "GibbedMarineExtra", "HeadsOnAStick",
    "Gibs", "HeadOnAStick", "HeadCandles", "DeadStick", "LiveStick",
    "BigTree", "BurningBarrel", "HangNoGuts", "HangBNoBrain",
    "HangTLookingDown", "HangTSkull", "HangTLookingUp", "HangTNoBrain",
    "ColonGibs", "SmallBloodPool", "BrainStem"
};

// One name for every type there is, or the build stops here.
typedef char thingnames_match_mobjtypes
    [sizeof(thingnames)/sizeof(*thingnames) == NUMMOBJTYPES ? 1 : -1];


//
// No Rest for the Living, as the BFG Edition that brought it plays it, for
// the nerve.wad of that edition, which has no UMAPINFO of its own. The 2024
// re-release's does, and says the same; being read after this, it replaces
// it. Without these the expansion took DOOM II's rules for MAP01 to MAP09:
// the city sky over its hell maps, no way back from its secret level, and
// no end after MAP08.
//
static const char nerveinfo[] =
"map MAP01 { episode = clear\n"
"            episode = \"M_EPICOM\", \"No Rest for the Living\", \"n\"\n"
"            label = \"Level 1\" levelname = \"The Earth Base\"\n"
"            partime = 75 music = \"D_MESSAG\" }\n"
"map MAP02 { label = \"Level 2\" levelname = \"The Pain Labs\"\n"
"            partime = 105 music = \"D_DDTBLU\" }\n"
"map MAP03 { label = \"Level 3\" levelname = \"Canyon of the Dead\"\n"
"            partime = 120 music = \"D_DOOM\" }\n"
"map MAP04 { label = \"Level 4\" levelname = \"Hell Mountain\"\n"
"            skytexture = \"SKY3\" partime = 105 music = \"D_SHAWN\"\n"
"            nextsecret = \"MAP09\" }\n"
"map MAP05 { label = \"Level 5\" levelname = \"Vivisection\"\n"
"            skytexture = \"SKY3\" partime = 210 music = \"D_IN_CIT\" }\n"
"map MAP06 { label = \"Level 6\" levelname = \"Inferno of Blood\"\n"
"            skytexture = \"SKY3\" partime = 105 music = \"D_THE_DA\"\n"
"            intertext = clear }\n"
"map MAP07 { label = \"Level 7\" levelname = \"Baron's Banquet\"\n"
"            skytexture = \"SKY3\" partime = 165 music = \"D_IN_CIT\"\n"
"            bossaction = clear }\n"
"map MAP08 { label = \"Level 8\" levelname = \"Tomb of Malevolence\"\n"
"            skytexture = \"SKY3\" partime = 105 music = \"D_SHAWN\"\n"
"            endcast = true interbackdrop = \"SLIME16\"\n"
"            intertext =\n"
"              \"Trouble was brewing again in your favorite\",\n"
"              \"vacation spot... Hell. Some cyberdemon\",\n"
"              \"punk thought he could turn Hell into a\",\n"
"              \"personal amusement park, and make Earth\",\n"
"              \"the ticket booth.\", \"\",\n"
"              \"Well that half-robot freak show didn't\",\n"
"              \"know who was coming to the fair. There's\",\n"
"              \"nothing like a shooting gallery full of\",\n"
"              \"hellspawn to get the blood pumping...\", \"\",\n"
"              \"Now the walls of the demon's labyrinth\",\n"
"              \"echo with the sound of his metallic limbs\",\n"
"              \"hitting the floor. His death moan gurgles\",\n"
"              \"out through the mess you left of his face.\", \"\",\n"
"              \"This ride is closed.\" }\n"
"map MAP09 { label = \"Level 9\" levelname = \"March of the Demons\"\n"
"            partime = 135 music = \"D_DDTBLU\" next = \"MAP05\" }\n";


//
// MAP NAMES
//

boolean U_ParseMapName (const char* name, int* episode, int* map)
{
    const char*	p;
    int		e = 1;
    int		m;

    if (gamemode == commercial)
    {
	if (strncasecmp (name, "MAP", 3) || !isdigit ((unsigned char)name[3]))
	    return false;
	p = name + 3;
    }
    else
    {
	if (toupper ((unsigned char)name[0]) != 'E'
	    || !isdigit ((unsigned char)name[1]))
	    return false;
	e = strtol (name + 1, (char **)&p, 10);
	if (toupper ((unsigned char)*p) != 'M' || !isdigit ((unsigned char)p[1]))
	    return false;
	p++;
    }

    m = strtol (p, (char **)&p, 10);
    if (*p || e < 1 || m < 1 || e > 99 || m > 99)
	return false;

    *episode = e;
    *map = m;
    return true;
}

void U_MapName (char* buf, int episode, int map)
{
    if (gamemode == commercial)
	sprintf (buf, "MAP%02d", map % 100);
    else
	sprintf (buf, "E%dM%d", episode % 10, map % 100);
}

umapentry_t* U_FindMap (int episode, int map)
{
    int		i;

    for (i = 0; i < nummaps; i++)
	if (maps[i].map == map
	    && (gamemode == commercial || maps[i].episode == episode))
	    return &maps[i];
    return NULL;
}

umapentry_t* U_ThisMap (void)
{
    return U_FindMap (gameepisode, gamemap);
}


//
// SCANNER
//
enum
{
    T_EOF,
    T_STRING,
    T_NUMBER,
    T_WORD,
    T_PUNCT		// one of { } = ,
};

#define MAXTOKEN	256

static const char*	scan;		// where the scanner is
static int		scanline;
static const char*	scanwhat;	// what is being read, for messages
static int		ttype;
static char		token[MAXTOKEN];

static void U_Warn (const char* msg, const char* what)
{
    fprintf (stderr, "UMAPINFO: %s, line %d: %s%s%s\n",
	     scanwhat, scanline, msg, what ? " " : "", what ? what : "");
}

static void U_Next (void)
{
    int		n = 0;

    // blanks and comments
    for (;;)
    {
	while (*scan && isspace ((unsigned char)*scan))
	    if (*scan++ == '\n')
		scanline++;
	if (scan[0] == '/' && scan[1] == '/')
	{
	    while (*scan && *scan != '\n')
		scan++;
	}
	else if (scan[0] == '/' && scan[1] == '*')
	{
	    for (scan += 2; *scan && !(scan[0] == '*' && scan[1] == '/'); scan++)
		if (*scan == '\n')
		    scanline++;
	    if (*scan)
		scan += 2;
	}
	else
	    break;
    }

    token[0] = 0;

    if (!*scan)
    {
	ttype = T_EOF;
	return;
    }

    if (*scan == '"')
    {
	ttype = T_STRING;
	for (scan++; *scan && *scan != '"'; scan++)
	{
	    int c = *scan;

	    if (c == '\\' && scan[1])
	    {
		c = *++scan;
		if (c == 'n')
		    c = '\n';
	    }
	    if (c == '\n')
		scanline++;
	    if (n < MAXTOKEN - 1)
		token[n++] = c;
	}
	if (*scan)
	    scan++;
	token[n] = 0;
	return;
    }

    if (strchr ("{}=,", *scan))
    {
	ttype = T_PUNCT;
	token[0] = *scan++;
	token[1] = 0;
	return;
    }

    if (isdigit ((unsigned char)*scan) || *scan == '-' || *scan == '+')
	ttype = T_NUMBER;
    else if (isalpha ((unsigned char)*scan) || *scan == '_')
	ttype = T_WORD;
    else
    {
	ttype = T_PUNCT;
	token[0] = *scan++;
	token[1] = 0;
	return;
    }

    while (*scan && (isalnum ((unsigned char)*scan) || strchr ("_.-+", *scan)))
    {
	if (n < MAXTOKEN - 1)
	    token[n++] = *scan;
	scan++;
    }
    token[n] = 0;
}

static boolean U_IsPunct (int c)
{
    return ttype == T_PUNCT && token[0] == c;
}


//
// PARSER
//

// The values after a key's =, as many as there are commas for.
#define MAXVALUES	64

static int	numvalues;
static int	valuetype[MAXVALUES];
static char	values[MAXVALUES][MAXTOKEN];

static boolean U_ReadValues (void)
{
    numvalues = 0;
    for (;;)
    {
	U_Next ();
	if (ttype != T_STRING && ttype != T_NUMBER && ttype != T_WORD)
	{
	    U_Warn ("expected a value, found", ttype == T_EOF ? "the end" : token);
	    return false;
	}
	if (numvalues < MAXVALUES)
	{
	    valuetype[numvalues] = ttype;
	    strcpy (values[numvalues], token);
	    numvalues++;
	}

	// Another after a comma. Looked at without taking it, as a key on
	// the next line is where the list ends.
	{
	    const char*	was = scan;
	    int		wasline = scanline;

	    U_Next ();
	    if (!U_IsPunct (','))
	    {
		scan = was;
		scanline = wasline;
		return true;
	    }
	}
    }
}

static boolean U_Word (int i, const char* word)
{
    return valuetype[i] == T_WORD && !strcasecmp (values[i], word);
}

static boolean U_Clear (void)
{
    return numvalues == 1 && U_Word (0, "clear");
}

static char* U_Strdup (const char* s)
{
    char*	d = malloc (strlen (s) + 1);

    if (!d)
	I_Error ("UMAPINFO: out of memory");
    return strcpy (d, s);
}

// A lump name, upper case as the WAD directory has them.
static void U_LumpValue (char* dest, const char* key)
{
    int		i;

    if (numvalues != 1 || valuetype[0] != T_STRING || !values[0][0])
    {
	U_Warn ("expected one lump name for", key);
	return;
    }
    if (strlen (values[0]) > 8)
	U_Warn ("lump names are at most eight characters:", values[0]);
    for (i = 0; i < 8 && values[0][i]; i++)
	dest[i] = toupper ((unsigned char)values[0][i]);
    dest[i] = 0;
}

static boolean U_BoolValue (const char* key)
{
    if (numvalues == 1 && U_Word (0, "true"))
	return true;
    if (numvalues != 1 || !U_Word (0, "false"))
	U_Warn ("expected true or false for", key);
    return false;
}

static void U_MapValue (int* episode, int* map, const char* key)
{
    if (numvalues != 1 || valuetype[0] == T_NUMBER
	|| !U_ParseMapName (values[0], episode, map))
    {
	U_Warn ("not a map of this game's:", values[0]);
	*episode = *map = 0;
    }
}

// The lines of an intertext, as one string, or "" for clear.
static char* U_TextValue (void)
{
    size_t	len = 1;
    char*	text;
    int		i;

    if (U_Clear ())
	return U_Strdup ("");

    for (i = 0; i < numvalues; i++)
	len += strlen (values[i]) + 1;
    text = malloc (len);
    if (!text)
	I_Error ("UMAPINFO: out of memory");
    text[0] = 0;
    for (i = 0; i < numvalues; i++)
    {
	if (i)
	    strcat (text, "\n");
	strcat (text, values[i]);
    }
    if (numvalues == MAXVALUES)
	U_Warn ("the text is cut at this many lines:", "64");
    return text;
}

static void U_BossAction (umapentry_t* m)
{
    umbossaction_t	ba;
    int			i;

    m->bossactions_set = true;
    if (U_Clear ())
    {
	m->numbossactions = 0;
	return;
    }

    if (numvalues != 3 || valuetype[0] != T_WORD
	|| valuetype[1] != T_NUMBER || valuetype[2] != T_NUMBER)
    {
	U_Warn ("expected bossaction = monster, special, tag", NULL);
	return;
    }

    for (i = 0; i < NUMMOBJTYPES; i++)
	if (!strcasecmp (values[0], thingnames[i]))
	    break;
    if (i == NUMMOBJTYPES)
    {
	U_Warn ("no such thing as", values[0]);
	return;
    }

    ba.type = i;
    ba.special = atoi (values[1]);
    ba.tag = atoi (values[2]);

    // What the specification leaves out: doors that open the line's own
    // back sector, which a boss action has no line for; locked doors; and
    // teleporters, which would move the player.
    switch (ba.special)
    {
      case 1: case 26: case 27: case 28: case 31: case 32: case 33:
      case 34: case 117: case 118:
      case 99: case 133: case 134: case 135: case 136: case 137:
      case 39: case 97: case 125: case 126:
	U_Warn ("a bossaction cannot have special", values[1]);
	return;
    }

    // Tag 0 would be every sector without a tag: only the exits, which have
    // no sector to act on, may have it.
    if (ba.tag == 0
	&& ba.special != 11 && ba.special != 51
	&& ba.special != 52 && ba.special != 124)
    {
	U_Warn ("a bossaction needs a tag, unless it is an exit:", values[1]);
	return;
    }

    m->bossactions = realloc (m->bossactions,
			      (m->numbossactions + 1) * sizeof(ba));
    if (!m->bossactions)
	I_Error ("UMAPINFO: out of memory");
    m->bossactions[m->numbossactions++] = ba;
}

static void U_Episode (umapentry_t* m)
{
    umepisode_t*	e;

    if (U_Clear ())
    {
	um_numepisodes = 0;
	um_episodesclear = true;
	return;
    }
    if (numvalues != 3 || valuetype[0] != T_STRING
	|| valuetype[1] != T_STRING || valuetype[2] != T_STRING)
    {
	U_Warn ("expected episode = \"patch\", \"name\", \"key\"", NULL);
	return;
    }

    if (um_numepisodes == UM_MAXEPISODES)
    {
	U_Warn ("no room in the menu for", values[1]);
	return;
    }

    e = &um_episodes[um_numepisodes++];
    snprintf (e->patch, sizeof(e->patch), "%.8s", values[0]);
    e->name = U_Strdup (values[1]);
    e->key = tolower ((unsigned char)values[2][0]);
    e->episode = m->episode;
    e->map = m->map;
}

static void U_Key (umapentry_t* m, const char* key)
{
    if (!strcasecmp (key, "levelname"))
	m->levelname = U_Strdup (values[0]);
    else if (!strcasecmp (key, "label"))
    {
	if (U_Clear ())
	    m->labelclear = true;
	else
	    m->label = U_Strdup (values[0]);
    }
    else if (!strcasecmp (key, "author"))
	m->author = U_Strdup (values[0]);
    else if (!strcasecmp (key, "levelpic"))
	U_LumpValue (m->levelpic, key);
    else if (!strcasecmp (key, "skytexture"))
	U_LumpValue (m->skytexture, key);
    else if (!strcasecmp (key, "music"))
	U_LumpValue (m->music, key);
    else if (!strcasecmp (key, "exitpic"))
	U_LumpValue (m->exitpic, key);
    else if (!strcasecmp (key, "enterpic"))
	U_LumpValue (m->enterpic, key);
    else if (!strcasecmp (key, "interbackdrop"))
	U_LumpValue (m->interbackdrop, key);
    else if (!strcasecmp (key, "intermusic"))
	U_LumpValue (m->intermusic, key);
    else if (!strcasecmp (key, "next"))
	U_MapValue (&m->nextepisode, &m->nextmap, key);
    else if (!strcasecmp (key, "nextsecret"))
	U_MapValue (&m->secretepisode, &m->secretmap, key);
    else if (!strcasecmp (key, "partime"))
	m->partime = atoi (values[0]);
    else if (!strcasecmp (key, "endgame"))
	m->ending = U_BoolValue (key) ? UM_END_DEFAULT : UM_END_NONE;
    else if (!strcasecmp (key, "endpic"))
    {
	U_LumpValue (m->endpic, key);
	if (m->endpic[0])
	    m->ending = UM_END_PIC;
    }
    else if (!strcasecmp (key, "endbunny"))
    {
	if (U_BoolValue (key))
	    m->ending = UM_END_BUNNY;
    }
    else if (!strcasecmp (key, "endcast"))
    {
	if (U_BoolValue (key))
	    m->ending = UM_END_CAST;
    }
    else if (!strcasecmp (key, "nointermission"))
	m->nointermission = U_BoolValue (key);
    else if (!strcasecmp (key, "intertext"))
	m->intertext = U_TextValue ();
    else if (!strcasecmp (key, "intertextsecret"))
	m->intertextsecret = U_TextValue ();
    else if (!strcasecmp (key, "episode"))
	U_Episode (m);
    else if (!strcasecmp (key, "bossaction"))
	U_BossAction (m);

    // Anything else is some port's own, or a later revision's.
}

static void U_AddMap (umapentry_t* m)
{
    umapentry_t*	old = U_FindMap (m->episode, m->map);

    if (old)
    {
	*old = *m;
	return;
    }
    maps = realloc (maps, (nummaps + 1) * sizeof(*maps));
    if (!maps)
	I_Error ("UMAPINFO: out of memory");
    maps[nummaps++] = *m;
}

static void U_Parse (const char* text, const char* what)
{
    umapentry_t	m;
    char	name[MAXTOKEN];
    boolean	valid;

    scan = text;
    scanline = 1;
    scanwhat = what;

    for (;;)
    {
	U_Next ();
	if (ttype == T_EOF)
	    return;

	if (ttype != T_WORD || strcasecmp (token, "map"))
	{
	    U_Warn ("expected MAP, found", token);
	    return;
	}

	U_Next ();
	if (ttype != T_WORD && ttype != T_STRING)
	{
	    U_Warn ("expected a map's name, found", token);
	    return;
	}
	strcpy (name, token);

	memset (&m, 0, sizeof(m));
	valid = U_ParseMapName (name, &m.episode, &m.map);
	if (!valid)
	    U_Warn ("not a map of this game's, passed over:", name);

	U_Next ();
	if (!U_IsPunct ('{'))
	{
	    U_Warn ("expected {, found", token);
	    return;
	}

	for (;;)
	{
	    char	key[MAXTOKEN];

	    U_Next ();
	    if (U_IsPunct ('}'))
		break;
	    if (ttype != T_WORD)
	    {
		U_Warn ("expected a key, found", ttype == T_EOF ? "the end" : token);
		return;
	    }
	    strcpy (key, token);

	    U_Next ();
	    if (!U_IsPunct ('='))
	    {
		U_Warn ("expected =, found", token);
		return;
	    }
	    if (!U_ReadValues ())
		return;

	    // A map this game cannot have is read through and let go.
	    if (valid)
		U_Key (&m, key);
	}

	if (valid)
	    U_AddMap (&m);
    }
}


//
// BEX PAR TIMES
//
// Boom's extension of DEHACKED gave it a [PARS] section, "par episode map
// seconds" -- "par map seconds" for DOOM II -- and SIGIL's DEHACKED lump
// is that and nothing else: the par times of its E5. The rest of DEHACKED,
// which changes things, weapons and text, this engine does not read.
//
void U_ReadBexPars (const char* text)
{
    char	line[256];
    boolean	inpars = false;
    int		a, b, c, n;

    while (*text)
    {
	const char*	nl = strchr (text, '\n');
	size_t		len = nl ? (size_t)(nl - text) : strlen (text);
	char*		p = line;

	snprintf (line, sizeof(line), "%.*s", (int) len, text);
	text += nl ? len + 1 : len;

	while (isspace ((unsigned char)*p))
	    p++;

	if (*p == '[')
	{
	    inpars = !strncasecmp (p, "[PARS]", 6);
	    continue;
	}
	if (!inpars || strncasecmp (p, "par", 3)
	    || !isspace ((unsigned char)p[3]))
	    continue;

	n = sscanf (p + 3, "%d %d %d", &a, &b, &c);
	if (n == 2)
	{
	    c = b;		// DOOM II's: par map seconds
	    b = a;
	    a = 1;
	}
	else if (n != 3)
	    continue;

	bexpars = realloc (bexpars, (numbexpars + 1) * sizeof(*bexpars));
	if (!bexpars)
	    I_Error ("DEHACKED: out of memory");
	bexpars[numbexpars].episode = a;
	bexpars[numbexpars].map = b;
	bexpars[numbexpars].seconds = c;
	numbexpars++;
    }
}

int U_BexPar (int episode, int map)
{
    int		i;

    // The last word on a map is the one that counts.
    for (i = numbexpars - 1; i >= 0; i--)
	if (bexpars[i].map == map
	    && (gamemode == commercial || bexpars[i].episode == episode))
	    return bexpars[i].seconds;
    return 0;
}


//
// U_Init
//
void U_Init (void)
{
    int		i;
    int		found = 0;

    if (nervepack && gamemode == commercial)
	U_Parse (nerveinfo, "No Rest for the Living");

    for (i = 0; i < numlumps; i++)
    {
	char*	text;
	int	len;
	char	what[32];

	boolean	umapinfo = !strncasecmp (lumpinfo[i].name, "UMAPINFO", 8);

	if (!umapinfo && strncasecmp (lumpinfo[i].name, "DEHACKED", 8))
	    continue;

	len = W_LumpLength (i);
	text = malloc (len + 1);
	if (!text)
	    I_Error ("UMAPINFO: out of memory");
	W_ReadLump (i, text);
	text[len] = 0;

	if (umapinfo)
	{
	    sprintf (what, "lump %d", i);
	    U_Parse (text, what);
	    found++;
	}
	else
	    U_ReadBexPars (text);
	free (text);
    }

    if (found || nummaps)
	printf ("U_Init: UMAPINFO describes %d map%s.\n",
		nummaps, nummaps == 1 ? "" : "s");
}
