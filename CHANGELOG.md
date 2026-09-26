# Changelog

Notable changes to the containerised DOOM. Versions are the image tags
published to `ghcr.io/krippler/doom`, so `1.10.0` here is `:1.10.0` there.
`:latest` always tracks the newest release and `:edge` the tip of `master`.

The version follows the engine this is built from, linuxdoom-1.10.

## [Unreleased]

### Changed
- **The engine draws in truecolour, and the container's display is depth 24.**
  The 1997 X driver only accepted an 8-bit colour-mapped display, which is why
  the container ran one and x11vnc converted every frame to truecolour for the
  browser (`-8to24`) — the most expensive thing it did. The engine now turns its
  palette into pixels itself. With a client pulling frames at the game's 35 a
  second, x11vnc dropped from 26% of a core to 13% and Xvfb from 17% to 11%, the
  engine the same and the picture identical pixel for pixel. `DOOM_X_DEPTH=8`
  puts the old way back.
- **The engine reads the controller itself, and it vibrates.** A pad used to be
  the page's business: it turned buttons into X key presses and needed its own
  panel of bindings in the browser, told separately about every key rebound in
  the game, and some keys — Enter, Tab, the F keys — never reached the game at
  all. The page now only passes the pad's state across, and the engine does the
  rest (`i_pad.c`), the way the Quake port does. It is set up in the game, on a
  new **Options → Setup → Controller** page: what each button does, turn speed,
  vibration, swapping the sticks, and whether a full push runs — saved in
  `.doomrc`. The sticks walk and turn in proportion to how far they are pushed.
  In menus A chooses, B goes back, and on a question they are yes and no.
  Bindings saved in a browser by earlier versions are not read any more; the
  default layout is the same one.
- **Vibration**, on firing — each weapon its own kick — and on being hurt,
  harder for more damage, through the force-feedback hook id left in the 1997
  code and never filled in. It needs a browser that can drive the pad's
  motors, and the container log says whether this one can.

### Added
- **A smoke test, run on every push.** `tools/smoke-test.sh` starts the engine
  on Xvfb against the shareware WAD and plays a little of E1M1 with a simulated
  controller: the level has to appear, the sticks walk and turn, a shot is
  heard and felt, Start opens the menu, quitting saves the config, the title
  music plays, the 8-bit display draws exactly what the truecolour one does,
  and a crash leaves a readable backtrace. Half a minute, and CI keeps the
  screenshots. Until now every release was checked by hand, on a phone.

### Fixed
- **The view border and status bar could stay tinted after a damage or pickup
  flash.** A palette change recolours the screen without touching a pixel, so
  x11vnc never saw anything to send, and whatever was not redrawn afterwards
  stayed in the flash's colours in the browser — sampled for a minute, a border
  pixel was still red in every sample. In truecolour a palette change is a new
  frame like any other.

## [1.10.74] — 2026-09-17

Documentation and the Unraid template. Nothing in the image changed.

### Changed
- **The README is installation and setup, and nothing else.** It had grown into
  the project's essay — seven paragraphs on what the 1997 sources needed, the
  instruments built to chase a stutter, a tour of the menus the port added — all
  worth reading, none of it what a README is for, and it pushed the volume mount
  and the state directory past the fold.

  It now covers running it, mounting your own IWAD, keeping savegames, the
  images, building it, where the rest is documented, and the licence. Cutting
  the prose exposed two things it had never said: **a state volume**, without
  which savegames go when the container does, and what **PUID/PGID** do when it
  starts as root.

  *What this actually is* and *What was added* move whole to **ABOUT.md**,
  since neither belonged in the two documents that already exist —
  `PORTING-NOTES.md` is engine changes and `DOCKER.md` is how to run it.

- **The Unraid template is current again.** Its change list stopped at 1.10.47,
  twenty-six releases back — the same drift 1.10.50 fixed once already when it
  had stopped at 1.10.21. Anyone reading it in Community Applications was
  deciding on notes that predate controller support, the iPhone work and the
  crash fix.

  The overview gained the controller and the phone, and lost two pieces of
  advice that stopped being true — clicking into the canvas before typing, and
  reaching for noVNC's modifier pad when Ctrl or Alt will not go through. The
  client the WebUI opens handles both.

  Three settings the container reads were not offered: `DOOM_IWAD`, `DOOM_SOUND`
  and `DOOM_RESTART`.

  Audited rather than eyeballed: every required CA field is populated, every
  variable the template offers is genuinely read by the entrypoint, the icon is
  a 128×128 png that resolves on `master`, and both XML files parse.

## [1.10.73] — 2026-09-17

Nothing in the image changed. This release added a script and a backup
directory for pruning the older release list, and both were removed again
straight afterwards: `gh release delete <tag> --yes --cleanup-tag` already does
that job in one line, so the script was reinventing it, and the backup was
4,000 lines of clutter in a repository that keeps its own history.

Deleting releases and container images is not possible from the session this
repository is worked on from, so that tidying is done with `gh` from a machine
logged in as the owner.

## [1.10.72] — 2026-09-17

### Changed
- **A phone gets the sound through a media element by default.** `?speaker=1`
  was confirmed on the iPhone it was added for: the sound comes out of the
  phone's own speaker with it, and out of the attached controller's headphone
  socket without it. So it is now the default wherever there is no pointer to
  capture, and `?speaker=0` goes back to the plain output. Desktop browsers are
  unchanged — they never had the problem, and one object fewer sits in the way.

  Each case was checked: desktop takes `ctx.destination`, a phone takes the
  media element, and either parameter overrides either default, with audio
  flowing in all four.

- **Music and effects were confirmed to be one stream**, which answers "what
  about the music?" — `audiostream` mixes the engine's music with the sound
  server's effects and sends the sum, so music goes wherever the effects go.
  Measured out of the real mixer with the engine idle in a level and nothing
  firing: 99.9% non-zero at a peak of 11649, and the only thing making that is
  the music. One of the two being silent is therefore a volume in **Options →
  Sound Volume**, not a routing fault.

## [1.10.71] — 2026-09-17

### Added
- **`?speaker=1`, for a phone that sends the game's sound to the wrong place.**
  Reported from an iPhone with a Backbone plugged in: the sound came out of the
  controller's headphone socket and never out of the phone's speaker — while
  every other app on that phone, with the same controller plugged in, used the
  speaker. So iOS is not routing everything to the accessory, it is routing
  *this* to the accessory.

  The difference is which audio session the sound belongs to. Web Audio straight
  to `ctx.destination` is WebKit's own, and a page cannot pick an output device
  for it: `setSinkId` does not exist on iOS. Sound played through a media element
  is governed like any other app's, so `?speaker=1` sends the same samples
  through a `MediaStreamAudioDestinationNode` into a hidden `<audio>` instead.

  Off by default, because it cannot be tested from this side and the ordinary
  path does work into the socket. Both routes were checked to carry identical
  audio — same frames, same callbacks, same amplitude — so switching it on
  cannot cost the sound that already works; the only question is which output
  iOS then picks. The log says which way it went.

## [1.10.70] — 2026-09-16

### Fixed
- **The music crashed the game.** A kernel log from the machine it happens on
  named it outright:

  ```
  linuxxdoom[932935]: segfault at 14c9f8ed0414 error 4 in libfluidsynth.so.3.2.2
  ```

  Inside FluidSynth, not inside DOOM — and this file's fault all the same. The
  music is rendered by a thread of its own, calling `fluid_synth_write_s16` in a
  loop, while the game thread does `delete_fluid_player()` and builds a new one
  every time the music changes, which is every level. There was **no
  synchronisation between them at all** — not a mutex in `i_sound.c`. The player
  was being freed out from under the thread rendering it, and `error 4` is a
  read of an unmapped page.

  It needs a level change to land in the wrong microsecond, which is why it was
  rare, patternless, and looked like a fault in the music library.

  ThreadSanitizer cannot catch it — both halves happen inside
  `libfluidsynth.so`, which is not instrumented — so it was reproduced on its
  own in forty lines doing nothing but those two things from two threads:

  | | |
  | --- | --- |
  | without a lock | **SIGSEGV, five runs out of five** |
  | with one lock over both | survived 4000 music changes, five out of five |

  One mutex now covers the render call and every player operation. It is never
  held across the blocking write into the music pipe, and shutdown releases it
  before joining the thread, so neither the game nor the exit can stall on it.
  Checked after the change: music still renders and the engine still shuts down
  on the first ask.

### Changed
- **A crash, or quitting, restarts the game instead of killing the container.**
  Both used to end the entrypoint, taking the display, the sound and the proxy
  with it — so the page said *"connection lost, reload to try again"* where
  reloading could not possibly work, and getting back in meant a `docker
  restart` from somewhere that was not the phone in your hand. Picking QUIT GAME
  did the same thing, which is a strange fate for a menu item.

  Everything except the engine now outlives it, so the browser reconnects to the
  same session on its own and the game comes back at the title screen. There is
  no crash recovery in 1997 code and this does not pretend otherwise: a savegame
  is still the only way back to where you were.

  Not restarted: a shutdown, or a fatal error the engine reported itself. And
  three runs in a row that end within seconds of starting stop the loop and say
  so, rather than burying the reason under an endless retry. `DOOM_RESTART=0`
  turns the whole thing off.

  Checked against the real entrypoint with a stub engine: a crash 15 s in
  restarts, a clean quit restarts, an instant crash three times over gives up,
  and a fatal error does not restart at all.

### Added
- **The page says whether the sound is playing, in the container's log.** "I
  can't hear anything" has causes on both sides of the glass and they look
  identical from here:

  ```
  [doom] sound: playing via the fallback, 22050 Hz in, 44100 Hz out, context running.
  [doom] sound: not playing: this browser has no Web Audio.
  ```

  *Playing* means the page has done its part and the silence is the volume, a
  phone's silent switch, or wherever the audio has been routed — a controller
  with its own headphone socket can take the output while it is plugged in.

  The iOS path was verified end to end rather than assumed: a browser with the
  Pointer Lock API and AudioWorklet both removed, fed the real audio protocol,
  renders through the `ScriptProcessorNode` fallback at the right rate with the
  right amplitude.

## [1.10.69] — 2026-09-16

### Fixed
- **A control bound to Enter fires at nothing, and now the page knows it.**
  Reported as "triggers don't work" with a log that plainly showed the trigger
  working: `down b7 -> fire` / `sent Return down`, every press, correctly.

  The key was arriving and the game was never seeing it. `G_Responder` passes
  each event through the heads-up display, the status bar, the automap and the
  finale before the code that records a keypress, and `HU_MSGREFRESH` — the key
  that re-shows the last message — **is `KEY_ENTER`**. `HU_Responder` eats it
  unconditionally. A `.doomrc` with `key_fire 13` can never fire.

  Measured by setting `key_fire` to each in turn, warping into a level, holding
  the key and tracing `G_Responder`:

  | `key_fire` | reaches the game? |
  | --- | --- |
  | 157 `Control_L`, 32 `space`, 182 `Shift_L` | yes |
  | 13 `Enter` | **no** — message refresh |
  | 9 `Tab` | **no** — automap |
  | 27 `Escape` | **no** — menu |
  | 187 `F1` | **no** — the F-keys |

  The engine is left alone; every one of those eaters is doing its job. The
  controller now treats such a setting exactly like one with no keysym at all,
  so **fire falls back to the mouse button and works without anybody editing a
  config**:

  ```
  plan fire in=b7 sends=mouse1 src=default doomrc=13 unusable=13
  note fire is key 13 in the engine, which the game never sees
  ```

  The defaults that deliberately use Tab and Escape — the automap, backing out
  of a menu — are untouched, since being eaten is the point of those two.

## [1.10.68] — 2026-09-16

### Fixed
- **The start screen never went away on iOS, so the controller was never allowed
  to send anything.** This is the whole of "the controller doesn't work on the
  phone", and it had nothing to do with controllers.

  One line hides that screen:

  ```js
  document.addEventListener('pointerlockchange', () => {
    overlayEl.hidden = locked();
  ```

  iOS has no Pointer Lock API, so `pointerlockchange` never fires, so the screen
  never hid. The game was running behind it the whole time. And `padActive()` —
  the test the page uses before sending a single key — is `overlay.hidden`, so a
  perfectly detected, perfectly bound pad sat there with every press discarded
  on purpose. Nine releases of controller work could not have fixed it, because
  none of it was wrong.

  `enterPlay()` now hides the screen itself where there is no pointer lock to do
  it. Checked against a browser with `Element.prototype.requestPointerLock`
  removed: the overlay hides, `padActive()` goes true, and a press arrives in
  the container log as `down b0 -> act` / `sent space down`, none of which
  happened before.

- **"Pair a game controller" stayed up after one was paired.** The note was
  written once, 700 ms after play started, and never revisited — but iOS hands
  the page a pad only *after* a button is pressed on it, which is the very thing
  the note asks for. So it told somebody holding a working controller to go and
  connect a controller. It is answered every frame now and clears the moment a
  pad appears.

- **The start screen explained mouse capture to a phone.** It now says the
  controller is the whole of the input on a device with no pointer to capture.

## [1.10.67] — 2026-09-16

### Fixed
- **A key value out of range was a segmentation fault.** `G_Responder` records a
  key with a bounds check; `G_BuildTiccmd` reads the same array back with none —
  and `key_fire` and the rest come out of `.doomrc` through `sscanf("%i")`
  unchecked. A config file holding a large number is therefore an out-of-bounds
  read on every tic. Reproduced with `key_fire 2000000000`: SIGSEGV within a
  second, every time. `65515` and `-1` did not crash, which is worse rather than
  better — they quietly read whatever sits next to the array.

  Getting such a value in there took no effort: `xlatekey` passes an
  unrecognised keysym straight through, so a Super key or a media key arrives as
  65515 or thereabouts, and the Controls screen stored whatever it was handed.
  Reads are range-checked now, and the Controls screen refuses a key the game
  could never record.

  **This is not the crash reported against 1.10.66** — that log's keymap line
  shows every value in range. It is a real one found while looking for it.

