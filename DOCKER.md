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

### `?encoding=hextile`

x11vnc and noVNC settle on Tight, which sends the busiest parts of a DOOM
screen as JPEG. noVNC decodes those by handing each one to an `<img>` and
waiting for the browser to decode it, and while it waits it reads nothing more
off the socket and asks for no further frames. That is the one place in the
client that can go quiet for an unbounded time with the browser itself
perfectly responsive.

Whether it is worth anything depends on the machine you are playing on, so the
page measures it. Press Escape and read the picture note: *decoding held this
page up for at most N ms of that*. If N is small, this switch will not help
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

The mouse turns you. It does **not** walk you forward and back — the original
used the mouse's Y axis for movement, since there was nothing to aim
vertically at, and with a modern hand on the mouse that mostly walks you
about by accident. **Options → Setup → Mouse → MOVE WITH MOUSE** turns the
original behaviour back on. The buttons are assignable on the same page. *Grab pointer* is on by
default and is what `play.html` needs; see the section above.

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

The start screen carries two lines under the buttons:

```
Sound: on — 22050 Hz in, 48000 Hz out, 12.4 s received
1.10.17
```

The first is the live state; if there is no sound it says why instead — a
refused connection, a worklet the browser would not load, a stream that opens
and stays quiet. The same reason appears at the bottom of the screen during
play. The second is the build the **page** came from.

The container prints its own build at startup as `DOOM <version>`, and says
which way it sent the sound on a line beginning `sound:`. That gives three
checks that between them cover everything:

| | |
| --- | --- |
| Page build older than the log's | The browser is running a cached client. Reload with Ctrl+Shift+R. |
| `Sound: … via the fallback` | Normal over plain HTTP — see above. Not a fault. |
| `Sound: … holding N ms` | How far behind the sound is running. It starts near 40 ms and rises only if this machine cannot keep up. |

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
| `Sound: on … N s received` and still silent | It is arriving and being played, so the problem is past the browser: a muted tab, or the machine's output device. |

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
note answers half of it (press Escape and read it). `doom-probe` answers the
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
python3 doom-probe.py your-nas
```

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
