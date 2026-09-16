// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	System interface for sound.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_unix.c,v 1.5 1997/02/03 22:45:10 b1 Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include <math.h>

#include <sys/time.h>
#include <sys/types.h>

#ifndef LINUX
#include <sys/filio.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

// Linux voxware output.
#include <linux/soundcard.h>

// Timer stuff. Experimental.
#include <time.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#include "z_zone.h"

#include "i_system.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"

#include "doomdef.h"

#ifdef MUSIC_FLUIDSYNTH
#include <fluidsynth.h>
#include "mus2mid.h"

// Defined with the rest of the music code further down.
void I_FluidSetGain (int volume);
#endif

// UNIX hack, to be removed.
#ifdef SNDSERV
// Separate sound server process.
FILE*	sndserver=0;
char*	sndserver_filename = "./sndserver ";
#elif SNDINTR

// Update all 30 millisecs, approx. 30fps synchronized.
// Linux resolution is allegedly 10 millisecs,
//  scale is microseconds.
#define SOUND_INTERVAL     500

// Get the interrupt. Set duration in millisecs.
int I_SoundSetTimer( int duration_of_tick );
void I_SoundDelTimer( void );
#else
// None?
#endif


// A quick hack to establish a protocol between
// synchronous mix buffer updates and asynchronous
// audio writes. Probably redundant with gametic.
static int flag = 0;

// The number of internal mixing channels,
//  the samples calculated for each mixing step,
//  the size of the 16bit, 2 hardware channel (stereo)
//  mixing buffer, and the samplerate of the raw data.


// Needed for calling the actual sound output.
#define SAMPLECOUNT		512
#define NUM_CHANNELS		8
// It is 2 for 16bit, and 2 for two channels.
#define BUFMUL                  4
#define MIXBUFFERSIZE		(SAMPLECOUNT*BUFMUL)

#define SAMPLERATE		11025	// Hz
#define SAMPLESIZE		2   	// 16bit

// The actual lengths of all sound effects.
int 		lengths[NUMSFX];

// The actual output device.
// -1 when no OSS device could be opened; the mixer then runs silently
// instead of aborting, which is the normal case on modern kernels.
int	audio_fd = -1;

// The global mixing buffer.
// Basically, samples from all active internal channels
//  are modifed and added, and stored in the buffer
//  that is submitted to the audio device.
signed short	mixbuffer[MIXBUFFERSIZE];


// The channel step amount...
unsigned int	channelstep[NUM_CHANNELS];
// ... and a 0.16 bit remainder of last step.
unsigned int	channelstepremainder[NUM_CHANNELS];


// The channel data pointers, start and end.
unsigned char*	channels[NUM_CHANNELS];
unsigned char*	channelsend[NUM_CHANNELS];


// Time/gametic that the channel started playing,
//  used to determine oldest, which automatically
//  has lowest priority.
// In case number of active sounds exceeds
//  available channels.
int		channelstart[NUM_CHANNELS];

// The sound in channel handles,
//  determined on registration,
//  might be used to unregister/stop/modify,
//  currently unused.
int 		channelhandles[NUM_CHANNELS];

// SFX id of the playing sound effect.
// Used to catch duplicates (like chainsaw).
int		channelids[NUM_CHANNELS];			

// Pitch to stepping lookup, unused.
int		steptable[256];

// Volume lookups.
int		vol_lookup[128*256];

// Hardware left and right channel volume lookup.
int*		channelleftvol_lookup[NUM_CHANNELS];
int*		channelrightvol_lookup[NUM_CHANNELS];




//
// Safe ioctl, convenience.
//
void
myioctl
( int	fd,
  int	command,
  int*	arg )
{   
    int		rc;
    
    rc = ioctl(fd, command, arg);  
    if (rc < 0)
    {
	fprintf(stderr, "ioctl(dsp,%d,arg) failed\n", command);
	fprintf(stderr, "errno=%d\n", errno);
	exit(-1);
    }
}





//
// This function loads the sound data from the WAD lump,
//  for single sound.
//
void*
getsfx
( char*         sfxname,
  int*          len )
{
    unsigned char*      sfx;
    unsigned char*      paddedsfx;
    int                 i;
    int                 size;
    int                 paddedsize;
    char                name[20];
    int                 sfxlump;

    
    // Get the sound data from the WAD, allocate lump
    //  in zone memory.
    sprintf(name, "ds%s", sfxname);

    // Now, there is a severe problem with the
    //  sound handling, in it is not (yet/anymore)
    //  gamemode aware. That means, sounds from
    //  DOOM II will be requested even with DOOM
    //  shareware.
    // The sound list is wired into sounds.c,
    //  which sets the external variable.
    // I do not do runtime patches to that
    //  variable. Instead, we will use a
    //  default sound for replacement.
    if ( W_CheckNumForName(name) == -1 )
      sfxlump = W_GetNumForName("dspistol");
    else
      sfxlump = W_GetNumForName(name);
    
    size = W_LumpLength( sfxlump );

    // Debug.
    // fprintf( stderr, "." );
    //fprintf( stderr, " -loading  %s (lump %d, %d bytes)\n",
    //	     sfxname, sfxlump, size );
    //fflush( stderr );
    
    sfx = (unsigned char*)W_CacheLumpNum( sfxlump, PU_STATIC );

    // Pads the sound effect out to the mixing buffer size.
    // The original realloc would interfere with zone memory.
    paddedsize = ((size-8 + (SAMPLECOUNT-1)) / SAMPLECOUNT) * SAMPLECOUNT;

    // Allocate from zone memory.
    paddedsfx = (unsigned char*)Z_Malloc( paddedsize+8, PU_STATIC, 0 );
    // ddt: (unsigned char *) realloc(sfx, paddedsize+8);
    // This should interfere with zone memory handling,
    //  which does not kick in in the soundserver.

    // Now copy and pad.
    memcpy(  paddedsfx, sfx, size );
    for (i=size ; i<paddedsize+8 ; i++)
        paddedsfx[i] = 128;

    // Remove the cached lump.
    Z_Free( sfx );
    
    // Preserve padded length.
    *len = paddedsize;

    // Return allocated padded data.
    return (void *) (paddedsfx + 8);
}





