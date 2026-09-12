// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Sends the game's picture to the browser.
//
//	This replaces VNC for video. VNC is a request/response protocol: the
//	server sends a frame only once the client has asked for the next one,
//	which it does after decoding the last, and x11vnc has to poll an X
//	window and convert 8-bit colour to 24 before any of that. Measured end
//	to end, pressing a key and seeing it cost about 140 ms, and none of
//	x11vnc's timing options changed it, because the cost was the shape of
//	the arrangement rather than any setting in it.
//
//	So the engine writes its own frames to a pipe -- the same indexed
//	320x200 buffer it has already drawn -- and this pushes them at the
//	browser as they arrive, with no one asking first. The palette goes with
//	the frame that changed it and is applied at the far end.
//
//	Only what moved is sent. The screen is cut into tiles and a tile is
//	sent when it differs from the one before it, which through a doorway is
//	most of them and standing still is almost none.
//
//-----------------------------------------------------------------------------

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define VIDEO_MAGIC	0xd0

// The engine's screen, which is the one size it has ever had.
#define MAXW		640
#define MAXH		400

// 16x10 divides 320x200 exactly, so there are no ragged edges to special-case
// and every tile is the same 160 bytes.
#define TILE_W		16
#define TILE_H		10

#define MAXCLIENTS	4

// What one listener may fall behind by before frames are dropped for them
// rather than queued. Two full screens: enough to ride out a stutter, little
// enough that they cannot get seconds adrift.
#define MAXQUEUE	(2 * MAXW * MAXH)

#define MSG_PALETTE	1
#define MSG_KEYFRAME	2
#define MSG_TILES	3


static int	verbose = 0;


static void
die (const char* what)
{
    fprintf (stderr, "videostream: %s: %s\n", what, strerror (errno));
    exit (1);
}


typedef struct
{
    int		fd;
    unsigned char* out;
    size_t	len;		// bytes queued
    size_t	off;		// bytes of those already written
    size_t	cap;
    int		needkey;	// fell behind, so the next send must be whole
    int		hello;		// told what size the picture is
} client_t;


static int
ClientQueue
( client_t*		c,
  const unsigned char*	data,
  size_t		n )
{
    if (c->len - c->off + n > MAXQUEUE)
    {
	// Too far behind to catch up by queueing more. Throw away what is
	// waiting and start again from a whole frame.
	c->len = c->off = 0;
	c->needkey = 1;
	return 0;
    }

    if (c->len + n > c->cap)
    {
	size_t	want = (c->len + n) * 2;
	unsigned char*	p = realloc (c->out, want);

	if (!p)
	    return 0;

	c->out = p;
	c->cap = want;
    }

    memcpy (c->out + c->len, data, n);
    c->len += n;
    return 1;
}


//
// Writes what it can and keeps the rest. Returns 0 when the listener has gone.
//
static int
ClientFlush (client_t* c)
{
    while (c->off < c->len)
    {
	ssize_t	n = write (c->fd, c->out + c->off, c->len - c->off);

	if (n > 0)
	{
	    c->off += (size_t) n;
	    continue;
	}

	if (n < 0 && errno == EINTR)
	    continue;

	if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
	    return 1;

	return 0;
    }

    c->off = c->len = 0;
    return 1;
}


static void
PutHeader
( unsigned char*	h,
  int			type,
  size_t		len )
{
    h[0] = (unsigned char) type;
    h[1] = (unsigned char) (len & 0xff);
    h[2] = (unsigned char) ((len >> 8) & 0xff);
    h[3] = (unsigned char) ((len >> 16) & 0xff);
    h[4] = (unsigned char) ((len >> 24) & 0xff);
}


