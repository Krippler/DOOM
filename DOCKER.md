# DOOM in a container

Builds the original `linuxdoom-1.10` sources and runs them inside the image,
with the game reachable from a browser. Nothing needs to be installed on the
host beyond Docker — no X server, no display, and nothing to set up for sound:
it comes out of the same browser tab as the picture.

```
docker run --rm -p 6080:6080 ghcr.io/krippler/doom
```

Or build it yourself, which is what the rest of this page assumes when it
writes `doom` instead of the full image name:

```
docker build -t doom .
docker run --rm -p 6080:6080 -v "$PWD/wads:/wads:ro" doom
```

Then open **<http://localhost:6080/play.html>** and click to play.

With Compose:

```
mkdir -p wads && cp /path/to/DOOM1.WAD wads/
docker compose up --build
```

## Game data

The shareware IWAD is in the image — episode 1, nine levels — so the container
plays out of the box with nothing mounted. It is id's file, distributed under
their shareware terms, and the entrypoint checks its checksum before using it.

To play anything else, mount a directory at `/wads` containing your own IWAD.
A mounted IWAD always wins over the bundled one. The engine recognises these
names, and the container matches them case-insensitively, so `DOOM1.WAD` works
as well as `doom1.wad`:

| File | Game |
| --- | --- |
| `doom1.wad` | Doom shareware |
| `doom.wad` | Doom registered |
| `doomu.wad` | The Ultimate Doom |
| `doom2.wad` | Doom II |
| `doom2f.wad` | Doom II, French |
| `plutonia.wad` | Final Doom: The Plutonia Experiment |
| `tnt.wad` | Final Doom: TNT Evilution |

The freely redistributable shareware `DOOM1.WAD` is the easiest way to try
this; it ships with any copy of shareware Doom. Commercial IWADs come from
your own copy of the game.

If no IWAD is mounted the container falls back to the bundled shareware file
and says so. It stops only if that is missing or fails its checksum too.

With several mounted, Doom is started ahead of Doom II — the engine's own
search order picks the sequel, so a directory holding both always started
Doom II. Set `DOOM_IWAD` to a filename or path to choose directly.

**The Ultimate Doom** is recognised by its contents, not its name. The 1997
code expected the four-episode version to be called `doomu.wad`; Steam, GOG and
every other re-release install it as `DOOM.WAD`, which that code reads as plain
registered Doom with *Thy Flesh Consumed* unreachable. The engine now looks for
E4M1 in the IWAD itself and identifies the game from that.

Mods (PWADs) go in the same directory. They are not matched against the table
above; anything ending in `.wad` shows up in the in-game WAD menu below.

## The browser client

Two pages are served:

| | |
| --- | --- |
| `/` | Redirects to `play.html`. |
| `/play.html` | Captures the mouse. Use this to play. |
| `/vnc.html?autoconnect=1&resize=off` | Stock noVNC, no capture and no sound. Useful for looking at the screen without grabbing your pointer. |
| `/play.html?encoding=hextile` | The same page, with the picture compressed differently. For chasing stutter — see below. |
| `/play.html?stats=1` | The same page, with the sound and picture measurements on the start screen. Off by default — see below. |

`play.html` offers two ways in, and both capture the mouse with the Pointer
Lock API — the cursor disappears into the game, so turning never runs out of
screen and the pointer cannot slide off into the rest of your desktop:

| | |
| --- | --- |
| **Play fullscreen** | Fills the screen, and keeps Escape for the game (below). |
| **Play in this window** | Same capture, no fullscreen. Escape releases the mouse, so use `` ` `` for the menu. |

Once the capture is released, clicking the picture resumes it the same way you
started, so Escape then click will not drop you into fullscreen unexpectedly.

Either way that first click also starts the sound, which browsers will not
play without one. See [Sound](#sound).

### `?stats=1`

The start screen is quiet by design: press Escape mid-game and you get the
title, the two buttons and the build, with nothing to read. Sound that is
*not* working still says so, because that is the one thing the page can tell
you that you cannot see for yourself.

Add `?stats=1` and the measurements come back — what the sound is doing, and
what the picture did while you were playing:

```
Picture, while you were playing: best 43 frames a second of the 35 the game
draws, arriving at 495 KB/s, longest gap 800 ms, 518 late. The picture's data
arrived in gaps of at most 799 ms, and this page took at most 3 ms to ask for
the next frame after one arrived — 0 ms of that with a finished picture
already in hand. Decoding held this page up for at most 0 ms of that, the
slowest of 18876 pictures taking 5 ms. The sound, over the same link, went
quiet for at most 104 ms.
```

The figures describe the last spell of play and hold while you read them —
they are cleared when play starts again, not on a timer, so what is on screen
is always the run you just did. They are what found the stutter that
`-nowireframe` fixed, and they are the first thing to reach for if a picture
misbehaves again. Everything below that needs them says so.

### `?encoding=hextile`

x11vnc and noVNC settle on Tight, which sends the busiest parts of a DOOM
screen as JPEG. noVNC decodes those by handing each one to an `<img>` and
waiting for the browser to decode it, and while it waits it reads nothing more
off the socket and asks for no further frames. That is the one place in the
client that can go quiet for an unbounded time with the browser itself
perfectly responsive.

Whether it is worth anything depends on the machine you are playing on, so the
page measures it. Open it with `?stats=1`, play for a few seconds, press Escape
and read the picture note: *decoding held this page up for at most N ms of
that*. If N is small, this switch will not help
you and the stutter is somewhere else. If N is a large part of the *this page
took at most N ms to ask for the next frame* figure beside it, this is the
cause and the switch is the fix.

`?encoding=hextile` drops Tight from what the browser offers, so the server
falls back to Hextile, which noVNC writes straight into the framebuffer with
nothing to wait for. It is not the default because it is not free — over the
same 25 seconds of walking into a room, 873 KB/s with Tight against 2286 KB/s
with Hextile. That is nothing over a wire and quite a lot over wifi.

The picture is scaled up to fill the window, by the same factor in both
directions, so it keeps its shape — black bars on whichever axis has room
left over rather than a stretched image. `DOOM_SCALE` sets how many pixels the
engine actually renders; the page then scales that to whatever size the window
is, so raise it if the result looks soft on a large screen.

**Escape stays DOOM's, in fullscreen.** Pointer lock normally gives Escape to
the browser, which cancels the capture — no use at all when Escape is the
game's menu key. The page also takes a Keyboard Lock on Escape, which hands it
back to the game. That only works in fullscreen, which is the one real
advantage fullscreen has. **Hold** Escape to actually leave; browsers guarantee
that way out and it cannot be taken away.

Playing in the window, Escape always releases the mouse — nothing can change
that. This is why backquote (`` ` ``) opens the menu too, in every mode: the
game uses that key for nothing else and no browser claims it, so it reaches
DOOM whatever the pointer lock is doing.