//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
int
addsfx
( int		sfxid,
  int		volume,
  int		step,
  int		seperation )
{
    static unsigned short	handlenums = 0;
 
    int		i;
    int		rc = -1;
    
    int		oldest = gametic;
    int		oldestnum = 0;
    int		slot;

    int		rightvol;
    int		leftvol;

    // Chainsaw troubles.
    // Play these sound effects only one at a time.
    if ( sfxid == sfx_sawup
	 || sfxid == sfx_sawidl
	 || sfxid == sfx_sawful
	 || sfxid == sfx_sawhit
	 || sfxid == sfx_stnmov
	 || sfxid == sfx_pistol	 )
    {
	// Loop all channels, check.
	for (i=0 ; i<NUM_CHANNELS ; i++)
	{
	    // Active, and using the same SFX?
	    if ( (channels[i])
		 && (channelids[i] == sfxid) )
	    {
		// Reset.
		channels[i] = 0;
		// We are sure that iff,
		//  there will only be one.
		break;
	    }
	}
    }

    // Loop all channels to find oldest SFX.
    for (i=0; (i<NUM_CHANNELS) && (channels[i]); i++)
    {
	if (channelstart[i] < oldest)
	{
	    oldestnum = i;
	    oldest = channelstart[i];
	}
    }

    // Tales from the cryptic.
    // If we found a channel, fine.
    // If not, we simply overwrite the first one, 0.
    // Probably only happens at startup.
    if (i == NUM_CHANNELS)
	slot = oldestnum;
    else
	slot = i;

    // Okay, in the less recent channel,
    //  we will handle the new SFX.
    // Set pointer to raw data.
    channels[slot] = (unsigned char *) S_sfx[sfxid].data;
    // Set pointer to end of raw data.
    channelsend[slot] = channels[slot] + lengths[sfxid];

    // Reset current handle number, limited to 0..100.
    if (!handlenums)
	handlenums = 100;

    // Assign current handle number.
    // Preserved so sounds could be stopped (unused).
    channelhandles[slot] = rc = handlenums++;

    // Set stepping???
    // Kinda getting the impression this is never used.
    channelstep[slot] = step;
    // ???
    channelstepremainder[slot] = 0;
    // Should be gametic, I presume.
    channelstart[slot] = gametic;

    // Separation, that is, orientation/stereo.
    //  range is: 1 - 256
    seperation += 1;

    // Per left/right channel.
    //  x^2 seperation,
    //  adjust volume properly.
    leftvol =
	volume - ((volume*seperation*seperation) >> 16); ///(256*256);
    seperation = seperation - 257;
    rightvol =
	volume - ((volume*seperation*seperation) >> 16);	

    // Sanity check, clamp volume.
    if (rightvol < 0 || rightvol > 127)
	I_Error("rightvol out of bounds");
    
    if (leftvol < 0 || leftvol > 127)
	I_Error("leftvol out of bounds");
    
    // Get the proper lookup table piece
    //  for this volume level???
    channelleftvol_lookup[slot] = &vol_lookup[leftvol*256];
    channelrightvol_lookup[slot] = &vol_lookup[rightvol*256];

    // Preserve sound SFX id,
    //  e.g. for avoiding duplicates of chainsaw.
    channelids[slot] = sfxid;

    // You tell me.
    return rc;
}





//
// SFX API
// Note: this was called by S_Init.
// However, whatever they did in the
// old DPMS based DOS version, this
// were simply dummies in the Linux
// version.
// See soundserver initdata().
//
void I_SetChannels()
{
  // Init internal lookups (raw data, mixing buffer, channels).
  // This function sets up internal lookups used during
  //  the mixing process. 
  int		i;
  int		j;
    
  int*	steptablemid = steptable + 128;
  
  // Okay, reset internal mixing channels to zero.
  /*for (i=0; i<NUM_CHANNELS; i++)
  {
    channels[i] = 0;
  }*/

  // This table provides step widths for pitch parameters.
  // I fail to see that this is currently used.
  for (i=-128 ; i<128 ; i++)
    steptablemid[i] = (int)(pow(2.0, (i/64.0))*65536.0);
  
  
  // Generates volume lookup tables
  //  which also turn the unsigned samples
  //  into signed samples.
  for (i=0 ; i<128 ; i++)
    for (j=0 ; j<256 ; j++)
      vol_lookup[i*256+j] = (i*(j-128)*256)/127;
}	

 
void I_SetSfxVolume(int volume)
{
  // Identical to DOS.
  // Basically, this should propagate
  //  the menu/config file setting
  //  to the state variable used in
  //  the mixing.
  snd_SfxVolume = volume;
}

