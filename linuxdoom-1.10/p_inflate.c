//
// Decompressing ZDBSP's compressed nodes (ZNOD). On its own because zlib's
// header brings in the system's close(), which a door type of id's is also
// called.
//

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "i_system.h"
#include "p_inflate.h"

unsigned char* P_Inflate (unsigned char* in, int inlen, int* outlen)
{
    z_stream		zs;
    unsigned		cap = inlen * 4 + 1024;
    unsigned char*	out = malloc (cap);
    int			r;

    memset (&zs, 0, sizeof(zs));
    zs.next_in = in;
    zs.avail_in = inlen;
    if (!out || inflateInit (&zs) != Z_OK)
	I_Error ("P_Inflate: cannot start zlib");

    for (;;)
    {
	zs.next_out = out + zs.total_out;
	zs.avail_out = cap - zs.total_out;
	r = inflate (&zs, Z_NO_FLUSH);
	if (r == Z_STREAM_END)
	    break;
	if (r != Z_OK && r != Z_BUF_ERROR)
	    I_Error ("P_Inflate: compressed nodes are damaged (zlib %d)", r);
	if (zs.avail_out)
	    I_Error ("P_Inflate: compressed nodes end early");
	cap *= 2;
	if (!(out = realloc (out, cap)))
	    I_Error ("P_Inflate: no memory for the nodes");
    }

    *outlen = zs.total_out;
    inflateEnd (&zs);
    return out;
}