Keyboard Lock is a Chromium feature, so on Firefox and Safari Escape ends the
capture even in fullscreen. The page says so when that is the case; backquote
still works. To move the menu somewhere else, **Options → Setup → Controls →
MENU** binds it to whatever you like, saved with the rest of your controls.
Escape is read directly and always opens the menu when the browser lets it
through, so this only ever adds a key.

If the browser refuses to capture the pointer, the page says so and carries on
as an ordinary viewer rather than failing.

**GRAB POINTER**, under Options → Setup → Mouse, is off by default and should
stay that way in a browser. It makes the engine pull the pointer back to the
middle of the screen itself, which is right when you are running the engine on
a real X display and wrong through VNC — the page already keeps the pointer
where it needs to be, and the two fight.

## Controls

Click into the canvas first — the browser only sends keys to a focused canvas.

| | |
| --- | --- |
| Move | arrow keys |
| Strafe | `,` and `.`, or hold Alt and steer |
| Run | hold Shift |
| Fire | Ctrl |
| Open, use | Space |
| Weapons | `1`–`7` |
| Map | Tab |
| Menu | `` ` ``, or Esc when the browser lets it through |

All of these can be changed from the game: **` → Options → Setup →
Controls**, the menu key included. Pick a line, press Return, then press the key you want. The
choice is written to `.doomrc` in the state directory, so it survives a
restart.

A game controller works too, and is rebound in the browser rather than in the
game — see below.

The mouse turns you. It does **not** walk you forward and back — the original
used the mouse's Y axis for movement, since there was nothing to aim
vertically at, and with a modern hand on the mouse that mostly walks you
about by accident. **Options → Setup → Mouse → MOVE WITH MOUSE** turns the
original behaviour back on. The buttons are assignable on the same page. *Grab pointer* is on by
default and is what `play.html` needs; see the section above.

### A game controller

An Xbox pad on a desktop and a Backbone One on a phone both work, along with
anything else the browser reports as a standard gamepad — a DualSense, an
8BitDo, a Switch Pro pad.

Plug it in or pair it, open `play.html`, and **press a button on it**. That last
step is not optional and is not this page being fussy: browsers do not admit a
gamepad exists until something on it has been pressed, so a pad that is paired,
charged and idle is genuinely invisible. Once it has been seen, a line appears
under the buttons naming it, with **Buttons** beside it to rebind anything.

The layout out of the box:

| | |
| --- | --- |
| Left stick | move and sidestep — push it all the way to break into a run |
| Right stick | turn. Analog, so a nudge turns slowly |
| RT | fire |
| LT | run |
| A | open / use, and confirm in menus |
| B | back out of a menu |
| X, Y, LB, RB | shotgun, pistol, chaingun, rockets |
| Left stick click, Right stick click | fist/chainsaw, plasma rifle |
| D-pad | move and turn, for menus and for keyboard-style play |
| View, Menu | automap, game menu |

The BFG has no button by default — there is one weapon more than there are
comfortable buttons — and neither do the sidestep keys or the strafe modifier,
because the left stick already does that. All of them are in the panel if you
want them.

There is no next-weapon button because the 1997 engine has no such key: it
only has *select weapon N*, and the page cannot cycle on your behalf since it
has no idea which weapons you are carrying. A digit for a weapon you have not
picked up is ignored, so a cycle would stick on the gaps.

#### The pad works in the menus but not in the game

This is handled automatically now, and is worth knowing about anyway because it
explains the shape of it. The engine **hardcodes the arrow keys and Return in its
menus** but reads *configurable* bindings during play — `key_up`, `key_down`,
`key_fire`, `key_use`, `key_speed`, `key_strafe`, all from `.doomrc`. A pad
pressing the built-in defaults therefore drives menus perfectly and goes
completely dead in a level as soon as those have been changed under **Options →
Setup → Controls**.

The container reads `.doomrc` at startup and serves the bindings beside the page
as `doom-keys.json`, so the pad presses whatever *this* engine listens for — one
key per action, and the engine's menus now accept the movement bindings as
navigation. So with `key_up` set to `w`, the d-pad's up sends `w`: it walks you
forward in a level and moves the cursor in a menu, because `M_Responder` maps it
onto its own up key.

The panel's third column shows each key and where it came from: **green** from
the game's own config, **grey** the engine default, **pink** set here by hand.