void I_SetMusicVolume(int volume)
{
  // Internal state variable.
  snd_MusicVolume = volume;

#ifdef MUSIC_FLUIDSYNTH
  I_FluidSetGain (volume);
#endif
}


//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
int
I_StartSound
( int		id,
  int		vol,
  int		sep,
  int		pitch,
  int		priority )
{

  // UNUSED
  priority = 0;
  
#ifdef SNDSERV 
    if (sndserver)
    {
	fprintf(sndserver, "p%2.2x%2.2x%2.2x%2.2x\n", id, pitch, vol, sep);
	fflush(sndserver);
    }
    // warning: control reaches end of non-void function.
    return id;
#else
    // Debug.
    //fprintf( stderr, "starting sound %d", id );
    
    // Returns a handle (not used).
    id = addsfx( id, vol, steptable[pitch], sep );

    // fprintf( stderr, "/handle is %d\n", id );
    
    return id;
#endif
}



void I_StopSound (int handle)
{
  // You need the handle returned by StartSound.
  // Would be looping all channels,
  //  tracking down the handle,
  //  an setting the channel to zero.
  
  // UNUSED.
  handle = 0;
}


int I_SoundIsPlaying(int handle)
{
    // Ouch.
    return gametic < handle;
}




//
// This function loops all active (internal) sound
//  channels, retrieves a given number of samples
//  from the raw sound data, modifies it according
//  to the current (internal) channel parameters,
//  mixes the per channel samples into the global
//  mixbuffer, clamping it to the allowed range,
//  and sets up everything for transferring the
//  contents of the mixbuffer to the (two)
//  hardware channels (left and right, that is).
//
// This function currently supports only 16bit.
//
void I_UpdateSound( void )
{
#ifdef SNDINTR
  // Debug. Count buffer misses with interrupt.
  static int misses = 0;
#endif

  
  // Mix current sound data.
  // Data, from raw sound, for right and left.
  register unsigned int	sample;
  register int		dl;
  register int		dr;
  
  // Pointers in global mixbuffer, left, right, end.
  signed short*		leftout;
  signed short*		rightout;
  signed short*		leftend;
  // Step in mixbuffer, left and right, thus two.
  int				step;

  // Mixing channel index.
  int				chan;
    
    // Left and right channel
    //  are in global mixbuffer, alternating.
    leftout = mixbuffer;
    rightout = mixbuffer+1;
    step = 2;

    // Determine end, for left channel only
    //  (right channel is implicit).
    leftend = mixbuffer + SAMPLECOUNT*step;

    // Mix sounds into the mixing buffer.
    // Loop over step*SAMPLECOUNT,
    //  that is 512 values for two channels.
    while (leftout != leftend)
    {
	// Reset left/right value. 
	dl = 0;
	dr = 0;

	// Love thy L2 chache - made this a loop.
	// Now more channels could be set at compile time
	//  as well. Thus loop those  channels.
	for ( chan = 0; chan < NUM_CHANNELS; chan++ )
	{
	    // Check channel, if active.
	    if (channels[ chan ])
	    {
		// Get the raw data from the channel. 
		sample = *channels[ chan ];
		// Add left and right part
		//  for this channel (sound)
		//  to the current data.
		// Adjust volume accordingly.
		dl += channelleftvol_lookup[ chan ][sample];
		dr += channelrightvol_lookup[ chan ][sample];
		// Increment index ???
		channelstepremainder[ chan ] += channelstep[ chan ];
		// MSB is next sample???
		channels[ chan ] += channelstepremainder[ chan ] >> 16;
		// Limit to LSB???
		channelstepremainder[ chan ] &= 65536-1;

		// Check whether we are done.
		if (channels[ chan ] >= channelsend[ chan ])
		    channels[ chan ] = 0;
	    }
	}
	
	// Clamp to range. Left hardware channel.
	// Has been char instead of short.
	// if (dl > 127) *leftout = 127;
	// else if (dl < -128) *leftout = -128;
	// else *leftout = dl;

	if (dl > 0x7fff)
	    *leftout = 0x7fff;
	else if (dl < -0x8000)
	    *leftout = -0x8000;
	else
	    *leftout = dl;

	// Same for right hardware channel.
	if (dr > 0x7fff)
	    *rightout = 0x7fff;
	else if (dr < -0x8000)
	    *rightout = -0x8000;
	else
	    *rightout = dr;

	// Increment current pointers in mixbuffer.
	leftout += step;
	rightout += step;
    }

#ifdef SNDINTR
    // Debug check.
    if ( flag )
    {
      misses += flag;
      flag = 0;
    }
    
    if ( misses > 10 )
    {
      fprintf( stderr, "I_SoundUpdate: missed 10 buffer writes\n");
      misses = 0;
    }
    
    // Increment flag for update.
    flag++;
#endif
}


