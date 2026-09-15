# Getting linuxdoom-1.10 to build and run on a modern 64-bit Linux

The 1997 release targets 32-bit Linux and a 1990s toolchain. These are the
changes needed to make it compile, run and render correctly on x86-64 with
current gcc and glibc. Behaviour is otherwise left alone.

## Would not compile

- **`i_video.c`** — `#include <errnos.h>` is a typo for `<errno.h>`.
- **`i_video.c`, `i_sound.c`** — `extern int errno;` inside a function. `errno`
  has been a macro for decades; the declaration made the link fail with
  *"TLS definition in libc.so.6 ... mismatches non-TLS reference"*.
- **`Makefile`** — dropped `-lnsl` (those symbols moved into libc) and the
  `/usr/X11R6/lib` path. `CC`/`CFLAGS`/`LDFLAGS` are now overridable, and the
  object directory is created on demand so a clean checkout builds with `make`.
- **`m_misc.c`** — the config table stored string defaults by casting `char*`
  into an `int` field. On a 64-bit target that is a narrowing conversion of a
  non-constant, which is a hard error in a static initialiser. The table now
  carries an explicit type tag and a separate `char*` field, so
  `M_SaveDefaults`/`M_LoadDefaults` no longer have to guess a value's type from
  whether it happens to fall in `-0xfff..0xfff`.

## Crashed at startup on 64-bit

- **`r_data.c`** — `maptexture_t` is read straight out of the WAD, but its
  obsolete `columndirectory` field was declared `void**`. At 8 bytes instead of
  the 4 on disk it shifted `patchcount` and `patches` out of place, so texture
  loading walked off into garbage. It is now a fixed-width `int32_t`, with
  `_Static_assert`s pinning every field offset to the file format.
- **`r_data.c`, `p_setup.c`** — arrays of pointers were allocated as
  `count * 4`, which is half the space needed when a pointer is 8 bytes. All of
  these now use `sizeof(*ptr)`.
- **`d_main.c`** — `IdentifyVersion` hand-counted the length of each IWAD path
  and counted `"doomu.wad"` as 8 characters. The resulting one-byte heap
  overflow is caught by glibc's fortify checks and aborted the process before
  the game drew anything. Replaced with a helper that measures the strings.
- **`r_data.c`, `r_draw.c`** — `(byte*)(((int)p + 255) & ~0xff)` to align an
  allocation truncates the pointer to 32 bits. Now `intptr_t`.

## Wrong behaviour

- **`i_sound.c`** — when `/dev/dsp` could not be opened the code printed a
  warning and then issued `ioctl`s on the failed descriptor anyway; the "safe"
  `myioctl` wrapper responds to failure with `exit(-1)`. Since OSS no longer
  exists on modern kernels, *every* start ended there. The device is now
  configured only if it opened.
- **`i_sound.c`** — `audio_fd` was an uninitialised global, so it was 0, and
  `I_SubmitSound` writes to it every frame. With no audio device that meant
  2 KB of PCM written to file descriptor 0 about 35 times a second. It now
  starts at −1 and every read/write/close is guarded.
- **`i_sound.c`** — aliased sound effects (the chaingun reusing the pistol
  sound) looked up their length with `(S_sfx[i].link - S_sfx) / sizeof(sfxinfo_t)`.
  A pointer difference is already an element count, so this was near enough
  always 0 and every linked sound got the wrong length.
- **`i_video.c`** — the 4× scaler (`-4`) is written around a 65536-entry table
  of `double`s, and both the table and its lookup assumed big-endian byte
  order. On x86 that mirrored every pair of pixels; text in the status bar came
  out backwards. Both halves now agree with the host's byte order.
- **`d_main.c`, `d_net.c`** — `eventhead = (++eventhead) & (MAXEVENTS-1)`
  modifies the variable twice with no sequence point between, which is
  undefined. Rewritten as `(eventhead + 1) & ...`.
- **`g_game.c`, `d_main.c`** — a demo lump from a different engine version made
  `G_DoPlayDemo` return with no level loaded, after which the play loop ticked
  a player with no `mobj` and segfaulted. This is reachable from `-playdemo`
  and `-timedemo`, and matters because released IWADs carry v1.9 demos while
  this source is v1.10. It now falls back to the title screen.
- **`p_saveg.c`** — savegames round-trip array indices through pointer fields;
  the `(int)` casts are now `intptr_t` so nothing is truncated.