Because it is read at startup, a binding changed in the game reaches the pad on
the next restart — the engine writes `.doomrc` when it exits. **Key** on a row
overrides it straight away without waiting: press **Key**, then press the key you
actually use. **Default key** hands the row back to the config.

Two things that look like the same fault and are not:

- **Turning still works** when everything else has stopped, because the sticks
  turn by sending pointer motion rather than a key, and the mouse is not
  rebindable. It is the one input that cannot be broken this way.
- **Most weapon buttons do nothing at the start of a level**, correctly: the
  engine ignores a weapon you are not carrying, and E1M1 starts with a fist and
  a pistol.

#### Triggers, and pads that report them as axes

The standard layout puts LT and RT at buttons 6 and 7, and plenty of pads and
browsers do not: they report the triggers as **analog axes** instead, and then
there is no button 6 or 7 at all. Every other control works, which from the
outside looks exactly like "the triggers don't work".

Those are found automatically now: an axis that **rests at one extreme** is a
trigger, because a stick resting at ±1 is a broken stick. The lower-numbered one
is added to Run and the next to Fire — *added*, not substituted, so the row reads
`RT or Axis 5 +` and works whichever the pad really uses. Nothing to set by hand.

#### What the container log says about your pad

Everything about a controller happens in the browser, where the container cannot
see it — so the page writes what it does into the container's log, which is the
thing anyone actually pastes when a control does not work. Read it with
`docker logs <container>` and look for `[doom] controller:`.

One line goes in on every page load, whether or not there is a pad:

```
[doom] controller: client 1.10.63, keymap from the engine: key_down=115 key_fire=0 ...
```

That is the build the browser is really running — not the one you pulled, the one
the tab has — and the engine's own bindings as the page received them. No such
line at all means the browser is not running this page: a stale tab, or a cached
copy from before controllers existed. Reload it.

Then, once per pad:

```
[doom] controller: found id=Microsoft X-Box 360 pad (Vendor: 045e Product: 028e)
       mapping=none buttons=11 axes=0.00,0.00,-1.00,0.00,0.00,-1.00,0.00,0.00
```

That is the whole shape of the pad: its name, whether the browser calls its
layout standard, how many buttons it admits to, and where each axis rests. A
`-1.00` is a trigger sitting at rest; `mapping=none` means the button numbers
will not match their usual names.

Then what every control is *going* to do, one line each:

```
[doom] controller: plan fire in=b7,a5+ sends=mouse1 src=default doomrc=0 unusable=0
[doom] controller: plan act  in=b0     sends=e      src=engine  doomrc=101
[doom] controller: plan run  in=b6,a2+ sends=NOTHING src=default doomrc=0 unusable=0
```

`in=` is what you press — `b7` is button 7, `a5+` an axis pushed positive, and
**`-` nothing at all**, which is the other answer worth looking for: a control
with no button cannot work however the keys are set. (`strafeleft`,
`straferight` and `strafemod` read `-` on a stock layout by design — the left
stick strafes, and there are not sixteen buttons to go round.) `sends=` is what the game will receive, and **`sends=NOTHING`
is the answer** wherever a control does nothing: that one says the engine has no
key for Run, so nothing can be sent for it. `src=` is where the key came from
(`engine` from `.doomrc`, `default` built in, `learned` set in the panel),
`doomrc=` the engine's own number, and `unusable=` a number that has no key at
all. This is the half of the log worth reading: it is the answer before the
question, for all twenty controls at once.

A line before the plan means a layout saved by an earlier build was missing
buttons for actions that build did not have, and the defaults were put back:

```
[doom] controller: filled in from the defaults, saved layout had no button for: back_out=b1 weapon3=b2 ...
```

Only gaps are filled. Anything you rebound stays where you put it, and anything
you **Clear**ed stays cleared.

Finally, as you play, what actually happened:

```
[doom] controller: down b0 -> act
[doom] controller: sent e down
[doom] controller: up   b0
[doom] controller: sent e up
[doom] controller: down b7 -> fire
[doom] controller: mouse mask 0 -> 1
```

Every button as it goes down and comes up, including ones bound to nothing
(`down b4 -> nothing bound`), and every key and mouse button sent because of it.
Axes appear only where something is bound to them, or the sticks would fill the
log by themselves. It stops after 400 lines, which is far more than a diagnosis
needs.

It reaches the log over the same connection that carries the picture: the page
asks the container's own proxy for a URL, and the proxy prints it. Nothing
leaves the machine.

Where that does not apply, **Bind takes an axis as well as a button** — press
**Bind** and squeeze the trigger. It records which way the axis travelled from where it was resting, so a
trigger that sits at -1 and runs to +1 binds correctly and is not treated as held
while it rests. The panel shows such a binding as `Axis 5 +`.

Two things make this visible rather than mysterious. A row whose button does not
exist on the connected pad says so — **`RT — not on this pad`** — and the
readout's *pressed now* line names every input that is on, in the same form a
binding uses, so a squeezed trigger appears as `Axis 5 +` the moment you pull it.

A trigger that reports only an analog value and never sets `pressed` counts from
30% of its travel, so one that tops out low still works.

#### If a control also picks things in menus

Then the engine has it on **Enter**, and the log says so:

```
[doom] controller: plan fire in=b7 sends=Return src=engine doomrc=13
[doom] controller: note fire is Enter in the engine, which also confirms menu items
```

`M_Responder` reads Enter as *confirm* whatever else it is bound to, so a
control sitting on it works in a level and picks menu items everywhere else.
Fix it in the game's own **Options → Setup → Controls**.

Nobody chooses this: the Controls screen is opened with Enter, so pressing Enter
again — the obvious thing to do when you are not sure the first press
registered — used to bind Enter to whatever row you were on. It no longer can;
the prompt now ignores Enter and waits for a real key, and Escape still cancels.