// 
// This would be used to write out the mixbuffer
//  during each game loop update.
// Updates sound buffer and audio device at runtime. 
// It is called during Timer interrupt with SNDINTR.
// Mixing now done synchronous, and
//  only output be done asynchronous?
//
void
I_SubmitSound(void)
{
  // Write it to DSP device.
  if (audio_fd >= 0
      && write(audio_fd, mixbuffer, SAMPLECOUNT*BUFMUL) < 0)
    fprintf(stderr, "I_SubmitSound: write to audio device failed\n");
}



void
I_UpdateSoundParams
( int	handle,
  int	vol,
  int	sep,
  int	pitch)
{
  // I fail too see that this is used.
  // Would be using the handle to identify
  //  on which channel the sound might be active,
  //  and resetting the channel parameters.

  // UNUSED.
  handle = vol = sep = pitch = 0;
}




void I_ShutdownSound(void)
{    
#ifdef SNDSERV
  if (sndserver)
  {
    // Send a "quit" command.
    fprintf(sndserver, "q\n");
    fflush(sndserver);
  }
#else
  // Wait till all pending sounds are finished.
  int done = 0;
  int i;
  

  // FIXME (below).
  fprintf( stderr, "I_ShutdownSound: NOT finishing pending sounds\n");
  fflush( stderr );
  
  while ( !done )
  {
    for( i=0 ; i<8 && !channels[i] ; i++);
    
    // FIXME. No proper channel output.
    //if (i==8)
    done=1;
  }
#ifdef SNDINTR
  I_SoundDelTimer();
#endif
  
  // Cleaning up -releasing the DSP device.
  if (audio_fd >= 0)
  {
    close ( audio_fd );
    audio_fd = -1;
  }
#endif

  // Done.
  return;
}






void
I_InitSound()
{ 
#ifdef SNDSERV
  char buffer[256];

  // The server is talked to over a pipe. If it dies -- no audio device, no
  // WAD it recognises -- the next write would raise SIGPIPE and take the
  // game down with it. Losing sound is enough.
  signal(SIGPIPE, SIG_IGN);

  // Somebody is already doing the mixing, so there is no server to start:
  // connect to it and talk the same protocol down a socket instead of down a
  // child process's standard input. audiostream mixes the effects itself now,
  // which takes two stages of buffering out of the path between asking for a
  // sound and hearing it.
  //
  // Nothing here may return early: I_InitMusic is called at the end of this
  // function, and skipping it is how the music went silent the first time
  // this was written.
  //
  const char*	sockpath = getenv("DOOM_SFX_SOCKET");

  if (sockpath && *sockpath)
  {
    struct sockaddr_un	addr;
    int			fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0)
      fprintf(stderr, "Could not make a sound socket (%s)\n", strerror(errno));
    else
    {
      memset(&addr, 0, sizeof(addr));
      addr.sun_family = AF_UNIX;
      strncpy(addr.sun_path, sockpath, sizeof(addr.sun_path) - 1);

      if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0)
      {
	fprintf(stderr, "Could not reach the mixer at %s (%s), "
		"running without sound\n", sockpath, strerror(errno));
	close(fd);
      }
      else
      {
	sndserver = fdopen(fd, "w");

	if (!sndserver)
	  close(fd);
	else
	  fprintf(stderr, "sound: mixing in audiostream, commands to %s\n",
		  sockpath);
      }
    }
  }
  else if (getenv("DOOMWADDIR"))
    sprintf(buffer, "%s/%s",
	    getenv("DOOMWADDIR"),
	    sndserver_filename);
  else
    sprintf(buffer, "%s", sndserver_filename);

  // start sound process
  if (!sndserver && (!sockpath || !*sockpath))
  {
    if ( !access(buffer, X_OK) )
    {
      strcat(buffer, " -quiet");
      sndserver = popen(buffer, "w");
    }
    else
      fprintf(stderr, "Could not start sound server [%s]\n", buffer);
  }
#else
    
  int i;
  
#ifdef SNDINTR
  fprintf( stderr, "I_SoundSetTimer: %d microsecs\n", SOUND_INTERVAL );
  I_SoundSetTimer( SOUND_INTERVAL );
#endif
    
  // Secure and configure sound device first.
  fprintf( stderr, "I_InitSound: ");
  
  audio_fd = open("/dev/dsp", O_WRONLY);
  if (audio_fd<0)
  {
    fprintf(stderr, "Could not open /dev/dsp, running without sound\n");
  }
  else
  {
    i = 11 | (2<<16);                                           
    myioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &i);
    myioctl(audio_fd, SNDCTL_DSP_RESET, 0);
  
    i=SAMPLERATE;
  
    myioctl(audio_fd, SNDCTL_DSP_SPEED, &i);
  
    i=1;
    myioctl(audio_fd, SNDCTL_DSP_STEREO, &i);
  
    myioctl(audio_fd, SNDCTL_DSP_GETFMTS, &i);
  
    if (i&=AFMT_S16_LE)    
      myioctl(audio_fd, SNDCTL_DSP_SETFMT, &i);
    else
      fprintf(stderr, "Could not play signed 16 data\n");

    fprintf(stderr, " configured audio device\n" );
  }

    
  // Initialize external data (all sounds) at start, keep static.
  fprintf( stderr, "I_InitSound: ");
  
  for (i=1 ; i<NUMSFX ; i++)
  { 
    // Alias? Example is the chaingun sound linked to pistol.
    if (!S_sfx[i].link)
    {
      // Load data from WAD file.
      S_sfx[i].data = getsfx( S_sfx[i].name, &lengths[i] );
    }	
    else
    {
      // Previously loaded already?
      S_sfx[i].data = S_sfx[i].link->data;
      lengths[i] = lengths[S_sfx[i].link - S_sfx];
    }
  }

  fprintf( stderr, " pre-cached all sound data\n");
  
  // Now initialize mixbuffer with zero.
  for ( i = 0; i< MIXBUFFERSIZE; i++ )
    mixbuffer[i] = 0;
  
  // Finished initialization.
  fprintf(stderr, "I_InitSound: sound module ready\n");
    
