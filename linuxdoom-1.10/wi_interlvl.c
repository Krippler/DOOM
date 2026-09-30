// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//  ID24's INTERLEVEL lumps, read from their JSON.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "doomdef.h"
#include "m_json.h"
#include "wi_interlvl.h"

static void WI_LumpName (char* out, const char* s)
{
    strncpy (out, s, 8);
    out[8] = 0;
}

// An array's elements that read; *count of them.
static void* WI_ReadArray (json_t* arr, int size, int* count,
			   boolean (*read) (json_t* js, void* out))
{
    char*	out;
    int		n = JS_Size (arr);
    int		i;

    *count = 0;
    if (!n || !(out = calloc (n, size)))
	return NULL;
    for (i = 0; i < n; i++)
	if (read (JS_Index (arr, i), out + *count*size))
	    (*count)++;
    return out;
}

static boolean WI_ReadCond (json_t* js, void* out)
{
    ilcond_t*	c = out;
    json_t*	cond = JS_Get (js, "condition");
    json_t*	param = JS_Get (js, "param");

    if (!cond || cond->type != JS_NUMBER || !param || param->type != JS_NUMBER)
	return false;
    c->condition = JS_Int (cond, 0);
    c->param = JS_Int (param, 0);
    return true;
}

static boolean WI_ReadFrame (json_t* js, void* out)
{
    ilframe_t*	f = out;
    const char*	image = JS_String (JS_Get (js, "image"), NULL);

    if (!image || !JS_Get (js, "type"))
	return false;
    WI_LumpName (f->image, image);
    f->type = JS_Int (JS_Get (js, "type"), 0);
    // in seconds
    f->duration = JS_Number (JS_Get (js, "duration"), 0) * TICRATE;
    f->maxduration = JS_Number (JS_Get (js, "maxduration"), 0) * TICRATE;
    return true;
}

static boolean WI_ReadAnim (json_t* js, void* out)
{
    ilanim_t*	a = out;
    json_t*	x = JS_Get (js, "x");
    json_t*	y = JS_Get (js, "y");

    if (!x || x->type != JS_NUMBER || !y || y->type != JS_NUMBER)
	return false;
    a->x = JS_Int (x, 0);
    a->y = JS_Int (y, 0);
    a->frames = WI_ReadArray (JS_Get (js, "frames"), sizeof(ilframe_t),
			      &a->numframes, WI_ReadFrame);
    a->conds = WI_ReadArray (JS_Get (js, "conditions"), sizeof(ilcond_t),
			     &a->numconds, WI_ReadCond);
    if (!a->numframes)
    {
	free (a->frames);
	free (a->conds);
	return false;
    }
    return true;
}

static boolean WI_ReadLayer (json_t* js, void* out)
{
    illayer_t*	l = out;

    l->anims = WI_ReadArray (JS_Get (js, "anims"), sizeof(ilanim_t),
			     &l->numanims, WI_ReadAnim);
    l->conds = WI_ReadArray (JS_Get (js, "conditions"), sizeof(ilcond_t),
			     &l->numconds, WI_ReadCond);
    return true;
}

interlevel_t* WI_ParseInterlevel (const char* lump)
{
    json_t*		js = JS_ParseLump (lump, "interlevel");
    json_t*		data = JS_Get (js, "data");
    const char*		music = JS_String (JS_Get (data, "music"), NULL);
    const char*		back = JS_String (JS_Get (data, "backgroundimage"), NULL);
    interlevel_t*	il = NULL;

    if (js && (!music || !back))
	fprintf (stderr, "%s: no music or background\n", lump);
    else if (js && (il = calloc (1, sizeof(*il))))
    {
	WI_LumpName (il->music, music);
	WI_LumpName (il->background, back);
	il->layers = WI_ReadArray (JS_Get (data, "layers"), sizeof(illayer_t),
				   &il->numlayers, WI_ReadLayer);
    }
    JS_Free (js);
    return il;
}

void WI_FreeInterlevel (interlevel_t* il)
{
    int		i;
    int		j;

    if (!il)
	return;
    for (i = 0; i < il->numlayers; i++)
    {
	for (j = 0; j < il->layers[i].numanims; j++)
	{
	    free (il->layers[i].anims[j].frames);
	    free (il->layers[i].anims[j].conds);
	}
	free (il->layers[i].anims);
	free (il->layers[i].conds);
    }
    free (il->layers);
    free (il);
}
