# DOOM in a container

Builds the original `linuxdoom-1.10` sources and runs them inside the image,
with the game reachable from a browser. Nothing needs to be installed on the
host beyond Docker — no X server, no display, no audio.

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

Anything you pass after the image name goes straight to the engine:

```
docker run --rm -p 6080:6080 -v "$PWD/wads:/wads:ro" doom -warp 1 1 -skill 4
```

Useful engine flags: `-warp <episode> <map>`, `-skill 1..5`, `-nomonsters`,
`-respawn`, `-fast`, `-file <pwad>`, `-devparm` (F1 writes a PCX screenshot).

## Saves and config

`/doom/state` holds `.doomrc` and `doomsavN.dsg`. The Compose file keeps it in
a named volume so saves survive `docker compose down`. With plain `docker run`,
mount it yourself if you want saves to persist:

```
docker run --rm -p 6080:6080 -v "$PWD/wads:/wads:ro" -v "$PWD/state:/doom/state" doom
```

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

There is none. The engine predates ALSA and expects either an OSS `/dev/dsp`
or the external `sndserver` helper talking to one. OSS is long gone from
modern kernels and there is no audio device in the container, so the game runs
silent. Everything else is unaffected — the missing device used to be fatal,
and that is fixed (see `PORTING-NOTES.md`).

## Troubleshooting

**The browser shows a black canvas.** Click it first; noVNC only forwards
keyboard input once the canvas has focus.

**"no game data to run".** The mounted directory has no recognised IWAD in its
top level. `-v "$PWD/wads:/wads:ro"` mounts `./wads`, so the file must be at
`./wads/DOOM1.WAD`, not in a subdirectory.

**Keys do nothing.** Doom uses Ctrl to fire and Alt to strafe, which browsers
and window managers like to intercept. noVNC's toolbar has a modifier-key
pad for exactly this.

**Diagnostics.** `xvfb.log`, `x11vnc.log` and `websockify.log` are written to
`/doom/state`.