#endif

  // Nothing called this in the original release, because every music
  // function was a dummy. I_Quit already calls I_ShutdownMusic.
  I_InitMusic ();
}




//
// MUSIC API.
//
// The WADs store music as MUS lumps, a trimmed down MIDI. mus2mid.c turns
// one into a Standard MIDI File and FluidSynth plays it against a General
// MIDI soundfont. Rendering happens on a thread of its own -- FluidSynth's
// when it has an audio device to drive, ours when DOOM_MUSIC_PIPE says there
// is none -- so the game loop is not involved and the music is mixed with the
// sound effects outside the process.
//
// Built without MUSIC_FLUIDSYNTH these go back to being the dummies the
// original release shipped.
//

static int	looping=0;
static int	musicdies=-1;

#ifdef MUSIC_FLUIDSYNTH

static fluid_settings_t*	fl_settings = NULL;
static fluid_synth_t*		fl_synth = NULL;
static fluid_audio_driver_t*	fl_driver = NULL;
static fluid_player_t*		fl_player = NULL;

//
// One lock over all of it.
//
// The music is rendered by a thread of its own -- see the pipe below -- while
// the game thread throws the player away and builds a new one every time the
// music changes, which is every level. Nothing here was synchronised, so
// delete_fluid_player() ran while fluid_synth_write_s16() was walking the same
// object, and the renderer read memory that had just been freed.
//
// That is a real crash, seen in the wild and reported by the kernel as
//
//   linuxxdoom[932935]: segfault at 14c9f8ed0414 ... error 4
//     in libfluidsynth.so.3.2.2
//
// -- error 4 being a read of an unmapped page, from a thread inside FluidSynth
// rather than anywhere in DOOM. It needs a level change to land in the wrong
// microsecond, so it is rare, arrives without a pattern, and blames the music
// library for something this file did.
//
// Held for the render call and for each player operation, and never across the
// blocking write to the pipe: that write waits on the far end by design, and
// holding a lock through it would stop the game every time the reader paused.
//
static pthread_mutex_t	fl_lock = PTHREAD_MUTEX_INITIALIZER;

// Rendering into a pipe instead of to an audio device.
//
// FluidSynth's audio drivers all want a sound system to talk to, and the
// container has none: what it has is audiostream, reading this pipe and the
// sound server's on a real-time schedule and sending the sum to the browser.
// So when DOOM_MUSIC_PIPE names one, the synth is pulled by a thread of our
// own rather than pushed by a driver.
//
// The blocking write is what keeps that thread in time. It is also what
// advances the music: FluidSynth's player is clocked by the samples the synth
// renders, not by a wall clock, so a thread that renders only as fast as the
// pipe drains plays at exactly the right speed.
#define MUSIC_PIPE_FRAMES	512

static int		fl_pipe_fd = -1;
static pthread_t	fl_pipe_thread;
static volatile int	fl_pipe_running = 0;

// DOOM registers one song at a time, so one slot is enough.
static byte*		song_midi = NULL;
static size_t		song_midi_len = 0;

// DOOM's own music volume runs 0..15. FluidSynth's gain is a float where
// 10 is the maximum; this is the gain used at full volume, low enough that
// a busy score does not clip.
#define MUSIC_MAX_GAIN	0.6f

// Searched in order when neither -soundfont nor DOOM_SOUNDFONT says
// otherwise.
//
// The default-GM links come first because the distribution points them at
// whichever General MIDI soundfont is installed, which is the answer we
// want whatever was packaged. The rest are fallbacks, best first. SF3 files
// are Ogg-compressed soundfonts; FluidSynth reads them directly.
static const char*	soundfont_paths[] =
{
    "/usr/share/sounds/sf2/default-GM.sf2",
    "/usr/share/sounds/sf3/default-GM.sf3",
    "/usr/share/sounds/sf2/FluidR3_GM.sf2",
    "/usr/share/sounds/sf3/FluidR3Mono_GM.sf3",
    "/usr/share/sounds/sf3/MuseScore_General.sf3",
    "/usr/share/soundfonts/default.sf2",
    "/usr/share/sounds/sf2/TimGM6mb.sf2",
    NULL
};


void I_FluidSetGain (int volume)
{
    float	gain;

    if (!fl_synth)
	return;

    // S_SetMusicVolume pokes 127 through here before setting the real
    // value; clamp rather than letting it read as many times maximum.
    if (volume > 15)
	volume = 15;
    if (volume < 0)
	volume = 0;

    gain = (float)volume / 15.0f * MUSIC_MAX_GAIN;

    pthread_mutex_lock (&fl_lock);
    fluid_synth_set_gain (fl_synth, gain);
    pthread_mutex_unlock (&fl_lock);
}