**Moving it to the mouse button instead does not help.** `M_Responder` reads
mouse button 1 as Enter as well:

```c
if (ev->data1&1)
    ch = KEY_ENTER;
```

so that is the same conflict by another route. Use an ordinary key — `Ctrl` is
what fire is bound to out of the box.

#### If Fire does nothing whatever it is bound to

Then it is not the pad, and rebinding will not help: check what the **game** has
fire bound to. `G_BuildTiccmd` reads it as

```c
gamekeydown[key_fire] || mousebuttons[mousebfire] || joybuttons[joybfire]
```

so somebody who fires with the mouse may have no *key* for firing at all. A
controller that only sends keys can never fire for them, however it is mapped.

The page reads `mouseb_fire` out of `.doomrc` alongside the keys, and where the
engine has no usable key for an action it sends that mouse button instead. The
panel says which: **`mouse 1 (key 0 unusable)`**. Where there is a usable key it
sends only the key, because holding a mouse button in a menu reads as Return and
a working fire key should not make menus confirm themselves.

**Run has no mouse button** — the engine has no `mouseb_speed`, only
`key_speed` — so if that row says `key 0 — nothing to send`, press **Key** on it
and press the key you actually run with.

#### If Fire or Run in particular do nothing

Those two are the ones most often rebound, so check what the engine is actually
bound to. The container logs it at startup, in full:

```
[doom] controller keys: key_down=115 key_fire=120 key_left=172 key_menu=96 ...
```

If `key_fire` is a number the page cannot turn into a key — a few values have no
keysym at all — the panel says **`game uses 144 — unknown here`** on that row and
sends **nothing** rather than pressing the default at an engine listening for
something else. Press **Key** on that row and press the key you use, and it will
send that instead.

**Rebinding** is in that panel: pick a line, press **Bind**, then press the
button or squeeze the trigger.
The sticks have a deadzone, a turn speed, invert, a swap, and a switch for
whether a full push runs. Everything is saved in the browser — not in
`.doomrc` — because the container never sees the controller, and because the
pad on your phone and the pad on your desk are different browsers and usually
want different layouts. **Reset to defaults** puts it all back.

Two things worth knowing:

- The panel sends **keys**, not intentions. Rebind fire in the game's own
  **Options → Setup → Controls** and the controller's *Fire* has to be pointed
  at the new key as well, or it will go on pressing Ctrl.
- On a phone there is no pointer to capture, so the controller is the whole of
  the input. Up to 1.10.67 that stopped the pad working entirely: the start
  screen was hidden only by a pointer-lock event, iOS has no Pointer Lock API,
  so the screen never went away — and the page will not send a key while it is
  up, however well the pad is detected. The *grab pointer* warning does not
  apply either. Turning still works:
  the right stick sends the same relative motion a captured mouse would. iOS has
  no Pointer Lock API at all, so the page no longer warns about a capture it
  never attempted — it says to pair a controller instead, and says nothing once
  one is connected. The controller log line records which it found:
  `client 1.10.66, pointerlock no, keymap …`.

Nothing reaches the game while the start screen is up, so a pad knocked off a
desk cannot empty a chaingun into a room nobody is watching, and a pad that
disconnects mid-game lets go of whatever it was holding rather than leaving the
trigger down.

#### When only some of it works

**Start with the container's log.** `docker logs <container> | grep controller:`
answers this without anyone having to watch a screen — see *what the container
log says about your pad* above. The rest of this section is the same information
from inside the browser, for when the log is not to hand.

The panel ends with **what the pad is reporting**, which exists because "some
buttons work and the rest do nothing" has three completely different causes and
they cannot be told apart by playing. It shows, live, how many pads the browser
admits to, the name and layout it claims, how many buttons and axes it has,
which button indices are pressed *right now*, and — kept from the last spell of
play, since nothing is sent while you are reading it — the keys that actually
went out.

Play, press the things that do not work, press Escape, open the panel and read
the bottom of it:

| | |
| --- | --- |
| `pads seen 0` | The browser is not giving the page the pad at all. Press a button on it; if it stays at 0, no binding here can help. |
| A button you pressed never appears in `pressed now` | The browser is not reporting that button. If `layout` is not `standard` the indices will not match the names, and rebinding by pressing is the fix. |
| It appears in `pressed now`, but no key is listed | The page saw the button and sent nothing — the action is unbound. Bind it. |
| The key is listed and the game ignored it | It reached the far end. Almost always the engine's own binding was changed under **Options → Setup → Controls**, so the key the panel sends is no longer the key the game listens for. |

**The quickest answer is on screen while you play.** Open
`/play.html?stats=1` and a strip sits over the top-left of the picture:

```
pads 1  ·  layout standard  ·  pressed 7=RT  ·  holding Control_L  ·  last key Control_L down
```

Hold the button that does nothing and read that line as the game fails to react.
It settles the whole question in one look, because the panel cannot: the panel
is only visible when the game is not, and the moment worth watching is the one
moment it is hidden.

| what the strip says | what it means |
| --- | --- |
| `pads 0` | the pad is not reaching the page; nothing here can help |
| `pressed —` while you hold the button | the browser is not reporting that button |
| `pressed 7=RT` but `last key none sent yet` | the page saw it and sent nothing — it is unbound |
| `pressed 7=RT` and `holding Control_L` | the key went out — see **the pad works in menus but not in the game** below |

`?stats=1` also shows the controller line when no pad has been seen, so the panel
and its readout are reachable when the fault is that nothing is detected. The
panel's readout has a **Copy this** button, which is the easiest thing to paste
into a bug report.

## Loading WADs from the game

