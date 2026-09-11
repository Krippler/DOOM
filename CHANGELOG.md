# Changelog

Notable changes to the containerised DOOM. Versions are the image tags
published to `ghcr.io/krippler/doom`, so `1.10.0` here is `:1.10.0` there.
`:latest` always tracks the newest release and `:edge` the tip of `master`.

The version follows the engine this is built from, linuxdoom-1.10.

## [Unreleased]

### Added
- The shareware IWAD ships in the image, so the container plays with nothing
  mounted — `docker run -p 6080:6080 ghcr.io/krippler/doom` is now the whole
  of it. Episode 1, nine levels, id's file under their shareware terms. A
  mounted IWAD still takes precedence, the entrypoint verifies the bundled
  file's checksum before using it, and commercial game data is still yours to
  supply. Adds 6 MB to the image.
- `play.html`, a browser client that captures the mouse. noVNC is a remote
  desktop client and reports where the pointer *is*; a game needs to know how
  far it *moved*. This page locks the pointer instead and sends movement, so
  turning never runs out of screen and the cursor cannot slide off into the
  rest of your desktop. It is what the WebUI and the printed URL now open;
  plain noVNC is still served at `/vnc.html`.

### Fixed
- Mouse look stopped partway through a turn. The pointer was only re-centred
  while the grab was on, and the grab was off by default, so the pointer
  walked to an edge of the screen and stayed there — measured after one sweep
  it sat at x=639 of 640. It is now re-centred after every motion the engine
  acts on, and the grab defaults on.
- The game never claimed the X input focus. With no window manager in the
  container, X leaves the focus at `PointerRoot`, which delivers keystrokes to
  whichever window the pointer is over, so the keyboard depended on where the
  pointer had drifted to.

### Changed
- Added a README. GitHub was showing `README.TXT` as the landing page, which
  is id Software's note from the 1997 source drop: accurate about the code it
  shipped with, and silent on the port, the container, the sound and music, or
  how to run any of it. `README.TXT` is left as it was, as the historical
  document it is.
- The Unraid template and Community Applications profile mention the in-game
  Setup menus, and the template's change list covers 1.10.1.
- `DOCKER.md` leads with pulling the published image rather than building one.

## [1.10.1] — 2026-09-11

### Added
- In-game **Options → Setup**, with three pages the 1997 release had no
  equivalent of:
  - **Controls** rebinds the ten movement and action keys. Bindings are
    written to `.doomrc` and survive a restart.
  - **Mouse** turns the mouse on and off, assigns its buttons, and toggles a
    pointer grab that confines the pointer to the window while playing.
  - **Load WAD** lists the `.wad` files in the mounted WAD directory, marks
    each one `GAME` or `MOD` by reading its signature, and loads the one you
    pick. Nothing about the loaded WADs can change while the engine runs, so
    this restarts it — a couple of seconds back to the title screen.
- `-iwad <file>` on the command line, to name the game data outright rather
  than take whichever of seven fixed filenames turns up first.

### Fixed
- The browser stretched the picture. noVNC was told to scale its canvas to the
  window; the engine renders a fixed 320×200. The link in the docs now passes
  `resize=off`, and `DOOM_SCALE` is the way to get a bigger picture — 3 and 4
  render 960×600 and 1280×800 natively, which beats upscaling.
- The mouse did not work at all. The window's event mask had every pointer
  event commented out, and the filter meant to drop the engine's own
  re-centring warp threw away any motion that merely shared a row or column
  with the centre.
- `I_ShutdownGraphics` detached a shared memory segment it had never attached
  when the MIT-SHM extension was missing, and dereferenced a null `Display*`
  when startup failed before graphics came up, turning a clean error message
  into a segfault.
- The engine wrote a NUL into its own `DISPLAY` environment variable while
  checking whether the X connection was local.

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
