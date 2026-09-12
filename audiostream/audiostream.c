// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Mixes the game's two audio streams and serves the result to the
//	browser as raw PCM over a socket.
//
//	The container has no sound card and no sound daemon. It cannot have
//	one: the only packaged PulseAudio pulls in systemd, GStreamer, cairo
//	and a set of video codecs -- 172 packages -- to do a job that is, here,
//	adding two streams together and sending them somewhere.
//
//	So this does that job. The sound server writes effects to one pipe at
//	11025 Hz; the engine writes FluidSynth's output to another at whatever
//	the output rate is. This reads both on a real-time schedule, resamples
//	the effects up, sums them, and writes to whoever is connected.
//
//	Reading on a schedule is also what paces the two producers. Both write
//	blocking into a pipe small enough to hold a fraction of a second, the
//	way the original sound server was paced by a blocking write to
//	/dev/dsp, so neither can run ahead of real time.
//
//-----------------------------------------------------------------------------

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// The sound server's output rate, fixed in its own header as SPEED.
#define SFX_RATE	11025

// How many listeners to serve at once. More than one is only ever a second
// browser tab, which is not worth much, but refusing it outright would leave
// a reloading page unable to get back in until the old socket timed out.
#define MAXCLIENTS	4

// Frames per period. At 22050 Hz this is 12 ms, which is the granularity of
// everything downstream: it bounds the added latency and the mixing cost.
#define PERIOD		256

#define NSEC_PER_SEC	1000000000L


static int	out_rate = 22050;
static int	upsample;		// out_rate / SFX_RATE, exactly
static int	verbose = 0;


static void
die (const char* what)
{
    fprintf (stderr, "audiostream: %s: %s\n", what, strerror (errno));
    exit (1);
}


//
// One input pipe.
//
// Holds whatever arrived but did not add up to a whole period, because a pipe
// read stops wherever the writer happened to stop and a frame is four bytes.
//
typedef struct
{
    const char*		path;
    int			fd;
    unsigned char	partial[PERIOD * 4 * 4];
    size_t		have;
    long		underruns;
    long		periods;
} source_t;


//
// Opened read-write on purpose.
//
// A FIFO opened read-only blocks until a writer appears, and reports end of
// file every time the last writer leaves -- which the sound server does at
// every WAD change. Holding a write end of our own means the open returns at
// once and the pipe simply goes quiet between writers.
//
static void
OpenSource
( source_t*	s,
  const char*	path,
  int		pipesize )
{
    s->path = path;
    s->have = 0;
    s->underruns = 0;

    unlink (path);

    if (mkfifo (path, 0600) < 0)
	die (path);

    s->fd = open (path, O_RDWR | O_NONBLOCK);

    if (s->fd < 0)
	die (path);

    // Latency is whatever the pipe holds, so keep it small: a full pipe of
    // this size is 93 ms of sound at the rate that fills it. The kernel
    // rounds up to a page and refuses to go below one, so this is the floor
    // for the effects pipe and one page more than the floor for music.
    if (fcntl (s->fd, F_SETPIPE_SZ, pipesize) < 0 && verbose)
	fprintf (stderr, "audiostream: %s: cannot set pipe size: %s\n",
		 path, strerror (errno));
}


//
// Fills buf with exactly want bytes, padding with silence if the writer has
// not kept up. Anything read beyond want stays for the next period.
//
static void
ReadSource
( source_t*	s,
  short*	buf,
  size_t	want )
{
    while (s->have < want)
    {
	ssize_t	n = read (s->fd, s->partial + s->have, want - s->have);

	if (n > 0)
	{
	    s->have += (size_t) n;
	    continue;
	}

	if (n < 0 && errno == EINTR)
	    continue;

	// EAGAIN: nothing more to be had this period.
	break;
    }

    if (s->have < want)
    {
	memset (s->partial + s->have, 0, want - s->have);
	s->underruns++;
    }



    memcpy (buf, s->partial, want);
    s->have = 0;
}