**Options → Setup → Load WAD** lists everything in the mounted WAD directory,
marked `GAME` for an IWAD and `MOD` for a PWAD, and loads whichever you pick.

The engine builds its textures, sprites, sound cache and every zone allocation
once at startup around the files it was given, and none of that can be swapped
while it runs. So choosing a file restarts the engine with it: a couple of
seconds, and you land back on the title screen. Anything not yet saved is
lost, the same as quitting.

Shareware refuses to load mods — that is the engine's own restriction, not the
container's — and the menu says so instead of restarting into a fatal error.

## Size

About 216 MB to pull, 603 MB unpacked. The engine and sound server together
are under 600 kB; the rest is what it takes to run an X server and reach it
from a browser.

The Dockerfile removes three things the distro packages drag in but this image
never uses, together about 400 MB:

- The Mesa GLX driver and the LLVM behind it. `xvfb` links `libGL.so.1` so the
  dispatch library stays, but nothing here renders through GLX and the driver
  is never loaded.
- Node and `net-tools`, which the `novnc` package depends on for tooling this
  image does not run. noVNC itself is 1.2 MB of static files, unpacked
  directly and served by websockify.
- numpy and LAPACK, which websockify uses only to unmask client-to-server
  WebSocket frames — for a VNC session that is keystrokes, not video.

What remains is roughly 142 MB of soundfont (see below to change it), the
Ubuntu base, and the Python runtime websockify needs.

## Options

Everything is set through the environment:

| Variable | Default | Meaning |
| --- | --- | --- |
| `DOOM_SCALE` | `2` | Pixel scale, 1–4. The window is 320×200 times this. |
| `DOOM_WADDIR` | `/wads` | Where to look for IWADs. |
| `DOOM_BUNDLED_WAD` | `/usr/share/doom/doom1.wad` | Shareware IWAD used when nothing is mounted. |
| `DOOM_IWAD` | unset | Which IWAD to start when several are mounted. A filename or a path. |
| `DOOM_STATE` | `/doom/state` | Config file, savegames and logs. |
| `DOOM_WEB_PORT` | `6080` | noVNC HTTP port. |
| `DOOM_VNC_PORT` | `5900` | VNC port. |
| `DOOM_VNC_PASSWORD` | unset | If set, the VNC session requires this password. |
| `DOOM_VNC_WAIT` | `5` | Milliseconds between x11vnc screen polls. |
| `DOOM_VNC_DEFER` | `5` | Milliseconds x11vnc holds an update back. |
| `DOOM_VNC_ARGS` | unset | Extra flags passed to x11vnc. |
| `DOOM_SOUND` | `1` | Set to `0` for no sound at all. |
| `DOOM_AUDIO_RATE` | `22050` | Rate the sound reaches the browser at. `11025` or `44100` also work. |
| `DOOM_AUDIO_PORT` | `5901` | Internal mixer port. Nothing to publish. |
| `PULSE_SERVER` | unset | Send the sound to this PulseAudio server instead of the browser. |
| `DOOM_SOUNDFONT` | auto | General MIDI soundfont for music; empty means search for an installed one. |
| `PUID` / `PGID` | `1001` | User to drop to, when the container starts as root. |

Anything you pass after the image name goes straight to the engine:

```
docker run --rm -p 6080:6080 -v "$PWD/wads:/wads:ro" doom -warp 1 1 -skill 4
```

Useful engine flags: `-warp <episode> <map>`, `-skill 1..5`, `-nomonsters`,
`-respawn`, `-fast`, `-file <pwad>`, `-devparm` (F1 writes a PCX screenshot).

## Saves and config

`/doom/state` holds `.doomrc` and `doomsavN.dsg`. The Compose file keeps it in
a named volume so saves survive `docker compose down`, and a named volume with
plain `docker run` works the same way:

```
docker run --rm -p 6080:6080 -v "$PWD/wads:/wads:ro" -v doom-state:/doom/state doom
```

Bind-mounting a host directory works too, but the container has to write as
somebody who owns it. Started as root it takes ownership of the state
directory as `PUID:PGID` and then drops to that user for everything else, so
this is enough:

```
docker run --rm -p 6080:6080 -e PUID="$(id -u)" -e PGID="$(id -g)" \
    -v "$PWD/wads:/wads:ro" -v "$PWD/state:/doom/state" doom
```

`PUID`/`PGID` default to 1001. Passing `--user` instead skips the whole thing
and runs as that user from the start, in which case the directory has to be
writable by them already — the container checks and says so rather than
failing obscurely.

`docker stop` is handled gracefully: the engine gets a SIGINT, which is the
signal it already treats as "save the config and quit".

## Playing with a native VNC client

Map the VNC port and point any client at it:

```
docker run --rm -p 5900:5900 -v "$PWD/wads:/wads:ro" doom
vncviewer localhost:5900
```

There is no password unless you set `DOOM_VNC_PASSWORD`. Only ports you
explicitly publish are reachable, but do set a password if you map the port on
a machine others can reach.

## Why it runs its own X server

The 1997 code only ever supported one kind of display:

```c
if (!XMatchVisualInfo(X_display, X_screen, 8, PseudoColor, &X_visualinfo))
    I_Error("xdoom currently only supports 256-color PseudoColor screens");
```

An 8-bit PseudoColor visual is something no current X server still offers, so
handing the container your host's `$DISPLAY` will generally fail with that
error. The container sidesteps this by running its own `Xvfb` at depth 8 —
which is also what makes it work identically on macOS and Windows hosts, where
there is no X server at all. `x11vnc -8to24` converts the colormapped output
to true colour for the VNC client.

## Sound

