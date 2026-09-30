// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//  A small JSON reader, for the lumps the 2024 re-release and ID24 give
//  as JSON: a tree of values, read whole, and asked for its parts.
//
//-----------------------------------------------------------------------------

#ifndef __M_JSON__
#define __M_JSON__

typedef enum
{
    JS_NULL,
    JS_BOOL,
    JS_NUMBER,
    JS_STRING,
    JS_ARRAY,
    JS_OBJECT
} jstype_t;

typedef struct json_s
{
    jstype_t		type;
    double		number;		// a number's value, or a bool's 0 or 1
    char*		string;		// a string's value
    char*		key;		// its key, in an object
    int			size;		// an array's or object's members
    struct json_s*	child;		// the first of them
    struct json_s*	next;		// the member after this one
} json_t;

// The value in text, or NULL when it is not JSON.
json_t* JS_Parse (const char* text, int len);

// A lump of JSON whose "type" is type, or NULL, saying why.
json_t* JS_ParseLump (const char* name, const char* type);

void JS_Free (json_t* js);

// An object's member, or an array's element; NULL when there is none.
json_t* JS_Get (json_t* js, const char* key);
json_t* JS_Index (json_t* js, int i);
int JS_Size (json_t* js);

// A value, or def when it is missing or of another type.
int JS_Int (json_t* js, int def);
double JS_Number (json_t* js, double def);
const char* JS_String (json_t* js, const char* def);

#endif
