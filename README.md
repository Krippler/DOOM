# DOOM

The 1997 `linuxdoom-1.10` source release, repaired until it builds and runs on
a current 64-bit Linux, given the sound and music the original left unfinished,
and packaged as a container you play in a browser.

```
docker run --rm -p 6080:6080 ghcr.io/krippler/doom
```

Then open **<http://localhost:6080/play.html>** and click to play.

Nothing is installed on the host — no X server, no display, no audio setup,
and no game data to find: the shareware IWAD is in the image, so that command
is the whole of it. To play the full game, mount your own IWAD and it takes
precedence:

```
docker run --rm -p 6080:6080 -v "$PWD/wads:/wads:ro" ghcr.io/krippler/doom
```

---

## What this actually is

id Software published these sources in December 1997. `README.TXT` is
Carmack's note from that release and is left exactly as it was — it describes
the code, not this repository.

What it does not describe is that the code no longer compiles, and did not run
once it did. Getting from there to a playable game took:

- **19 fixes before it would build and run correctly** — a one-byte heap overflow
  in the IWAD search, a WAD structure whose on-disk layout shifted under
  64-bit pointers, pointer arrays allocated at half their size, a 4× scaler
  that assumed big-endian and mirrored every pixel pair, undefined behaviour
  in the input queue, and a sound path that called `exit(-1)` on any system
  without OSS — which is every system now.
- **Sound**, through the separate sound server that shipped with the release,
  given a PulseAudio backend. The original wrote to `/dev/dsp`, which no
  current kernel provides, and PulseAudio's own OSS shim was removed upstream
  in PulseAudio 16.
- **Music**, which was never implemented at all: every music function in the
  1997 sources is an empty stub. The WAD's MUS lumps are converted to Standard
  MIDI and rendered by FluidSynth.
- **A display the engine will accept.** It only ever supported an 8-bit
  PseudoColor X visual, which no current X server offers, so the container
  brings its own Xvfb at depth 8 and exports it over noVNC.
- **A browser client that captures the mouse.** noVNC is a remote desktop
  client and reports where the pointer is; a game needs to know how far it
  moved. `play.html` locks the pointer instead, so turning never runs out of
  screen and the cursor cannot wander off into the rest of your desktop —
  fullscreen or in the window, whichever you pick.

[`PORTING-NOTES.md`](PORTING-NOTES.md) documents every change, with the
original code and why it broke.

## What was added

Three pages under **Options → Setup**, none of which the 1997 release had any
equivalent of:

- **Controls** — rebind the eleven movement, action and menu keys. Saved to
  `.doomrc`.
- **Mouse** — turn the mouse on and off, assign its buttons, and capture the
  pointer so it cannot slide out of the window while you turn.
- **Load WAD** — list the `.wad` files you mounted, marked `GAME` or `MOD`,
  and load one. The engine builds its textures, sprites and sound cache once
  at startup, so choosing a file restarts it: a couple of seconds back to the
  title screen.

## Documentation

| | |
| --- | --- |
| [DOCKER.md](DOCKER.md) | Running it: game data, controls, options, saves, sound, troubleshooting |
| [PORTING-NOTES.md](PORTING-NOTES.md) | Every change made to the 1997 sources, and why |
| [PUBLISHING.md](PUBLISHING.md) | Releases, image tags, and the Unraid listing |
| [CHANGELOG.md](CHANGELOG.md) | What changed in each release |
| [README.TXT](README.TXT) | id Software's original 1997 release note |

## Images

Published to `ghcr.io/krippler/doom`, 609 MB unpacked including the shareware
game data, `linux/amd64` only. `latest` is the newest release, `edge` tracks `master`.
Signed with cosign on every push.

Unraid users: the Community Applications template is
[`templates/unraid.xml`](templates/unraid.xml).

## Building it yourself

```
docker build -t doom .
```

Or without a container, if you have an 8-bit X display to point it at:

```
make -C linuxdoom-1.10
make -C sndserv
```

## Licence and game data

The sources are GPLv2 — see [LICENSE.TXT](LICENSE.TXT), which is the licence
id relicensed them under in 1999. The per-file headers still carry the older
1997 DOOM Source Code License notice; they were never updated upstream.

None of that covers the game data. The shareware `DOOM1.WAD` in
[`shareware/`](shareware/) is id's, distributed under their shareware terms —
free to copy unmodified, not to sell — and is the one data file included.
Episode 1, nine levels.

`DOOM.WAD`, `DOOM2.WAD`, `TNT.WAD` and `PLUTONIA.WAD` are commercial and come
from your own copy of the game. They are not here, are not in the published
images, and the ignore rules exclude every `*.wad` except the shareware one by
name so they cannot be committed or baked in by accident.

DOOM is a trademark of id Software LLC. This is an unaffiliated port of the
sources they published.
