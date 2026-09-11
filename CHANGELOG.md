# Changelog

Notable changes to the containerised DOOM. Versions are the image tags
published to `ghcr.io/krippler/doom`, so `1.10.0` here is `:1.10.0` there.
`:latest` always tracks the newest release and `:edge` the tip of `master`.

The version follows the engine this is built from, linuxdoom-1.10.

## [1.10.0] — 2026-09-11

### Added
- Sound effects, through the sound server that shipped with the original
  release, given a PulseAudio backend in place of the `/dev/dsp` one.
- Music. Every music function in the 1997 sources was an empty stub; the WAD's
  MUS lumps are now converted to Standard MIDI and rendered by FluidSynth.
- A container that builds the engine and serves it over noVNC on an 8-bit
  Xvfb, which is the only kind of display the engine supports.
- An Unraid Community Applications template, with PUID/PGID support.

### Fixed
- The 1997 sources did not compile, and did not run once they did. A one-byte
  heap overflow in the IWAD path search, a WAD structure whose layout shifted
  under 64-bit pointers, pointer arrays allocated at half their size, a 4x
  scaler that assumed big-endian and mirrored every pixel pair, undefined
  behaviour in the input queue, and a sound path that called `exit(-1)` on any
  system without OSS. `PORTING-NOTES.md` has the full list.
- Sound effects played at about an eighth of their intended volume: the game
  passes a 0..15 volume to mixers that index a 0..127 table.
