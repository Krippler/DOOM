//
// zlib's inflate, for ZDBSP's compressed nodes. The result is malloc'd.
//
#ifndef __P_INFLATE__
#define __P_INFLATE__

unsigned char* P_Inflate (unsigned char* in, int inlen, int* outlen);

#endif