- **The sprite name list had no terminator.** `R_InitSpriteDefs` counts its
  argument by walking to a NULL, and `sprnames[NUMSPRITES]` in `info.c` holds
  exactly `NUMSPRITES` entries and no NULL. The walk ran off the end of the
  array and went on reading whatever global followed it, as `char*`, until a
  zero turned up — then dereferenced each one.

  AddressSanitizer reports it as a global-buffer-overflow **on every startup**,
  which is how it was found: built with `-fsanitize=address` and started,
  nothing else needed. What it did afterwards depended on what the linker put
  next, which is why it survived thirty years unnoticed. The array is
  `NUMSPRITES + 1` with a trailing `NULL` now; ASan is clean through startup and
  several minutes of input.

  Also **not** known to be the crash reported from a phone.

### Added
- **A crash says where it died.** `Segmentation fault` on its own is not enough
  to act on, and there is no core file in a container nobody runs gdb in. The
  engine now prints a backtrace for SIGSEGV, SIGBUS, SIGFPE, SIGILL and SIGABRT
  and re-raises, so the log names the function:

  ```
  DOOM died on SIGSEGV (bad memory access). Innermost frame first:
  /usr/local/games/linuxxdoom(TryRunTics+0x220)[...]
  /usr/local/games/linuxxdoom(D_DoomLoop+0x36f)[...]
  ```

  `backtrace_symbols_fd` rather than `backtrace_symbols`, because the latter
  calls `malloc` and would deadlock exactly when it is needed. `-rdynamic` puts
  the names in and they survive the image's `strip`, which was checked against a
  stripped binary rather than assumed.

- **The exit line names the signal**, rather than `DOOM exited with status 139`.

## [1.10.66] — 2026-09-16

### Fixed
- **Two wrong messages on an iPhone**, reported together from one: *"The browser
  did not capture the mouse … turn GRAB POINTER off"* and *"No sound: this
  browser has no Web Audio."* Both were the page's fault, and neither described
  anything that was actually wrong with the device.

  **iOS has no Pointer Lock API at all**, so the request was never made — there
  was no capture to fail. The page checked `locked()` 700 ms after play started
  and complained whenever it came back false, which on a phone is always,
  telling somebody holding a touchscreen to click the picture and change a mouse
  setting. It now asks whether the platform *has* pointer lock first. Where it
  does not, and no pad is connected, it says the useful thing instead:

  > No mouse to capture on this device — pair a game controller and press a
  > button on it. Everything else works.

  With a controller connected it says nothing, because nothing is wrong. The
  log records which case it was: `client 1.10.66, pointerlock no, keymap …`.

  **The sound gave up before reaching its own fallback.** It required
  `window.AudioWorkletNode` up front, but the worklet is optional — it is gated
  on a secure context, and where it is missing the sound goes through a
  `ScriptProcessorNode` instead, which is the path this container normally uses
  over plain HTTP. A browser exposing no AudioWorklet whatever therefore got no
  sound at all rather than the fallback that was sitting right there. Only an
  `AudioContext` constructor is required now, `webkitAudioContext` included.

- **The controller log cut its most useful line in half.** Lines were capped at
  200 characters, and the keymap line is 225 — so it arrived ending
  `… key_use=32 mouseb_`, losing `mouseb_fire` and `mouseb_strafe`, which are
  exactly the two settings the fire fallback turns on. The cap is 400 now, and
  the build and the keymap are two lines rather than one over-long one. This is
  the second time a cap has eaten this information; the first was the
  `controller keys:` line at 120 characters in 1.10.59.

  Checked by running the page under a browser with `Element.prototype.requestPointerLock`
  removed and again with `AudioWorkletNode` removed: 1.10.65 gives the mouse
  warning and `via="" reason="this browser has no Web Audio."`, and this build
  gives the controller note and `via="fallback"`. A real iPhone has not been
  tested from here — the controller itself was already polled independently of
  pointer lock, so it should have been working behind those messages all along.

## [1.10.65] — 2026-09-16

### Fixed
- **The Controls screen let Enter bind itself.** It is opened with Enter, and
  the next key pressed becomes the binding — so pressing Enter again, which is
  what anybody does when they are not sure the first press registered, silently
  bound the menu's own confirm key to a game control. Escape was refused;
  Enter was not.

  That control then works in a level and picks menu items everywhere else,
  because `M_Responder` reads Enter as confirm whatever else it is bound to. A
  real `.doomrc` arrived with `key_fire 13` by this route, reported as a
  controller fault and nothing of the kind.

  Enter is now ignored while the prompt is open, and the prompt stays open so
  the next real key binds. Escape still cancels. Checked by driving the menus
  over VNC and reading `key_fire` back out of the config afterwards: *Enter,
  Enter again, then Ctrl* gave 13 before and gives 157 now, while *Enter then
  Ctrl* and *Enter then space* are unchanged at 157 and 32.

  **Moving the control to a mouse button would not have helped** — `M_Responder`
  turns mouse button 1 into `KEY_ENTER` as well, so that is the same conflict by
  another route. An ordinary key is the answer; `Ctrl` is the stock one.

### Added
- **The log says when a control is on Enter**, since an existing config can
  still carry one:

  ```
  [doom] controller: note fire is Enter in the engine, which also confirms menu items
  ```

## [1.10.64] — 2026-09-16

### Fixed
- **A saved controller layout hid every button added after it was saved.** The
  first log from a real pad answered the whole thing in one line — `weapon1`
  through `weapon7`, `back_out` and both strafes all reading `in=-`, no button
  at all, while B, X and Y logged `nothing bound` as they were pressed.

  Loading the saved layout *replaced* the defaults rather than filling in around
  them. So anybody who rebound one control before an action existed kept a
  layout with a permanent hole in it: the action had no button, the panel had no
  way to say why, and nothing short of **Reset to defaults** would bring it back.
  That is the whole of *"Y doesn't map to any action"* and of *"left and right
  stick buttons don't work — they never have"*, which were both reported as pad
  faults and were neither.

  A saved layout now has its gaps filled from the defaults. Only gaps: a button
  you rebound keeps what you gave it, an action that already has a button is
  left alone, and a default button you have since put something else on is not
  taken back. **Clear** is remembered separately so it survives a reload, which
  it has to now that a gap gets filled.

  The log says when it happens, and which actions it was:

  ```
  [doom] controller: filled in from the defaults, saved layout had no button for: back_out=b1 weapon3=b2 ...
  ```

## [1.10.63] — 2026-09-16

### Added
- **A real log of what the controller does, in the container's log.** Asked for
  in as many words, and the right answer to the whole episode: nine releases went
  out inferring what a pad three thousand miles away was doing, each one fixing
  something that was already right. `docker logs <container> | grep controller:`
  now answers it directly.

  One line on every page load, with or without a pad:

  ```
  [doom] controller: client 1.10.63, keymap from the engine: key_fire=0 key_speed=0 ...
  ```

  — the build the browser is *actually* running, which distinguishes a controller
  fault from a tab still running a client from two releases ago, and the engine's
  bindings as the page received them. Then, once per pad, its name, layout,
  button count and where every axis rests. Then what each of the twenty controls
  is going to do:

  ```
  [doom] controller: plan fire in=b7,a5+ sends=mouse1  src=default doomrc=0 unusable=0
  [doom] controller: plan run  in=b6,a2+ sends=NOTHING src=default doomrc=0 unusable=0
  ```

  `sends=NOTHING` is the answer wherever a control does nothing, printed before
  anybody has to ask. Then, as you play, every button down and up — including
  ones bound to nothing — and every key and mouse button sent because of it.
  Capped at 400 lines; axes only where something is bound to them.

### Fixed
- **The pad report added in 1.10.61 never reached the log.** It asked for a URL
  that did not exist and relied on websockify's request log to carry the query
  string. That log is written **only under `--verbose`** — on a 404 all that gets
  printed is the code — and Python block-buffers stdout through the entrypoint's
  pipe besides, so even the lines that were written would have arrived in 4 KB
  clumps long after the moment they described. Anybody who went looking for
  `controller found:` on that release found nothing, and nothing was wrong with
  their container.

  `doom-wsproxy` now answers the URL itself, prints the lines flushed, and
  returns 204. Whitespace is collapsed where it is printed, so nothing a page can
  send can forge a line of container log.

- **The plan was printed before the engine's bindings arrived.** `doom-keys.json`
  is fetched, and the report fired the moment a pad appeared — often first — so
  the log described the built-in defaults while the page went on to send
  something else. It now waits for that fetch to settle, and says which way it
  settled.

## [1.10.62] — 2026-09-16

### Fixed
- **Fire can be a mouse button, and the controller only ever sent keys.** The
  report that settled it was *"mapped fire to B and it doesn't work there
  either"* — B exists, Bind captured it, and fire still did not fire. So the pad
  was never the problem and no amount of rebinding was going to help.

  `G_BuildTiccmd` reads fire as
  `gamekeydown[key_fire] || mousebuttons[mousebfire] || joybuttons[joybfire]`.
  Somebody who fires with the mouse has **no key for firing at all**, and a
  controller that sends only keys cannot fire for them however it is mapped.

  The container now reads `mouseb_*` out of `.doomrc` beside the keys, and where
  the engine has no usable key for an action the pad sends that mouse button
  instead — folded into the same mask the real mouse uses, on its own bits so
  neither clears the other. Only as a fallback: where there is a usable key it
  sends just the key, because holding a mouse button in a menu reads as Return
  and a working fire key should not make menus confirm themselves.

  **Run has no mouse button to fall back on** — the engine has no `mouseb_speed`
  — so that row now says `key 0 — nothing to send` and is fixed with **Key**.

- **`unknownKeyFor` returned nothing when the setting was 0.** It ended in
  `|| null`, and 0 is falsy, so the one value most likely to mean "this engine has
  no key for that" was reported as no problem at all. The panel said nothing and
  the action silently sent nothing. Now the row reads `key 0 — nothing to send`,
  or `mouse 1 (key 0 unusable)` where a mouse button covers it.

- **The panel shows the engine's own number beside each key** — `Control_L (157)`
  — so a value that maps to a plausible-looking key can still be checked against
  what the game was told, rather than taken on trust.

### Notes
- **Every previous guess about the pad is ruled out by one sentence.** Fire bound
  to B, with B demonstrably read, still not firing means the fault was never in
  which button or axis the pad reports. Five releases went at the pad side —
  code names, axis binding, keysym passthrough, rest-position detection, adding
  axes beside buttons — and the answer was on the other side of the action
  entirely. `Y` is also fine: it is bound to Pistol and sends `2`, verified.

## [1.10.61] — 2026-09-16

### Added
- **The page tells the container's log what controller it found.** Every report
  in this run has come down to something only the pad can answer — how many
  buttons it claims, where its axes rest — and there was no way to get that from
  a browser into the log somebody actually pastes. So the page asks for a URL
  that does not exist, with the answer in the query string, and the entrypoint
  lifts it out of websockify's request log:

  ```
  [doom] controller found: id=Microsoft X-Box 360 pad (Vendor: 045e Product: 028e)
         mapping=none buttons=11 axes=0.00,0.00,-1.00,0.00,0.00,-1.00,0.00,0.00
  ```

  Name, whether the browser calls the layout standard, the button count and every
  axis's resting value, once per pad. Nothing leaves the machine — it is a request
  to the same container serving the page, and the 404 is the mechanism rather than
  a fault. Six rounds of asking somebody to read a panel could have been one
  round of reading a log line.

### Fixed
- **Trigger axes are added beside the buttons, not only where a button is
  missing.** 1.10.60 bound them when the button index did not exist on the pad,
  which misses the layout that actually causes this: a pad reporting **eleven**
  buttons where 6 and 7 are View and Menu rather than the triggers, with the
  triggers on axes 2 and 5. Nothing looks missing there, so nothing was bound.

  An action can hold more than one input, so both are bound and whichever the pad
  really uses works — the row reads `RT or Axis 5 +`. If 6 and 7 really are the
  triggers, they go on working untouched.

  Verified against that layout, with no manual binding: Fire came out as `b7` and
  `a5+` together, Run as `b6` and `a2+`, squeezing axis 5 sent `Control_L`,
  squeezing axis 2 sent `Shift_L`, and button 7 still fired. A standard
  seventeen-button pad is unchanged, since none of its axes rest at an extreme.

  This also explains why the stick clicks never worked: on such a pad there is no
  button 11 at all, and button 10 is not the right stick. The start-screen line
  names those rows now, and any of them can be rebound by pressing.

## [1.10.60] — 2026-09-16

### Fixed
- **Triggers reported as axes are found without being bound by hand.** 1.10.58
  made it *possible* to bind an axis and left the doing of it to whoever was
  holding the pad, which is not a fix. An axis that **rests at one extreme** is
  now taken to be a trigger — a stick resting at ±1 is a broken stick — and when
  the layout points Fire and Run at buttons the pad does not have, the
  lower-numbered trigger axis becomes Run and the next Fire. The panel marks
  those rows *found as a trigger axis*.

  It is an observation rather than a guess at indices, it never replaces a
  binding the pad can satisfy, and anything set by hand wins. Verified against a
  synthetic pad with six buttons and triggers on axes 4 and 5 resting at -1, with
  no manual binding at all: Fire came out as `a5+` and Run as `a4+`, squeezing
  each sent `Control_L` and `Shift_L`, and the four stick axes resting at 0 were
  not mistaken for triggers. A standard seventeen-button pad is untouched — Fire
  stays on `b7`, Run on `b6`.

- **The start screen says which actions cannot work.** An action goes dead two
  ways without looking wrong: its key is a value the page cannot express, or its
  input is a button the connected pad does not have. Both were silent. The
  controller line now names them — *Fire, Run: no such button on this pad* — at
  most three with "and N more" after, so a glance is enough and the screen cannot
  fill up.

  Six rounds of this went by with that line saying only that a controller was
  present, which was the least useful true thing it could have said.