- **`d_net.c`, `d_main.c`, `i_system.c`** — `TryRunTics`' wait-for-the-next-tic
  loop has no sleep in the 1997 sources, so the engine spun on the clock for
  five sixths of every tic: 97% of a core to draw a 320x168 view. That was free
  on a machine doing nothing else, and here it starves the X server, the VNC
  server reading that server's framebuffer, the Python websocket proxy, the
  mixer and the synth — which is what a stuttering picture turned out to be.
  A new `I_Sleep` in the platform layer, called with one millisecond, takes it
  to 5%: 35 times finer than the tic being waited for, so the timing is
  unchanged (156 ms keypress-to-picture either way) while a container held to
  one core goes from 27 frames a second reaching the browser to 35. The melt
  between screens had the same spin in its own loop and gets the same fix.
  Since 1.10.28 the wait is split rather than purely slept: a millisecond at a
  time while there is time to spare, holding the CPU through the last few
  milliseconds, sized to how late this machine has actually handed the CPU
  back. Sleeping only is free but depends on the kernel being punctual, and in
  the field it was 43 ms late; spinning only is punctual but costs the core
  this note is about.
- **`i_system.c`, `m_misc.c`** — the zone allocator was getting two megabytes.
  `mb_used` is initialised to 6, but `M_LoadDefaults` runs before `Z_Init` and
  overwrites it from the config table, whose default was 2; every config file
  written by this container therefore says `mb_used 2`. The zone is the cache
  for every texture, sprite and level structure in play, so at that size it
  spends the game purging what it is about to need and reading it back from the
  WAD — 230 lumps and 825 KB re-read in three minutes of the shareware demos,
  and `W_ReadLump` blocks the thread that draws the frame. Now 32 MB, applied
  as a floor because the old value is already written into people's config
  files: 33 re-reads over the same three minutes, which is first-time loads
  and nothing more.
- **`z_zone.c`, `d_net.c`, `d_main.c`, `i_video.c`** — remaining
  pointer-to-integer casts widened to `intptr_t`/`uintptr_t`.
- **`i_video.c`** — the MIT-SHM check works out whether the display is local by
  cutting the host off the display name, and did it by writing a NUL over the
  `:` of the string `getenv("DISPLAY")` returned. That string is the process's
  own environment, so `DISPLAY=:0` became `DISPLAY=`. Nothing noticed while
  DOOM only ever started once; the WAD menu re-executes the engine, and the
  new process came up with no display. It copies the host out now.
- **`i_video.c`** — `I_FinishUpdate` asked `XShmPutImage` for a completion event
  and blocked until the server sent it. That guards against overwriting the
  shared image while the server is still reading it, which cannot happen here:
  the next frame is a tic away and the copy takes microseconds. What it did
  instead was couple the engine's frame rate to how busy the X server was, and
  the X server is busy because x11vnc is polling the same framebuffer — 74 ms
  in that wait for a single frame, measured in a container in use, against
  under 2 ms of actual drawing. The event is no longer requested and the wait
  is gone. It was also the only thing pumping input during the wait, which
  `I_StartTic` does every tic through `NetUpdate` regardless.
- **`i_video.c`** — `I_ShutdownGraphics` detached the shared memory segment
  unconditionally: with no MIT-SHM extension it detached one that was never
  attached, and on a startup failure before `I_InitGraphics` it passed a null
  `Display*` to Xlib and faulted, which turned a clean `I_Error` into a
  segfault.
- **`i_video.c`** — mouse motion was discarded whenever the pointer shared a
  row *or* column with the centre of the window, because the filter that
  throws away the engine's own re-centring warp tested both coordinates with
  `&&`. Moving straight across the middle of the screen did nothing.
- **`i_video.c`** — `Button1Mask` is `1<<8`, and all three input handlers used
  it raw while buttons two and three were correctly converted to bits 1 and 2.
  The release path computed `(state & Button1Mask) ^ 1` = 257, whose bit 0 is
  still set, so `mousebuttons[0]` stayed true and the weapon kept firing after
  the button came up; the motion path reported a bare 256, whose bit 0 is
  clear, so moving the mouse while holding fire turned it off. Holding still
  stuck on, holding and moving stopped: the two complaints were the same bug
  seen from opposite ends.
- **`i_video.c`** — nothing ever set the X input focus. A window manager would
  normally do it; there is none in the container, so X left the focus at
  `PointerRoot` and delivered keystrokes to whichever window the pointer was
  over. `XSetInputFocus` on the game window now settles it.
