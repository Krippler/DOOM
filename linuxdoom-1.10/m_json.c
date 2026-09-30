// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//  A small JSON reader: RFC 8259's grammar, read into a tree. A string's
//  \u escapes past ASCII become UTF-8.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_json.h"

typedef struct
{
    const char*	p;
    const char*	end;
    int		depth;
} jsreader_t;

static json_t* JS_Value (jsreader_t* r);

static void JS_Space (jsreader_t* r)
{
    while (r->p < r->end && (*r->p == ' ' || *r->p == '\t'
			     || *r->p == '\r' || *r->p == '\n'))
	r->p++;
}

static json_t* JS_New (jstype_t type)
{
    json_t*	js = calloc (1, sizeof(*js));

    if (js)
	js->type = type;
    return js;
}

static boolean JS_Word (jsreader_t* r, const char* word)
{
    int		n = strlen (word);

    if (r->end - r->p < n || strncmp (r->p, word, n))
	return false;
    r->p += n;
    return true;
}

static int JS_Hex (jsreader_t* r)
{
    int		v = 0;
    int		i;
    int		c;

    if (r->end - r->p < 4)
	return -1;
    for (i = 0; i < 4; i++)
    {
	c = *r->p++;
	v <<= 4;
	if (c >= '0' && c <= '9')
	    v |= c - '0';
	else if (c >= 'a' && c <= 'f')
	    v |= c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
	    v |= c - 'A' + 10;
	else
	    return -1;
    }
    return v;
}

// A string, from its opening quote; NULL when it is not one.
static char* JS_StringText (jsreader_t* r)
{
    const char*	q;
    char*	s;
    int		n = 0;
    int		c;

    if (r->p >= r->end || *r->p != '"')
	return NULL;
    r->p++;

    // no longer than its source
    for (q = r->p; q < r->end && *q != '"'; q++)
	if (*q == '\\')
	    q++;
    if (q >= r->end)
	return NULL;
    s = malloc (q - r->p + 1);
    if (!s)
	return NULL;

    while (*r->p != '"')
    {
	c = (unsigned char)*r->p++;
	if (c < 0x20)
	    goto bad;
	if (c != '\\')
	{
	    s[n++] = c;
	    continue;
	}
	switch (*r->p++)
	{
	  case '"':	s[n++] = '"'; break;
	  case '\\':	s[n++] = '\\'; break;
	  case '/':	s[n++] = '/'; break;
	  case 'b':	s[n++] = '\b'; break;
	  case 'f':	s[n++] = '\f'; break;
	  case 'n':	s[n++] = '\n'; break;
	  case 'r':	s[n++] = '\r'; break;
	  case 't':	s[n++] = '\t'; break;
	  case 'u':
	    // six characters of source, at most three of UTF-8; a
	    // surrogate pair's halves are written as they come
	    if ((c = JS_Hex (r)) < 0)
		goto bad;
	    if (c < 0x80)
		s[n++] = c;
	    else if (c < 0x800)
	    {
		s[n++] = 0xc0 | c >> 6;
		s[n++] = 0x80 | (c & 0x3f);
	    }
	    else
	    {
		s[n++] = 0xe0 | c >> 12;
		s[n++] = 0x80 | (c >> 6 & 0x3f);
		s[n++] = 0x80 | (c & 0x3f);
	    }
	    break;
	  default:
	    goto bad;
	}
    }
    r->p++;
    s[n] = 0;
    return s;

  bad:
    free (s);
    return NULL;
}

static json_t* JS_NumberValue (jsreader_t* r)
{
    char	buf[64];
    char*	e;
    int		n = 0;
    json_t*	js;

    while (r->p < r->end && n < 63 && strchr ("+-0123456789.eE", *r->p))
	buf[n++] = *r->p++;
    buf[n] = 0;
    js = JS_New (JS_NUMBER);
    if (!js)
	return NULL;
    js->number = strtod (buf, &e);
    if (!n || *e)
    {
	free (js);
	return NULL;
    }
    return js;
}