### Notes
- **Modifier keys were ruled out by tracing the engine, not by argument.** Fire
  and Run are the only two controls that send a modifier — `Control_L` and
  `Shift_L` — and x11vnc manages modifier state itself, so its `-modtweak` was a
  plausible culprit. A trace at the top of `G_Responder`, with keys sent from a
  raw RFB client, settled it:

  ```
  Control_L held:  down 157  up 157   (key_fire=157)
  Shift_L held:    down 182  up 182   (key_speed=182)
  space held:      down 32   up 32
  ```

  The engine receives them exactly as it receives any other key, so nothing about
  the modifiers, x11vnc or the wire is at fault, and the remaining explanation is
  on the pad side. One apparent oddity in the same trace — a missing keydown for
  the arrow — turned out to be the menu eating it, which is correct behaviour and
  not a bug. The trace is not in the shipped build.

## [1.10.59] — 2026-09-16

### Fixed
- **A binding the page could not express sent the wrong key, silently.**
  `doomKeyToX` inverts `xlatekey`, and it stopped at printable characters and the
  `KEY_*` constants. But `xlatekey` returns the **keysym unchanged** for
  everything else, so a control bound to a keypad key, Caps Lock, Insert, the
  Menu key or Print Screen is stored as that keysym — 0xff8d, 0xffe5 and so on —
  and every one of those fell through to the built-in default. The pad then
  pressed Ctrl at an engine listening for something quite different, with nothing
  said about it.

  Values above 0xff are sent as keysyms now, which is exactly what they are.
  Checked across the range: `KP_Enter`, `Caps_Lock`, `Control_R` and the Menu key
  all map, the `KEY_*` constants and printable characters are unchanged.

  Where a value genuinely has no keysym, the action now sends **nothing** and the
  panel says **`game uses 144 — unknown here`** on that row. Pressing the wrong
  key is worse than pressing none: the default may well be bound to something
  else. **Key** on the row still sets it by hand.

- **The log line meant to answer this truncated before reaching it.** 1.10.55
  added `controller keys:` to the startup log and cut it at 120 characters, and
  awk emits its keys in no particular order — so `key_fire` and `key_speed`, the
  two a controller report is most likely to be about, were the ones that fell off
  the end. It is now logged in full and sorted:

  ```
  [doom] controller keys: key_down=115 key_fire=120 key_left=172 key_menu=96 ...
  ```

  A diagnostic that hides the thing being asked about is worse than none, and
  this one hid it for three releases.

## [1.10.58] — 2026-09-16

### Fixed
- **Triggers, on a pad that reports them as axes.** With everything else working,
  the report was "triggers don't work" — and that is the shape of a specific
  thing rather than a vague one. The standard layout puts LT and RT at buttons 6
  and 7; plenty of pads and browsers instead report them as **analog axes**, and
  then there is no button 6 or 7 on the pad at all. Every true button works and
  the two triggers are dead.

  A binding now names an input rather than a button number — `b7` for a button,
  `a5+` for an axis pushed positive — and **Bind takes a squeeze as well as a
  press**. It records which way the axis travelled from where it was resting, so
  a trigger that sits at -1 and runs to +1 binds as `+` and is not read as held
  while it rests. Layouts saved before this are read as button bindings rather
  than discarded.

  A second cause is closed at the same time: a trigger reporting only an analog
  `value` and never setting `pressed` had to pass the half-way mark, so one
  topping out at 0.4 never registered. The mark is 30% of travel now.

- **The panel says when a binding names a button the pad does not have.** The
  Fire row on such a pad now reads **`RT — not on this pad`** instead of looking
  correct, and the readout's *pressed now* line names every input that is on in
  the same form a binding uses — so a squeezed trigger shows as `Axis 5 +` the
  moment it is pulled. Six rounds of this went by with the page unable to say
  the one thing that would have identified it.

  Verified against a synthetic pad with six buttons and triggers on axes 4 and 5
  resting at -1: the Fire row reads `RT — not on this pad`, squeezing shows
  `Axis 5 +`, Bind captures `a5+`, the axis then sends `Control_L` down and up,
  and a resting trigger sends nothing. A standard seventeen-button pad is
  unchanged — fire on `b7`, run on `b6`, both still firing — a trigger reaching
  only 0.4 now registers, and a layout stored as `{"7":"fire"}` comes back as
  `b7`.

## [1.10.57] — 2026-09-16

### Fixed
- **A rebound control now works the menu, without moving the cursor twice.**
  1.10.56's fix for the menus was to send the engine's key *and* the menu's
  hardcoded one. It worked and it was wrong, and the report said exactly how:
  *down jumped four menu selections, up was fine.*

  `M_Responder` reads the arrows and Return literally and sends every other
  character to a **hotkey search**, which jumps to the item beginning with that
  letter. With `key_down` set to `s`, the pad sent `s` — and in the main menu `s`
  is **SAVE GAME**, three items along from NEW GAME — plus the ArrowDown beside
  it, which moved one more. Four. `up` was fine because nothing in that menu
  begins with `w`. Two keys also meant two Returns for Open/use, which selects
  twice, and with `key_use` set to `e` it would have jumped to **END GAME** in the
  Options menu and then confirmed it.

  So the mapping moves into the engine, where it belongs. `m_menu.c` now maps
  `key_up`, `key_down`, `key_left`, `key_right` and `key_use` onto the menu's own
  navigation keys before that switch, and the page sends **one key per action**.
  The keys you walk with are the keys you navigate with — which is worth having
  on a keyboard too, not just with a pad. Only where they differ from the
  defaults, and only with a menu open, since every earlier branch of
  `M_Responder` has already returned by then. See PORTING-NOTES.md.

  Verified against the running engine with a trace on `M_Responder` and
  `key_down` set to `s`: two presses of the arrow gave `ch=175 itemOn=0` then
  `ch=175 itemOn=1`, and two presses of `s` gave the identical pair — one line
  each, where the second would previously have jumped to item 3. `key_use` set
  to `e` arrived as `ch=13` and selected the item.

### Notes
- **Three measurement rigs were wrong before one worked, and that is the useful
  part.** Diffing the framebuffer to find the menu cursor failed because the
  attract demo keeps animating behind an open menu — 160 of 400 rows changing
  between grabs — so the "widest changing band" was the demo, not the skull. A
  file-based handshake between the driver and the watcher raced, and reported 0
  px for every trial. An earlier grab asked X for the wrong drawable because the
  vendor string in the connection setup reply starts at byte 40, not 32, and X
  answered BadDrawable.

  What finally settled it was not a better measurement of the picture but
  choosing a different thing to observe: a trace print inside `M_Responder`, so
  the question "what does the menu do with this key" was answered by the menu
  rather than inferred from pixels. The trace was removed before committing.

## [1.10.56] — 2026-09-15

### Fixed
- **1.10.55 broke menu navigation, which is the bug it had just fixed arrived at
  from the other side.** Reading the engine's bindings made the pad work in a
  level — analog sticks, doors, the menu key all correct — and stopped the d-pad
  moving the menu cursor.

  The cause is the same split that started all this, missed in the other
  direction. `m_menu.c` reads the four arrows, Return and Escape **literally**,
  and `am_map.c` pans the automap with the same arrows, while play goes through
  the configurable bindings. 1.10.55 *replaced* each action's key with the
  configured one, so with `key_up` set to `w` the d-pad sent `w` — which walks
  you forward and means nothing at all to a menu.

  It now sends **both** where they differ: the configured key and the hardcoded
  one. `Open / use` already did this with Return, which is exactly the rule that
  should have been generalised instead of special-cased. `forward`, `back`,
  `turnleft` and `turnright` get their arrow alongside; a key with no menu
  counterpart, like Fire, still sends one key. A key set by hand in the panel
  gets the same treatment, so learning `w` for Forward cannot break the menus
  either.

  Neither key interferes with the other: in a level the arrow is unbound and does
  nothing, and in a menu the configured key is not a menu key and does nothing.
  On the automap the arrow is a bonus — the d-pad pans the map.

  Verified against the real stack with `key_up` set to `w`: holding the d-pad's
  up held **both** `w(25)` and `Up(111)` at the X server for exactly the two
  seconds it was down. `turnleft` stays a single key because `key_left` is still
  the arrow, so nothing is sent twice. Learning `k` for Forward keeps `ArrowUp`
  beside it; learning `q` for Fire stays one key.

## [1.10.55] — 2026-09-15

### Fixed
- **The controller presses the keys *this* engine listens for, read from its own
  config.** 1.10.54 made each action's key settable by hand, which was the right
  diagnosis and the wrong remedy: it asked somebody to find out what their own
  `.doomrc` says and copy it into a panel a row at a time, and the next report
  was unchanged.

  The container can just read it. `.doomrc` is in the state directory; the
  entrypoint parses its `key_*` settings at startup into `doom-keys.json`, and a
  symlink in the noVNC root serves it — so nothing has to be writable at runtime
  and the page fetches the bindings the engine is about to load. `doomKeyToX()`
  is `i_video.c`'s `xlatekey` read backwards, so a stored `120` becomes `x`/`KeyX`
  and a stored `172` becomes `ArrowLeft`.

  Precedence: a key set by hand beats the config, the config beats the built-in
  default, and a missing, empty or unparsable file leaves the defaults alone.
  `Open / use` keeps Return alongside whatever `key_use` says, because menus
  hardcode Return. The panel's third column says where each key came from — green
  from the config, grey the default, pink set by hand.

  Read at startup, so a binding changed in the game reaches the pad on the next
  restart: the engine writes `.doomrc` when it exits. **Key** on a row still
  overrides immediately.

### Notes
- **This was found by running the stack rather than by asking for a reading.**
  Three releases went out asking for one off an instrument, and none came back,
  which is a fair verdict on asking. Xvfb, x11vnc and websockify install in one
  command, so the whole path was rebuilt here: the real x11vnc with the flags the
  entrypoint uses, the real page, a synthetic pad injected into headless
  Chromium, and a raw-X `QueryKeymap` watcher reporting which keycodes are
  physically held on the server.

  That settled in one run what three rounds of questions had not. Holding the
  trigger held `Control_L(37)` at the X server for exactly the two seconds it was
  down — one key-down, no spurious release — so the pad, the page, `sendKey`,
  x11vnc and XTEST were all correct, and the only thing left was which key the
  engine listens for. With a rebound `.doomrc` in place the same test holds
  `x(53)` instead, which is the fix demonstrated rather than argued.

  Two early attempts at that watcher reported "the key never went down" and were
  wrong both times: the X connection setup request is 12 bytes and 10 were sent,
  so the server was still waiting for the rest, and Python was block-buffering
  the watcher's output into a file that was read too early. Worth recording
  because both failures looked exactly like the bug being hunted.

## [1.10.54] — 2026-09-15

### Fixed
- **A controller that worked in the menus and did nothing in the game.** The
  field report across 1.10.51 to 1.10.53 was "left and right work, the menu
  button works, nothing else does", and then the detail that settled it: *up and
  down do work in the menu, along with A*.

  The engine splits exactly along that line. `m_menu.c` **hardcodes**
  `KEY_UPARROW`, `KEY_DOWNARROW` and `KEY_ENTER`, so a pad pressing those
  navigates menus whatever `.doomrc` says. Play reads *configurable* bindings for
  everything — `key_up`, `key_down`, `key_fire`, `key_use`, `key_speed`,
  `key_strafe` — so once those have been changed under Options → Setup →
  Controls, a pad sending the defaults does nothing at all in a level. The
  container cannot see the controller and the page cannot see `.doomrc`, so
  neither end could notice the mismatch.

  Two details of the report that looked like more of the same fault and were
  not: **turning kept working** because the sticks send pointer motion rather
  than a key, and the mouse is not rebindable — it is the one input that cannot
  break this way. And **most weapon buttons legitimately do nothing** at the
  start of a level, because the engine ignores a weapon you are not carrying and
  E1M1 hands you a fist and a pistol.

  So each action can now be told which key to press, learned from the keyboard:
  the panel's third column shows the key, **Key** on that row captures the next
  keypress, and **Default key** puts it back. The keysym and code name come from
  noVNC's own translation of the event, so what the pad sends afterwards is byte
  for byte what pressing that key sends. The panel says all this at the top,
  because "it presses keys, not intentions" was documented in 1.10.51 and
  documenting it turned out not to be the same as handling it.

  Verified in Chromium: the panel lists the key every action sends, pressing
  **Key** on Fire and then W makes the trigger send `w`/`KeyW` down and up
  instead of `Control_L`, it survives a reload, **Default key** restores
  `Control_L`, **Reset to defaults** clears every learned key, and an unknown
  action or a null keysym is ignored rather than stored.

## [1.10.53] — 2026-09-15

### Added
- **The controller readout is on screen while you play.** 1.10.52 put it in the
  start-screen panel, which was the wrong place for the one fault it was built
  for: the panel is only visible when the game is not, and the moment worth
  watching is the moment it is hidden. Asking somebody to play, stop, and then
  read a description of what happened is asking them to remember instead of
  look.

  With `?stats=1` a strip now sits over the top-left of the picture:

  ```
  pads 1 · layout standard · pressed 7=RT · holding Control_L · last key Control_L down
  ```

  Hold the button that does nothing and read that line while the game fails to
  react. Four readings, four different culprits, and DOCKER.md has the table.
  It answers in one look what the panel could only answer from memory.

  The panel's readout also gets a **Copy this** button, because the second
  hardest part of a remote diagnosis is getting the text out of the machine
  it is on.