**It comes out of the browser.** Nothing to mount, nothing to configure — the
same page that shows the picture plays the sound, so it works when the
container is on a server in another room, which is where most of them are.

The engine plays effects through a separate `sndserver` process, which is how
the original release worked, and renders music itself: linuxdoom shipped every
music function as an empty stub, so this adds them, with `mus2mid.c` turning
the WAD's MUS lumps into Standard MIDI and FluidSynth rendering them against a
General MIDI soundfont.

Neither of those can reach you on their own, because the container has no
sound card and no sound daemon, and cannot practically be given one: the only
packaged PulseAudio brings systemd, GStreamer, cairo and a set of video codecs
with it — 172 packages to add two streams together. VNC is no help either; it
carries a picture and nothing else.

So the two streams are written to pipes and `audiostream` mixes them: effects
at 11025 Hz resampled up, music rendered at the output rate, summed and sent
as raw 16-bit stereo PCM. It reads on a real-time schedule, which is also what
keeps either producer from running ahead — the same job a blocking write to
`/dev/dsp` did in 1997.

That PCM reaches the page over a second WebSocket on the port you already
published, and is played through a small ring buffer that absorbs the
difference between the container's clock and your sound card's. About 88 KB/s,
or 700 kbit/s. Sound starts with the same click that starts play — browsers
will not play audio without one.

The ring buffer is driven one of two ways. An `AudioWorklet` is the better of
them, running on the audio thread where nothing the page is doing can
interrupt it, but browsers expose it only in a **secure context** — HTTPS, or
localhost. Reaching this container at `http://<host>:6080` is neither, so
there the sound goes through a `ScriptProcessorNode`: deprecated for years,
implemented everywhere, and needing no secure context. The start screen says
which is in use. Serving the page over HTTPS through a reverse proxy gets you
the worklet, and is worth nothing else.

| | |
| --- | --- |
| `DOOM_AUDIO_RATE` | `22050`. Also `11025` (half the bandwidth, noticeably duller music) or `44100` (double it). |
| `DOOM_AUDIO_PORT` | `5901`, internal only. Nothing to publish; the sound shares the web port. |
| `DOOM_SOUND=0` | No sound server, no mixer, no audio socket. |

### When there is no sound

The start screen says so under the buttons. While the sound is working it says
nothing at all — the line is there only when there is something to report:

```
Sound: not started — click to play.
1.10.17
```

A refused connection, a worklet the browser would not load, a stream that opens
and stays quiet: each says which.

**`no Web Audio` on a browser that has it** was a bug up to 1.10.65. The check
demanded `AudioWorkletNode` before doing anything, even though the worklet is
optional — it is gated on a secure context, and where it is missing the sound
goes through a `ScriptProcessorNode` instead. A browser with no AudioWorklet at
all therefore got no sound at all rather than the fallback. iOS Safari over
plain HTTP is where that showed up. Only the `AudioContext` constructor is
required now, prefixed or not. The same reason appears at the bottom of the
screen during play. The second line is always there, and is the build the
**page** came from.

For the figures on a stream that *is* working — rates, buffer, underruns, and
which of the two audio paths is in use — add `?stats=1`:

```
Sound: on — 22050 Hz in, 48000 Hz out, 12.4 s received, holding 47 ms
```

The container prints its own build at startup as `DOOM <version>`, and says
which way it sent the sound on a line beginning `sound:`. That gives three
checks that between them cover everything:

| | |
| --- | --- |
| Page build older than the log's | The browser is running a cached client. Reload with Ctrl+Shift+R. |
| `Sound: … via the fallback` (with `?stats=1`) | Normal over plain HTTP — see above. Not a fault. |
| `Sound: … holding N ms` (with `?stats=1`) | How far behind the sound is running. It starts near 40 ms and rises only if this machine cannot keep up. |

### Delay

The picture answers a keypress in about **48 ms** — roughly one of the
engine's 35 frames a second, plus a browser repaint. There is not much there
to win.

What you may notice instead is that firing feels slower than 48 ms, and it
is: the pistol spends four tics winding up before it goes off, which is
114 ms of the game itself and is how DOOM has always behaved.

The **sound runs about 150 ms behind the picture**, spread across the sound
server's buffer, the mixer, the browser's ring buffer and your own audio
device. That gap is the real one.

Most of what is left is not adjustable from here, but two things are: a
smaller `DOOM_AUDIO_RATE` moves less data, and serving the page over HTTPS
gets the `AudioWorklet` path, which runs on the audio thread rather than
competing with noVNC for the main one.

`DOOM_VNC_WAIT`, `DOOM_VNC_DEFER` and `DOOM_VNC_ARGS` tune x11vnc's timing,
though none of them measured as worth anything: the picture is already within
a frame of the engine. `PORTING-NOTES.md` has the measurements.
| `sound: to PulseAudio` in the log | It went to a host audio server rather than to you. Unset `PULSE_SERVER`. |
| `Sound: on … N s received` (with `?stats=1`) and still silent | It is arriving and being played, so the problem is past the browser: a muted tab, or the machine's output device. |

Effects and music have separate volume sliders under Options → Sound Volume.

### Sending it to the host's speakers instead

If the container is on the machine you are sitting at, share the host's audio
socket and the game plays through it directly — the sound server switches to
its PulseAudio backend (`sndserv/pulse.c`), FluidSynth connects to the same
server, and nothing is streamed to the browser:

```
docker run --rm -p 6080:6080 \
    -v "$PWD/wads:/wads:ro" \
    -v "/run/user/$(id -u)/pulse/native:/tmp/pulse:ro" \
    -v "$HOME/.config/pulse/cookie:/doom/state/.pulse-cookie:ro" \
    -e PULSE_SERVER=unix:/tmp/pulse \
    -e PULSE_COOKIE=/doom/state/.pulse-cookie \
    doom
```

