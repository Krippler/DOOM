# DOOM in a container

Builds the original `linuxdoom-1.10` sources and runs them inside the image,
with the game reachable from a browser. Nothing needs to be installed on the
host beyond Docker — no X server and no display. Sound is optional and needs
only a shared audio socket; see below.

```
docker build -t doom .
docker run --rm -p 6080:6080 -v "$PWD/wads:/wads:ro" doom
```

Then open **<http://localhost:6080/vnc.html?autoconnect=1&resize=scale>** and
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

## Size

The image is about 330 MB to pull and 1.1 GB unpacked. Most of that is not
DOOM: the FluidR3 soundfont is 142 MB, `xvfb` pulls in Mesa and LLVM for GLX
support at around 180 MB, and `novnc` depends on Node and a Python stack. The
engine and sound server together are under 600 kB. Swapping
`fluid-soundfont-gm` for `timgm6mb-soundfont` in the Dockerfile is the one
easy saving, at the cost of music quality.

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
| `DOOM_SOUNDFONT` | FluidR3 GM | General MIDI soundfont used for music. |

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

Bind-mounting a host directory there needs one extra step, because the
container runs as uid 1001 and a directory you created is owned by you. Run
the container as yourself:

```
docker run --rm -p 6080:6080 --user "$(id -u):$(id -g)" \
    -v "$PWD/wads:/wads:ro" -v "$PWD/state:/doom/state" doom
```

or hand the directory over with `chown 1001:1001 state`. The container checks
this at startup and says which to do rather than failing obscurely.

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

The image ships FluidR3 GM, which is the good one. It costs about 140 MB of
image size, but not much else: FluidSynth is told to load sample data as
instruments are used rather than reading the whole file at startup, so the
engine reaches the title screen in about a tenth of a second and the extra
memory is roughly 20 MB over a small soundfont. Reading it up front instead
would stall startup for ten seconds and cost 140 MB of RSS.

To use a different one, mount it and point `DOOM_SOUNDFONT` at it:

```
docker run --rm -p 6080:6080 \
    -v "$PWD/wads:/wads:ro" \
    -v "$PWD/other.sf2:/sf/gm.sf2:ro" \
    -e DOOM_SOUNDFONT=/sf/gm.sf2 \
    -e PULSE_SERVER=unix:/tmp/pulse \
    -v "/run/user/$(id -u)/pulse/native:/tmp/pulse:ro" \
    doom
```

`-soundfont <file>` on the command line does the same thing. To trade the
music quality back for a much smaller image, replace `fluid-soundfont-gm`
with `timgm6mb-soundfont` in the Dockerfile; the engine finds it on its own.
If no soundfont can be read the game still runs, without music.

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

**"state directory is not writable".** A host directory bind-mounted at
`/doom/state` is owned by you, not by the container's user. See *Saves and
config* above; a named volume avoids the problem entirely.

**Diagnostics.** `xvfb.log`, `x11vnc.log` and `websockify.log` are written to
`/doom/state`.