### Notes
- **1.10.52's change to how keys go on the wire did not fix the field report,
  and that is worth recording.** Controller keys now carry a DOM code name and
  so take the same branch of noVNC's `sendKey` a typed key does. The report
  after it was unchanged — "left and right work, the menu button works, nothing
  else does" — which rules the wire format out rather than leaving it a
  suspect, and says the two versions agree because both reach the server the
  same way.

  What is left is narrower and the strip above is pointed straight at it: either
  the pad's buttons are not reaching the page, or they are and the engine is not
  listening for the keys they send. No fix is being guessed at in the meantime;
  the last time this project guessed at an unmeasured fault it guessed wrong
  eleven times.

## [1.10.52] — 2026-09-15

### Added
- **The controller panel says what the pad is actually reporting.** The first
  field report of 1.10.51 was "left and right work, the menu button works,
  nothing else does", and there was no way to tell from here which of three
  faults that was: the browser not reporting the buttons, the page not sending
  keys for them, or the engine not listening for the keys it sent. One symptom,
  three causes, no instrument — which is the shape of the eleven releases this
  project spent on the stutter, so this time the instrument comes first.

  The panel now ends with a live readout: how many pads the browser admits to,
  the name and layout it claims, its button and axis counts, which indices are
  pressed at that moment, and the keys that went out during the last spell of
  play — kept, because nothing is sent while the panel is being read. DOCKER.md
  has a table turning each reading into the thing to do about it.

  `?stats=1` now shows the controller line even when no pad has been seen, so
  the readout is reachable when the fault is that nothing is detected at all.

### Changed
- **Controller keys now go on the wire exactly as the keyboard's do.** Each
  action carries the DOM code name beside its keysym, so `sendKey` takes the
  same branch for a pad press as for the same key typed: noVNC sends a QEMU
  extended key event where it has a code name and the server supports the
  extension, and a plain keysym event otherwise, and 1.10.51 passed no code
  name and so could take the other path.

  Both are legal and it worked in the lab, but it left the controller as the
  one input in the page that could not be compared against a keyboard already
  known to work here. Whether it is what the field is seeing is not yet known —
  the readout above is what will say.

## [1.10.51] — 2026-09-15

### Added
- **A game controller works, on a phone as well as a desktop.** An Xbox pad, a
  Backbone One, or anything else the browser reports as a standard gamepad.

  The engine needed no change for any of it, and could not have been given one
  usefully: input reaches it as X keysyms and pointer reports over the same VNC
  connection the picture comes back on, so a controller is entirely the page's
  business. Buttons become key presses. The right stick becomes the same
  relative pointer motion the captured mouse produces, which is what makes
  turning analog — a nudge turns slowly, where a key can only turn or not.
  Measured over the same 400 ms, a stick at 40% travels 24 mouse pixels against
  321 at full push.

  Sensible out of the box: left stick moves and sidesteps and breaks into a run
  when pushed all the way, right stick turns, RT fires, LT runs, A opens doors
  and confirms in menus, B backs out, the face and shoulder buttons are weapons,
  the d-pad works the menus.

  Two of those needed a decision rather than a default. **A sends space and
  Return together**, because one button has to open a door and choose a menu
  item and the page cannot tell which screen is up; each key is meaningless on
  the other one. And **there is no next-weapon button**, because the 1997 engine
  has no such key — only *select weapon N* — and a page cannot cycle for you
  when it has no idea which weapons you are carrying. A digit for a weapon you
  have not got is ignored, so a cycle would stall on the gaps.

- **Every button is rebindable, from the start screen.** A *Controller* line
  appears under the play buttons once a pad has been seen, with a panel behind
  it: pick an action, press Bind, press the button. The sticks get a deadzone,
  a turn speed, invert, a swap, and a switch for whether a full push runs.

  It is saved in the browser rather than in `.doomrc`, which is not a shortcut:
  the container never sees the controller, so it has nothing to write, and the
  pad on a phone and the pad on a desk are different browsers that usually want
  different layouts. It follows that the panel sends **keys, not intentions** —
  rebind fire in the game's own Options → Setup → Controls and the controller's
  *Fire* has to be pointed at the new key too. The panel says so.

  The line appears only once a pad has been seen, and says nothing at all before
  then. A browser reports no gamepad until a button on it has been pressed, so a
  pad that is paired, charged and idle genuinely is invisible — but a sentence
  explaining that is one every keyboard player would read for ever in order to
  spare a pad owner a single press of a button they are about to make anyway.
  It is in DOCKER.md instead, and the start screen stays as quiet as the entry
  below left it.

  Verified against a synthetic pad in Chromium rather than by inspection: the
  trigger sends `XK_Control_L` down and up, a full push forward sends the arrow
  and the run key together and a half push only the arrow, A sends both of its
  keysyms, B sends the game's Escape without the page's own Escape handler
  taking it, turning produces pointer reports and no key events, inverting
  turns the other way, rebinding moves a binding rather than duplicating it and
  survives a reload, and *Reset to defaults* puts it back.

- **Nothing reaches the game when it should not.** Checked, because a key the
  engine is told to hold is one it goes on obeying: a pad is not read at all
  while the start screen is up, so one knocked off a desk cannot empty a
  chaingun into a room nobody is watching, and losing focus, hiding the tab or
  unplugging the pad mid-game releases whatever it was holding — the same
  hazard the mouse buttons already guarded against, and the same fix.

### Changed
- **The start screen is quiet again.** Pressing Escape mid-game brought back
  the title and then three paragraphs of measurements — sample rates, buffer
  growth, underrun counts, frame rates, byte rates, decode times, gap
  attributions — followed by a sentence explaining that Escape brings the start
  screen back. On a screen you can only be reading because you pressed Escape.

  All of it was built to find the stutter and all of it earned its place doing
  that. None of it earns a place in front of somebody who stopped playing for a
  moment. The figures move behind `?stats=1`, unchanged, and DOCKER.md points
  everything that reads them at the switch. The Escape hint is shown before the
  first game, where it is worth something, and retired once it has been taken.

  One thing still speaks without being asked: sound that is **not** working.
  A refused connection, a worklet the browser would not load, a stream that
  opens and stays quiet — the page still says which, because that is the only
  thing on that screen you cannot check by looking at the game. Sound that is
  working now says nothing.

## [1.10.50] — 2026-09-15

### Fixed
- **The Unraid template's change list stopped at 1.10.21, twenty-eight releases
  ago.** Someone deciding whether to update was reading notes that predate the
  stutter fix, the sound work, and the pause when a door opened.

  It is current now, and written for someone choosing whether to press update
  rather than for whoever is going to maintain this: the stutter and what it
  was, the container saying when one of its own pieces dies, the engine no
  longer spinning on a core or re-reading the WAD mid-level, Escape behaving,
  and the sound arriving 14 ms behind the picture instead of 102. The fifteen
  releases of instrumentation are one entry between them, because none of them
  changes how it plays.

  The 1.10.21 entry claiming a 140 ms delay is left as written and marked as
  superseded, since 1.10.22 corrected it to 48 ms — most of which is the
  pistol's own wind-up animation.

## [1.10.49] — 2026-09-15

### Added
- **The stutter is written up.** PORTING-NOTES.md gains *The stutter while the
  fire button is down*: what it was, the four configurations measured, the
  thirteen suspects cleared before it — the browser, noVNC's JPEG path,
  websockify, the network, CPU contention, a cgroup quota, `-8to24`, X DAMAGE,
  Nagle, the audio stream, numpy, memory reclaim, pointer-input eating — and
  why it took eleven releases to find something the first report described
  correctly.

  It also records what the measuring taught, because four instruments in this
  repository exist only because a confident answer turned out to be an artefact
  of how it was measured: a silence is not a stall, the change that ends a
  silence is not motion during it, the second thing measured wears the minute,
  and `schedstat` cannot see a process that is blocked rather than waiting.

  The README gets the finding in its own list and a link to the detail.

## [1.10.48] — 2026-09-15

### Fixed
- **Quitting the game reported a fault.** The watchdog added in 1.10.46 kept
  watching while the container was shutting down, so a normal quit — which
  tears the helpers down on purpose — came out as:

  ```
  [doom] DOOM exited with status 0
  [doom] noVNC has exited -- nothing works without it
  ```

  True, and useless. The watchdog stops the moment the engine exits, and again
  first thing in cleanup, so nothing it says can be about a teardown. A real
  failure still reports: killing x11vnc outright still gives "x11vnc has
  exited -- nothing works without it".

- **And it quoted a complaint from minutes earlier.** It took the first
  matching line in the process's log, but these logs outlive a restart, so a
  stale `Failed to connect to localhost:5900` from an earlier crash was
  presented as the reason for a later, unrelated exit. It reads the last
  complaint in the final forty lines now.

## [1.10.47] — 2026-09-15

### Fixed
- **The stutter: x11vnc holds the picture back while a mouse button is down.**
  `-wireframe` and `-scrollcopyrect` are both on by default and both are for a
  desktop — they watch for a window being dragged or a pane being scrolled
  while a button is held, and keep the picture back while they decide. A game
  holds the fire button down. There is one window, it never moves, and nothing
  scrolls.

  Measured with the fire button held and the player turning, 80 seconds each:

  | | answers | KB/s | median | p90 | p99 | worst | over 200 ms |
  | --- | --- | --- | --- | --- | --- | --- | --- |
  | both on (the default) | 1271 | 257 | 41 ms | 62 | 91 | 118 | 0 |
  | `-nowireframe` only | 1079 | 250 | 43 ms | 62 | 94 | 118 | 0 |
  | `-noscrollcopyrect` only | 1029 | 121 | 15 ms | 140 | 306 | **341** | **41** |
  | both off | 2774 | 554 | 12 ms | 15 | 24 | 44 | 0 |

  The same run without the button held is 2752 answers at 539 KB/s and a 12 ms
  median — so the cost appears only when someone fires. The two do different
  damage: `-scrollcopyrect` slows everything evenly, which hid the other one,
  and `-wireframe` holds the picture for about 300 ms at a time. `t2` in its
  own default timing string, `0.15+0.30+5.0+0.125`, is how long it waits for a
  window to start moving after a button goes down, and it repaints nothing
  while it waits. **Those 300 ms holds are the stutter**, and that is why every
  field report clustered at 273 to 345 ms.

  Both are off now. With the fire button held the shipping default gives 3099
  answers, 549 KB/s, a 12 ms median, worst 34 ms and nothing over 200 — better
  than the old no-button baseline.

  It never reproduced here for eleven releases because nothing in the lab ever
  held a mouse button: the probe sends no input by design, and the attract demo
  has no pointer at all.

## [1.10.46] — 2026-09-15

### Fixed
- **`DOOM_VNC_8TO24=0` warned that colours "may be wrong in some clients". They
  will be wrong, in this one, and badly.** noVNC cannot use a colour map at all
  — `_handleSetColourMapMsg` drops the connection — so at depth 8 it asks for a
  pixel format of `floor(8/3)` bits per channel, which is two, which is 64
  colours. Measured against the same scene: **50 distinct colours instead of
  15,746**, at half the mean brightness.

  So `-8to24` is not an optimisation that can be traded away for the bandwidth
  it costs; it is the only reason the picture is watchable. The switch stays,
  because it answered its question — the stalls are still there without it —
  but it now says what it does, at startup and in DOCKER.md.


### Fixed
- **A supporting process could die and the container would carry on looking
  well.** Only the engine was waited on. So when x11vnc exited, the game kept
  drawing, the log kept reporting its 35 frames a second, and the only symptom
  was a browser saying "connection lost" with nothing in the container's log to
  explain it — the reason sitting in x11vnc's own log, which nobody knew to
  look at.

  x11vnc, Xvfb, websockify and audiostream are watched now. When one goes, the
  container says which, quotes the first complaint from that process's log, and
  stops rather than sitting there healthy-looking:

  ```
  [doom] x11vnc has exited -- nothing works without it
  [doom]   *** unrecognized option(s) ***
  [doom]   the rest is in /doom/state/x11vnc.log
  ```

- **An option in `DOOM_VNC_ARGS` without its leading dash is refused up front.**
  x11vnc treats anything it does not recognise as fatal, and `noxdamage` where
  `-noxdamage` was meant is an easy thing to type into a template field that
  does not show the dash. The container now says so and declines to start,
  naming the correction, rather than starting an x11vnc that will be dead a
  second later.

## [1.10.45] — 2026-09-14

### Added
- **`DOOM_VNC_8TO24=0`, so the most expensive thing x11vnc does can be turned
  off for a measurement.** With the stall now confirmed — 931 ms with 31 of 87
  looks at the screen showing it changing, x11vnc silent throughout — the
  question is which part of x11vnc is holding it, and `-8to24` is the obvious
  suspect by cost alone. It was hardcoded, so testing it meant a custom build.

  `-noxdamage` and `-threads` need nothing new: `DOOM_VNC_ARGS` already carries
  them. All three are documented in DOCKER.md as diagnostics rather than
  settings, and all four configurations are checked to start and serve a
  picture.

  An earlier A/B of `-8to24` found no difference and that result is withdrawn:
  it counted silences without knowing whether the picture was moving, and on
  that machine nearly all of them were still. Any retest has to count only the
  MOVING ones.

## [1.10.44] — 2026-09-14

### Fixed
- **A silence shorter than about 250 ms could not be judged, and was called
  still regardless.** The screen was read twenty times a second and motion in
  the last 150 ms was discounted, because the change that ends a silence is
  inside it by definition. That left a 220 ms silence with a single sample to
  decide on, and every short one came back "still" whether it was or not — on
  the machine in the field, where two of six were labelled from one sample
  apiece.

  A hundred samples a second and a 60 ms tail leaves about sixteen looks at a
  220 ms silence instead of one. X answers these in a tenth of a millisecond,
  so the extra reads cost nothing worth counting. The verdict now shows its
  working:

  ```
  straight to x11vnc   331 ms at t+119.7s  -- picture MOVING (19 of 27 looks changed), a stall
  straight to x11vnc   249 ms at t+64.9s   -- picture STILL (18 looks, none changed), so x11vnc had nothing to send
  ```

  A silence too short to judge at all now says so rather than guessing.