static int
Listen (int port)
{
    struct sockaddr_in	addr;
    int			fd;
    int			one = 1;

    fd = socket (AF_INET, SOCK_STREAM, 0);

    if (fd < 0)
	die ("socket");

    setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    memset (&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    addr.sin_port = htons ((unsigned short) port);

    if (bind (fd, (struct sockaddr*) &addr, sizeof(addr)) < 0)
	die ("bind");

    if (listen (fd, MAXCLIENTS) < 0)
	die ("listen");

    if (fcntl (fd, F_SETFL, O_NONBLOCK) < 0)
	die ("listen socket");

    return fd;
}


int
main
( int		argc,
  char**	argv )
{
    const char*		pipepath = NULL;
    int			port = 5902;
    int			i;

    int			pfd, lfd;
    client_t		client[MAXCLIENTS];
    int			nclients = 0;

    // The record being read, and the last complete screen, to compare against.
    static unsigned char	in[8 + 768 + MAXW*MAXH];
    size_t		have = 0;
    static unsigned char	prev[MAXW*MAXH];
    static unsigned char	pal[768];
    int			havepal = 0, haveprev = 0;
    int			width = 0, height = 0;

    static unsigned char	msg[8 + 768 + MAXW*MAXH + 2*((MAXW/TILE_W)*(MAXH/TILE_H))];

    for (i = 1; i < argc; i++)
    {
	if (!strcmp (argv[i], "--pipe") && i+1 < argc)
	    pipepath = argv[++i];
	else if (!strcmp (argv[i], "--port") && i+1 < argc)
	    port = atoi (argv[++i]);
	else if (!strcmp (argv[i], "--verbose"))
	    verbose = 1;
	else
	{
	    fprintf (stderr, "usage: %s --pipe PATH [--port N] [--verbose]\n",
		     argv[0]);
	    return 1;
	}
    }

    if (!pipepath)
    {
	fprintf (stderr, "videostream: --pipe is required\n");
	return 1;
    }

    unlink (pipepath);

    if (mkfifo (pipepath, 0600) < 0)
	die (pipepath);

    // Read-write for the same reason as the sound pipes: the open returns at
    // once rather than waiting for the engine, and the engine restarting does
    // not show up here as end of file.
    pfd = open (pipepath, O_RDWR | O_NONBLOCK);

    if (pfd < 0)
	die (pipepath);

    // Room for a couple of frames. The engine abandons a frame it cannot
    // write rather than waiting, so this only has to cover the gap between
    // one read and the next.
    fcntl (pfd, F_SETPIPE_SZ, 2 * (8 + 768 + 320*200));

    signal (SIGPIPE, SIG_IGN);

    lfd = Listen (port);

    memset (client, 0, sizeof(client));

    fprintf (stderr, "videostream: listening on port %d\n", port);
    fflush (stderr);

    for (;;)
    {
	struct pollfd	pfds[2 + MAXCLIENTS];
	int		nfds = 0;

	pfds[nfds].fd = pfd;      pfds[nfds].events = POLLIN; nfds++;
	pfds[nfds].fd = lfd;      pfds[nfds].events = POLLIN; nfds++;

	for (i = 0; i < nclients; i++)
	{
	    pfds[nfds].fd = client[i].fd;
	    pfds[nfds].events = client[i].off < client[i].len ? POLLOUT : 0;
	    nfds++;
	}

	if (poll (pfds, nfds, 1000) < 0 && errno != EINTR)
	    die ("poll");

	// Anyone with room, take it.
	for (i = 0; i < nclients; i++)
	{
	    if (!ClientFlush (&client[i]))
	    {
		close (client[i].fd);
		free (client[i].out);
		client[i] = client[--nclients];
		i--;

		if (verbose)
		    fprintf (stderr, "videostream: viewer gone (%d)\n", nclients);
	    }
	}

	for (;;)
	{
	    int	c = accept (lfd, NULL, NULL);

	    if (c < 0)
		break;

	    if (nclients == MAXCLIENTS)
	    {
		close (c);
		continue;
	    }

	    fcntl (c, F_SETFL, O_NONBLOCK);
	    {
		int one = 1;
		setsockopt (c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	    }

	    memset (&client[nclients], 0, sizeof(client_t));
	    client[nclients].fd = c;
	    client[nclients].needkey = 1;
	    nclients++;

	    if (verbose)
		fprintf (stderr, "videostream: viewer connected (%d)\n", nclients);
	}

	// Take everything waiting, and keep only the newest complete frame:
	// an old one is of no use to anybody.
	for (;;)
	{
	    ssize_t	got;
	    size_t	need, body;

	    if (have < 6)
	    {
		got = read (pfd, in + have, 6 - have);

		if (got <= 0)
		    break;

		have += (size_t) got;

		if (have < 6)
		    break;

		if (in[0] != VIDEO_MAGIC)
		{
		    // Out of step with the writer; there is no way back to the
		    // record boundary, so start over.
		    fprintf (stderr, "videostream: bad frame marker, resyncing\n");
		    have = 0;
		    continue;
		}
	    }

	    width  = in[2] | (in[3] << 8);
	    height = in[4] | (in[5] << 8);

	    if (width <= 0 || width > MAXW || height <= 0 || height > MAXH)
	    {
		fprintf (stderr, "videostream: absurd frame size %dx%d\n",
			 width, height);
		have = 0;
		continue;
	    }

	    body = (size_t) width * height + ((in[1] & 1) ? 768 : 0);
	    need = 6 + body;

	    got = read (pfd, in + have, need - have);

	    if (got > 0)
		have += (size_t) got;

	    if (have < need)
		break;

	    // A whole frame.
	    {
		unsigned char*	frame = in + 6;
		size_t		out = 0;
		int		tx, ty;
		int		tilesw = width / TILE_W;
		int		tilesh = height / TILE_H;
		int		changed = 0;
		unsigned char*	countp;

		if (in[1] & 1)
		{
		    memcpy (pal, frame, 768);
		    frame += 768;
		    havepal = 1;
		}

		if (havepal && (in[1] & 1))
		{
		    PutHeader (msg + out, MSG_PALETTE, 768);
		    out += 5;
		    memcpy (msg + out, pal, 768);
		    out += 768;
		}

		// Whole frame for anyone who has just arrived or fallen
		// behind, tiles for everybody else. Both are built here so a
		// mixed set of listeners costs one pass.
		PutHeader (msg + out, MSG_TILES, 0);   // length filled in below
		{
		    size_t	hdr = out;

		    out += 5;
		    countp = msg + out;
		    out += 2;

		    for (ty = 0; ty < tilesh; ty++)
		    {
			for (tx = 0; tx < tilesw; tx++)
			{
			    int		row;
			    size_t	base = (size_t)(ty*TILE_H)*width
					     + (size_t)(tx*TILE_W);
			    int		same = haveprev;

			    if (same)
			    {
				for (row = 0; row < TILE_H; row++)
				{
				    if (memcmp (frame + base + (size_t)row*width,
						prev  + base + (size_t)row*width,
						TILE_W))
				    {
					same = 0;
					break;
				    }
				}
			    }

			    if (same)
				continue;

			    {
				int idx = ty*tilesw + tx;
				msg[out++] = (unsigned char) (idx & 0xff);
				msg[out++] = (unsigned char) ((idx >> 8) & 0xff);
			    }

			    for (row = 0; row < TILE_H; row++)
			    {
				memcpy (msg + out,
					frame + base + (size_t)row*width,
					TILE_W);
				out += TILE_W;
			    }

			    changed++;
			}
		    }

		    countp[0] = (unsigned char) (changed & 0xff);
		    countp[1] = (unsigned char) ((changed >> 8) & 0xff);
		    PutHeader (msg + hdr, MSG_TILES, out - hdr - 5);
		}

		memcpy (prev, frame, (size_t) width * height);
		haveprev = 1;

		for (i = 0; i < nclients; i++)
		{
		    client_t*	c = &client[i];

		    if (!c->hello)
		    {
			unsigned char	hello[12];

			memcpy (hello, "DOOMVID1", 8);
			hello[8]  = (unsigned char) (width & 0xff);
			hello[9]  = (unsigned char) ((width >> 8) & 0xff);
			hello[10] = (unsigned char) (height & 0xff);
			hello[11] = (unsigned char) ((height >> 8) & 0xff);
			ClientQueue (c, hello, sizeof(hello));
			c->hello = 1;
		    }

		    if (c->needkey)
		    {
			unsigned char	hdr[5];

			if (havepal)
			{
			    PutHeader (hdr, MSG_PALETTE, 768);
			    ClientQueue (c, hdr, 5);
			    ClientQueue (c, pal, 768);
			}

			PutHeader (hdr, MSG_KEYFRAME, (size_t) width * height);
			ClientQueue (c, hdr, 5);
			ClientQueue (c, frame, (size_t) width * height);
			c->needkey = 0;
		    }
		    else if (changed || (in[1] & 1))
		    {
			ClientQueue (c, msg, out);
		    }

		    if (!ClientFlush (c))
		    {
			close (c->fd);
			free (c->out);
			*c = client[--nclients];
			i--;
		    }
		}
	    }

	    have = 0;
	}
    }

    return 0;
}