static const char* I_FindSoundFont (void)
{
    static char		path[512];
    const char*		env;
    int			p;
    int			i;

    p = M_CheckParm ("-soundfont");

    if (p && p < myargc - 1)
	return myargv[p + 1];

    env = getenv ("DOOM_SOUNDFONT");

    if (env && *env)
    {
	if (!access (env, R_OK))
	    return env;

	// Fall through to the search rather than losing music entirely
	// because of one bad path.
	fprintf (stderr, "I_InitMusic: DOOM_SOUNDFONT [%s] is not readable, "
		 "looking for another\n", env);
    }

    for (i = 0; soundfont_paths[i]; i++)
    {
	if (!access (soundfont_paths[i], R_OK))
	{
	    snprintf (path, sizeof(path), "%s", soundfont_paths[i]);
	    return path;
	}
    }

    return NULL;
}


//
// Renders music until told to stop, at the speed the far end reads it.
//
static void*
I_FluidPipeThread (void* unused)
{
    short	buf[MUSIC_PIPE_FRAMES * 2];

    (void) unused;

    while (fl_pipe_running)
    {
	const char*	p = (const char*) buf;
	size_t		left = sizeof(buf);

	// Interleaved stereo: left in the even shorts, right in the odd ones.
	pthread_mutex_lock (&fl_lock);
	fluid_synth_write_s16 (fl_synth, MUSIC_PIPE_FRAMES,
			       buf, 0, 2, buf, 1, 2);
	pthread_mutex_unlock (&fl_lock);

	while (left && fl_pipe_running)
	{
	    ssize_t	n = write (fl_pipe_fd, p, left);

	    if (n > 0)
	    {
		p += n;
		left -= (size_t) n;
		continue;
	    }

	    if (n < 0 && errno == EINTR)
		continue;

	    fprintf (stderr, "I_InitMusic: music pipe closed (%s), "
		     "no more music\n", strerror (errno));
	    fl_pipe_running = 0;
	}
    }

    return NULL;
}


//
// The rate everything downstream is mixed at. audiostream sets this so the
// synth renders at the output rate and nothing has to resample music.
//
static double
I_MusicSampleRate (void)
{
    const char*	rate = getenv ("DOOM_AUDIO_RATE");
    int		hz = rate && *rate ? atoi (rate) : 0;

    return hz >= 8000 && hz <= 48000 ? (double) hz : 44100.0;
}


void I_InitMusic(void)
{
    const char*		soundfont;
    const char*		driver;
    const char*		pipepath;

    soundfont = I_FindSoundFont ();

    if (!soundfont)
    {
	fprintf (stderr, "I_InitMusic: no soundfont found, no music. "
		 "Set DOOM_SOUNDFONT or pass -soundfont <file>.\n");
	return;
    }

    fl_settings = new_fluid_settings ();

    if (!fl_settings)
    {
	fprintf (stderr, "I_InitMusic: could not create synth settings\n");
	return;
    }

    driver = getenv ("DOOM_FLUID_AUDIO_DRIVER");
    fluid_settings_setstr (fl_settings, "audio.driver",
			   driver && *driver ? driver : "pulseaudio");
    fluid_settings_setstr (fl_settings, "audio.pulseaudio.media-role", "game");
    fluid_settings_setnum (fl_settings, "synth.sample-rate",
			   I_MusicSampleRate ());
    fluid_settings_setint (fl_settings, "synth.midi-channels", 16);

    // Load sample data as instruments are actually used. A full General MIDI
    // soundfont is well over a hundred megabytes, and reading all of it up
    // front stalls startup for several seconds before the game appears.
    fluid_settings_setint (fl_settings, "synth.dynamic-sample-loading", 1);

    fl_synth = new_fluid_synth (fl_settings);

    if (!fl_synth)
    {
	fprintf (stderr, "I_InitMusic: could not create synth\n");
	I_ShutdownMusic ();
	return;
    }

    if (fluid_synth_sfload (fl_synth, soundfont, 1) == FLUID_FAILED)
    {
	fprintf (stderr, "I_InitMusic: could not load soundfont [%s]\n",
		 soundfont);
	I_ShutdownMusic ();
	return;
    }

    pipepath = getenv ("DOOM_MUSIC_PIPE");

    if (pipepath && *pipepath)
    {
	// The reader holds a write end of its own, so this does not wait for
	// one to appear.
	fl_pipe_fd = open (pipepath, O_WRONLY);

	if (fl_pipe_fd < 0)
	{
	    fprintf (stderr, "I_InitMusic: could not open %s (%s), "
		     "no music\n", pipepath, strerror (errno));
	    I_ShutdownMusic ();
	    return;
	}

	// A listener closing the far end is not a reason to take the game
	// down with it; the write reports it instead.
	signal (SIGPIPE, SIG_IGN);

	fl_pipe_running = 1;

	if (pthread_create (&fl_pipe_thread, NULL, I_FluidPipeThread, NULL))
	{
	    fprintf (stderr, "I_InitMusic: could not start the music thread "
		     "(%s), no music\n", strerror (errno));
	    fl_pipe_running = 0;
	    I_ShutdownMusic ();
	    return;
	}
    }
    else
    {
	fl_driver = new_fluid_audio_driver (fl_settings, fl_synth);

	if (!fl_driver)
	{
	    fprintf (stderr, "I_InitMusic: no audio output for music\n");
	    I_ShutdownMusic ();
	    return;
	}
    }

    I_FluidSetGain (snd_MusicVolume);

    fprintf (stderr, "I_InitMusic: using soundfont [%s]\n", soundfont);
}