### Notes
- **The fault does not reproduce here with the picture genuinely moving, and
  that is worth recording.** Every silence this machine produces is a still
  one: the attract demo stands still for seconds at a stretch, and pressing a
  key to stop it doing that aborts the demo and leaves the title screen, which
  is stiller still — twenty-five silences of 234 ms apiece, correctly called
  still, from a script written to create motion.

  Starting an actual level and holding a turn key gives ninety seconds with the
  picture never stopping, 2933 answers, worst 76 ms and **nothing over 200 ms
  at all**. So whatever is stalling x11vnc in the field is not in the stack
  alone; iterating on it has to happen there rather than here.

## [1.10.43] — 2026-09-14

### Fixed
- **The screen watcher only worked at one screen size, which was not the one in
  the field.** 1.10.42 added a check for whether the picture was moving during
  each silence, and it came back from a real machine saying the screen could not
  be read at all — from inside the container, where it certainly could.

  `DOOM_SCALE` decides the screen size, so it is 320x200 as often as 640x400,
  and the two sample strips were hardcoded at 640 wide and 200 down. On a
  320x200 screen that is off the end of it: X answers BadMatch, and the thread
  died on the spot. The size is read from X now, and the strips are placed a
  quarter and a half of the way down whatever it says. Checked at `DOOM_SCALE`
  1, 2 and 4 — 320x200, 640x400 and 1280x800 — and by freezing the engine at
  320x200, where the silence that follows is correctly called still.

- **And it died silently, which is why a one-line bug survived a release.** Any
  failure now says what happened rather than falling back to "the screen could
  not be read": no X socket, X refusing the connection, X refusing the image, or
  the error itself.

### Notes
- A silence longer than two seconds is measured short, because the probe gives
  up waiting after two and asks again. Worth knowing when reading "worst": it is
  a floor, not a ceiling.

## [1.10.42] — 2026-09-13

### Fixed
- **`doom-probe` could not tell a stall from a still picture, and every stall
  it has ever reported here was the latter.** VNC answers a request only when
  something has moved, so a motionless screen produces silences of any length
  and they are the protocol working, not a fault. The probe measured the
  silences and called them all faults.

  Reading the framebuffer directly and lining the two up settles it. Five
  silences over five minutes, 233 to 1612 ms:

  | silence | overlap with a screen that was not changing |
  | --- | --- |
  | 233 ms | 100%, inside a 0.4 s still stretch |
  | 469 ms | 100%, inside a 0.7 s still stretch |
  | 1535 ms | 99%, inside a **6.0 s** still stretch |
  | 1612 ms | 98%, inside a **6.0 s** still stretch |
  | 714 ms | 100%, inside a 5.2 s still stretch |

  Each one ends the millisecond the screen changes again. It is the attract
  demo standing still, reported as x11vnc stalling — and it is what "it is
  x11vnc" was based on in the previous release's notes. That conclusion is
  withdrawn.

  The probe now reads two strips across the middle of the view twenty times a
  second while it measures, and says of each silence whether the picture was
  moving:

  ```
  straight to x11vnc  1399 ms at t+140.1s  -- picture STILL, so x11vnc had nothing to send
  ```

  Two strips rather than one patch, because a patch can sit still while the
  rest of the screen moves. It only works from inside the container, where the
  X socket is; run from another machine it says so rather than guessing.

- **The change that ends a silence is no longer counted as motion during it.**
  x11vnc answers the moment something moves, so every silence is terminated by
  a change and that change falls inside the window by definition. Counting it
  turned "still for 1.4 seconds" into "moving, 1 change" and reversed the
  verdict on the same silences a previous run had called correctly. Motion in
  the last 150 ms no longer counts.

- **The closing advice no longer contradicts the verdict above it.** It used to
  print "a worst of several hundred here is the stutter" directly after
  explaining that every one of them was a still picture.

## [1.10.41] — 2026-09-13

### Fixed
- **`doom-probe` measures both paths at the same instant, and that settles
  whose stall it is.** Measured one after the other, the second path wears
  whatever the minute brings, and this produced two confident and opposite
  findings from the same container under the same load:

  | order | straight to x11vnc | through the proxy |
  | --- | --- | --- |
  | x11vnc first | worst 34 ms | worst **1357 ms** |
  | proxy first | worst **1449 ms** | worst 29 ms |

  Splitting each into halves did not fix it, and believing that it had was the
  worse mistake: both halves still ran in the same order, so the proxy stayed
  in second place and kept the bias. Over five minutes and twenty thousand
  samples it looked conclusive — six stalls against zero, twice over, with and
  without `-8to24` — and the conclusion was that websockify's pure-Python relay
  was the amplifier. It was the running order again, just better dressed.

  Run in two threads at once there is nothing left to confound, and the answer
  is not subtle:

  ```
  straight to x11vnc:  279 ms at t+104.0s,  463 ms at t+104.5s
  through the proxy:   278 ms at t+104.0s,  463 ms at t+104.5s
  ```

  The same stalls, to the millisecond, at the same instants, in both streams.
  **It is one thing upstream of the proxy, hitting every client at once** — so
  the proxy is not the amplifier and never was. The probe now says so itself
  when it sees stalls coincide, and prints when each one happened so the
  coincidence is visible rather than asserted.

### Notes
- **`-8to24` is not the cause.** It looked strong: x11vnc's own manual says the
  mode walks the window tree three levels deep, polls it with `XGetImage()`
  every 50 ms, transforms the whole screen, and "does hog resources" — and our
  Xvfb is depth 8, so every frame goes through it. Built without it and
  measured against the shipping build concurrently, twice, five minutes each:
  six stalls against four, then six against three. No difference. The
  bandwidth is nearly the same too (713 against 676 KB/s), which makes the
  note in PORTING-NOTES.md about `-8to24` being where the bandwidth goes worth
  re-checking.

- Also worth recording: the stalls arrive in **clusters** — 104.0 and 104.5 s,
  or 114.4, 114.9, 146.4 and 150.7 s — rather than evenly. Whatever this is,
  it happens in bursts, and a twenty-second measurement will often miss it
  entirely.

## [1.10.40] — 2026-09-13

### Added
- **`doom-probe` runs from another machine too.** Inside the container both
  paths came back clean on the machine that stutters — through the proxy,
  worst 55 ms, nothing over 200 — while the browser talking to that same proxy
  was seeing this:

  ```
  [doom] picture gap 1452 ms: the browser asked 0 ms in, x11vnc answered 1451 ms later
  ```

  The difference between those two is the network, and every measurement so far
  has been over loopback where a client drains instantly. Give the probe a host
  and it puts the real link in the path:

  ```
  python3 doom-probe.py your-nas
  ```

  x11vnc's port is usually not published, so that line says it could not
  connect and the other still runs.

- **The log says how much of the machine the container was given.** One line at
  startup, `4 core(s) available` or `4 core(s) visible, limited to 150% of
  one`. Read from the affinity mask and the cgroup, so both pinning and a quota
  show up.

### Fixed
- **A shortage of CPU is not the cause, and the warning claiming it was got
  written and then deleted.** The run-queue figures look damning — x11vnc
  waiting 648 ms of every 5000 on the machine that stutters — and the first
  version of the line above told people to give the container more cores.

  Tested before shipping, which is the only reason it is not in this release:

  | container | x11vnc waiting, per 5 s | worst answer | over 200 ms |
  | --- | --- | --- | --- |
  | 4 cores | — | 45 ms | 0 |
  | pinned to 1 core | — | 40 ms | 0 |
  | pinned to 1 core, busy process on it | **2225 ms** | 48 ms | 0 |

  Forty-four percent of wall time on the run queue, more than three times the
  worst real report, and not one answer over 200 ms. Starvation does not
  produce this stall. The figures stay in the log because they are worth
  reading; the advice is gone, and the comment above them now says outright
  that it was ruled out, so nobody spends an evening rearranging CPU pinning
  on the strength of them.

## [1.10.39] — 2026-09-13

### Added
- **`doom-probe`, a stopwatch on x11vnc.** 1.10.38 cleared the browser
  completely — it asks within 1 ms and holds a finished picture for 0 — and
  every gap line came back the same shape:

  ```
  [doom] picture gap 386 ms: the browser asked 1 ms in, x11vnc answered 385 ms later
  ```

  On this machine x11vnc answers in 10 ms and has never once passed 91 over
  repeated runs, so the next thing worth knowing is what it does on the
  machine that actually stutters. The probe asks for pictures the way the
  browser does, straight to x11vnc and then through websockify and the proxy,
  and times both — no browser anywhere in it:

  ```
  straight to x11vnc:     496 answers,  757 KB/s   median  11 ms   p90  16   p99   34   worst   37   over 200 ms: 0
  through the proxy:      496 answers,  665 KB/s   median  12 ms   p90  16   p99   28   worst   32   over 200 ms: 0
  ```

  It sends no input — the attract-mode demo moves the picture quite enough,
  and sending keys would play the game for whoever is at the screen. It
  refuses to report at all on a still picture rather than call that a result,
  and says so plainly when x11vnc wants a password instead of producing
  numbers that mean nothing. Both refusals are tested, by freezing the engine
  and by starting a container with `DOOM_VNC_PASSWORD` set.

  It also runs against a container that is already up, with no pull and no
  restart — see DOCKER.md.

### Fixed
- Nothing this time, and one thing deliberately not shipped. The last
  unwatched boundary is the proxy handing bytes to the browser, and an
  instrument for it was written and then thrown away: across four attempts —
  a bandwidth-limited relay, a client that stopped reading, a client draining
  at 346 KB/s with the queue backing up — the figure never moved once.
  Shipping a number that has only ever read zero is how `rfb._onFlush` got
  wired to something nothing calls two releases ago.

  Ruled out by measurement rather than argument, so nobody has to re-check
  them: the JPEG decode path (0 ms across 3606 pictures in the field), the
  audio stream competing with the picture (identical distributions with and
  without), websockify's missing numpy (it only unmasks the browser's 10-byte
  requests, never the picture), browser-side backpressure (websockify's relay
  loop always reads from x11vnc regardless of what the client is doing), and
  x11vnc eating pointer input before repolling — which its own manual
  describes and which sounded exactly right, but 120 pointer events a second
  changed nothing at all.

## [1.10.38] — 2026-09-13

### Fixed
- **"The browser asked N ms in" was counting keystrokes as frame requests.**
  Both ends of that figure took any traffic from the browser as the browser
  asking for a picture, and during play most of what the browser sends is
  pointer and key events — a player mouse-looking sends them continuously.

  In the proxy that is the worse of the two: it takes the *last* browser
  message before x11vnc answers, so a stray mouse event landing late in a
  silence reads as "the browser asked 422 ms in" when the request for the
  picture went in at 5 and x11vnc sat on it for 457. Every one of those lines
  so far has been open to that reading.

  Both ends now parse the stream properly and count only RFB client message 3,
  the FramebufferUpdateRequest. Parsed rather than sniffed for a byte, because
  a pointer event carries coordinates and any of those bytes can be a 3; the
  parser is tested against eight cases including pointer and key events whose
  payloads are full of 3s, and a request split across two writes.

  Measured against the old definition over the same run, three times: worst
  gap to any send 141, 96, 165 ms against 104, 90, 165 ms to a real request —
  so the old figure ran up to a third high, and twice the worst "ask" it
  reported was a keystroke. (I had said this could only ever make the browser
  look better than it was. It is the other way round.)

### Added
- **How long the page sat on a picture it had already finished with.** noVNC
  asks for the next picture on the same turn it finishes one, so this is the
  page's own share of the wait and nothing else's:

  ```
  ...took at most 5 ms to ask for the next frame after one arrived — 1 ms of
  that with a finished picture already in hand.
  ```

  It is close to zero by construction, and that is the point: it turns the
  figure beside it from ambiguous into an answer. If the page takes 400 ms to
  ask and 0 ms of it was spent holding a finished picture, then the page never
  held anything up — the wait was spent part way through a picture, waiting
  for the rest of it to arrive, and it belongs upstream.

## [1.10.37] — 2026-09-13

### Added
- **The page says how much of the wait was the browser decoding the picture,
  and offers a way round it.** 1.10.36 split the browser's silence into data
  arriving late and the page being slow to ask, and the field answered
  clearly: the page was slow to ask.

  ```
  [doom] picture gap 618 ms: the browser asked 576 ms in, x11vnc answered 42 ms later
  ```

  With no *this browser's own drawing stopped* sentence in the same note —
  meaning the rAF loop never once missed 50 ms — the browser was wide awake
  for the whole 576 ms and simply did not ask. There is exactly one thing in
  noVNC that does that. x11vnc and noVNC settle on Tight (checked on the wire:
  13,000 Tight rects in 25 seconds and nothing else), Tight sends the busiest
  parts of a DOOM screen as JPEG, and noVNC decodes a JPEG by base64-encoding
  it into a `data:` URL, handing it to an `<img>`, and stopping — the message
  pump reads nothing more off the socket and asks for no further frames until
  that image comes back decoded. It is the one place in this client that can
  go quiet for an unbounded time with the main thread perfectly responsive.

  The note now measures it:

  ```
  Decoding held this page up for at most 0 ms of that, the slowest of 650
  pictures taking 65 ms.
  ```

  Reported every run, not just bad ones: a run where this is 0 rules the
  decoder out, and that is worth as much as catching it.

- **`?encoding=hextile`.** Drops Tight from what the browser offers, so the
  server falls back to Hextile, which noVNC writes straight into the
  framebuffer — no base64, no `<img>`, nothing to wait for. Verified on the
  wire: 2147 Hextile rects and not one Tight. It is not the default because
  it is not free — over the same 25 seconds of walking into a room, 873 KB/s
  with Tight against 2286 KB/s with Hextile — so it is a switch to try when
  the figure above says decoding is the problem, not a change to everyone.

  x11vnc has no server-side say in this: it was the obvious place to look
  first, and this build of it has no `-nojpeg` and no encoding options at all
  beyond `-tightfilexfer`.

