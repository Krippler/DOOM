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

## Not changed

- Networking (`i_net.c`) is untouched and untested here.
- There is still no music; linuxdoom never implemented any.
- The `-DUSEASM` assembly paths remain off; they are 32-bit x86 only.
- `SNDSERV` is still defined by default, as upstream had it, so sound goes
  through the external `sndserver` helper rather than the in-process OSS mixer.
  The in-process mixer's own fixes are still in place for anyone who builds
  without it.

## Verifying

```
make -C linuxdoom-1.10
Xvfb :99 -screen 0 640x400x8 &
DISPLAY=:99 DOOMWADDIR=/path/to/wads HOME=/tmp linuxdoom-1.10/linux/linuxxdoom -2 -warp 1 1
```

The display has to be 8-bit PseudoColor; see `DOCKER.md` for why.