- **`g_game.c`** — `G_Responder` assigned each mouse event's movement to
  `mousex`/`mousey` rather than adding it, so of all the mouse events arriving
  inside one 35 Hz tic only the last was used. Every mouse reports faster than
  the tic rate, so most movement was silently dropped; it is summed now, and
  `G_BuildTiccmd` still zeroes the pair once it has built the command.
- The browser client reports the pointer by walking it around the screen and
  re-centring only near an edge, rather than always naming centre-plus-delta.
  The latter reads better but names the same coordinates for the whole of a
  steady drag, and x11vnc drops a pointer report that does not move the
  pointer — so most of the movement never reached the engine. The engine's
  "ignore a motion event landing exactly on the centre" rule is what makes the
  re-centring free: it costs no turn and re-bases the next delta.
- **`i_video.c`** — after re-centring the pointer the engine left `lastmousex`
  and `lastmousey` pointing at where the last event landed, and corrected them
  only when the warp's own motion event came back. That event is queued behind
  anything already received, and a client reporting absolute positions gets
  several in per tic, so every report after the first was measured against the
  previous report rather than the centre. A steady drag arrived as a run of
  near-cancelling deltas. It records the centre when it warps now.
- **`i_video.c`** — the pointer was re-centred only once a tic, and only with
  the grab on, which was not the default. Ungrabbed, it walked to an edge of
  the screen and stopped there, and mouse look stopped with it. It is now
  re-centred immediately after each motion the engine acts on, so every one is
  measured from the middle however many arrive between tics.
- **`d_main.c`** — the game is identified from the IWAD's filename, and the
  four episode version was `doomu.wad` in 1997. It has not been sold under that
  name in decades: every re-release installs The Ultimate Doom as `doom.wad`,
  which this code reads as three-episode registered Doom, leaving *Thy Flesh
  Consumed* in the WAD with no way to reach it. It now reads the IWAD's lump
  directory and identifies the game by whether E4M1 is in it. Reading the file
  rather than asking `W_CheckNumForName` matters: the latter searches every
  loaded WAD, so a PWAD adding an E4M1 would have been mistaken for retail.
- **`g_game.c`** — every released IWAD's demos are recorded by version 1.9, and
  this source announces itself as 1.10, so `G_DoPlayDemo` rejected all of them:
  no attract loop, and "Demo is from a different game version!" on the console
  for each attempt. 1.10 is 1.9's game code with the version bumped, so the
  demos play back correctly and 109 is accepted.
- **`s_sound.c`** — the guard around "16bit and not pre-cached - wtf?" is
  spelled `SNDSRV`, while the build and every other file spell it `SNDSERV`, so
  it never compiled out and the warning printed for every sound played through
  the sound server.

## Sound server (`sndserv/`)

The engine drives sound through a separate `sndserver` process (`SNDSERV` is
defined by default, and the directory's own README explains that the separate
process gave the best results). It needed the same treatment as the engine:

- **`linux.c`** — `extern int errno` broke the link, `<sys/ioctl.h>` was
  missing so `ioctl` was implicitly declared, and a failed `/dev/dsp` open was
  followed by `ioctl`s on the bad descriptor, which `myioctl` answers with
  `exit(-1)`. It also ignored its own `samplerate` argument.
- **`soundsrv.c`** — `<string.h>` was missing, so `strlen` and `strcmp` were
  implicitly declared; the IWAD paths repeated the `"doomu.wad"` off-by-one;
  and aliased sounds divided a pointer difference by the element size, as in
  the engine. The server now also recognises `tnt.wad` and `plutonia.wad`,
  which it previously refused to load sounds from.
- **`i_sound.c`** (engine side) — the server is spoken to over a pipe, so if
  it exited the engine's next write raised `SIGPIPE` and took the game down.
  `SIGPIPE` is now ignored; losing sound is enough.

### PulseAudio output

`sndserv/linux.c` writes to `/dev/dsp`, which no current kernel provides.
PulseAudio's OSS shim (`padsp`) was removed upstream in PulseAudio 16 and is
no longer packaged, and `aoss` needs a real ALSA stack and fails the format
negotiation this code does at startup.

`sndserv/pulse.c` is a new platform layer implementing the same five-function
interface against libpulse's simple API, and is what `make -C sndserv` builds
by default. Like the OSS version its write blocks until the server accepts
more audio, which is what paces the sound server's main loop. The original
backend is still there: `make -C sndserv SNDBACKEND=oss`.