### Fixed
- **The new figure was wired to something nothing calls.** The first version
  wrapped `rfb._onFlush`, but noVNC's constructor takes a bound copy of that
  method and the display calls the copy, so replacing it afterwards replaces
  something that is never invoked again. It would have reported a confident
  zero on every machine forever, and read as the decoder being innocent.
  It hooks `display.onflush` instead.

- **And gated so the worst cases were the ones thrown away.** The figure only
  counts while the picture is moving, which is the gate that four earlier sets
  of confident numbers needed. But a long enough block stops frames being
  painted, so asking *after* the block reports that nobody was playing — the
  gate would have discarded exactly the blocks worth seeing and kept the
  harmless ones. Asking only before the block was not enough either: it
  dropped a real 300 ms block in one calibration run out of three, because
  the paint rate had dipped under the mark in the instant it began. Either
  end counts.

  Calibrated against known answers, by stopping the render queue from
  draining for a set time and leaving the main thread alone — which is the
  field's case exactly, a responsive browser that is not asking:

  | held | reported | of the page's total wait |
  | --- | --- | --- |
  | 0 ms ×3 | 0, 0, 0 | 125, 133, 130 ms |
  | 300 ms ×4 | 289, 234, 272, 255 | 289, 238, 271, 263 ms |
  | 600 ms ×2 | 573, 656 | 575, 655 ms |

  Every earlier attempt at that stimulus was wrong in a way worth recording:
  an `Image` with no `src` reports `complete === true`, so noVNC takes its
  already-decoded branch, fails the dimension check and returns with the entry
  still at the front of the queue — a permanent deadlock rather than a
  measured hold, and one that showed the real hazard plainly: the pump never
  recovered and the page never asked for another frame again. An image big
  enough to decode slowly is big enough to freeze the main thread, which is
  the opposite of the case being reproduced.

## [1.10.36] — 2026-09-13

### Added
- **The page splits the browser's share of the wait in two.** 1.10.35 showed
  x11vnc answering every request within 0 to 12 ms while the browser went 300
  to 350 ms sending nothing at all — no frame request, no pointer events:

  ```
  [doom] picture gap 478 ms: the browser asked 478 ms in, x11vnc answered 0 ms later
  [doom] picture gap 328 ms: the browser asked 327 ms in, x11vnc answered 1 ms later
  ```

  x11vnc offers no ContinuousUpdates — checked, `_supportsContinuousUpdates` is
  false — so this is strictly one frame per round trip and nothing arrives
  until the browser asks. noVNC asks the instant an update finishes, so that
  silence is either data taking a long time to reach the page or the page
  taking a long time to finish with it. The note now separates them:

  ```
  The picture's data arrived in gaps of at most 35 ms, and this page took at
  most 4 ms to ask for the next frame after one arrived.
  ```

  Checked against a known answer: freezing x11vnc for 600 ms gives 698 ms of
  waiting for data against 20 ms of asking — the wait correctly attributed to
  the data, not to the page.

### Fixed
- **Everything the page measures is now gated on the picture actually moving,
  in one place.** The two new figures came out at 355 ms and 194 ms on a
  machine whose longest real gap was 50, because VNC sends what changed and
  nothing else: a player standing still gets no messages at all and a figure
  gathered then describes the pause, not a fault.

  This is the fourth time in this file that a measurement has been taken
  against a motionless screen and believed — the paint figures, the worst-case
  figures, the byte rate, and now these. There is one `playing()` test for it
  now, and a comment saying why, because the mistake is clearly not one anybody
  makes only once.

### Known
- **X DAMAGE is available and x11vnc is already using it.** Checked directly,
  since it is the obvious thing to suspect when a VNC server seems slow:

  ```
  X DAMAGE available on display, using it for polling hints.
  ```

  It is used as a hint to narrow what x11vnc polls, not as the sole source of
  change, so x11vnc still scans at its `-wait` interval — which is where its
  27% of a core goes. It does not explain the stutter either way: x11vnc has
  the frame ready and answers within milliseconds of being asked.

## [1.10.35] — 2026-09-13

### Added
- **The proxy says which side of the picture connection went quiet.** 1.10.34's
  comparison came back unambiguous — 629 ms with no picture while the sound,
  over the same link, never paused longer than 17 ms, and no CPU starvation
  anywhere in the container. x11vnc went silent on its own.

  VNC is request-driven: the server sends an update only after the client asks.
  So a silence is either the browser not asking or x11vnc not answering, and
  those are opposite faults. The proxy is the only thing that sees both halves
  of the conversation:

  ```
  [doom] picture gap 629 ms: the browser asked 1 ms in, x11vnc answered 617 ms later
  [doom] picture gap 629 ms: the browser never asked during it
  ```

  It only counts a gap that interrupts a picture that was moving. A still
  screen produces silences of any length that are not faults — the server is
  supposed to hold the request until something changes — and the first two
  versions of this reported an idle game's perfectly correct 200 ms holds as
  stalls. Counting reads did not fix it, because an idle picture still gets
  read several times a second; counting bytes did.

- **The CPU watchdog reports time used as well as time waited for.** Waiting
  was only half the question. A process using most of a core is not starved and
  can still be the thing that stalls, and nothing here could see that at all —
  which matters because x11vnc with `-8to24` re-renders in truecolor from a
  depth-8 display, and DOOM changes its palette on every damage flash and
  pickup.

  For scale, measured on a healthy machine at 320×200 with the picture moving:
  **x11vnc 27% of a core, the engine 8%.** The line appears past 50%.

## [1.10.34] — 2026-09-13

### Added
- **The page compares the two streams, which is what is left to ask.** The
  field log cleared everything that had a measurement: nothing in the container
  waited for a CPU in 160 seconds of play, the engine reported no late frames at
  all, and the browser's own drawing never stopped. The browser was ready and
  willing, and for 462 ms nothing arrived.

  The picture and the sound are separate connections to the same proxy over the
  same link, so what the sound was doing during that gap settles where it came
  from. The note now says:

  ```
  ... longest gap 717 ms, 4 late. The sound, over the same link, went quiet for
  at most 26 ms. The sound kept coming through the gap, so the link and the
  proxy were working and the picture alone went quiet.
  ```

  Checked against both answers rather than assumed. Freezing x11vnc alone gives
  a 717 ms picture gap against 26 ms of sound — the picture alone. Freezing the
  whole container, which stops both by definition, gives 667 ms against 639 —
  both together. Ordinary play gives 67 against 72, and no verdict at all,
  because at that size the two figures are the normal lumpiness of two streams
  through one proxy and comparing them announced "both stopped together" about
  a perfectly healthy gap.

  The first version of this counted *bytes* delivered during the gap rather
  than timing, and reported a frozen container as "the picture alone went
  quiet" — when a stall ends the backlog arrives in a burst, so the volume
  comes out right and the stall is invisible. Only the timing shows it.

## [1.10.33] — 2026-09-13

### Changed
- **The CPU-wait line says the worst single stretch, not just the total.**
  Reported from the field:

  ```
  [doom] waiting for a CPU in the last 5s: doom 120ms x11vnc 275ms Xvfb 124ms
  ```

  That is real starvation — x11vnc is the one process here doing work per
  frame, and it is being denied a core — but 275 ms in five seconds is 5% of
  the time, and a total cannot tell 275 ms spent in one stall from the same
  275 ms spread over fifty. One of those stutters; the other does not. It is
  sampled four times a second now and reports both:

  ```
  [doom] waiting for a CPU in the last 5s: x11vnc 328ms (worst stretch 43ms)
  ```

  Measured under sixteen competing processes at raised priority, the waiting
  turns out to be spread thin — 300 to 360 ms in total, never more than 46 ms
  at a stretch. A host that produces a 483 ms gap in the picture would have to
  look very different from that.

### Added
- **The page says when the browser's own drawing stopped.** Everything the page
  does happens on one thread: noVNC decoding the picture, the sound where it
  runs through the `ScriptProcessorNode` fallback rather than a worklet, and
  the probe that counts painted frames. If that thread is busy, frames that
  arrived perfectly well are never painted — and from inside the container that
  is indistinguishable from frames that never arrived. It was the last link in
  the chain with nothing watching it:

  ```
  Picture, while you were playing: best 36 frames a second of the 35 the game
      draws, arriving at 1137 KB/s, longest gap 83 ms, 16 late. This browser's
      own drawing stopped 1 times, worst 67 ms — frames that arrive during one
      of those are never painted.
  ```

  One stall of 67 ms in twenty seconds is this machine being healthy. The
  figure to compare against a 483 ms gap in the picture is the one from the
  machine that has the gap.

## [1.10.32] — 2026-09-13

### Added
- **The container says who is waiting for a CPU rather than using one.** A
  process that is ready to run but not scheduled looks exactly like a slow
  process from the outside, and everything here shares the host's cores with
  whatever else that host is running. `/proc/PID/schedstat` says which: its
  second field is nanoseconds spent on the run queue wanting to run. A line
  every five seconds when something waited more than 2% of the interval, and
  nothing when nothing did:

  ```
  [doom] waiting for a CPU in the last 5s: doom 450ms x11vnc 430ms
  [doom] waiting for a CPU in the last 5s: doom 420ms x11vnc 286ms Xvfb 255ms audiostream 114ms
  ```

  This is the question left after the first trustworthy picture-side reading:

  ```
  Picture, while you were playing: best 36 frames a second of the 35 the game
      draws, arriving at 457 KB/s, longest gap 613 ms, 165 late.
  ```

  Best 36 of 35 means the path can carry the full rate, so it is not bandwidth
  and not throughput — 457 KB/s at 320×200 is the whole stream. What it is, is
  full speed punctuated by stalls: 165 frames of about 1600 arriving late, one
  gap of 613 ms. The sound over the same link in the same session underran
  three times while holding 20 ms, which a 613 ms stall would have destroyed,
  so the stall is not the network either. That leaves the one process in this
  container that does real work per frame, and whether it is being starved or
  stalling on its own is exactly what the new line answers.

## [1.10.31] — 2026-09-13

### Fixed
- **The picture figures were being wiped while you read them, which is how they
  came to say the opposite of the truth.** They aged out on a 15-second timer
  that kept running while the start screen was up — and the start screen can
  only be read once you have stopped playing, so what you read was whatever had
  survived the last wipe, measured against a motionless picture.

  That made every reading of them incomparable, and one of those readings is
  what I used to conclude the picture was bandwidth-limited. It is not: a
  quarter-size run reported **8 frames a second where full size reported 22**,
  which is the wrong way round for a bandwidth limit and should have been the
  end of that theory rather than a puzzle.

  The figures now describe the last spell of play and hold until play starts
  again, so what is on screen is always the run just finished:

  ```
  Picture, while you were playing: best 35 frames a second of the 35 the game
      draws, arriving at 1130 KB/s, longest gap 67 ms, 16 late.
  ```

  The advice about `DOOM_SCALE=1` that went with the old figures is gone with
  them.

### Known

- **Removing `-8to24` from x11vnc cuts the picture's bandwidth by 2.8x and
  breaks it.** Worth writing down, because it looks like the obvious answer to
  a link that cannot carry the picture. x11vnc is given a depth-8 display and
  presents it to clients as 32-bit truecolor, so four times as much has to be
  compressed as the engine actually produced. Serving the colour-mapped
  framebuffer instead measured **457 KB/s against 1268**, at the same 35 frames
  a second — and noVNC, which has no colour-map support, renders it as a green
  and black mess. The bandwidth was real; the picture was not.

  `DOOM_SCALE=1` does measure smaller here — 474 KB/s against 1265, at 34.4
  frames a second against 35.0 — but it does not follow that it helps, and in
  the field it did not: see the fix above. The bandwidth numbers on this page
  are measurements of this machine and nothing more.

## [1.10.30] — 2026-09-13

### Fixed
- **The sound could be left permanently out of step when the listener's socket
  filled, which is what "super loud, then cut out, then back to normal" is.**
  `write` on a socket takes as many bytes as it feels like — any number, not
  necessarily a whole 16-bit stereo frame. audiostream dropped whatever was
  left of the period when the socket came back `EAGAIN`, wherever in a frame
  that happened to be. The listener is then holding half a frame, and every
  sample after it is assembled from the wrong pair of bytes: not a glitch, but
  full-scale noise that never recovers until the page's ring buffer gives up
  and resynchronises.

  What is owed is now kept and offered again next period, and when the backlog
  is too old to be worth sending, **whole frames** are dropped from the front
  of it. A gap in the sound is a click; a gap that is not a whole number of
  frames is noise for ever.

  Driven against a socket that takes an arbitrary number of bytes at a time,
  the old code handed over **73,701 bytes — not a multiple of four**, so the
  listener's stream was genuinely not a whole number of frames. The new code
  delivered the whole test stream in order. The audible noise itself was not
  reproduced end to end: several attempts foundered on faults in the test
  rather than the program, and what is demonstrated is the misalignment, not
  the sound it makes.

### Added
- **The page says how fast the picture is arriving, not just how much of it
  gets painted.** A container reporting a steady 35 frames a second to a page
  painting 11 means two thirds are going nowhere, and only the byte rate says
  whether that is the link or something before it:

  ```
  Picture: 11 frames a second painted (the game draws 35), best lately 11,
      arriving at 402 KB/s, longest gap 179 ms, 13 late.
  ```

  Well short of 35 now also says what a full-rate picture costs, because the
  answer is usually a setting rather than a bug. Measured here, turning
  continuously at `screenblocks 10`:

  | | bandwidth | painted |
  | --- | --- | --- |
  | `DOOM_SCALE=2` (default, 640×400) | 1265 KB/s | 35.0/s |
  | `DOOM_SCALE=1` (320×200) | **474 KB/s** | 34.4/s |

  A link that can carry 400 KB/s gets a third of the frames at the default
  size and very nearly all of them at half it.

### Known

