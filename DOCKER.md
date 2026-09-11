# DOOM in a container

Builds the original `linuxdoom-1.10` sources and runs them inside the image,
with the game reachable from a browser. Nothing needs to be installed on the
host beyond Docker — no X server and no display. Sound is optional and needs
only a shared audio socket; see below.

```
docker run --rm -p 6080:6080 -v "$PWD/wads:/wads:ro" ghcr.io/krippler/doom
```

Or build it yourself, which is what the rest of this page assumes when it
writes `doom` instead of the full image name:

```
docker build -t doom .
docker run --rm -p 6080:6080 -v "$PWD/wads:/wads:ro" doom
```

Then open **<http://localhost:6080/vnc.html?autoconnect=1&resize=off>** and
click into the canvas to give it the keyboard.

With Compose:

```
mkdir -p wads && cp /path/to/DOOM1.WAD wads/
docker compose up --build
```

## Game data

The source release contains no game data, so you have to supply an IWAD. Put
one in the directory you mount at `/wads`. The engine recognises these names,
and the container matches them case-insensitively, so `DOOM1.WAD` works as
well as `doom1.wad`:

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

If no IWAD is found the container stops immediately and lists what it looked
for.

Mods (PWADs) go in the same directory. They are not matched against the table
above; anything ending in `.wad` shows up in the in-game WAD menu below.

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
| Menu | Esc |

All of these can be changed from the game: **Esc → Options → Setup →
Controls**. Pick a line, press Return, then press the key you want. The
choice is written to `.doomrc` in the state directory, so it survives a
restart.

The mouse turns and moves, and is off until you turn it on under **Options →
Setup → Mouse**, where the buttons are assignable too. With *grab pointer* on,
the pointer is confined to the game window while you play; Esc releases it
along with everything else, since it only holds while the menu is closed.

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
| `DOOM_STATE` | `/doom/state` | Config file, savegames and logs. |
| `DOOM_WEB_PORT` | `6080` | noVNC HTTP port. |
| `DOOM_VNC_PORT` | `5900` | VNC port. |
| `DOOM_VNC_PASSWORD` | unset | If set, the VNC session requires this password. |
| `DOOM_SOUND` | `1` | Set to `0` to not start the sound server at all. |
| `PULSE_SERVER` | unset | PulseAudio server for sound, e.g. `unix:/tmp/pulse`. |
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

The engine plays effects through a separate `sndserver` process, which is how
the original release worked. That server has been given a PulseAudio backend
(`sndserv/pulse.c`), so audio leaves the container over a PulseAudio socket.

Music works too. linuxdoom shipped every music function as an empty stub, so
this adds them: `mus2mid.c` converts the WAD's MUS lumps into Standard MIDI
and FluidSynth renders them against a General MIDI soundfont, on its own
connection to the same PulseAudio server. Both need the same shared socket,
so the instructions below cover music as well.

Note on `padsp`: the usual way to feed OSS-era software into PulseAudio no
longer exists. `padsp` and its `libpulsedsp.so` were removed upstream in
PulseAudio 16 and are not in any current distribution, and `aoss` (the ALSA
equivalent) needs a real ALSA stack in the container and fails the format
negotiation the engine does at startup. Talking to PulseAudio directly avoids
both problems and works against PipeWire too, since `pipewire-pulse` accepts
PulseAudio clients unchanged.

To hear the game, share the host's audio socket. On a normal Linux desktop
running PulseAudio or PipeWire:

```
docker run --rm -p 6080:6080 \
    -v "$PWD/wads:/wads:ro" \
    -v "/run/user/$(id -u)/pulse/native:/tmp/pulse:ro" \
    -v "$HOME/.config/pulse/cookie:/doom/state/.pulse-cookie:ro" \
    -e PULSE_SERVER=unix:/tmp/pulse \
    -e PULSE_COOKIE=/doom/state/.pulse-cookie \
    doom
```

The cookie is only needed if your PulseAudio requires authentication; many
setups work without it. If the socket is not readable by the container user,
add `--user "$(id -u):$(id -g)"`.

Without any of that the container prints how to enable sound and plays
silently — a missing or unreachable audio server is not fatal.

Set `DOOM_SOUND=0` to skip starting the sound server entirely. Effects and
music have separate volume sliders under Options → Sound Volume.

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

## Troubleshooting

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