### PCM output, and getting the sound out of the container

A PulseAudio backend only helps if there is a PulseAudio server to talk to.
The container has none, and cannot practically be given one: the only packaged
`pulseaudio` pulls in systemd, GStreamer, cairo, ffmpeg's codecs and ICU --
172 packages, most of what the image goes out of its way to strip -- to do a
job that is, here, adding two streams together. Nor does the picture's own
transport help, because VNC carries a framebuffer and nothing else.

So there is a program of its own for that case:

- **`audiostream/audiostream.c`** mixes the effects itself, reads the engine's
  music pipe at the output rate, resamples the effects up by the whole-number
  factor between 11025 Hz and the output rate, sums the two with clipping, and
  writes 16-bit stereo PCM to whoever has connected, after a short header
  naming the rate. It works to an absolute schedule off `CLOCK_MONOTONIC`, one
  256-frame period at a time.

  The effects are mixed by the 1997 mixer, not a new one: `audiostream`'s
  Makefile compiles `sndserv/soundsrv.c` with `-DSNDMIX_LIB`, which takes that
  file's `main()` out and leaves `grabdata`, `initdata`, `addsfx` and `mix`
  behind, and the engine sends its `'p'` commands to a UNIX socket instead of
  to a child process's pipe. So there is no separate sound server on this path
  at all, and a sound asked for is mixed into the next period -- twelve
  milliseconds of output -- rather than into a 46 ms block that then has to
  wait its turn in a pipe. Doing it the other way first cost about 90 ms:
  sound lagged the picture by ~105 ms with the separate server and by ~12 ms
  without it, measured in the browser off the audio clock.

  That schedule is also the pacing for everything upstream. The music is
  written blocking into a pipe sized to hold about 93 ms, so the synth cannot
  run ahead of a reader that only takes a period at a time -- exactly the job
  the blocking write to `/dev/dsp` did in 1997. The pipe is read whether or
  not anyone is listening, because a producer blocked on a full pipe would
  block the engine behind it.

- **`i_sound.c`** grew a matching path for music. With `DOOM_MUSIC_PIPE` set
  it creates no FluidSynth audio driver at all and pulls the synth from a
  thread of its own with `fluid_synth_write_s16`, writing into the pipe. The
  blocking write keeps that thread in time, and also keeps the *music* in
  time: FluidSynth's player is clocked by the samples the synth renders rather
  than by a wall clock, so a thread that renders only as fast as the pipe
  drains plays at exactly the right speed. Without the variable it creates a
  driver as before, so sharing the host's PulseAudio socket still works
  unchanged.

- **`-8to24` is load-bearing, and it is where the bandwidth goes.** x11vnc is
  given a depth-8 display and presents it to clients as 32-bit truecolor, which
  multiplies what has to be compressed by four -- its own log shows
  `rfb_fb_bytes_per_line: 2560` for a 640-wide screen. Turning it off does cut
  the wire traffic from 1268 KB/s to 457, at the same 35 frames a second, which
  looks like the obvious win and is not one: noVNC has no colour-map support
  and renders the result as a green and black mess. The screenshot is
  unambiguous. So the picture costs what it costs. `DOOM_SCALE=1` does measure
  smaller -- 474 KB/s against 1265, for a quarter of the pixels -- but smaller
  is not the same as faster: tried in the field it delivered fewer frames, not
  more, which is what ended the idea that the picture was bandwidth-limited at
  all.

- **`docker/doom-wsproxy.py`** is websockify with the sound alongside the
  picture on the one port, so no second port has to be published. websockify
  can already route by a token in the query string but refuses a connection
  without one, which would break stock `/vnc.html`; the first change is that a
  missing token means the screen.

  The second is `Cache-Control: no-cache` on everything it serves. websockify
  hands the client to `SimpleHTTPRequestHandler`, which sends `Last-Modified`
  and nothing else, and a browser given no freshness may pick one itself --
  so an updated image could be serving a client the browser had decided not to
  ask about. That is invisible from inside: the container's log shows a healthy
  server talking to nobody. `no-cache` means revalidate, not do not store, so
  the usual answer is a 304.