Setting `PULSE_SERVER`, or having `/run/user/$(id -u)/pulse/native` visible
inside the container, is what selects this. The cookie is only needed if your
PulseAudio requires authentication; many setups work without it. If the socket
is not readable by the container user, add `--user "$(id -u):$(id -g)"`.

Note on `padsp`: the usual way to feed OSS-era software into PulseAudio no
longer exists. `padsp` and its `libpulsedsp.so` were removed upstream in
PulseAudio 16 and are not in any current distribution, and `aoss` (the ALSA
equivalent) needs a real ALSA stack in the container and fails the format
negotiation the engine does at startup. Talking to PulseAudio directly avoids
both problems and works against PipeWire too, since `pipewire-pulse` accepts
PulseAudio clients unchanged.

### Soundfont

The image ships FluidR3 GM, which is the good one, and it is the single
largest thing in the image. FluidSynth is told to load its sample data as
instruments are used rather than reading the whole file at startup, so it
costs little at runtime — the engine reaches the title screen in about a
tenth of a second and uses roughly 20 MB more memory than with a small
soundfont. Reading it up front instead would stall startup for ten seconds
and cost 140 MB of RSS.

Pick a different one at build time with `SOUNDFONT_PACKAGE`:

| Build arg | Soundfont | Installed |
| --- | --- | --- |
| `fluid-soundfont-gm` (default) | FluidR3 GM | 142 MB |
| `fluidr3mono-gm-soundfont` | FluidR3 Mono, Ogg-compressed | 23 MB |
| `musescore-general-soundfont-small` | MuseScore General, lossy | 39 MB |
| `timgm6mb-soundfont` | TimGM6mb | 6 MB |

```
docker build --build-arg SOUNDFONT_PACKAGE=fluidr3mono-gm-soundfont -t doom .
```

The engine searches for whatever is installed, so nothing else needs changing.
(TimGM6mb is always present regardless — `libfluidsynth3` depends on it.)

To use a soundfont of your own instead, mount it and point `DOOM_SOUNDFONT`
at it:

```
docker run --rm -p 6080:6080 \
    -v "$PWD/wads:/wads:ro" \
    -v "$PWD/other.sf2:/sf/gm.sf2:ro" \
    -e DOOM_SOUNDFONT=/sf/gm.sf2 \
    -e PULSE_SERVER=unix:/tmp/pulse \
    -v "/run/user/$(id -u)/pulse/native:/tmp/pulse:ro" \
    doom
```

`-soundfont <file>` on the command line does the same thing. If the path is
unreadable the engine says so and falls back to an installed soundfont rather
than losing music; if it finds none at all the game still runs, silently.

## Unraid

`templates/unraid.xml` is a Community Applications template, with the icon
beside it and `ca_profile.xml` at the repo root for the maintainer card.
[PUBLISHING.md](PUBLISHING.md) covers how images are built and what is left to
do before the template can be submitted. It can be tried without CA by pasting
its raw URL into the *Template* field of **Docker → Add Container**.

## Measuring a stutter

If the picture stutters, the question is which end is late, and the page's own
note answers half of it — load `/play.html?stats=1`, play, then press Escape
and read it. `doom-probe` answers the
other half from inside the container, with no browser involved at all: it asks
x11vnc for pictures the way the browser does, first straight to x11vnc and
then through websockify and the proxy, and times the answers.

```bash
docker exec <container> doom-probe
```

Leave the game moving while it runs — its own attract-mode demo is enough, and
the probe deliberately sends no input, so it will not play the game for you. It
takes a minute (thirty seconds each way); `DOOM_PROBE_SECONDS` changes that.

It also runs from another machine, which is the more interesting case — that
puts the network in the path, where a browser actually sits:

```bash
curl -sO https://raw.githubusercontent.com/Krippler/DOOM/master/docker/doom-probe.py
python3 doom-probe.py 192.168.1.10:380
```

Give it the address the way it appears in the address bar when you play — a
host, a host and port, or the whole URL all work, and the port is whatever the
web page was published as rather than 6080.

x11vnc's port is usually not published, so the first of the two lines will say
it could not connect; the second still runs. If it stalls from another machine
and not from inside the container, the link and the proxy feeding it are the
thing to look at rather than x11vnc.

To run it against a container that is already up, without pulling a new image
or restarting anything:

```bash
curl -sL https://raw.githubusercontent.com/Krippler/DOOM/master/docker/doom-probe.py \
  | docker exec -i <container> python3 -
```

On a machine with nothing wrong both lines read about 10 ms in the middle and
never pass 60 at the worst, with nothing over 200 ms:

```
straight to x11vnc:     496 answers,  757 KB/s   median  11 ms   p90  16   p99   34   worst   37   over 200 ms: 0
through the proxy:      496 answers,  665 KB/s   median  12 ms   p90  16   p99   28   worst   32   over 200 ms: 0
```

A worst of several hundred milliseconds is the stutter, caught with nothing
but x11vnc in the picture. It is not a shortage of CPU: pinned to one core
with a busy process competing for it, x11vnc spent 2225 ms of every 5000
waiting for a core — far worse than any real report — and still answered
everything inside 48 ms. If only the second line is slow, it is the proxy.
If the picture was not moving the probe says so and refuses to report, because
VNC sends what changed and nothing else — a still screen goes quiet for as
long as it likes and that is not a fault. It needs a server without a password
(`DOOM_VNC_PASSWORD` unset); it says so plainly rather than guessing.

### Why x11vnc runs with `-nowireframe -noscrollcopyrect`

