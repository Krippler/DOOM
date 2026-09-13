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
//	So this does that job. It mixes the sound effects itself, from the
//	engine's own commands, and reads FluidSynth's music from a pipe; both
//	on a real-time schedule, summed and sent to whoever is connected.
//
//	The effects used to come from the separate sound server the 1997
//	release shipped, over a pipe. That cost two stages of delay nobody
//	wanted: the server mixed 46 ms at a time, so a shot waited on average
//	half a block to be mixed at all, and the block then waited its turn in
//	a pipe that could not be smaller than 93 ms. The mixer itself is good
//	-- it is the original, in sndserv/soundsrv.c, linked here rather than
//	rewritten -- so what changed is who calls it and how often: a shot now
//	lands in the next few milliseconds of output instead of the next block
//	of a process downstream.
//
//	Reading the music pipe on a schedule is what paces FluidSynth, the way
//	the original sound server was paced by a blocking write to /dev/dsp.
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
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "soundsrv.h"

// The rate the mixer works at, fixed in its own header as SPEED.
#define SFX_RATE	11025

// A play command is 'p' and eight hex digits, then a newline.
#define CMD_PLAY_LEN	10

// How many listeners to serve at once. More than one is only ever a second
// browser tab, which is not worth much, but refusing it outright would leave
// a reloading page unable to get back in until the old socket timed out.
#define MAXCLIENTS	4

// Frames per period. At 22050 Hz this is 12 ms, which is the granularity of
// everything downstream: it bounds the added latency and the mixing cost.
#define PERIOD		256

#define NSEC_PER_SEC	1000000000L


//
// A listener, and what is still owed to it.
//
// A socket write takes as much as it feels like -- any number of bytes, not
// necessarily a whole 16-bit stereo frame. The first version of this dropped
// whatever was left of the period when the socket filled, which leaves the
// listener holding half a frame: every sample after that is assembled from
// the wrong pair of bytes, which is not a glitch but full-scale noise that
// never recovers. Reported from the field as the sound going "super loud,
// then cut out completely, then back to normal" -- the last part being the
// page's ring buffer giving up and resynchronising.
//
// So what the socket would not take is kept and offered again next period.
// When the backlog is too old to be worth sending, whole frames are dropped
// from the front of it: a gap in the sound is a click, and a gap that is not
// a whole number of frames is noise for ever.
//
#define BACKLOG_PERIODS	8

typedef struct
{
    int			fd;
    unsigned char	pend[PERIOD * 4 * BACKLOG_PERIODS];
    size_t		pendlen;
} client_t;


//
// Adds bytes to what is owed, dropping the oldest whole frames if that is the
// only way to make room.
//
static void
Owe
( client_t*		c,
  const unsigned char*	buf,
  size_t		len )
{
    if (len > sizeof(c->pend))
	return;			// cannot happen: a period is far smaller

    if (c->pendlen + len > sizeof(c->pend))
    {
	// Drop from the front, rounded up to a whole frame so what is left
	// still starts on one.
	size_t	drop = c->pendlen + len - sizeof(c->pend);

	drop = (drop + 3) & ~(size_t) 3;

	if (drop > c->pendlen)
	    drop = c->pendlen & ~(size_t) 3;

	memmove (c->pend, c->pend + drop, c->pendlen - drop);
	c->pendlen -= drop;
    }

    memcpy (c->pend + c->pendlen, buf, len);
    c->pendlen += len;
}


//
// Hands over as much as the socket will take. Returns 0 if the listener has
// gone away.
//
static int
Flush (client_t* c)
{
    while (c->pendlen)
    {
	ssize_t	w = write (c->fd, c->pend, c->pendlen);

	if (w > 0)
	{
	    c->pendlen -= (size_t) w;

	    if (c->pendlen)
		memmove (c->pend, c->pend + w, c->pendlen);

	    continue;
	}

	if (w < 0 && errno == EINTR)
	    continue;

	// Full socket. What is left stays owed, still frame-aligned, and goes
	// out next period.
	if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
	    return 1;

	return 0;
    }

    return 1;
}



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
    // this size is 93 ms of music. The kernel rounds up to a page and refuses
    // to go below one, so this is one page more than the floor.
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


//
// The engine's sound commands.
//
// A unix socket rather than the pipe the 1997 engine wrote to, because the
// engine used to start the sound server itself and talk to its standard
// input; there is no separate process to start now, so it connects to this
// instead. The protocol is unchanged -- 'p' and eight hex digits for id,
// pitch, volume and stereo position -- so the engine's side of it is the
// same two lines it always was.
//
static int	cmd_lfd = -1;
static int	cmd_fd = -1;
static unsigned char cmd_buf[256];
static size_t	cmd_have = 0;


