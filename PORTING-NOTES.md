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
- **`z_zone.c`, `d_net.c`, `d_main.c`, `i_video.c`** — remaining
  pointer-to-integer casts widened to `intptr_t`/`uintptr_t`.
- **`i_video.c`** — the MIT-SHM check works out whether the display is local by
  cutting the host off the display name, and did it by writing a NUL over the
  `:` of the string `getenv("DISPLAY")` returned. That string is the process's
  own environment, so `DISPLAY=:0` became `DISPLAY=`. Nothing noticed while
  DOOM only ever started once; the WAD menu re-executes the engine, and the
  new process came up with no display. It copies the host out now.
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
FluidSynth runs its own audio thread and its own connection to the sound
system, so the game loop is untouched and music mixes with the effects
outside the process.

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
