# Changelog

Notable changes to the containerised DOOM. Versions are the image tags
published to `ghcr.io/krippler/doom`, so `1.10.0` here is `:1.10.0` there.
`:latest` always tracks the newest release and `:edge` the tip of `master`.

The version follows the engine this is built from, linuxdoom-1.10.

## [1.10.13] — 2026-09-12

### Added
- **Play in this window**, alongside **Play fullscreen**, on the page you land
  on. Both capture the mouse; fullscreen is no longer the price of playing.
  Its one remaining advantage is that Keyboard Lock only works there, so only
  fullscreen can keep Escape for the game's menu — in the window Escape always
  releases the mouse, which no page can change.
- Once the capture is released, clicking the picture resumes it the way you
  started, so Escape then click cannot drop you into fullscreen unexpectedly.

## [1.10.12] — 2026-09-12

### Fixed
- **Pressing "Detail" in Options broke the game on the next launch.** Low
  detail does not work in this port — the 1997 source says so itself and the
  call that would apply it is commented out — but the menu item still flipped
  the setting, and the flipped value was written to `.doomrc`. The next start
  applied it for real, selecting a drawing routine that range-checks the
  column against the full screen width and *then* doubles it, so it indexes
  past the end of its column table and writes wherever that lands: garbled
  rendering, then a segfault, then a restart loop.

  The menu no longer changes the setting and says why, the renderer refuses
  the mode whatever a config file asks for, and a value left behind by an
  older build is corrected on load — so an already-broken config fixes itself
  with nothing to edit. The range check is corrected too, so the routine is
  not a trap if it is ever revived.

## [1.10.11] — 2026-09-12

### Changed
- The renderer's range checks no longer end the game. They are 1997 debug
  asserts that call `I_Error`, so one bad frame became a crash loop in a
  container that restarts. The offending column or span is skipped instead —
  nothing is written outside the framebuffer either way — and it is reported,
  up to eight times, with the view geometry that caused it.
- Startup logs the view size: `R_SetViewSize: blocks 11, detail 0 -> view
  320x200`. `blocks` above 11 is what breaks the renderer, so this line says
  immediately whether that is the problem — and whether a build with the
  clamp is running, since a clamped one can never print more than 11.

## [1.10.10] — 2026-09-12

### Fixed
- **A bad `screenblocks` in `.doomrc` broke the renderer**, with errors like
  `R_DrawColumn: 206 to 219 at 178` and, past 12, a segfault on the first
  frame — and since the container restarts, a crash loop. The view height is
  derived straight from that number and anything above 11 makes the view
  taller than the 200-line screen, so the renderer draws off the end of the
  framebuffer. The 1997 code trusted the value, which is fine for the menu
  that produces it and not for a text file. It is clamped now, both where the
  view size is set and on load, so a bad value is corrected and saved back
  rather than carried around.

## [1.10.9] — 2026-09-12

### Fixed
- **The mouse no longer walks you forward and back.** The original used its Y
  axis for movement, because there was nothing to aim at vertically; with the
  same hand turning you, it mostly moves you about by accident. Turning is
  unchanged. **Options → Setup → Mouse → MOVE WITH MOUSE** puts the original
  behaviour back, and the choice is saved as `novert` in `.doomrc`.
- `PUID=0` looped forever instead of starting. The entrypoint re-executes
  itself to drop privileges whenever it is root, and dropping to root left it
  root, so it did it again. It now stays root and says so.

## [1.10.8] — 2026-09-12

### Fixed
- **Holding a mouse button did not repeat the action, and releasing it did not
  always stop.** A 1997 bug: `Button1Mask` is `1<<8`, and the engine used it
  raw where buttons two and three are correctly turned into bits 1 and 2. So a
  release reported `256 ^ 1` = 257, whose bit 0 is still set — the game never
  saw the button come up and kept firing — while any mouse movement made while
  the button was held reported 256, whose bit 0 is clear, switching fire back
  off. Holding still stuck; holding and moving stopped. Both now report 1.

## [1.10.7] — 2026-09-12

### Fixed
- **Mouse look still jittered, and this is the cause the last two releases
  missed.** The page reported the pointer as *centre plus the movement*, so a
  steady drag named the same coordinates over and over — and x11vnc discards a
  pointer report that does not move the pointer. Most of the movement never
  reached X at all. The page walks the pointer around the screen now and puts
  it back in the middle only when it would run off the edge, which the engine
  ignores for movement. Measured end to end through a real browser, pointer
  lock and x11vnc, over an identical drag: 1.1° before, 83.3° now, the same in
  both directions, and no drift at all with the mouse still.
- **The picture did not fill the window.** noVNC writes its own width and
  height onto the canvas whenever the framebuffer size arrives, which replaced
  the page's sizing; the scale is applied with `!important` now, and the page
  watches for the canvas being resized rather than measuring it once.
