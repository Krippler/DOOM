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
is the whole of it, and picture and sound both arrive in the browser.

## Your own game data

The shareware IWAD plays episode 1. To play the full game, mount a directory
holding your own IWAD; a mounted one always takes precedence over the bundled
file.

```
docker run --rm -p 6080:6080 -v "$PWD/wads:/wads:ro" ghcr.io/krippler/doom
```

`DOOM.WAD`, `DOOM2.WAD`, `TNT.WAD` and `PLUTONIA.WAD` are recognised, as are
`DOOMU.WAD` and `DOOM2F.WAD`, case-insensitively. Mods go in the same
directory and are loaded from the game's own **Options → Setup → Load WAD**.

## Keeping savegames and settings

Savegames and `.doomrc` live in `/doom/state`. Without a volume there they go
when the container does:

```
docker run --rm -p 6080:6080 \
  -v "$PWD/wads:/wads:ro" -v doom-state:/doom/state \
  ghcr.io/krippler/doom
```

With Compose:

```
mkdir -p wads && cp /path/to/DOOM1.WAD wads/
docker compose up --build
```

Running as root is not required. Started as root the container takes ownership
of the state directory as `PUID:PGID` (1001 by default, 99:100 on Unraid) and
drops to that user; started with `--user` it stays as whoever you gave it.

## Images

Published to `ghcr.io/krippler/doom`, 609 MB unpacked including the shareware
game data, `linux/amd64` only. `latest` is the newest release, `edge` tracks
`master`. Signed with cosign on every push.

Unraid users: the Community Applications template is
[`templates/unraid.xml`](templates/unraid.xml).

## Building it yourself

```
docker build -t doom .
```

Or without a container, on any Linux X display:

```
make -C linuxdoom-1.10
make -C sndserv
```

`tools/smoke-test.sh` checks a build: it plays a little of E1M1 on Xvfb and
says what worked. See [PORTING-NOTES.md](PORTING-NOTES.md#verifying).

## Documentation

| | |
| --- | --- |
| [ABOUT.md](ABOUT.md) | What this actually is: what the 1997 sources needed, and what the port added |
| [DOCKER.md](DOCKER.md) | Running it: game data, controls, game controllers, options, saves, sound, measuring a stutter, troubleshooting |
| [PORTING-NOTES.md](PORTING-NOTES.md) | Every change made to the 1997 sources, and why |
| [PUBLISHING.md](PUBLISHING.md) | Releases, image tags, and the Unraid listing |
| [CHANGELOG.md](CHANGELOG.md) | What changed in each release |
| [README.TXT](README.TXT) | id Software's original 1997 release note |

Every environment variable, every in-game and in-browser setting, and the
troubleshooting for sound, controllers and a stuttering picture are in
[DOCKER.md](DOCKER.md).

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