void I_ShutdownMusic(void)
{
    pthread_mutex_lock (&fl_lock);

    if (fl_player)
    {
	fluid_player_stop (fl_player);
	delete_fluid_player (fl_player);
	fl_player = NULL;
    }

    pthread_mutex_unlock (&fl_lock);

    // Outside the lock: the thread takes it for every buffer it renders, so
    // joining while holding it would wait for a thread that is waiting for us.
    // Once it has been joined nothing else touches the synth, and the teardown
    // below needs no lock at all.
    if (fl_pipe_running)
    {
	// The reader keeps draining, so a thread blocked in write returns
	// within a period rather than holding up the exit.
	fl_pipe_running = 0;
	pthread_join (fl_pipe_thread, NULL);
    }

    if (fl_pipe_fd >= 0)
    {
	close (fl_pipe_fd);
	fl_pipe_fd = -1;
    }

    if (fl_driver)
    {
	delete_fluid_audio_driver (fl_driver);
	fl_driver = NULL;
    }

    if (fl_synth)
    {
	delete_fluid_synth (fl_synth);
	fl_synth = NULL;
    }

    if (fl_settings)
    {
	delete_fluid_settings (fl_settings);
	fl_settings = NULL;
    }

    free (song_midi);
    song_midi = NULL;
    song_midi_len = 0;
}


//
// A lump that is already a Standard MIDI File carries its length in its
// chunk headers, which is the only way to recover it here: I_RegisterSong
// is handed a pointer and no size.
//
static size_t I_MidiFileLength (const byte* data)
{
    size_t	pos = 0;
    size_t	len;
    int		ntracks;
    int		i;

    if (memcmp (data, "MThd", 4))
	return 0;

    len = ((size_t)data[4] << 24) | ((size_t)data[5] << 16)
	| ((size_t)data[6] << 8) | data[7];

    if (len < 6)
	return 0;

    ntracks = (data[10] << 8) | data[11];
    pos = 8 + len;

    for (i = 0; i < ntracks; i++)
    {
	if (memcmp (data + pos, "MTrk", 4))
	    return 0;

	len = ((size_t)data[pos + 4] << 24) | ((size_t)data[pos + 5] << 16)
	    | ((size_t)data[pos + 6] << 8) | data[pos + 7];
	pos += 8 + len;
    }

    return pos;
}


int I_RegisterSong(void* data)
{
    size_t	muslen;
    size_t	midlen;

    if (!fl_synth || !data)
	return 1;

    free (song_midi);
    song_midi = NULL;
    song_midi_len = 0;

    muslen = MUS_LumpLength (data);

    if (muslen)
    {
	if (!mus2mid (data, muslen, &song_midi, &song_midi_len))
	{
	    fprintf (stderr, "I_RegisterSong: could not convert MUS\n");
	    song_midi = NULL;
	    song_midi_len = 0;
	}
    }
    else
    {
	// Some PWADs replace the music with plain MIDI lumps.
	midlen = I_MidiFileLength ((const byte *) data);

	if (midlen)
	{
	    song_midi = malloc (midlen);

	    if (song_midi)
	    {
		memcpy (song_midi, data, midlen);
		song_midi_len = midlen;
	    }
	}
	else
	{
	    fprintf (stderr, "I_RegisterSong: unrecognised music lump\n");
	}
    }

    return 1;
}


void I_PlaySong(int handle, int loops)
{
    handle = 0;

    looping = loops;
    musicdies = gametic + TICRATE*30;

    if (!fl_synth || !song_midi)
	return;

    // All of it under the lock, not just the delete: between throwing the old
    // player away and the new one being ready there is no player at all, and
    // the rendering thread must not be looking while that is true.
    pthread_mutex_lock (&fl_lock);

    if (fl_player)
    {
	fluid_player_stop (fl_player);
	delete_fluid_player (fl_player);
	fl_player = NULL;
    }

    fl_player = new_fluid_player (fl_synth);

    if (!fl_player)
    {
	pthread_mutex_unlock (&fl_lock);
	return;
    }

    if (fluid_player_add_mem (fl_player, song_midi, song_midi_len)
	== FLUID_FAILED)
    {
	delete_fluid_player (fl_player);
	fl_player = NULL;
	pthread_mutex_unlock (&fl_lock);
	fprintf (stderr, "I_PlaySong: synth rejected the song\n");
	return;
    }

    fluid_player_set_loop (fl_player, loops ? -1 : 1);
    fluid_player_play (fl_player);

    pthread_mutex_unlock (&fl_lock);
}


void I_PauseSong (int handle)
{
    handle = 0;

    pthread_mutex_lock (&fl_lock);

    if (fl_player)
    {
	fluid_player_stop (fl_player);

	// Stopping mid-note would otherwise leave it sounding.
	if (fl_synth)
	    fluid_synth_all_notes_off (fl_synth, -1);
    }

    pthread_mutex_unlock (&fl_lock);
}