- **The mouse capture could silently fail to start.** The click asked for
  fullscreen and awaited it before requesting pointer lock, by which time the
  click's user activation was spent and the browser refused — leaving an
  ordinary remote-desktop pointer with the engine recentring underneath it.
  Both requests go out inside the click now, and the page says so if the
  capture does not take.

### Changed
- `grab_mouse` is off by default again. The engine's own recentring is for
  running on a real X display; through VNC the page handles it, and the two
  fought.

## [1.10.6] — 2026-09-12

### Fixed
- **Mouse look still jittered**, and this was the real cause rather than the
  one fixed in 1.10.5. `G_Responder` *assigned* each mouse event's movement to
  `mousex` instead of adding it, so only the last event of each 35 Hz tic
  counted and every earlier one was discarded. Any mouse reports faster than
  that — all of them, and a pointer-locked browser sends one event per report —
  so most of the movement never reached the game. Measured on the player's
  facing over an identical drag: 1.1° in 1.10.4, 22.3° in 1.10.5, 147.7° now.
- **Mouse buttons could stick on.** Losing the capture with a button held —
  pressing Esc, switching window — left the game holding it down for ever,
  which in DOOM means firing for ever. The page now releases every button when
  the capture, the focus or the tab goes away. Button changes are also sent on
  their own and immediately, rather than riding along on a movement report that
  could be queued behind a frame's worth of motion.
- **Fullscreen did not fill the screen.** The picture was scaled by whole
  numbers only, so a 640×400 render could manage just 3× on a 2560×1440
  display and left a wide border. It is scaled to fit the window now, by the
  same factor on both axes so the shape is kept.

### Changed
- The page sends one pointer report per animation frame with the movement
  since the last one, instead of one per mouse event. A gaming mouse reporting
  several hundred times a second was flooding the VNC link.

## [1.10.5] — 2026-09-12

### Fixed
- **Mouse look jittered instead of turning.** With the pointer captured the
  browser reports several positions inside one 28 ms tic, and only the first
  was measured from the centre of the screen: the rest were measured against
  the previous report, so a steady drag became a run of near-cancelling
  deltas. The engine re-centres the pointer after each motion but was waiting
  for the warp's own event to say so, and that event is queued behind
  everything already received. It now records the centre at the moment it
  warps. Measured on the player's facing over an identical drag: 1.1 degrees
  before, 22.7 after.

## [1.10.4] — 2026-09-12

### Fixed
- **Escape belongs to DOOM again.** Capturing the mouse meant the browser
  claimed Escape to cancel the capture, which is precisely the key the game
  wants for its menu. `play.html` now goes fullscreen and takes a Keyboard Lock
  on Escape, so it reaches the game; holding Escape is still the way out, and
  no page can take that away.

### Added
- A second, bindable key for the menu — **Options → Setup → Controls → MENU**,
  saved with the rest of your controls. Keyboard Lock is Chromium-only, so on
  Firefox and Safari Escape still ends the capture; binding the menu to
  something else (backquote is unused by DOOM) avoids the problem entirely.

## [1.10.3] — 2026-09-12

### Fixed
- **The Ultimate Doom was being played as plain registered Doom**, with *Thy
  Flesh Consumed* sitting in the WAD where the menu could not reach it. The
  1997 code identifies the game from the IWAD's filename and expected the four
  episode version to be called `doomu.wad`; Steam, GOG and every other
  re-release install it as `DOOM.WAD`. The engine now looks for E4M1 inside the
  IWAD and identifies it from that, so the banner, the episode menu and the
  fourth episode all agree.
- **A directory holding both games always started Doom II.** That is the
  engine's own search order. The container now picks the IWAD itself, prefers
  Doom over Doom II, and names it on the command line rather than leaving it to
  the search. `DOOM_IWAD` chooses directly.
- **"Demo is from a different game version!" on repeat, and no attract loop.**
  Every released IWAD's demos were recorded by 1.9 and this engine announces
  itself as 1.10, so it rejected all of them and sat on the title screen. The
  1.10 source is 1.9's game code with the version bumped, so the demos play
  back correctly; they are accepted now.
- **"S_StartSoundAtVolume: 16bit and not pre-cached - wtf?" for every sound.**
  The guard around that warning is spelled `SNDSRV`; every other file, and the
  build, spells it `SNDSERV`. A one-character typo in the 1997 source meant the
  branch never compiled out.
- The bare URL now serves the client that captures the mouse instead of a
  directory listing, so reaching the plain viewer by accident is harder.
- A WAD the game cannot read — owned by someone else, or not world-readable —
  now says so, and which user it tried as, instead of failing obscurely.

## [1.10.2] — 2026-09-11

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