Both are on by default in x11vnc and both are for a desktop: they watch for a
window being dragged, or a pane scrolled, while a mouse button is held, and
hold the picture back while they decide. A game holds the fire button down.
There is one window here, it never moves, and nothing scrolls.

Measured with the fire button held and the player turning, 80 seconds each:

| | answers | KB/s | median | p90 | p99 | worst | over 200 ms |
| --- | --- | --- | --- | --- | --- | --- | --- |
| both on (x11vnc's default) | 1271 | 257 | 41 ms | 62 | 91 | 118 | 0 |
| `-nowireframe` only | 1079 | 250 | 43 ms | 62 | 94 | 118 | 0 |
| `-noscrollcopyrect` only | 1029 | 121 | 15 ms | 140 | 306 | 341 | 41 |
| **both off** | **2774** | **554** | **12 ms** | **15** | **24** | **44** | **0** |

Without a button held the default costs nothing. With one held it halves the
frame rate and triples the median. The two do different damage:
`-scrollcopyrect` slows everything evenly, which hides the other, and
`-wireframe` holds the picture for about 300 ms at a time — `t2` in its own
default timing string, `0.15+0.30+5.0+0.125`, is how long it waits for a window
to start moving after a button goes down, repainting nothing meanwhile.

`DOOM_VNC_ARGS=-wireframe` and `DOOM_VNC_ARGS=-scrollcopyrect` put them back if
you want to see it for yourself.

### Narrowing a confirmed stall

If `doom-probe` reports silences with **picture MOVING**, x11vnc had something
to send and did not send it, and the next question is which part of it is
responsible. Three things are worth trying, each one run the same way — start
the container with the setting, play for three minutes, and compare the count
of MOVING silences against a run without it. Still ones do not count.

| | what it changes |
| --- | --- |
| `DOOM_VNC_ARGS=-noxdamage` | Stops x11vnc trusting the X DAMAGE extension to tell it what changed, and makes it compare the framebuffer itself. DOOM draws through MIT-SHM, and if those writes are not reported as damage, x11vnc only notices on a later pass. |
| `DOOM_VNC_ARGS=-threads` | Gives each client its own thread in libvncserver. x11vnc is single-threaded by default, so anything that blocks its one loop stops every client at once — which is the shape of what the probe sees. |
| `DOOM_VNC_8TO24=0` | Turns off the depth 8 to truecolor translation, the most expensive thing x11vnc does here. **The picture will be wrong**, and not subtly: noVNC cannot use a colour map at all — it drops the connection if one arrives — so at depth 8 it asks for two bits per channel and gets 64 colours. Measured against the same scene: 50 distinct colours instead of 15,746. It halves the bandwidth by throwing the colours away. Diagnostic only, and only for counting stalls. |

None of these is a recommended setting. They are there to find out which part
of x11vnc is holding the picture, on a machine where that is actually
happening.

## If the game crashes

The log says which signal and where:

```
DOOM died on SIGSEGV (bad memory access). Innermost frame first:
/usr/local/games/linuxxdoom(P_SetupLevel+0x1c4)[0x55e0...]
/usr/local/games/linuxxdoom(G_DoLoadLevel+0x58)[0x55e0...]
...
[doom] DOOM crashed (SIGSEGV, status 139). The backtrace above says where;
[doom] please include it, and the controller lines, in a bug report.
```

Quote the whole block. The innermost frame is at the top, and the names are what
turn "it crashed" into something that can be looked at — this is 1997 C, and
there are corners of it nobody has walked into for a long time.

One cause is worth ruling out first, because it is in your own config rather
than the game: a key binding with a value out of range. Check the keymap line
in the controller log, or `.doomrc` directly. Every `key_*` should be between 0
and 255; anything larger was an out-of-bounds read on every tic, and a large
enough one crashed within a second. Fixed since 1.10.67, but a config written by
an older build can still carry the value, and the fix makes it a dead control
rather than a crash. Rebind it in **Options → Setup → Controls**.

## Troubleshooting

**It starts, crashes immediately and keeps restarting.** Almost always a bad
`screenblocks` in `.doomrc` in the state directory: the view size is computed
from it, and above 11 the view is larger than the screen, so the renderer
draws off the end of it. Delete that file and it will be recreated with
defaults; you lose your settings, not your savegames.

The startup log says which view size is in use:

```
R_SetViewSize: blocks 11, detail 0 -> view 320x200
```

`blocks` is clamped to 3–11 and `detail` is always 0, so anything else in that
line means an old image — pull again.

**The browser shows a black canvas.** Click it first; noVNC only forwards
keyboard input once the canvas has focus.

**"no game data to run".** The mounted directory has no recognised IWAD in its
top level. `-v "$PWD/wads:/wads:ro"` mounts `./wads`, so the file must be at
`./wads/DOOM1.WAD`, not in a subdirectory.

**Keys do nothing.** Doom uses Ctrl to fire and Alt to strafe, which browsers
and window managers like to intercept. noVNC's toolbar has a modifier-key
pad for exactly this.

**No sound.** The container logs what it decided at startup, on the lines
beginning `[doom] sound:`. If it reports a `PULSE_SERVER` but you still hear
nothing, the sound server prints its own error (`Could not connect to
PulseAudio (...)`) in the same output — usually the socket is not readable by
the container user, which `--user "$(id -u):$(id -g)"` fixes.

**"state directory is not writable".** Only happens when `--user` was passed,
since that skips the ownership fix. Drop `--user` and set `PUID`/`PGID`
instead, or make the directory writable by the user you asked for.

**Diagnostics.** `xvfb.log`, `x11vnc.log` and `websockify.log` are written to
`/doom/state`.