- **`docker/play.html`** plays the stream through a ring buffer
  (`doom-ring.js`). The container sends at its own real-time rate and the
  sound card consumes at its own, which are never quite the same, so it pads
  when it runs dry, drops the oldest frame when it runs long, and resamples to
  whatever rate the `AudioContext` turns out to be. Scheduling a queue of
  buffers instead would drift until it stuttered or fell behind.

  How much it holds is adaptive, because how much a machine needs depends on
  the machine, the browser and what else the page is doing. It starts at 25 ms
  and grows by 20 ms whenever it runs dry, giving 10 ms back after every eight
  seconds that do not. Everything it holds is delay between pulling a trigger
  and hearing it, and a fixed figure is either too much for everyone or too
  little for somebody. The two rates have to be unequal or the target never
  settles: set to cancel exactly, a machine that stumbles every ten seconds
  sits still and underruns for ever.

  Holding the buffer *at* that target is a separate problem, because the two
  clocks do not agree -- the container sends on its own schedule, the browser
  plays on its sound card's -- and every block the browser is too busy to
  fetch leaves more sound in hand than was asked for. With nothing to remove
  it, that walked 35 ms of delay up to 195 over less than a minute. So the
  playback rate is nudged a fraction either side of true, up to two per cent,
  which corrects about 20 ms a second and cannot be heard on this material.
  Dropping the frames outright would be quicker and would click.

  Two things drive that buffer. An `AudioWorklet` (`doom-audio.js`) where the
  browser has one, and a `ScriptProcessorNode` where it does not -- which is
  most of the time, because `AudioWorklet` is gated on a secure context and
  this page is normally served over plain HTTP from a machine on the network.
  `ctx.audioWorklet` is absent there while the `AudioWorkletNode` constructor
  is present, so a support check that tests for the constructor passes and the
  `addModule` call then throws.

  `doom-ring.js` assigns its class onto `globalThis` rather than exporting it,
  because it is loaded two ways: as an ordinary script by the page, and as a
  worklet module into the `AudioWorkletGlobalScope`, which does not share the
  page's. A module export would only reach one of them, and the point of the
  file is that there is one copy of the algorithm rather than two that drift.

### Effect volume

`s_sound.c` passed `I_StartSound` and `I_UpdateSoundParams` a volume taken
from `snd_SfxVolume`, which runs 0..15, but both mixers index a volume lookup
table running 0..127. Every effect therefore played at roughly an eighth of
its intended amplitude. `m_menu.c` still carries the commented-out `*8` where
the scaling used to be. Measured off a PulseAudio null sink, a pistol shot at
the default volume setting went from a peak of 1693 to 14114 once the scaling
was restored.

## Music

Every music function in `i_sound.c` was an empty stub, and nothing ever
called `I_InitMusic` — there was no reason to, since it did nothing.

The WADs store music as MUS lumps, a trimmed-down MIDI with delays counted in
140 Hz ticks. `mus2mid.c` converts one into a Standard MIDI File in memory,
and `i_sound.c` renders it with FluidSynth against a General MIDI soundfont.
FluidSynth renders on a thread of its own, so the game loop is untouched and
music mixes with the effects outside the process -- in the host's sound server
when one is shared with the container, and otherwise in `audiostream`, which
is covered above.

Details worth knowing:

- `I_RegisterSong` is handed a pointer with no length. A MUS lump's header
  carries its own score length, which is what the converter uses. Lumps that
  are already Standard MIDI — some PWADs replace the music that way — have
  their length recovered by walking their chunk headers instead.
- MUS reserves channel 15 for percussion where MIDI uses channel 9, so
  channels are remapped as they are first used.
- The MIDI is written with a division of 70 ticks per quarter note and the
  default tempo of 500000 microseconds per quarter, which is exactly the
  140 ticks per second MUS counts in.
- `I_PauseSong` stops the player and sends an all-notes-off, otherwise a note
  sounding at that moment would hang.
- FluidSynth is set to load sample data on demand
  (`synth.dynamic-sample-loading`). A full General MIDI soundfont is well
  over a hundred megabytes and reading all of it at startup stalled the game
  for ten seconds before anything appeared; on demand it is a tenth of a
  second, and about 20 MB rather than 140 MB of resident memory, with
  identical output.
- Music volume, like effect volume, arrives on DOOM's 0..15 scale;
  `S_SetMusicVolume` also pokes 127 through before setting the real value, so
  the value is clamped before being turned into a synth gain.

Build with `make MUSIC=none` to get the original silent stubs back.

## The delay between pressing a key and seeing it

**About 48 ms**, and most of that is the engine's own frame rate. An earlier
version of this section said 140 ms, which was wrong, and wrong in a way worth
recording because it nearly bought a rewrite of the video path.