void I_ResumeSong (int handle)
{
    handle = 0;

    pthread_mutex_lock (&fl_lock);

    if (fl_player)
	fluid_player_play (fl_player);

    pthread_mutex_unlock (&fl_lock);
}


void I_StopSong(int handle)
{
    handle = 0;

    looping = 0;
    musicdies = 0;

    pthread_mutex_lock (&fl_lock);

    if (fl_player)
    {
	fluid_player_stop (fl_player);
	delete_fluid_player (fl_player);
	fl_player = NULL;
    }

    if (fl_synth)
    {
	fluid_synth_all_notes_off (fl_synth, -1);
	fluid_synth_all_sounds_off (fl_synth, -1);
    }

    pthread_mutex_unlock (&fl_lock);
}


void I_UnRegisterSong(int handle)
{
    handle = 0;

    free (song_midi);
    song_midi = NULL;
    song_midi_len = 0;
}


// Is the song playing?
int I_QrySongPlaying(int handle)
{
    handle = 0;

    // Reads the player, so it queues behind whoever is replacing it.
    pthread_mutex_lock (&fl_lock);

    if (fl_player)
    {
	int	playing =
	    fluid_player_get_status (fl_player) == FLUID_PLAYER_PLAYING;

	pthread_mutex_unlock (&fl_lock);
	return playing;
    }

    pthread_mutex_unlock (&fl_lock);

    return looping || musicdies > gametic;
}

#else	// !MUSIC_FLUIDSYNTH

void I_InitMusic(void)		{ }
void I_ShutdownMusic(void)	{ }

void I_PlaySong(int handle, int loops)
{
  // UNUSED.
  handle = loops = 0;
  musicdies = gametic + TICRATE*30;
}

void I_PauseSong (int handle)
{
  // UNUSED.
  handle = 0;
}

void I_ResumeSong (int handle)
{
  // UNUSED.
  handle = 0;
}

void I_StopSong(int handle)
{
  // UNUSED.
  handle = 0;
  
  looping = 0;
  musicdies = 0;
}

void I_UnRegisterSong(int handle)
{
  // UNUSED.
  handle = 0;
}

int I_RegisterSong(void* data)
{
  // UNUSED.
  data = NULL;
  
  return 1;
}

// Is the song playing?
int I_QrySongPlaying(int handle)
{
  // UNUSED.
  handle = 0;
  return looping || musicdies > gametic;
}

#endif	// MUSIC_FLUIDSYNTH



//
// Experimental stuff.
// A Linux timer interrupt, for asynchronous
//  sound output.
// I ripped this out of the Timer class in
//  our Difference Engine, including a few
//  SUN remains...
//  
#ifdef sun
    typedef     sigset_t        tSigSet;
#else    
    typedef     int             tSigSet;
#endif


// We might use SIGVTALRM and ITIMER_VIRTUAL, if the process
//  time independend timer happens to get lost due to heavy load.
// SIGALRM and ITIMER_REAL doesn't really work well.
// There are issues with profiling as well.
static int /*__itimer_which*/  itimer = ITIMER_REAL;

static int sig = SIGALRM;

// Interrupt handler.
void I_HandleSoundTimer( int ignore )
{
  // Debug.
  //fprintf( stderr, "%c", '+' ); fflush( stderr );
  
  // Feed sound device if necesary.
  if ( flag )
  {
    // See I_SubmitSound().
    // Write it to DSP device.
    if (audio_fd >= 0
	&& write(audio_fd, mixbuffer, SAMPLECOUNT*BUFMUL) < 0)
      fprintf(stderr, "I_HandleSoundTimer: write to audio device failed\n");

    // Reset flag counter.
    flag = 0;
  }
  else
    return;
  
  // UNUSED, but required.
  ignore = 0;
  return;
}

// Get the interrupt. Set duration in millisecs.
int I_SoundSetTimer( int duration_of_tick )
{
  // Needed for gametick clockwork.
  struct itimerval    value;
  struct itimerval    ovalue;
  struct sigaction    act;
  struct sigaction    oact;

  int res;
  
  // This sets to SA_ONESHOT and SA_NOMASK, thus we can not use it.
  //     signal( _sig, handle_SIG_TICK );
  
  // Now we have to change this attribute for repeated calls.
  act.sa_handler = I_HandleSoundTimer;
#ifndef sun    
  //ac	t.sa_mask = _sig;
#endif
  act.sa_flags = SA_RESTART;
  
  sigaction( sig, &act, &oact );

  value.it_interval.tv_sec    = 0;
  value.it_interval.tv_usec   = duration_of_tick;
  value.it_value.tv_sec       = 0;
  value.it_value.tv_usec      = duration_of_tick;

  // Error is -1.
  res = setitimer( itimer, &value, &ovalue );

  // Debug.
  if ( res == -1 )
    fprintf( stderr, "I_SoundSetTimer: interrupt n.a.\n");
  
  return res;
}


// Remove the interrupt. Set duration to zero.
void I_SoundDelTimer()
{
  // Debug.
  if ( I_SoundSetTimer( 0 ) == -1)
    fprintf( stderr, "I_SoundDelTimer: failed to remove interrupt. Doh!\n");
}