- **noVNC's compression and quality settings do nothing here.** Tried against
  the same 20-second turn, since a constrained link was the suspicion:
  defaults 1263 KB/s, compression 9 1259, compression 9 with quality 8 1634,
  with quality 5 1175. x11vnc is not honouring them in any useful way, so
  there is no bandwidth to be had from that direction and the picture costs
  what it costs.

## [1.10.29] — 2026-09-13

### Fixed
- **The engine no longer waits for the X server to say it has finished with
  each frame.** `I_FinishUpdate` handed the picture over with `XShmPutImage`
  asking for a completion event, then blocked until it arrived. That is correct
  if you are about to overwrite the shared image — but the next frame is a tic
  away, 28 ms, and the server copies it in microseconds. What the wait actually
  did was couple the engine to how busy the X server is, and the X server is
  busy because x11vnc is reading the same framebuffer as fast as it is allowed
  to.

  Measured in the field at **74 ms in that wait for one frame**, with the
  drawing itself under 2 ms. It is gone; the frame is handed over and the
  engine carries on. The loop was also pumping input events, which `I_StartTic`
  does every tic through `NetUpdate` anyway, so nothing is lost — verified by
  playing through it.

  Nothing else moved: delivery here is median 33 ms between painted frames, p90
  34, p99 52, and keypress-to-picture 154 ms with sound 13 ms behind it, all
  unchanged.

### Changed
- **The frame report names where a slow frame went, instead of lumping four
  things under "drawing".** The field logs had frames spending 36 ms in
  `drawing` with the picture handover at zero, which said only that the time
  went somewhere else. A frame is now split into the tic wait, the sound
  update, and the three parts of the draw — handing the picture to X, pumping
  input, and the rendering itself:

  ```
  frames: 176 in the last 5s, 12 late, 5 over 50 ms, 18% of a core; worst 59 ms
      (tic 59, sound 0, draw 0 = picture to X 0 + input 0 + render 0);
      woken 30 ms late at worst, holding the last 8 ms of each tic
  ```

  `input` is the other place a frame touches the X server — `I_StartTic` pumps
  the event queue and warps the pointer through there — so between it and the
  handover, a stall in the X path now has a name.

## [1.10.28] — 2026-09-13

### Fixed
- **Esc is handled by the page now, instead of hoping the browser does it.**
  Releasing the mouse on Esc was always the browser's own behaviour; the page
  only watched for the result. Two releases went by trying to fix Esc by
  reasoning about what browsers do, and both were wrong, because the whole
  mechanism lived somewhere neither I nor any test here could reach — a
  synthetic keypress does not trigger the browser's native pointer-lock
  release, so every test passed on a build that was broken in the field.

  The page now takes the key in the capture phase and asks for the release
  itself, with a fallback that shows the start screen anyway if neither the
  pointer lock nor fullscreen produced an event. That is the failure this
  exists for. Verified with a real Esc keypress in all four combinations —
  fullscreen and windowed, with and without Keyboard Lock — which is the first
  time this behaviour has been testable at all.

### Changed
- **The engine holds on to the CPU through the end of each tic, instead of
  sleeping and hoping to be woken on time.** 1.10.26 added a report of how
  late the kernel hands the CPU back, and the field answer was **22, 32 and 43
  ms** — against under a millisecond here. A machine consistently a few
  milliseconds late runs every frame late, and the same logs show stretches of
  126 to 142 frames per five seconds where 176 is the full rate: 28 a second
  instead of 35, which is a shudder rather than an occasional hitch.

  So the wait is now split. While there is time to spare it sleeps a
  millisecond at a time and costs nothing; through the last stretch — where
  being handed the CPU late is what costs a frame — it holds on. The stretch is
  as long as that machine has actually been late and no longer, capped at 8 ms,
  so a punctual machine spins one millisecond in twenty-eight:

  | | engine CPU, playing |
  | --- | --- |
  | 1997 (spin the whole wait) | 96.5% of a core |
  | 1.10.25–27 (sleep only) | 5% |
  | now, on a punctual machine | 9% |
  | now, worst case on a late one | ~28% |

  `DOOM_TIC_SPIN_MS=0` turns it off and restores the 1.10.25 behaviour.

  **This is not demonstrated to fix the stutter.** It cannot be: the fault is a
  property of the host's scheduler, and this machine does not have it — 24
  competing processes at raised priority against a container held to 0.6 of a
  core still produced not one late frame. What is measured is the mechanism, on
  the reporting machine, and that the countermeasure costs 4 points of CPU
  where it is not needed. Frame delivery here is unchanged: median gap 33 ms,
  p90 34, p99 52.

## [1.10.27] — 2026-09-13

### Fixed
- **Esc now releases the mouse and brings the start screen back, in both modes
  and every browser.** It did not, in fullscreen, on a browser that granted
  Keyboard Lock: the page deliberately kept Esc for the game there, and getting
  out meant *holding* it.

  That behaviour was correct for a premise that stopped being true in 1.10.14.
  Esc was taken from the browser because Esc was the game's own menu key and
  Pointer Lock hands Esc to the browser — but the menu moved to `` ` `` in
  1.10.14 and the lock stayed behind. Keyboard Lock also needs a secure
  context, so the same container answered Esc one way over HTTPS and another
  over plain HTTP on a LAN, which is not something anyone should have to know
  about. The lock is gone: `` ` `` opens the game menu, Esc gets you out, and
  the note on the start screen says so in one sentence instead of three
  depending on circumstances.

- **The picture readout added in 1.10.26 reported a player standing still as a
  disaster.** VNC sends what changed and nothing else, so a still screen paints
  nothing — and the line read `1 frames a second painted, worst wait 1467 ms`,
  which is true and completely misleading. It now says a still screen is still,
  counts gaps only while the picture is actually changing, and reports the best
  second lately, which is the honest answer to whether the path can carry 35 a
  second:

  ```
  Picture: 35 frames a second painted (the game draws 35), best lately 36, longest gap 67 ms, 6 late.
  Picture: still — nothing is changing on screen. Best lately 35 frames a second, of the 35 the game draws.
  ```

  This is the same mistake a test harness in this project made a release
  earlier, when it reported its own robot standing still as a 200 ms stall. It
  is worth naming twice: anything that measures a VNC screen has to know the
  difference between nothing arriving and nothing happening.

## [1.10.26] — 2026-09-13

### Added
- **The page says how many frames it actually painted, next to what the sound
  is doing.** The engine can report a healthy 35 frames a second while the
  browser paints far fewer — VNC sends only what changed, websockify relays it
  in Python, and the tab still has to decode and draw it. Nothing in the
  container can see the far end of that. The overlay now reads:

  ```
  Picture: 35 frames a second painted (the game draws 35), worst wait 100 ms, 12 arrived late.
  ```

  It counts the canvas actually changing, which is the one measure a frame that
  was sent and never arrived cannot fool, and it stops counting while the
  overlay is up — otherwise reading the figures records the reader as the worst
  stutter of the session, which is what the first version of this did.

  With the engine's own line in the container log, the two together say which
  half of the path a stutter is in. Not knowing that has cost two releases.

### Changed
- **The engine's frame report now counts frames that missed their slot, not
  just outright slow ones.** A tic is 28.6 ms; a frame a quarter longer than
  that means the engine runs two tics and draws once, and the picture jumps.
  The old threshold was 50 ms, so a steady stream of 40 ms frames — a judder —
  read as a clean log.

  It also reports how late the kernel was handing the CPU back. The wait for
  the next tic sleeps about 28 times a tic, and every one of those asks to be
  woken at a particular moment. A frame that misses its slot while the engine
  is using three per cent of a core was not the engine's doing, and that
  number says so.

### Known

- **Two things were tried against the remaining stutter and both were worse.**
  Written down because the measurements cost more than the changes would have.

  | Tried | Result |
  | --- | --- |
  | Sleeping straight to the tic boundary — one sleep a tic instead of 28, so one moment for the kernel to be late with | **Much worse.** Lands a whole tic past the boundary half the time: 88 frames in every 176 took 57 ms instead of 28, against 0 with the millisecond poll. A judder, introduced deliberately. Not kept. |
  | x11vnc at its own `-wait 20 -defer 20` instead of the 5 ms 1.10.21 set | **Much worse.** Median gap between painted frames 44 ms against 33, p90 65 against 34, and 1471 frames delivered in 60 seconds against 2054. The aggressive polling is what delivers 35 a second. Kept. |

  The second is worth stating plainly: 1.10.21 set those flags on the strength
  of a latency figure that turned out to be wrong, and 1.10.22 said as much.
  They are right anyway, for a different reason than the one given.

## [1.10.25] — 2026-09-13

### Fixed
- **The engine was spinning on a core waiting for the next tic, and starving
  everything that had to get the picture out.** `TryRunTics` has a loop that
  waits for the tic it is about to run, and in the 1997 sources that loop has
  no sleep in it: between one tic and the next — five sixths of the time, at 35
  tics a second — the engine asked the clock whether it was time yet as fast as
  the machine could manage. Measured here at **97% of a core** to draw a
  320×168 view.

  On a 1997 machine that was free, and getting to the tic the instant it
  arrived was the point. In this container the same box is also running an X
  server, a VNC server reading that server's framebuffer, a websocket proxy in
  Python, a mixer and a synth. A millisecond sleep in that loop is 35 times
  finer than the tic being waited for, so nothing about the timing changes:

  | | engine CPU, playing |
  | --- | --- |
  | before | 96.5% of a core |
  | after | 5% of a core |

  What that costs when the CPU is contended, measured in the browser by
  watching the canvas actually change, 70 seconds of walking and turning with
  the container held to one core:

  | | frames painted | median gap | p90 | p99 |
  | --- | --- | --- | --- | --- |
  | before | 1915, 1953 | 33 ms | **67 ms** | **84 ms** |
  | after | 2441, 2449 | 33 ms | **34 ms** | **51 ms** |

  The old build drops a quarter of its frames — 27 a second reaching the
  browser instead of 35 — and the ones that arrive come unevenly. Two runs each,
  the figures repeat. Keypress-to-picture is unchanged at 156 ms and
  sound-behind-picture at 14 ms, so none of this was buying responsiveness.

  The melt between screens had the same spin in its own inner loop, costing a
  second of a core at every level start. Same fix.

### Added
- **The engine says when its frames run late, and when it is using more CPU
  than drawing one takes.** A line every five seconds if either is true and
  nothing when neither is, so a quiet log is a clean one — the same bargain
  audiostream's underrun reporting makes. It names where the time went, split
  between waiting for the tic, drawing, and handing the finished picture to the
  X server:

  ```
  frames: 167 in the last 5s, 2 over 50 ms, 4% of a core; worst 95 ms
      (tics 28, draw 67, of which handing over the picture 67)
  ```

  The screen melt is excluded, being slow on purpose. A spinning engine is
  invisible in a frame count — the picture keeps arriving, just not on time —
  which is why the CPU figure is in there. Diagnosing the fault above took
  per-process CPU sampling inside a container; it should take reading the log.

## [1.10.24] — 2026-09-12

### Fixed
- **The engine was re-reading the WAD during play, which is what a pause when
  a door opens or a new monster appears actually is.** The zone allocator is
  the cache for every wall texture, every sprite and every level structure in
  play, and it was being given **two megabytes**. `i_system.c` initialises
  `mb_used` to 6, but `M_LoadDefaults` runs before `Z_Init` and overwrites it
  from the config table, where the default was 2 — and every config file this
  container has ever written says `mb_used 2`.

  Two megabytes does not hold a level's textures and the sprites of the things
  standing in it, so the zone spends the game purging what it is about to want
  again and reading it back. Measured on the shareware attract demos, counting
  every lump that had to be re-read after startup, over the same three minutes
  of the same demo each time:

  | zone heap | lumps re-read | bytes |
  | --- | --- | --- |
  | 1 MB | 1278 | 3094 KB |
  | **2 MB (what shipped)** | **230** | **825 KB** |
  | 16 MB | 41 | 320 KB |
  | **32 MB (now)** | **33** | **288 KB** |

  What is left at 32 MB is first-time loads and nothing more. And that is the
  smallest WAD and the smallest levels in existence — a retail IWAD has larger
  levels and many more monsters to hold sprites for, so the same 2 MB has more
  to thrash over.

  Those re-reads land exactly where the pause is reported: a door revealing a
  texture nothing was using, or a monster coming into view for the first time.
  `W_ReadLump` is a blocking read on the thread that draws the frame, and the
  IWAD is a symlink to whatever was mounted — on a NAS that is a share on the
  array, possibly behind a FUSE layer, possibly a disk that has spun down. The
  sound keeps playing while the picture waits, because the mixer is a separate
  process.

  The new value is a floor rather than a setting: a config file asking for
  less is a 1997 answer to a problem nobody has, so it is raised. Asking for
  more still works. The engine's resident memory went up by under a megabyte
  in three minutes of play — the zone is allocated, not touched.

  Measured on this machine the re-reads were too cheap to produce a visible
  stall, so the pause itself is not reproduced here; what is measured is that
  the reads are gone.

## [1.10.23] — 2026-09-12

### Changed
- **The sound effects are mixed where they are sent, and the separate sound
  server is gone from this path.** It was the last piece of the 1997
  arrangement still in the way: the engine started a child process, the child
  mixed 512 samples — 46 ms — at a time and wrote the result down a pipe, and
  `audiostream` read that pipe a period at a time and sent it on. A shot
  therefore waited half a block to be mixed at all and then waited its turn in
  a queue, all of it behind sound that had already been decided.

  `audiostream` now links the same mixer — `sndserv/soundsrv.c` compiled with
  `-DSNDMIX_LIB`, which takes its `main()` out and leaves `grabdata`,
  `initdata`, `addsfx` and `mix` — and calls it once per 12 ms period, and the
  engine sends the same `'p'` commands it always sent to a unix socket instead
  of to a child's standard input. Nothing about the mixing changed; what
  changed is who calls it and how often.

  Measured in the browser off the audio clock, twenty shots a run, music
  muted so the measurement starts from silence:

  | | keypress → picture | keypress → sound | sound behind picture |
  | --- | --- | --- | --- |
  | 1.10.22 | 155 ms | 259 ms | **102 ms** |
  | this release | 154 ms | 169 ms | **14 ms** |

  Repeated: 107 ms against 10 ms. The sound is now within a frame of the
  picture, which is where it was always supposed to be.

  This also explains 1.10.22's useful negative — shrinking the transport from
  93 ms to 23 ms changed nothing measurable. The queue was never the stage
  that mattered. The separate process was: a mixer that decides 46 ms at a
  time and runs ahead of the reader is 90 ms of delay no matter how short the
  pipe between them is.