//
// Effects, resampled up to the output rate.
//
// The rate is a whole multiple, so this is one linear step between each pair
// of input frames rather than a phase accumulator. last is the frame before
// the ones being read, without which every period would start from silence
// and click.
//
static void
Upsample
( const short*	in,
  int		inframes,
  short*	out,
  short*	last )
{
    int		i, j, c;

    for (i = 0; i < inframes; i++)
    {
	for (j = 0; j < upsample; j++)
	{
	    for (c = 0; c < 2; c++)
	    {
		int	a = last[c];
		int	b = in[i*2 + c];

		out[(i*upsample + j)*2 + c] =
		    (short) (a + (b - a) * j / upsample);
	    }
	}

	last[0] = in[i*2];
	last[1] = in[i*2 + 1];
    }
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
    const char*		sfxpath = NULL;
    const char*		musicpath = NULL;
    int			port = 5901;
    int			i;

    source_t		sfx, music;
    int			lfd;
    int			client[MAXCLIENTS];
    int			nclients = 0;

    short		sfxin[PERIOD * 2];
    short		sfxup[PERIOD * 2];
    short		musbuf[PERIOD * 2];
    short		out[PERIOD * 2];
    short		last[2] = { 0, 0 };

    struct timespec	next;
    long		period_ns;
    long		reports = 0;
    long		lastsfx = 0, lastmusic = 0;

    for (i = 1; i < argc; i++)
    {
	if (!strcmp (argv[i], "--sfx") && i+1 < argc)
	    sfxpath = argv[++i];
	else if (!strcmp (argv[i], "--music") && i+1 < argc)
	    musicpath = argv[++i];
	else if (!strcmp (argv[i], "--port") && i+1 < argc)
	    port = atoi (argv[++i]);
	else if (!strcmp (argv[i], "--rate") && i+1 < argc)
	    out_rate = atoi (argv[++i]);
	else if (!strcmp (argv[i], "--verbose"))
	    verbose = 1;
	else
	{
	    fprintf (stderr, "usage: %s --sfx PATH --music PATH "
		     "[--port N] [--rate N] [--verbose]\n", argv[0]);
	    return 1;
	}
    }

    if (!sfxpath || !musicpath)
    {
	fprintf (stderr, "audiostream: --sfx and --music are required\n");
	return 1;
    }

    // Whole multiples only. The resampler is a straight line between input
    // frames, which is enough for 11 kHz effects but only if the step is
    // exact; a rate that does not divide would need a phase accumulator and
    // there is nothing to gain from one here.
    if (out_rate % SFX_RATE || out_rate < SFX_RATE || out_rate > SFX_RATE*4)
    {
	fprintf (stderr, "audiostream: rate must be %d, %d or %d, got %d\n",
		 SFX_RATE, SFX_RATE*2, SFX_RATE*4, out_rate);
	return 1;
    }

    upsample = out_rate / SFX_RATE;

    // Everything a pipe holds is delay, so they hold as little as they can:
    // 4096 bytes is 93 ms of 11025 Hz stereo, and one page is the smallest a
    // pipe can be. The music pipe is sized for its own byte rate to come to
    // the same figure -- it used to be four times that, which is where a third
    // of a second of delay on the music was hiding.
    OpenSource (&sfx, sfxpath, 4096);
    OpenSource (&music, musicpath, 4096 * upsample);

    signal (SIGPIPE, SIG_IGN);

    lfd = Listen (port);

    fprintf (stderr, "audiostream: %d Hz, 16 bit, stereo on port %d\n",
	     out_rate, port);
    fflush (stderr);

    period_ns = (long) ((double) PERIOD * NSEC_PER_SEC / out_rate);

    if (clock_gettime (CLOCK_MONOTONIC, &next) < 0)
	die ("clock_gettime");

    for (;;)
    {
	int	n;
	int	sfxframes = PERIOD / upsample;

	// Wait out the period first, so a slow read never makes up time it
	// did not lose. Absolute deadlines rather than a sleep of a fixed
	// length, or the error from every period would accumulate.
	next.tv_nsec += period_ns;

	while (next.tv_nsec >= NSEC_PER_SEC)
	{
	    next.tv_nsec -= NSEC_PER_SEC;
	    next.tv_sec++;
	}

	while (clock_nanosleep (CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL)
	       == EINTR)
	    ;

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

	    // Sound is a steady trickle of small writes; waiting to fill a
	    // segment would add its own delay on top of everything else.
	    {
		int one = 1;
		setsockopt (c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	    }

	    // Says what the stream is before any of it arrives, so the page
	    // does not have to be told the rate separately and can tell it has
	    // reached the sound rather than something else.
	    {
		unsigned char	hdr[16];

		memcpy (hdr, "DOOMAUD1", 8);
		hdr[8]  = (unsigned char) (out_rate & 0xff);
		hdr[9]  = (unsigned char) ((out_rate >> 8) & 0xff);
		hdr[10] = (unsigned char) ((out_rate >> 16) & 0xff);
		hdr[11] = (unsigned char) ((out_rate >> 24) & 0xff);
		hdr[12] = 2;		// channels
		hdr[13] = 0;
		hdr[14] = 16;		// bits per sample
		hdr[15] = 0;

		if (write (c, hdr, sizeof(hdr)) != (ssize_t) sizeof(hdr))
		{
		    close (c);
		    continue;
		}
	    }

	    client[nclients++] = c;

	    if (verbose)
		fprintf (stderr, "audiostream: listener connected (%d)\n",
			 nclients);
	}

	// Say when a producer could not keep up, which is otherwise the one
	// fault nobody can hear: a period short of sound is padded with
	// silence, and silence is what an absent sound already sounds like.
	// Said only when it happens, so a quiet log means a clean one.
	if (++reports >= 22050 / PERIOD * 5)
	{
	    if (sfx.underruns != lastsfx || music.underruns != lastmusic)
	    {
		fprintf (stderr, "audiostream: short periods in the last 5s: "
			 "effects %ld, music %ld\n",
			 sfx.underruns - lastsfx, music.underruns - lastmusic);
		fflush (stderr);
		lastsfx = sfx.underruns;
		lastmusic = music.underruns;
	    }

	    reports = 0;
	}

	// Read both pipes whether or not anyone is listening. These reads are
	// the only thing letting the game's sound server move: if it blocks
	// on a full pipe the engine blocks behind it, waiting to be heard by
	// nobody.
	ReadSource (&sfx, sfxin, (size_t) sfxframes * 4);
	ReadSource (&music, musbuf, (size_t) PERIOD * 4);

	if (!nclients)
	    continue;

	Upsample (sfxin, sfxframes, sfxup, last);

	for (n = 0; n < PERIOD * 2; n++)
	{
	    int	v = sfxup[n] + musbuf[n];

	    out[n] = v > SHRT_MAX ? SHRT_MAX : (v < SHRT_MIN ? SHRT_MIN : v);
	}

	for (n = 0; n < nclients; n++)
	{
	    const char*	p = (const char*) out;
	    size_t	left = sizeof(out);
	    int		gone = 0;

	    while (left)
	    {
		ssize_t	w = write (client[n], p, left);

		if (w > 0)
		{
		    p += w;
		    left -= (size_t) w;
		    continue;
		}

		if (w < 0 && errno == EINTR)
		    continue;

		// Full socket: the listener is not keeping up, so drop what is
		// left of this period rather than stall everyone else. Sound
		// is worth nothing late.
		if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		    break;

		gone = 1;
		break;
	    }

	    if (gone)
	    {
		close (client[n]);
		client[n] = client[--nclients];
		n--;

		if (verbose)
		    fprintf (stderr, "audiostream: listener gone (%d)\n",
			     nclients);
	    }
	}
    }

    return 0;
}