static void
OpenCommands (const char* path)
{
    struct sockaddr_un	addr;

    unlink (path);

    cmd_lfd = socket (AF_UNIX, SOCK_STREAM, 0);

    if (cmd_lfd < 0)
	die ("command socket");

    memset (&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy (addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind (cmd_lfd, (struct sockaddr*) &addr, sizeof(addr)) < 0)
	die (path);

    if (listen (cmd_lfd, 1) < 0)
	die (path);

    if (fcntl (cmd_lfd, F_SETFL, O_NONBLOCK) < 0)
	die (path);
}


//
// Takes everything the engine has said since the last period and starts the
// sounds it asked for. Anything half-arrived waits for the rest of itself.
//
static void
ReadCommands (void)
{
    if (cmd_fd < 0)
    {
	cmd_fd = accept (cmd_lfd, NULL, NULL);

	if (cmd_fd < 0)
	    return;

	fcntl (cmd_fd, F_SETFL, O_NONBLOCK);
	cmd_have = 0;

	if (verbose)
	    fprintf (stderr, "audiostream: engine connected\n");
    }

    for (;;)
    {
	ssize_t	n;
	size_t	used = 0;

	n = read (cmd_fd, cmd_buf + cmd_have, sizeof(cmd_buf) - cmd_have);

	if (n > 0)
	    cmd_have += (size_t) n;
	else if (n == 0)
	{
	    close (cmd_fd);
	    cmd_fd = -1;

	    if (verbose)
		fprintf (stderr, "audiostream: engine disconnected\n");

	    return;
	}
	else if (errno == EINTR)
	    continue;
	else if (errno != EAGAIN && errno != EWOULDBLOCK)
	{
	    // The engine went away mid-sentence. Drop the connection and wait
	    // for the next one rather than reading the same error for ever.
	    close (cmd_fd);
	    cmd_fd = -1;
	    return;
	}

	// Parse whatever is complete.
	while (used < cmd_have)
	{
	    unsigned char*	c = cmd_buf + used;

	    if (*c == 'p')
	    {
		int	id, pitch, vol, sep, i, v[8];

		if (cmd_have - used < CMD_PLAY_LEN)
		    break;

		for (i = 0; i < 8; i++)
		{
		    unsigned char d = c[1+i];
		    v[i] = d >= 'a' ? d - 'a' + 10 : d - '0';
		}

		id    = (v[0] << 4) + v[1];
		pitch = (v[2] << 4) + v[3];
		vol   = (v[4] << 4) + v[5];
		sep   = (v[6] << 4) + v[7];

		addsfx (id, vol, steptable[pitch], sep);
		used += CMD_PLAY_LEN;
	    }
	    else if (*c == 'q')
	    {
		used += 2 <= cmd_have - used ? 2 : (cmd_have - used);
	    }
	    else
	    {
		// Out of step with the engine; skip a byte and look again.
		used++;
	    }
	}

	if (used)
	{
	    memmove (cmd_buf, cmd_buf + used, cmd_have - used);
	    cmd_have -= used;
	}

	if (n < 0)
	    break;		// EAGAIN: nothing more waiting
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
    const char*		cmdpath = NULL;
    const char*		musicpath = NULL;
    int			port = 5901;
    int			i;

    source_t		music;
    int			havesfx = 0;
    int			lfd;
    client_t		client[MAXCLIENTS];
    int			nclients = 0;

    short		sfxup[PERIOD * 2];
    short		musbuf[PERIOD * 2];
    short		out[PERIOD * 2];
    short		last[2] = { 0, 0 };

    struct timespec	next;
    long		period_ns;
    long		reports = 0;
    long		lastmusic = 0;

    for (i = 1; i < argc; i++)
    {
	if (!strcmp (argv[i], "--commands") && i+1 < argc)
	    cmdpath = argv[++i];
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
	    fprintf (stderr, "usage: %s --commands PATH --music PATH "
		     "[--port N] [--rate N] [--verbose]\n", argv[0]);
	    return 1;
	}
    }

    if (!cmdpath || !musicpath)
    {
	fprintf (stderr, "audiostream: --commands and --music are required\n");
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

    OpenCommands (cmdpath);

    // The mixer's own setup: the sound lumps out of the WAD, and the volume
    // and pitch tables. Without a WAD there are no effects, but the music
    // still has somewhere to go, so this is not fatal.
    if (grabdata (argc, argv) < 0)
    {
	fprintf (stderr, "audiostream: no WAD to take sounds from, "
		 "music only\n");
	havesfx = 0;
    }
    else
    {
	initdata ();
	havesfx = 1;
    }

    // Everything a pipe holds is delay, so this holds as little as it can:
    // one page is the smallest a pipe can be, and the music pipe is sized for
    // its own byte rate to come to the same 93 ms. The effects no longer come
    // through a pipe at all.
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

		client[nclients].fd = c;
		client[nclients].pendlen = 0;
		Owe (&client[nclients], hdr, sizeof(hdr));

		if (!Flush (&client[nclients]))
		{
		    close (c);
		    continue;
		}
	    }

	    nclients++;

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
	    if (music.underruns != lastmusic)
	    {
		fprintf (stderr, "audiostream: short periods in the last 5s: "
			 "music %ld\n", music.underruns - lastmusic);
		fflush (stderr);
		lastmusic = music.underruns;
	    }

	    reports = 0;
	}

	// The effects, mixed here and now: whatever the engine has asked for
	// since the last period goes into the period about to be sent, rather
	// than into a block of some process downstream.
	ReadCommands ();

	if (havesfx)
	    mix (sfxframes);
	else
	    memset (mixbuffer, 0, (size_t) sfxframes * 4);

	// Read the music whether or not anyone is listening. This read is the
	// only thing letting the synth thread move: if it blocks on a full
	// pipe the engine blocks behind it, waiting to be heard by nobody.
	ReadSource (&music, musbuf, (size_t) PERIOD * 4);

	if (!nclients)
	    continue;

	Upsample (mixbuffer, sfxframes, sfxup, last);

	for (n = 0; n < PERIOD * 2; n++)
	{
	    int	v = sfxup[n] + musbuf[n];

	    out[n] = v > SHRT_MAX ? SHRT_MAX : (v < SHRT_MIN ? SHRT_MIN : v);
	}

	for (n = 0; n < nclients; n++)
	{
	    Owe (&client[n], (const unsigned char*) out, sizeof(out));

	    if (!Flush (&client[n]))
	    {
		close (client[n].fd);
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