// An array's elements or an object's members, up to close.
static json_t* JS_Members (jsreader_t* r, jstype_t type, char close)
{
    json_t*	js = JS_New (type);
    json_t**	last;
    json_t*	v;
    char*	key = NULL;

    if (!js)
	return NULL;
    last = &js->child;
    r->p++;
    JS_Space (r);
    if (r->p < r->end && *r->p == close)
    {
	r->p++;
	return js;
    }
    for (;;)
    {
	JS_Space (r);
	if (type == JS_OBJECT)
	{
	    if (!(key = JS_StringText (r)))
		break;
	    JS_Space (r);
	    if (r->p >= r->end || *r->p++ != ':')
		break;
	}
	if (!(v = JS_Value (r)))
	    break;
	v->key = key;
	key = NULL;
	*last = v;
	last = &v->next;
	js->size++;

	JS_Space (r);
	if (r->p >= r->end)
	    break;
	if (*r->p == close)
	{
	    r->p++;
	    return js;
	}
	if (*r->p++ != ',')
	    break;
    }
    free (key);
    JS_Free (js);
    return NULL;
}

static json_t* JS_Value (jsreader_t* r)
{
    json_t*	js = NULL;

    JS_Space (r);
    if (r->p >= r->end || r->depth > 64)
	return NULL;

    r->depth++;
    switch (*r->p)
    {
      case '{':
	js = JS_Members (r, JS_OBJECT, '}');
	break;
      case '[':
	js = JS_Members (r, JS_ARRAY, ']');
	break;
      case '"':
	if ((js = JS_New (JS_STRING))
	    && !(js->string = JS_StringText (r)))
	{
	    free (js);
	    js = NULL;
	}
	break;
      case 't':
      case 'f':
	if ((js = JS_New (JS_BOOL)))
	{
	    js->number = *r->p == 't';
	    if (!JS_Word (r, js->number ? "true" : "false"))
	    {
		free (js);
		js = NULL;
	    }
	}
	break;
      case 'n':
	if (JS_Word (r, "null"))
	    js = JS_New (JS_NULL);
	break;
      default:
	js = JS_NumberValue (r);
	break;
    }
    r->depth--;
    return js;
}

json_t* JS_Parse (const char* text, int len)
{
    jsreader_t	r;
    json_t*	js;

    r.p = text;
    r.end = text + len;
    r.depth = 0;

    // a byte order mark
    if (len >= 3 && !memcmp (text, "\xef\xbb\xbf", 3))
	r.p += 3;

    js = JS_Value (&r);
    JS_Space (&r);
    if (js && r.p < r.end)
    {
	JS_Free (js);
	js = NULL;
    }
    return js;
}

json_t* JS_ParseLump (const char* name, const char* type)
{
    int		lump = W_CheckNumForName ((char*)name);
    json_t*	js;
    const char*	t;

    if (lump < 0)
	return NULL;
    js = JS_Parse (W_CacheLumpNum (lump, PU_CACHE), W_LumpLength (lump));
    if (!js)
    {
	fprintf (stderr, "%s: not JSON\n", name);
	return NULL;
    }
    t = JS_String (JS_Get (js, "type"), "");
    if (strcasecmp (t, type))
    {
	fprintf (stderr, "%s: of type \"%s\", not \"%s\"\n", name, t, type);
	JS_Free (js);
	return NULL;
    }
    return js;
}

void JS_Free (json_t* js)
{
    json_t*	next;

    for (; js; js = next)
    {
	next = js->next;
	JS_Free (js->child);
	free (js->string);
	free (js->key);
	free (js);
    }
}

json_t* JS_Get (json_t* js, const char* key)
{
    if (!js || js->type != JS_OBJECT)
	return NULL;
    for (js = js->child; js; js = js->next)
	if (!strcmp (js->key, key))
	    return js;
    return NULL;
}

json_t* JS_Index (json_t* js, int i)
{
    if (!js || js->type != JS_ARRAY || i < 0)
	return NULL;
    for (js = js->child; js && i; js = js->next)
	i--;
    return js;
}

int JS_Size (json_t* js)
{
    return js && js->type == JS_ARRAY ? js->size : 0;
}

double JS_Number (json_t* js, double def)
{
    return js && js->type == JS_NUMBER ? js->number : def;
}

int JS_Int (json_t* js, int def)
{
    return js && js->type == JS_NUMBER ? (int)js->number : def;
}

const char* JS_String (json_t* js, const char* def)
{
    return js && js->type == JS_STRING ? js->string : def;
}