- The sound server itself is unchanged and still built, for the one case that
  still uses it: a container sharing the host's PulseAudio socket. Its
  pipe-writing backend (`SNDBACKEND=stream`, added in 1.10.15) has no caller
  left and is removed.

- **The browser now asks for 25 ms of sound in hand rather than 40.** On a
  machine that can sustain it that is 15 ms less delay between a shot and
  hearing it; on one that cannot, the buffer grows to what that machine needs
  as it already did, so there is nothing to lose by asking for less first.

### Added
- **audiostream says when a producer could not keep up.** A period short of
  sound is padded with silence, and silence is exactly what an absent sound
  already sounds like — the one fault in the audio path that cannot be heard.
  It now prints a line when it happens, and nothing when it does not, so a
  quiet log means a clean one.

## [1.10.22] — 2026-09-12

### Fixed
- **The "140 ms video delay" in 1.10.21's notes was wrong, and is corrected.**
  It was measured from a keypress to the muzzle flash, and the pistol spends
  four tics in `S_PISTOL1` doing nothing before `A_FirePistol` runs — 114 ms
  of deliberate weapon animation, counted as delay. Timed against the menu
  key, which the game acts on the moment it arrives, the picture answers in
  **48 ms**: about one of the engine's 35 frames a second plus a browser
  repaint, which is the floor rather than a fault.

  The replacement video path those notes proposed was then built — the
  engine's frames pushed down the WebSocket beside the sound, x11vnc reduced
  to carrying the keyboard and mouse, the palette applied in the browser. It
  works and it is exactly as fast, 48 ms either way, while sending 1382 KB/s
  against VNC's 941 when walking. It is on the
  `claude/video-stream-experiment` branch and is deliberately not merged.

  What is left that is real: the sound arrives about 150 ms after the picture
  it belongs to.

## [1.10.21] — 2026-09-12

### Fixed
- **`sha-<short>` image tags pointed at whichever build finished last.** The
  same commit is built twice — once when a branch is pushed, once when it is
  merged — and since 1.10.17 gave images a version stamp those two builds
  differ, so both writing the same tag left it ambiguous. Only default-branch
  builds write it now; a branch build is still reachable by its branch name,
  and PR builds get a `pr-<n>` tag so they are never left with none.

### Changed
- x11vnc no longer naps, and polls and defers at 5 ms rather than 20. Its
  defaults are meant for a desktop nobody is waiting on — `-nap` deliberately
  slows polling when activity is low, which is the state of a DOOM screen in
  the instant before you press fire. `DOOM_VNC_WAIT`, `DOOM_VNC_DEFER` and
  `DOOM_VNC_ARGS` expose these.

  **It made no measurable difference**, and is kept only because the defaults
  are wrong in principle for something being played. See below.

### Known

- **The picture is about 140 ms behind the keypress, and this release does not
  fix it.** That is separate from the sound, which is a further 150 ms and did
  come down. Measured from the browser dispatching the keydown to the pixels
  arriving back:

  | Suspected | Measured |
  | --- | --- |
  | The browser drawing | 11 ms of the total |
  | x11vnc `-nap`/`-wait`/`-defer`, 20 → 5 → 1/0 | no change outside noise |
  | x11vnc `-threads` | no change |
  | `-8to24 poll` | no change |
  | Pixel volume (`DOOM_SCALE=1`, a quarter) | 139 vs 143 ms |
  | The audio sharing the proxy | 143 with, 145 without |
  | Throughput | 35 frames a second, arriving in pairs 2 ms apart |

  Nothing is starved and no knob touches it. What remains is structural: VNC
  is request/response, so a frame goes out only once the client has asked for
  the next, which it does after decoding the last; the engine samples input
  and draws once a tic, 28 ms apart; and all of it crosses websockify twice.

  Getting it materially lower means not using VNC for the picture — pushing
  the engine's frames down the WebSocket that already carries the sound, with
  the palette applied in the browser. That replaces the video path rather than
  tuning it, and has not been done.

## [1.10.20] — 2026-09-12

### Fixed
- **The sound got later the longer you played.** 1.10.19 made the browser's
  buffer adaptive but gave it no way to give anything back except a hard
  ceiling far above, so every block the browser was too busy to fetch left
  more sound sitting in it — permanently. Measured over 55 seconds of play,
  the delay walked from 35 ms up to 195 and kept going. Reported from the
  field at 80–85 ms and climbing, which is what sent me looking.

  Two clocks are involved and nothing makes them agree: the container sends
  on its own schedule and the browser plays on its sound card's. So playback
  is now nudged instead — a fraction fast while there is too much in hand, a
  fraction slow while there is too little, never more than two per cent. That
  is inaudible on gunfire and door mechanisms, and far less audible than
  dropping the frames outright, which clicks. It corrects about 20 ms a
  second, comfortably faster than the drift arrives.

  Over the same 55 seconds it now holds a median of 38 ms and never passes 80,
  with no audible gap. The same measurement on 1.10.19 gave a median of 93 and
  no upper bound at all.

- **The buffer's target could sit still while underrunning for ever.** Growth
  on an underrun and decay over time were set to exactly cancel for a machine
  that stumbles every ten seconds. Decay is now half as fast, so the target
  climbs until the underruns stop and only then drifts back down to whatever
  that machine actually needs. Same run: five underruns before, two after.

  When it has grown, the start screen says so and why.

## [1.10.19] — 2026-09-12

### Changed
- **The sound is about 220 ms quicker off the mark.** Measured from the
  keypress to the first sample of the gunshot leaving the browser: 542 ms
  before, 324 ms now. Of what is left, 168 ms is the picture — the muzzle
  flash arrives that long after the key too, which is VNC and the input path,
  not the audio — so the sound now sits about 146 ms behind what you see,
  where it was well over twice that.

  Four things, found by measuring each stage rather than guessing at them:

  - **The music pipe was four times the size it was meant to be**, holding
    371 ms instead of the 93 ms the comment beside it claimed. An arithmetic
    slip: it was sized in frames where the rate was in bytes.
  - **The mixer's period was halved**, 512 frames to 256, which is 23 ms to
    12 ms of granularity for everything downstream.
  - **The browser's buffer is adaptive now** instead of a fixed 140 ms guess.
    It starts at 40 ms and asks for more only when it actually runs dry,
    giving the cushion back after eight quiet seconds. How much a machine
    needs depends on the machine, the browser and what else the page is
    doing, none of which can be known from here — a fixed figure is either
    too much for everyone or too little for somebody.
  - **The fallback's own buffer came down** from 4096 frames to 1024. A
    `ScriptProcessorNode` is handed its work roughly two buffers ahead of
    when it is heard, so that alone was about 190 ms.

  Verified over 32 seconds of walking, shooting and menus on both paths: no
  dropouts, no underruns, and the buffer never had to grow past its starting
  40 ms.

  The start screen now shows how much sound is being held, which is the same
  thing as how far behind it is.

## [1.10.18] — 2026-09-12

### Fixed
- **Sound never worked anywhere but localhost.** 1.10.15 played it through an
  `AudioWorklet`, which browsers expose only in a *secure context* — HTTPS, or
  localhost. This page is normally served over plain HTTP from a machine on
  the network, where `ctx.audioWorklet` is simply absent, and asking it to
  load the worklet threw. Which is every real installation of this container:
  the one arrangement where it did work was the one every test used.

  What made it hard to see from either end is that `AudioWorkletNode` *is*
  defined in an insecure context even though `ctx.audioWorklet` is not, so the
  support check passed and the failure came later — and until 1.10.17 it came
  silently, leaving a container whose log was perfectly healthy because
  nothing in the container was wrong.

  Where there is no worklet the sound now goes through a `ScriptProcessorNode`
  instead: deprecated for years, implemented everywhere, and needing no secure
  context. Both feed the same ring buffer, in `doom-ring.js`, which the page
  and the worklet load from the one file so they cannot drift apart. The
  fallback keeps a little more sound in hand, since it runs on the main thread
  alongside noVNC's decoding. Measured over 37 seconds of play on an insecure
  origin: no dropouts, and audio arriving at 22045 Hz against a nominal 22050.

  The start screen says which of the two is in use.
- **Branch builds failed on the version stamp 1.10.17 added.** It was
  substituted with `sed s/…/…/`, and the stamp for a non-release build carries
  the branch name — which in this repository contains a slash, ending the
  substitution early. Released images were never affected, since a release
  stamp is just the version, but every other build broke.

  The substitution is a literal replace now rather than a regular expression,
  and the value is narrowed to characters that cannot escape the HTML text or
  the JavaScript string it lands in. The branch name is gone from the stamp as
  well: it identified nothing the commit did not.

## [1.10.17] — 2026-09-12

### Added
- **The page now says what the sound is doing, and which build it is.** Two
  releases running, "no sound" has been impossible to tell apart from "sound
  arriving and inaudible" from the outside: the container's log is identical
  in both cases, because everything that decides whether sound reaches you
  happens in the browser. The start screen now carries two lines — the sound's
  live state (`Sound: on — 22050 Hz in, 48000 Hz out, 12.4 s received`, or the
  reason there is none) and the build the page itself came from.

  The build stamp is printed at container startup too, as `DOOM <version>`.
  The two disagreeing is a complete diagnosis on its own: it means the browser
  is running an older client than the container it is talking to, which
  nothing in the container's log can reveal, because nothing in the container
  is wrong.

## [1.10.16] — 2026-09-12

### Fixed
- **1.10.15's sound never reached some browsers, and said nothing about it.**
  The container was mixing audio and serving it correctly; the page asking for
  it was the one from the release before, still in the browser cache.
  websockify serves the client with Python's `SimpleHTTPRequestHandler`, which
  sends `Last-Modified` and nothing else — no `Cache-Control`, no `ETag`. Given
  no freshness, a browser is entitled to guess one and reuse what it has
  without asking, so a tab could go on running a client from two releases back
  while the image underneath it was current. Nothing in the container's log
  looks wrong in that state, because nothing in the container is wrong.

  The client is now served `Cache-Control: no-cache`, which means ask first,
  not do not store: the ordinary answer is a 304 and the same traffic as
  before. This applies to every future release, not just this one — a hard
  reload is no longer needed after an update.

### Changed
- **Sound that does not work now says why.** It used to fail silently in every
  case — no Web Audio, a worklet the browser would not load, a refused
  connection, a container sending nothing — which left no way to tell a silent
  game from a broken one. Each of those now puts a line at the bottom of the
  page saying which it was. The audio watchdog also catches the case where the
  connection opens and nothing ever comes down it.
- The mouse-capture and sound messages no longer overwrite each other; they
  have a slot each.
- The audio worklet is served as a file rather than built from a `blob:` URL.
  Both are legal and the blob worked everywhere it was tried, but a worklet
  that fails to load takes the sound with it, and a plain URL has fewer ways
  to be refused.

## [1.10.15] — 2026-09-12

### Added
- **Sound, in the browser.** The game has had effects and music since 1.10.0,
  but only a container sharing the host's PulseAudio socket could play them —
  which is no use at all when the container is on a server in another room and
  you are looking at it through a browser. Now the audio arrives on the page,
  over the port that was already published. Nothing to install, nothing to
  mount, no flags: click to play and it plays.

  There was no shortcut to it. The container has no sound card and no sound
  daemon, and cannot practically be given one — the only packaged PulseAudio
  brings systemd, GStreamer, cairo and a set of video codecs with it, 172
  packages, to do a job that is adding two streams together. VNC was no help
  either; it carries a picture and nothing else.

  So the sound server gained a third output that writes raw PCM to a pipe, the
  engine learned to render music into another one, and a new `audiostream`
  mixes the two on a real-time schedule and sends 16-bit stereo to the page,
  where an `AudioWorklet` plays it through a ring buffer that absorbs the
  difference between the container's clock and your sound card's. Reading on
  that schedule is also what paces the game's audio, the same job a blocking
  write to `/dev/dsp` did in 1997.

  About 700 kbit/s at the default 22050 Hz; `DOOM_AUDIO_RATE` takes `11025` or
  `44100` instead. The image is no larger. Sharing the host's PulseAudio
  socket still works exactly as before and still takes precedence — set
  `PULSE_SERVER` and the sound goes there rather than to the browser. Stock
  `/vnc.html` is unchanged, and silent, as it always was.

## [1.10.14] — 2026-09-12

### Changed
- **The backquote key (`` ` ``) now opens the menu**, everywhere, out of the
  box. Escape still does whenever the browser lets it through — this only adds
  a key — but in a window, and on Firefox and Safari in any mode, the browser
  keeps Escape for releasing the pointer and it never reaches the game. The
  binding was already there under Options → Setup → Controls; it just
  defaulted to Escape, which made it do nothing until somebody found it. A
  config written by an older build is promoted on load, so this needs no
  editing.
- **Every menu is drawn at one size.** SETUP was the odd one out under
  Options: the original menus are built from graphic lumps 15 pixels tall, and
  there is no lump for a word id never shipped, so the added item was drawn in
  the small font next to nine large ones. Rather than leave the mismatch, the
  five original menus are now text too, and every menu moved to 13-pixel rows
  from 16. Everything matches, and the added pages have room for the options
  they needed.

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