That figure came from timing a keypress to the muzzle flash. The pistol spends
four tics in `S_PISTOL1` doing nothing at all before `S_PISTOL2` runs
`A_FirePistol` -- look at `info.c` -- so 114 ms of what was being called delay
is the game deliberately animating the weapon. Timed against the menu key,
which `M_Responder` acts on the moment it arrives, the same container answers
in 48 ms: roughly one tic of engine (28 ms), a browser repaint (11 ms), and
the trip there and back.

So there is nothing much to win here. 35 frames a second is 28 ms between
them, and no amount of transport work makes a frame that has not been drawn
yet arrive sooner.

The measurements below were made while the 140 ms figure was still believed,
and they still say something useful: every one of them ruled out a suspect,
and none of them moved the number, which in hindsight is exactly what should
have been expected of a number that was mostly weapon animation.

| Suspected | Measured |
| --- | --- |
| The browser drawing | 11 ms of the total |
| The test harness injecting keys | 1.2 ms |
| x11vnc's `-nap`, `-wait`, `-defer` | 20 → 5 → 1/0 ms: no change outside noise |
| x11vnc's `-threads` | no change |
| `-8to24 poll` | no change (the help says it is ignored on a depth 8 display) |
| Pixel volume | `DOOM_SCALE=1` is a quarter the pixels: 139 vs 143 ms |
| The audio stream sharing the proxy | 143 with it, 145 without |
| Throughput | 35 frames a second arrive, 2 ms apart in pairs |

None of them was ever going to move it, because what they were being measured
against was mostly the pistol's wind-up.

That replacement was then written, to be sure: the engine's frames pushed
down the WebSocket beside the sound, x11vnc dropped to `-nofb` and carrying
only the keyboard and mouse, the palette applied in the browser. It works, and
it is exactly as fast -- 48 ms either way -- while sending 1382 KB/s against
VNC's 941 when walking, because its tiles go raw where VNC compresses. It is
on the `claude/video-stream-experiment` branch rather than here.

What is actually worth attention is the other half: the sound arrives about
150 ms after the picture it belongs to, and that gap is real.

It is also stubborn. It is spread across six stages -- the sound server's pipe
(~50 ms), the block it mixes into (~23 ms), the mixer's period (12 ms), the
browser's ring buffer (~25-40 ms), the `ScriptProcessorNode` (~46 ms) and the
output device (~32 ms) -- with no one of them dominant, so each is worth
10-20 ms at best, and two of the three ways to take it cost more than they
save. A smaller mixer period triples the number of periods that arrive short
of sound; a smaller `ScriptProcessorNode` buffer halves its own 46 ms and is
then answered by the ring buffer growing its target to 125 ms.

The instructive failure was the transport. The pipe between the sound server
and the mixer is the largest single stage, and a pipe cannot be smaller than a
page -- 4096 bytes, 93 ms of 11025 Hz stereo. A unix socket can: asked for
2048 it holds 1024, which is 23 ms. Both ends have to be told, because the
sender blocks on its own `SO_SNDBUF` and the default is about 42 KB, a full
second of this stream; setting only the receiver's buys a second of delay and
no complaint from anything. Set properly on both, it works, it runs clean, and
it makes **no measurable difference to the total**. Seventy milliseconds came
out of the stage that was supposed to be holding them and the sound arrived no
sooner, which means the buffering that matters is not where the queue depth
says it is. Whoever picks this up next should find out where before changing
anything.

## The stutter while the fire button is down

**x11vnc holds the picture back for about 300 ms at a time whenever a mouse
button is held.** It ships with `-wireframe` and `-scrollcopyrect` on, and both
are desktop features: they watch for a window being dragged, or a pane
scrolled, while a button is down, and keep the screen back while they decide
what is moving. A game holds the fire button down. There is one window here, it
never moves, and nothing scrolls.

The 300 ms is `t2` of `-wireframe`'s own default timing string,
`0.15+0.30+5.0+0.125` -- by x11vnc's documentation, "how long to wait for the
window to start moving" after a button goes down. It repaints nothing while it
waits. Every field report of this clustered between 273 and 345 ms.

Measured with the fire button held and the player turning, 80 seconds each:

| | answers | KB/s | median | p90 | p99 | worst | over 200 ms |
| --- | --- | --- | --- | --- | --- | --- | --- |
| both on (x11vnc's default) | 1271 | 257 | 41 ms | 62 | 91 | 118 | 0 |
| `-nowireframe` only | 1079 | 250 | 43 ms | 62 | 94 | 118 | 0 |
| `-noscrollcopyrect` only | 1029 | 121 | 15 ms | 140 | 306 | 341 | 41 |
| **both off** | **2774** | **554** | **12 ms** | **15** | **24** | **44** | **0** |

The same run with no button held is 2752 answers at 539 KB/s and a 12 ms
median, so the cost appears only when someone fires. The two do different
damage, which is why neither alone helped: `-scrollcopyrect` slows everything
evenly and hides the other, and `-wireframe` is what produces the long holds.
Turn off only the scroll one and 41 stalls appear in 80 seconds, worst 341 ms.

Both are off by default now. `DOOM_VNC_ARGS=-wireframe` puts one back.

### What it was not

Each of these was suspected, measured, and in most cases announced as the
answer before the measurement contradicted it. They are listed because the
next person will suspect them too.

| Suspected | Measured |
| --- | --- |
| The browser being slow to ask | It asks within 1 ms and holds a finished picture for 0 |
| noVNC's JPEG path (base64 into an `<img>`) | 0 ms of hold across 9026 pictures in the field |
| websockify's pure-Python relay | Identical stalls, same millisecond, on a direct socket and through the proxy |
| The network to the browser | The stalls are there with no browser and no network in the path |
| CPU contention | Pinned to one core against a busy process: 2225 ms of run-queue wait per 5 s, **no stall over 200 ms** |
| A cgroup CPU quota | 400 of 567 periods throttled, worst answer 99 ms, no stall |
| `-8to24` | Six stalls against four, then six against three, measured concurrently |
| X DAMAGE (`-noxdamage`) | No change: 33 moving stalls with it off |
| Nagle on the socket to x11vnc | websockify already sets `TCP_NODELAY` both ways |
| The audio stream competing | Identical distributions with the audio socket open and closed |
| websockify's missing numpy | It only unmasks the browser's 10-byte requests, never the picture |
| Memory reclaim or disk | 48 major faults and 0 block-IO waits on the machine that stutters |
| x11vnc eating pointer input before repolling | Its manual describes exactly this; 120 pointer events a second changed nothing |

### Why it took eleven releases

Nothing in the lab ever held a mouse button. `doom-probe` sends no input by
design -- it would be playing the game for whoever is at the screen -- and the
attract demo has no pointer at all. So the fault could not occur here, and
every measurement came back clean while the field kept stuttering. The first
local reproduction came from a client that held button 1 down, and only after
`-noxdamage` failed and forced a look at what else x11vnc does with a mouse.

The report that turned out to be exactly right was the first one: *"happens
often when opening doors or encountering new enemies"* -- which is to say, when
you are firing.

### What the measuring taught

Four instruments in this repository exist because a confident answer turned out
to be an artefact of how it was measured.

- **A silence is not a stall.** VNC answers only when something has changed, so
  a still screen goes quiet for as long as it likes and that is the protocol
  working. Every stall reported here for two releases was the attract demo
  standing still: five of them, 233 to 1612 ms, each 98 to 100 per cent inside
  a stretch where the screen was not changing, each ending the millisecond it
  changed again. `doom-probe` reads the framebuffer 100 times a second now and
  labels each silence.
- **The change that ends a silence is not motion during it.** Counting it
  turned "still for 1.4 seconds" into "moving, 1 change" and reversed the
  verdict on silences a previous run had called correctly.
- **Measured in sequence, the second thing measured wears the minute.** Timing
  x11vnc and then the proxy said the proxy was twenty times worse; timing the
  proxy and then x11vnc moved the stall to x11vnc. Splitting each into halves
  did not fix it, because both halves still ran in the same order. Run in two
  threads at once, the same stalls appear in both streams to the millisecond.
- **`schedstat` cannot see a process that is blocked rather than waiting.** Its
  second field counts run-queue time only, so every "not CPU starvation"
  conclusion drawn from it was blind to uninterruptible sleep. That is worth
  knowing before trusting the `waiting for a CPU` lines in the container log,
  which are real but were never this.

## Added

Three things the 1997 release had no way to do, all reachable from
**Options → Setup**:

- **Controls** (`m_menu.c`) rebinds the eleven movement, action and menu keys. The
  engine already kept them in the `key_*` globals that `M_LoadDefaults`
  reads and writes, so a binding survives a restart with no new plumbing.
- **Mouse** (`m_menu.c`) turns the mouse on and off, assigns its buttons, and
  toggles a pointer grab (`I_SetMouseGrab`, `XGrabPointer`). The window's
  event mask had the pointer events commented out; they are back, and gated
  on `usemouse` so nothing changes when the mouse is off.
- **A second menu key** (`m_menu.c`, `g_game.c`, `m_misc.c`). The menu key is
  hardcoded as Escape throughout, which is awkward in a browser: pointer lock
  gives Escape to the browser. `key_menu` is a saved binding that defaults to
  backquote -- the game uses that key for nothing and no browser claims it --
  and `M_Responder` turns it into Escape early enough that every test for
  Escape downstream keeps working unchanged, but late enough that typing a
  savegame name is unaffected. Escape is still read directly, so this only
  ever adds a key. A config written before the default changed carries the
  old no-op value of Escape; `M_Init` promotes that to backquote.
- **Load WAD** (`m_menu.c`) lists the `.wad` files in `$DOOM_WADPATH`
  (or `$DOOMWADDIR`, or the current directory) and loads the one you pick,
  reading the four byte signature to tell an IWAD from a PWAD.

  Nothing about the loaded WADs can change while the engine runs — textures,
  sprites, the sound cache and every zone allocation are built once at
  startup — so this re-executes the engine with the file appended to its own
  arguments. `d_main.c` gained `-iwad` to receive it, which the original had
  no equivalent of: it searched fixed filenames in `$DOOMWADDIR` and took
  whichever it found first.

The menu structure grew two fields: a per-menu line height, and an optional
`text` string on each item.

The original draws every menu item from a graphic lump 15 pixels tall on
16-pixel rows, and there are no lumps for words it never shipped. The added
pages are therefore drawn with `M_WriteText` in the 7-pixel `hu_font`, which
left SETUP a visibly different size from the items above it on the same page.
`M_Drawer` now draws `text` when an item has it and falls back to the lump
otherwise, so a menu can be either; the original five were converted, and all
of them moved to 13-pixel rows.

That is also what makes the added pages fit: they need eleven rows above a
status bar that only redraws when it is marked dirty, so anything drawn below
y=168 smears and stays there. The skull cursor is 19 pixels tall against a
7-pixel row, so on a text menu it is centred on the row rather than hung at
the lump's -5 offset.

- **`g_game.c`** — `forward += mousey` is vanilla: the mouse's Y axis walked
  the player, since the engine has no vertical aiming to spend it on. It is
  behind `novert`, defaulting to off, the way later ports settled the same
  question.

- **`r_main.c`, `m_menu.c`** — `R_SetViewSize` took the screen size on trust.
  It is reached from the config file as well as from the menu, and the view
  height is computed straight from it: above 11 the view is taller than the
  200 line screen and the renderer writes past the end of the framebuffer —
  `R_DrawColumn: 206 to 219 at 178` at 12, a segfault at 13. Clamped to the
  range the menu can express, in both places, so a bad line in a text file
  cannot stop the game starting.

- **`m_menu.c`, `r_main.c`, `r_draw.c`** — the Options menu's detail toggle
  flipped `detailLevel` and then returned without applying it, above a comment
  reading "FIXME - does not work". The value was still saved, and applied on
  the next start, where `R_DrawColumnLow` range checks `dc_x` against
  `SCREENWIDTH` and then doubles it to index `columnofs` twice — so a column
  up to 319 passes the check and reads entry 638. Wild destination pointers,
  and a segfault a few frames later. The menu leaves the setting alone now,
  `R_SetViewSize` refuses the mode outright, a stale value is corrected on
  load, and the check is against half the width where it belongs.

## Not changed

- Networking (`i_net.c`) is untouched and untested here.
- The `-DUSEASM` assembly paths remain off; they are 32-bit x86 only.
- `SNDSERV` is still defined by default, as upstream had it, so sound goes
  through the external `sndserver` helper rather than the in-process OSS mixer.
  The in-process mixer's own fixes are still in place for anyone who builds
  without it.

## Verifying

```
make -C linuxdoom-1.10
make -C sndserv
Xvfb :99 -screen 0 640x400x8 &
DISPLAY=:99 DOOMWADDIR=/path/to/wads HOME=/tmp linuxdoom-1.10/linux/linuxxdoom -2 -warp 1 1
```

The display has to be 8-bit PseudoColor; see `DOCKER.md` for why. Sound needs
a reachable PulseAudio server, the sound server on `$DOOMWADDIR/sndserver`,
and for music a General MIDI soundfont (`-soundfont` or `DOOM_SOUNDFONT`).
