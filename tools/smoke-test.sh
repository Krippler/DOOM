#!/bin/sh
#
# Start the engine on a real X server and play a little of E1M1 with a
# controller, checking what comes out: the picture, the sound, the vibration,
# the menus, the config it saves, and the report it prints when it crashes.
#
# The game data is the shareware doom1.wad the repository already carries,
# which id distributes freely (shareware/README.md), so this reaches a real
# level: the WAD reader, the renderer, the status bar, the player's movement
# and weapons, the sound chain as far as the stream a browser would receive,
# and the controller from the socket the page talks to.
#
#   tools/smoke-test.sh [workdir]
#
# Needs Xvfb and python3; the engine and audiostream built first:
#
#   make -C linuxdoom-1.10 && make -C audiostream
#
# The phases, each on a fresh X server:
#
#   level     depth 24, the mixer, a controller: E1M1 appears, the sticks walk
#             and turn, RT fires and is heard and felt, Start opens the menu,
#             and SIGINT saves the config and exits 0
#   music     the title screen with music on is not silent (skipped when no
#             General MIDI soundfont is installed)
#   depth 8   the colour-mapped path the engine was written for draws the
#             status bar exactly as the truecolour one does
#   lerp      Max FPS 90 draws well over 35 frames a second, between tics,
#             and the setting is saved
#   umapinfo  a level starts with a UMAPINFO lump naming it, its sky and its
#             music, and the lump is read without complaint (the reader
#             itself is tools/umapinfo-test.c's)
#   flats     a mod's own F_START/F_END flats join the game's rather than
#             replace them, and E1M1 stays up with them
#   limits    E1M1 as a limit-removing map brings it: compressed ZDBSP
#             nodes with children past 15 bits, no BLOCKMAP, walls in a
#             256-tall texture from a TEXTURE1 of its own, a sprite range of
#             its own; it stays up and the texture is drawn
#   boom      E1M1 in a PWAD with Boom's light transfers (213 and 261) from
#             a dark room to every other: it is read as a Boom map, and the
#             floors and ceilings go dark, the blue pool with them
#   dehacked  a DEHACKED lump is read, and a mod's own pistol sound (silence)
#             is the one the mixer plays (needs xdotool)
#   mbf21     a patch past DOOM's own numbers (DSDHacked) and MBF21's code
#             pointers: the pistol, made silent, fires through a new frame
#             that plays sound 700 (A_WeaponSound), a tone of the patch's
#             own; the tone is heard, so the engine and the mixer both
#             took a sound numbered past 255 (needs xdotool)
#   id24      E1M1 ends by itself (special 11, on the easiest skill), and
#             its tally is an INTERLEVEL lump's: the background, and of its
#             layer's two squares the one whose condition holds
#   ogg       a level whose music is an Ogg Vorbis lump, as SIGIL II's are,
#             is heard at the tone's own pitch (needs oggenc, from
#             vorbis-tools, and a soundfont; skipped without)
#   recover   a WAD the engine cannot start on, as if chosen from Load WAD:
#             it goes back to the game before, still listening for the
#             controller, and says why
#   loadwad   Load WAD, walked through with keys, restarts the engine, and
#             the controller's port is listened on again (needs xdotool)
#   newgame   New Game offers the WAD folder first; another file restarts
#             the engine on it and on to the difficulty menu (needs xdotool)
#   savegame  a game saved on one WAD and loaded from the menu on another
#             restarts the engine on the first and loads it there; a save
#             that does not fit the map it names is refused, not loaded
#             into a crash (needs xdotool for the first half)
#   pointer   with capture on, the pointer is held at the centre in a level
#             and let go in the menu (needs xdotool; skipped without it)
#   crash     SIGSEGV prints "DOOM died on" and a backtrace with names in it
#
# Timings are waited for, not slept on, where there is anything to wait for:
# a shared CI runner takes as long as it takes.
#

set -u

here=$(cd "$(dirname "$0")/.." && pwd)
work=${1:-${TMPDIR:-/tmp}/doom-smoke}
engine="$here/linuxdoom-1.10/linux/linuxxdoom"
mixer="$here/audiostream/linux/audiostream"
wad="$here/shareware/doom1.wad"
client="$here/tools/smoke-client.py"

say() { printf '[smoke] %s\n' "$*"; }
die() { printf '[smoke] FAILED: %s\n' "$*" >&2; exit 1; }

[ -x "$engine" ] || die "no engine at $engine -- run: make -C linuxdoom-1.10"
[ -x "$mixer" ] || die "no mixer at $mixer -- run: make -C audiostream"
[ -r "$wad" ] || die "no shareware WAD at $wad"
command -v Xvfb >/dev/null 2>&1 || die "Xvfb is not installed"
command -v python3 >/dev/null 2>&1 || die "python3 is not installed"

rm -rf "$work"
mkdir -p "$work/wads"
ln -s "$wad" "$work/wads/doom1.wad"

# Ports and a display nobody else on the machine is likely to be using.
disp=:$((90 + $$ % 9))
audio_port=$((20000 + $$ % 1000))
pad_port=$((21000 + $$ % 1000))

xvfb_pid= mixer_pid= game_pid=

cleanup() {
    for pid in $game_pid $mixer_pid $xvfb_pid; do
        kill "$pid" 2>/dev/null || true
    done
    wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

start_x() {     # depth
    mkdir -p "$work/fb$1"
    Xvfb "$disp" -screen 0 640x400x"$1" -fbdir "$work/fb$1" -nolisten tcp \
        >"$work/xvfb$1.log" 2>&1 &
    xvfb_pid=$!
    i=0
    while [ ! -e "/tmp/.X11-unix/X${disp#:}" ]; do
        i=$((i + 1))
        [ "$i" -gt 100 ] && die "Xvfb did not start; see $work/xvfb$1.log"
        sleep 0.1
    done
}

stop_x() {
    kill "$xvfb_pid" 2>/dev/null || true
    wait "$xvfb_pid" 2>/dev/null || true
    xvfb_pid=
    i=0
    while [ -e "/tmp/.X11-unix/X${disp#:}" ] && [ "$i" -lt 50 ]; do
        i=$((i + 1))
        sleep 0.1
    done
}

start_mixer() {
    rm -f "$work/sfx.sock" "$work/music.pipe"
    DOOMWADDIR="$work/wads" "$mixer" --commands "$work/sfx.sock" \
        --music "$work/music.pipe" --rate 22050 --port "$audio_port" \
        >"$work/audiostream.log" 2>&1 &
    mixer_pid=$!
    i=0
    while [ ! -p "$work/music.pipe" ]; do
        i=$((i + 1))
        [ "$i" -gt 100 ] && die "audiostream did not start; see $work/audiostream.log"
        sleep 0.1
    done
}

stop_mixer() {
    kill "$mixer_pid" 2>/dev/null || true
    wait "$mixer_pid" 2>/dev/null || true
    mixer_pid=
}

# The engine as the container runs it, less the parts that belong to the
# container. HOME is where it keeps .doomrc.
# Savegames go where it is started: $game_dir, if set.
start_game() {  # log, then engine arguments
    log=$1
    shift
    ( cd "${game_dir:-.}" && DISPLAY="$disp" HOME="$work" \
        DOOMWADDIR="$work/wads" exec "$engine" -2 \
        -iwad "${game_iwad:-$work/wads/doom1.wad}" "$@" ) \
        >"$work/$log" 2>&1 &
    game_pid=$!
}

# Wait up to $1 seconds for the engine to exit, and leave its status in $st,
# or "timeout". A watchdog and a real wait, not a loop of kill -0: an engine
# that has exited stays a zombie until it is waited for, and kill -0 goes on
# succeeding on a zombie -- which is how the first version of this reported a
# two-second shutdown as a ten-second hang.
wait_game() {
    # In tenths, so that killing the watchdog leaves at most a tenth of a
    # second of sleep behind rather than an orphan for the whole wait.
    ( i=0
      while [ "$i" -lt $(($1 * 10)) ]; do sleep 0.1; i=$((i + 1)); done
      kill -KILL "$game_pid" 2>/dev/null; touch "$work/.timeout" ) &
    dog=$!
    rm -f "$work/.timeout"
    wait "$game_pid"
    st=$?
    kill "$dog" 2>/dev/null
    wait "$dog" 2>/dev/null
    [ -e "$work/.timeout" ] && st=timeout
    game_pid=
}

# add_lumps IN OUT NAME FILE [NAME FILE ...]
# A copy of a WAD with lumps added at its end, as a PWAD would add them: the
# shareware episode cannot load PWADs, so what a test adds goes into a copy
# of the WAD itself, still called doom1.wad for the engine to know it.
add_lumps() {
    python3 - "$@" <<'EOF'
import struct, sys
src, dst, rest = sys.argv[1], sys.argv[2], sys.argv[3:]
data = open(src, 'rb').read()
kind, count, diroff = struct.unpack('<4sii', data[:12])
body = bytearray(data[12:diroff])
directory = bytearray(data[diroff:diroff + 16 * count])
for name, path in zip(rest[0::2], rest[1::2]):
    lump = open(path, 'rb').read()
    directory += struct.pack('<ii8s', 12 + len(body), len(lump), name.encode())
    body += lump
    count += 1
open(dst, 'wb').write(struct.pack('<4sii', kind, count, 12 + len(body))
                      + body + directory)
EOF
}

# -------------------------------------------------------------------- level
say "level: E1M1 at depth 24, with the mixer and a controller"
start_x 24
start_mixer

# Music off for this phase, so the shot is the only sound there is to hear.
printf 'music_volume 0\nsfx_volume 15\n' >"$work/.doomrc"

DOOM_SFX_SOCKET="$work/sfx.sock" DOOM_MUSIC_PIPE="$work/music.pipe" \
DOOM_AUDIO_RATE=22050 DOOM_PAD_PORT="$pad_port" \
    start_game level.log -warp 1 1

python3 "$client" level --fb "$work/fb24/Xvfb_screen0" --pad "$pad_port" \
    --audio "$audio_port" --out "$work" \
    || { tail -20 "$work/level.log" >&2; die "the level phase; see $work/level.log"; }

grep -q "^Controller: from the browser, port $pad_port" "$work/level.log" \
    || die "the engine never said it was listening for the controller"
grep -q "^Controller: Smoke Test Pad" "$work/level.log" \
    || die "the engine never named the controller it was handed"
say "the engine took the controller and named it"

kill -INT "$game_pid"
wait_game 10
[ "$st" = 0 ] || die "SIGINT should quit cleanly with 0; got $st (see $work/level.log)"
grep -q '^pad_rt' "$work/.doomrc" \
    || die "quitting did not save the controller settings to .doomrc"
say "SIGINT quits with 0 and saves .doomrc, controller settings included"

stop_mixer
stop_x

# -------------------------------------------------------------------- music
soundfont=
for sf in /usr/share/sounds/sf2/default-GM.sf2 /usr/share/sounds/sf2/FluidR3_GM.sf2 \
          /usr/share/sounds/sf2/TimGM6mb.sf2 /usr/share/soundfonts/default.sf2; do
    [ -r "$sf" ] && { soundfont=$sf; break; }
done

if [ -z "$soundfont" ]; then
    say "music: skipped, no General MIDI soundfont installed"
else
    say "music: the title screen, with $(basename "$soundfont")"
    start_x 24
    start_mixer
    printf 'music_volume 15\nsfx_volume 0\n' >"$work/.doomrc"
    DOOM_SFX_SOCKET="$work/sfx.sock" DOOM_MUSIC_PIPE="$work/music.pipe" \
    DOOM_AUDIO_RATE=22050 DOOM_SOUNDFONT="$soundfont" \
        start_game music.log -nojoy
    python3 "$client" music --audio "$audio_port" \
        || { tail -20 "$work/music.log" >&2; die "the music phase; see $work/music.log"; }
    kill "$game_pid" 2>/dev/null
    wait_game 10
    stop_mixer
    stop_x
fi

# ------------------------------------------------------------------ depth 8
say "depth 8: the colour-mapped path"
start_x 8
rm -f "$work/.doomrc"
start_game depth8.log -warp 1 1 -nojoy
grep -q "truecolour" "$work/depth8.log" && die "an 8-bit screen was drawn in truecolour"
python3 "$client" compare --fb "$work/fb8/Xvfb_screen0" --wad "$wad" \
    --src "$here/linuxdoom-1.10/v_video.c" --out "$work" \
    || { tail -20 "$work/depth8.log" >&2; die "the depth 8 phase; see $work/depth8.log"; }
grep -q "I_InitGraphics: 8-bit PseudoColor" "$work/depth8.log" \
    || die "the engine did not report the 8-bit path"
kill "$game_pid" 2>/dev/null
wait_game 10
stop_x

# --------------------------------------------------------------------- lerp
# Max FPS above 35 draws frames between tics. The frame report, printed every
# five seconds with DOOM_FRAME_REPORT=1, counts them: at 90 there have to be
# well over the 175 frames that drawing once a tic would give. The second
# report, as the first counts from the level's first frame and may include
# the loading.
say "lerp: Max FPS 90 draws between tics"
start_x 24
printf 'max_fps 90\n' >"$work/.doomrc"
export DOOM_FRAME_REPORT=1
start_game lerp.log -warp 1 1 -nojoy
unset DOOM_FRAME_REPORT
i=0
until [ "$(grep -c '^frames: ' "$work/lerp.log" 2>/dev/null)" -ge 2 ]; do
    i=$((i + 1))
    [ "$i" -gt 300 ] && { tail -20 "$work/lerp.log" >&2; die "no frame report in 30s; see $work/lerp.log"; }
    sleep 0.1
done
frames=$(grep '^frames: ' "$work/lerp.log" | sed -n 2p | sed 's/^frames: \([0-9]*\) .*/\1/')
say "$frames frames in 5 seconds"
[ "$frames" -ge 250 ] \
    || die "Max FPS 90 drew $frames frames in 5 seconds, not over 250; see $work/lerp.log"
kill -INT "$game_pid" 2>/dev/null
wait_game 10
grep -q '^max_fps[[:space:]]*90' "$work/.doomrc" || die "max_fps was not saved in .doomrc"
rm -f "$work/.doomrc"
stop_x

# ----------------------------------------------------------------- umapinfo
say "umapinfo: E1M1 as a UMAPINFO lump describes it"
mkdir -p "$work/umapinfo"
cat >"$work/umapinfo/umapinfo.txt" <<'EOF'
MAP E1M1 { levelname = "Smoke Test" label = clear
  skytexture = "SKY1" music = "D_E1M2" partime = 45
  next = "E1M3" intertext = "One line", "and another"
  episode = clear episode = "M_EPI1", "Smoke", "s" }
EOF
add_lumps "$wad" "$work/umapinfo/doom1.wad" \
    UMAPINFO "$work/umapinfo/umapinfo.txt" \
    || die "could not add a UMAPINFO lump"
start_x 24
rm -f "$work/.doomrc"
game_iwad="$work/umapinfo/doom1.wad" start_game umapinfo.log -warp 1 1 -nojoy
game_iwad=
i=0
until grep -q "I_InitGraphics" "$work/umapinfo.log" 2>/dev/null; do
    i=$((i + 1))
    [ "$i" -gt 200 ] && die "the engine never opened its window; see $work/umapinfo.log"
    sleep 0.1
done
sleep 2
kill -0 "$game_pid" 2>/dev/null \
    || die "E1M1 with UMAPINFO did not stay up; see $work/umapinfo.log"
grep -q "^U_Init: UMAPINFO describes 1 map" "$work/umapinfo.log" \
    || die "the UMAPINFO lump was not read; see $work/umapinfo.log"
grep -q "^UMAPINFO:" "$work/umapinfo.log" \
    && die "the UMAPINFO lump was read with complaints; see $work/umapinfo.log"
kill "$game_pid" 2>/dev/null
wait_game 10
stop_x
say "the lump is read, and E1M1 plays by it"

# -------------------------------------------------------------------- flats
# A mod with floors of its own, between an F_START and F_END of its own, as a
# new episode brings them -- SIGIL II does. The engine used to take the last
# F_START/F_END pair as the only flats, so every one of the game's got a
# negative number, the sky's included, and drawing a level read far outside
# the arrays; in the field it crashed in R_GetColumn. Added to a copy of the
# shareware IWAD: one flat that replaces FLOOR4_8, one new.
say "flats: a mod's own F_START/F_END, one flat replaced and one added"
mkdir -p "$work/flats"
: >"$work/flats/marker"
python3 -c "import sys; sys.stdout.buffer.write(bytes([96]) * 4096)" >"$work/flats/grey"
python3 -c "import sys; sys.stdout.buffer.write(bytes([176]) * 4096)" >"$work/flats/red"
add_lumps "$wad" "$work/flats/doom1.wad" \
    F_START "$work/flats/marker" FLOOR4_8 "$work/flats/grey" \
    SMOKEFLT "$work/flats/red" F_END "$work/flats/marker" \
    || die "could not add the flats"
start_x 24
rm -f "$work/.doomrc"
game_iwad="$work/flats/doom1.wad" start_game flats.log -warp 1 1 -nojoy
game_iwad=
i=0
until grep -q "I_InitGraphics" "$work/flats.log" 2>/dev/null; do
    i=$((i + 1))
    [ "$i" -gt 200 ] && { tail -20 "$work/flats.log" >&2; die "the engine never opened its window; see $work/flats.log"; }
    sleep 0.1
done
sleep 3
kill -0 "$game_pid" 2>/dev/null \
    || { tail -20 "$work/flats.log" >&2; die "E1M1 with a mod's own flats did not stay up; see $work/flats.log"; }
# 56 flats in the shareware IWAD, counted as id counted them, and one more
grep -q "R_InitFlats: 57 flats, 1 replaced and 1 added" "$work/flats.log" \
    || die "the mod's flats were not merged with the game's; see $work/flats.log"
say "the game's flats and the mod's are one list: 57, one replaced, one added"
kill "$game_pid" 2>/dev/null
wait_game 10
stop_x

# ------------------------------------------------------------------- limits
# What a limit-removing map brings, the way a node builder or a mod writes it,
# into E1M1: its nodes in ZDBSP's compressed format, with 40000 empty
# subsectors first so node children go past the 15 bits the classic format
# has; no BLOCKMAP, so one has to be made; its walls in a 256-tall texture
# drawn from two posts, from a TEXTURE1 that has only that texture (the game's
# own have to be found in the IWAD's); and a sprite range of its own that
# replaces one frame. 1.13 stopped on every one of these.
say "limits: E1M1 with compressed ZDBSP nodes, no blockmap, a tall texture"
mkdir -p "$work/limits"
python3 - "$wad" "$work/limits" <<'EOF' || die "could not make the limits map"
import os, struct, sys, zlib
src, out = sys.argv[1], sys.argv[2]
d = open(src, 'rb').read()
n, ofs = struct.unpack('<ii', d[4:12])
lumps = [struct.unpack('<ii8s', d[ofs + 16 * i:ofs + 16 * i + 16]) for i in range(n)]
lumps = [(nm.rstrip(b'\0').decode(), d[fp:fp + sz]) for fp, sz, nm in lumps]
i = [nm for nm, _ in lumps].index('E1M1')
m = dict(lumps[i + 1:i + 11])
g = lambda data, fmt: [list(r) for r in struct.iter_unpack(fmt, data)]
lines = g(m['LINEDEFS'], '<7H')
sides = g(m['SIDEDEFS'], '<hh8s8s8sH')
verts = g(m['VERTEXES'], '<hh')
segs = g(m['SEGS'], '<HHhHhh')
ssecs = g(m['SSECTORS'], '<HH')
nodes = g(m['NODES'], '<12h2H')
pad = 40000
child = lambda c: (c & 0x7fff | 0x80000000) + pad if c & 0x8000 else c
used = max(max(l[0], l[1]) for l in lines) + 1
body = struct.pack('<II', used, len(verts) - used)
body += b''.join(struct.pack('<ii', x << 16, y << 16) for x, y in verts[used:])
body += struct.pack('<I', pad + len(ssecs))
body += struct.pack('<I', 0) * pad + b''.join(struct.pack('<I', c) for c, _ in ssecs)
body += struct.pack('<I', len(segs)) + b''.join(
    struct.pack('<IIHB', a, b, ln, sd) for a, b, _, ln, sd, _ in segs)
body += struct.pack('<I', len(nodes)) + b''.join(
    struct.pack('<12h2I', *r[:12], child(r[12]), child(r[13])) for r in nodes)
m['NODES'] = b'ZNOD' + zlib.compress(body)
m['SSECTORS'] = m['SEGS'] = m['BLOCKMAP'] = b''
m['VERTEXES'] = b''.join(struct.pack('<hh', *v) for v in verts[:used])
for l in lines:
    if l[6] == 0xffff:
        sides[l[5]][4] = b'SMOKTALL'
m['SIDEDEFS'] = b''.join(struct.pack('<hh8s8s8sH', *s) for s in sides)
# 64 by 256 in two posts of 128: 32-row bands, red (176) at the top
cols = []
for x in range(64):
    c = b''
    for top in (0, 128):
        px = bytes((176, 112, 200, 231)[(top + r) // 32 % 4] for r in range(128))
        c += bytes([top, 128, px[0]]) + px + px[-1:]
    cols.append(c + b'\xff')
patch = struct.pack('<4h', 64, 256, 0, 0)
at = 8 + 4 * 64
for c in cols:
    patch += struct.pack('<i', at)
    at += len(c)
patch += b''.join(cols)
tex = lambda name, h, p: name.ljust(8, b'\0') + struct.pack('<ihhihhhhhh', 0, 64, h, 0, 1, 0, 0, p, 1, 0)
texture1 = struct.pack('<iii', 2, 12, 12 + 32) + tex(b'AASTINKY', 128, 0) + tex(b'SMOKTALL', 256, 0)
put = {'marker': b'', 'pnames': struct.pack('<i', 1) + b'SMOKPAT\0', 'texture1': texture1,
       'patch': patch, 'sprite': dict(lumps)['BAR1A0']}
put.update({k: m[k] for k in m})
for k, v in put.items():
    open(os.path.join(out, k), 'wb').write(v)
EOF
L="$work/limits"
add_lumps "$wad" "$L/doom1.wad" \
    PNAMES "$L/pnames" TEXTURE1 "$L/texture1" \
    P_START "$L/marker" SMOKPAT "$L/patch" P_END "$L/marker" \
    SS_START "$L/marker" TROOA0 "$L/sprite" SS_END "$L/marker" \
    E1M1 "$L/marker" THINGS "$L/THINGS" LINEDEFS "$L/LINEDEFS" \
    SIDEDEFS "$L/SIDEDEFS" VERTEXES "$L/VERTEXES" SEGS "$L/SEGS" \
    SSECTORS "$L/SSECTORS" NODES "$L/NODES" SECTORS "$L/SECTORS" \
    REJECT "$L/REJECT" BLOCKMAP "$L/BLOCKMAP" \
    || die "could not add the limits map"
start_x 24
rm -f "$work/.doomrc"
game_iwad="$L/doom1.wad" start_game limits.log -warp 1 1 -nojoy
game_iwad=
i=0
until grep -q "I_InitGraphics" "$work/limits.log" 2>/dev/null; do
    i=$((i + 1))
    [ "$i" -gt 200 ] && { tail -20 "$work/limits.log" >&2; die "the engine never opened its window; see $work/limits.log"; }
    sleep 0.1
done
sleep 3
kill -0 "$game_pid" 2>/dev/null \
    || { tail -20 "$work/limits.log" >&2; die "E1M1 with the limits map did not stay up; see $work/limits.log"; }
grep -q "P_SetupLevel: ZDBSP extended nodes, compressed" "$work/limits.log" \
    || die "the ZDBSP nodes were not read as such; see $work/limits.log"
grep -q "P_CreateBlockMap: [0-9]* by [0-9]* blocks" "$work/limits.log" \
    || die "no blockmap was made for a map without one; see $work/limits.log"
grep -q "R_InitTextures: 124 textures from other files added" "$work/limits.log" \
    || die "the game's textures were not found beside the mod's; see $work/limits.log"
# The tall texture on the screen: its bands are red, green, blue and
# yellow, lit as the room is. Green is the test: E1M1's own start view has
# under a hundred green pixels (and thousands of blue, its pool).
python3 - "$work/fb24/Xvfb_screen0" <<'EOF' \
    || die "the 256-tall texture is not on the screen; see $work/limits.log"
import struct, sys
d = open(sys.argv[1], 'rb').read()
hsize, = struct.unpack('>I', d[:4])
w, h = struct.unpack('>II', d[16:24])
bpl, = struct.unpack('>I', d[48:52])
ncolors, = struct.unpack('>I', d[76:80])
px = d[hsize + ncolors * 12:]
blue = green = 0
for y in range(h):
    row = px[y * bpl:y * bpl + 4 * w]
    for x in range(0, 4 * w, 4):
        b, g, r = row[x], row[x + 1], row[x + 2]
        if b > 60 and b > 2 * max(r, g):
            blue += 1
        if g > 60 and g > 2 * max(r, b):
            green += 1
print('[smoke] %d blue and %d green pixels' % (blue, green))
sys.exit(0 if green > 1000 else 1)
EOF
cp "$work/fb24/Xvfb_screen0" "$work/limits.xwd"
say "compressed ZDBSP nodes, a made blockmap, a 256-tall texture: E1M1 stays up and draws it"
kill "$game_pid" 2>/dev/null
wait_game 10
stop_x

# --------------------------------------------------------------------- boom
# A Boom map: E1M1 with the darkest room's light put out, and two of its walls
# made Boom's light transfers, 213 for floors and 261 for ceilings, to every
# other sector. The map has to come from a PWAD (an IWAD's maps always play
# as DOOM's), and the shareware IWAD will not take one, so the IWAD here is a
# copy under a name the engine does not know. The special makes it a Boom
# map; drawn as one, the start view's floors and ceilings are black, and the
# blue pool in it (thousands of pixels in DOOM's drawing) is gone.
say "boom: E1M1 with light transfers (213, 261) from a dark room"
mkdir -p "$work/boom"
cp "$wad" "$work/boom/smoke.wad"
python3 - "$wad" "$work/boom/boom.wad" <<'EOF' || die "could not make the Boom map"
import struct, sys
src, out = sys.argv[1], sys.argv[2]
d = open(src, 'rb').read()
n, ofs = struct.unpack('<ii', d[4:12])
lumps = [struct.unpack('<ii8s', d[ofs + 16 * i:ofs + 16 * i + 16]) for i in range(n)]
lumps = [(nm.rstrip(b'\0').decode(), d[fp:fp + sz]) for fp, sz, nm in lumps]
i = [nm for nm, _ in lumps].index('E1M1')
maplumps = lumps[i:i + 11]
m = dict(maplumps[1:])
lines = [list(r) for r in struct.iter_unpack('<7H', m['LINEDEFS'])]
sides = [list(r) for r in struct.iter_unpack('<hh8s8s8sH', m['SIDEDEFS'])]
secs = [list(r) for r in struct.iter_unpack('<hh8s8shhh', m['SECTORS'])]
dark = min(range(len(secs)), key=lambda s: secs[s][4])
secs[dark][4] = 0
walls = [l for l in lines if l[6] == 0xffff and sides[l[5]][5] == dark]
walls[0][3:5] = [213, 999]
walls[1][3:5] = [261, 999]
for k, s in enumerate(secs):
    if k != dark:
        s[6] = 999
m['LINEDEFS'] = b''.join(struct.pack('<7H', *l) for l in lines)
m['SECTORS'] = b''.join(struct.pack('<hh8s8shhh', *s) for s in secs)
body = dirs = b''
for nm, data in [('E1M1', b'')] + [(k, m[k]) for k, _ in maplumps[1:]]:
    dirs += struct.pack('<ii8s', 12 + len(body), len(data), nm.encode())
    body += data
open(out, 'wb').write(struct.pack('<4sii', b'PWAD', 11, 12 + len(body)) + body + dirs)
EOF
start_x 24
rm -f "$work/.doomrc"
game_iwad="$work/boom/smoke.wad" start_game boom.log \
    -file "$work/boom/boom.wad" -warp 1 1 -nojoy
game_iwad=
i=0
until grep -q "I_InitGraphics" "$work/boom.log" 2>/dev/null; do
    i=$((i + 1))
    [ "$i" -gt 200 ] && { tail -20 "$work/boom.log" >&2; die "the engine never opened its window; see $work/boom.log"; }
    sleep 0.1
done
sleep 3
kill -0 "$game_pid" 2>/dev/null \
    || { tail -20 "$work/boom.log" >&2; die "E1M1 as a Boom map did not stay up; see $work/boom.log"; }
grep -q "P_SetupLevel: boom.wad's maps play as Boom's" "$work/boom.log" \
    || die "the map was not read as a Boom map; see $work/boom.log"
python3 - "$work/fb24/Xvfb_screen0" <<'EOF' \
    || die "the light transfers were not drawn: the pool is still lit; see $work/boom.log"
import struct, sys
d = open(sys.argv[1], 'rb').read()
hsize, = struct.unpack('>I', d[:4])
w, h = struct.unpack('>II', d[16:24])
bpl, = struct.unpack('>I', d[48:52])
ncolors, = struct.unpack('>I', d[76:80])
px = d[hsize + ncolors * 12:]
blue = 0
for y in range(h):
    row = px[y * bpl:y * bpl + 4 * w]
    for x in range(0, 4 * w, 4):
        b, g, r = row[x], row[x + 1], row[x + 2]
        if b > 60 and b > 2 * max(r, g):
            blue += 1
print('[smoke] %d blue pixels' % blue)
sys.exit(0 if blue < 100 else 1)
EOF
cp "$work/fb24/Xvfb_screen0" "$work/boom.xwd"
say "a Boom map from a PWAD: read as one, and its light transfers drawn"
kill "$game_pid" 2>/dev/null
wait_game 10
stop_x

# ----------------------------------------------------------------- dehacked
# A mod's DEHACKED lump is read, and a mod's own sounds reach the mixer. The
# mixer used to load every effect from an IWAD it found by itself, so a mod's
# sounds were never heard. Here a copy of the shareware IWAD carries a patch
# and a pistol sound of silence: firing has to be silent.
if command -v xdotool >/dev/null 2>&1; then
    say "dehacked: a DEHACKED lump, and a mod's own (silent) pistol"
    mkdir -p "$work/deh"
    printf 'Patch File for DeHackEd v3.0\n\nMisc 0\nInitial Health = 150\n\n[STRINGS]\nGOTCLIP = Smoke test clip\n' \
        >"$work/deh/dehacked"
    python3 -c "import struct, sys; sys.stdout.buffer.write(struct.pack('<HHI', 3, 11025, 4000) + bytes([128]) * 4000)" \
        >"$work/deh/dspistol"
    add_lumps "$wad" "$work/deh/doom1.wad" \
        DEHACKED "$work/deh/dehacked" DSPISTOL "$work/deh/dspistol" \
        || die "could not add the patch and the sound"
    start_x 24
    start_mixer
    printf 'music_volume 0\nsfx_volume 15\n' >"$work/.doomrc"
    DOOM_SFX_SOCKET="$work/sfx.sock" DOOM_MUSIC_PIPE="$work/music.pipe" \
    DOOM_AUDIO_RATE=22050 game_iwad="$work/deh/doom1.wad" \
        start_game dehacked.log -warp 1 1 -nojoy
    game_iwad=
    i=0
    until grep -q "I_InitGraphics" "$work/dehacked.log" 2>/dev/null; do
        i=$((i + 1))
        [ "$i" -gt 200 ] && { tail -20 "$work/dehacked.log" >&2; die "the engine never opened its window; see $work/dehacked.log"; }
        sleep 0.1
    done
    grep -q "^DEHACKED: doom1.wad: 2 changes" "$work/dehacked.log" \
        || die "the DEHACKED lump was not read; see $work/dehacked.log"
    grep -q "^sound: told the mixer where [0-9]* effects are" "$work/dehacked.log" \
        || die "the engine did not tell the mixer where its sounds are; see $work/dehacked.log"
    sleep 2
    python3 "$client" peak --audio "$audio_port" --secs 3 --quiet &
    listener=$!
    sleep 0.5
    DISPLAY="$disp" xdotool keydown Control_L
    sleep 1
    DISPLAY="$disp" xdotool keyup Control_L
    wait "$listener" \
        || die "the pistol was heard: the mixer did not take the mod's sound"
    kill "$game_pid" 2>/dev/null
    wait_game 10
    stop_mixer
    stop_x
    say "the patch was read, and the mod's pistol is the one the mixer plays"
else
    say "dehacked: skipped, xdotool is not installed"
fi

# -------------------------------------------------------------------- mbf21
# A patch's frames, sounds and code pointers past DOOM's own numbers, as
# MBF21 mods (Legacy of Rust among them) number theirs: the pistol's firing
# frame goes on to frame 5000, which plays sound 700 with A_WeaponSound
# (args: the sound). Sound 700 is named SMOKE in [SOUNDS], and DSSMOKE is a
# tone; the pistol's own sound is silence and A_FirePistol is taken off.
# Hearing the tone takes the frame, the pointer and its args, a sound
# numbered past 255, and the mixer's four-digit commands ('L', 'P').
if command -v xdotool >/dev/null 2>&1; then
    say "mbf21: the pistol plays sound 700 through a frame of its own"
    mkdir -p "$work/mbf21"
    printf '%s\n' 'Patch File for DeHackEd v3.0' 'Doom version = 2021' \
        'Patch format = 6' '' 'Frame 14' 'Next frame = 5000' '' \
        'Frame 5000' 'Sprite number = 3' 'Sprite subnumber = 1' \
        'Duration = 4' 'Next frame = 15' 'Args1 = 700' '' '[CODEPTR]' \
        'FRAME 14 = NULL' 'FRAME 5000 = WeaponSound' '' '[SOUNDS]' \
        '700 = SMOKE' >"$work/mbf21/dehacked"
    python3 -c "import struct, sys; sys.stdout.buffer.write(struct.pack('<HHI', 3, 11025, 4000) + bytes([128]) * 4000)" \
        >"$work/mbf21/dspistol"
    python3 -c "import math, struct, sys; n = 11025; sys.stdout.buffer.write(struct.pack('<HHI', 3, 11025, n) + bytes(int(128 + 100 * math.sin(2 * math.pi * 440 * i / 11025)) for i in range(n)))" \
        >"$work/mbf21/dssmoke"
    add_lumps "$wad" "$work/mbf21/doom1.wad" \
        DEHACKED "$work/mbf21/dehacked" DSPISTOL "$work/mbf21/dspistol" \
        DSSMOKE "$work/mbf21/dssmoke" \
        || die "could not add the patch and the sounds"
    start_x 24
    start_mixer
    printf 'music_volume 0\nsfx_volume 15\n' >"$work/.doomrc"
    DOOM_SFX_SOCKET="$work/sfx.sock" DOOM_MUSIC_PIPE="$work/music.pipe" \
    DOOM_AUDIO_RATE=22050 game_iwad="$work/mbf21/doom1.wad" \
        start_game mbf21.log -warp 1 1 -nojoy
    game_iwad=
    i=0
    until grep -q "I_InitGraphics" "$work/mbf21.log" 2>/dev/null; do
        i=$((i + 1))
        [ "$i" -gt 200 ] && { tail -20 "$work/mbf21.log" >&2; die "the engine never opened its window; see $work/mbf21.log"; }
        sleep 0.1
    done
    grep -q "^DEHACKED: doom1.wad: [0-9]* changes" "$work/mbf21.log" \
        && ! grep -q "^DEHACKED: .*line" "$work/mbf21.log" \
        || die "the patch was not read without complaint; see $work/mbf21.log"
    sleep 2
    python3 "$client" peak --audio "$audio_port" --secs 3 &
    listener=$!
    sleep 0.5
    DISPLAY="$disp" xdotool keydown Control_L
    sleep 1
    DISPLAY="$disp" xdotool keyup Control_L
    wait "$listener" \
        || die "sound 700 was not heard: the frame, the pointer or the mixer failed; see $work/mbf21.log and $work/audiostream.log"
    kill "$game_pid" 2>/dev/null
    wait_game 10
    stop_mixer
    stop_x
    say "frame 5000's A_WeaponSound played sound 700, the patch's own"
else
    say "mbf21: skipped, xdotool is not installed"
fi

# -------------------------------------------------------------------- id24
# An ID24 intermission, as Legacy of Rust's are: UMAPINFO gives E1M1 an
# exitanim, an INTERLEVEL lump of JSON whose background is a patch of solid
# blue and whose one layer, for the tally, has two squares -- green, shown
# when E1M1 has been played, and yellow, when E1M5 has. Every sector of E1M1
# is made special 11, the one that ends E1M8: on the easiest skill, which
# halves the damage, the player comes down to 10% and the level ends by
# itself, eight seconds in. The tally has to be blue, with the green square
# and without the yellow.
say "id24: E1M1's tally an INTERLEVEL lump's, with a layer of conditions"
mkdir -p "$work/id24"
python3 - "$wad" "$work/id24" <<'EOF' || die "could not make the intermission's lumps"
import json, struct, sys
src, out = sys.argv[1], sys.argv[2]
d = open(src, 'rb').read()
n, ofs = struct.unpack('<ii', d[4:12])
lumps = [struct.unpack('<ii8s', d[ofs + 16 * i:ofs + 16 * i + 16]) for i in range(n)]
lumps = [(nm.rstrip(b'\0').decode(), d[fp:fp + sz]) for fp, sz, nm in lumps]
i = [nm for nm, _ in lumps].index('E1M1')
for nm, data in lumps[i:i + 11]:
    if nm == 'SECTORS':
        secs = [list(r) for r in struct.iter_unpack('<hh8s8shhh', data)]
        for s in secs:
            s[5] = 11
        data = b''.join(struct.pack('<hh8s8shhh', *s) for s in secs)
    open('%s/%s' % (out, nm.lower() or 'e1m1'), 'wb').write(data)
def patch(w, h, colour):
    cols = b''.join(bytes([0, h, 0]) + bytes([colour]) * h + bytes([0, 255])
                    for x in range(w))
    head = struct.pack('<hhhh', w, h, 0, 0)
    size = h + 5
    return head + b''.join(struct.pack('<i', 8 + 4 * w + x * size)
                           for x in range(w)) + cols
open(out + '/smokebg', 'wb').write(patch(320, 200, 200))    # blue
open(out + '/smokegrn', 'wb').write(patch(30, 20, 112))     # green
open(out + '/smokeyel', 'wb').write(patch(30, 20, 231))    # yellow
def square(x, image, visited):
    return {'x': x, 'y': 130,
            'frames': [{'image': image, 'type': 1, 'duration': 0,
                        'maxduration': 0}],
            'conditions': [{'condition': 3, 'param': visited}]}
json.dump({'type': 'interlevel', 'version': '1.0.0', 'metadata': {},
           'data': {'music': 'D_INTER', 'backgroundimage': 'SMOKEBG',
                    'layers': [{'anims': [square(130, 'SMOKEGRN', 1),
                                          square(180, 'SMOKEYEL', 5)],
                                'conditions': [{'condition': 6,
                                                'param': 0}]}]}},
          open(out + '/smokeil', 'w'))
open(out + '/umapinfo', 'w').write('map E1M1\n{\n    exitanim = "SMOKEIL"\n}\n')
EOF
m="$work/id24"
add_lumps "$wad" "$m/doom1.wad" E1M1 "$m/e1m1" THINGS "$m/things" \
    LINEDEFS "$m/linedefs" SIDEDEFS "$m/sidedefs" VERTEXES "$m/vertexes" \
    SEGS "$m/segs" SSECTORS "$m/ssectors" NODES "$m/nodes" \
    SECTORS "$m/sectors" REJECT "$m/reject" BLOCKMAP "$m/blockmap" \
    UMAPINFO "$m/umapinfo" SMOKEIL "$m/smokeil" SMOKEBG "$m/smokebg" \
    SMOKEGRN "$m/smokegrn" SMOKEYEL "$m/smokeyel" \
    || die "could not add the map and the lumps"
start_x 24
rm -f "$work/.doomrc"
game_iwad="$work/id24/doom1.wad" start_game id24.log -warp 1 1 -skill 1 -nojoy
game_iwad=
python3 - "$work/fb24/Xvfb_screen0" <<'EOF' \
    || { tail -20 "$work/id24.log" >&2; die "the INTERLEVEL tally was not drawn as it should be; see $work/id24.log"; }
import struct, sys, time
def colours():
    d = open(sys.argv[1], 'rb').read()
    hsize, = struct.unpack('>I', d[:4])
    w, h = struct.unpack('>II', d[16:24])
    bpl, = struct.unpack('>I', d[48:52])
    ncolors, = struct.unpack('>I', d[76:80])
    px = d[hsize + ncolors * 12:]
    def at(x, y):
        o = y * bpl + 4 * x
        return px[o + 2], px[o + 1], px[o]
    blue = sum(1 for y in range(0, h, 4) for x in range(0, w, 4)
               if at(x, y)[2] > 200 and max(at(x, y)[:2]) < 60)
    def square(x0):
        # the square, at 320x200's (x0, 130), 30 by 20, on the doubled screen
        green = yellow = 0
        for y in range(2 * 130, 2 * 150):
            for x in range(2 * x0, 2 * (x0 + 30)):
                r, g, b = at(x, y)
                green += g > 200 and r < 160 and b < 160
                yellow += r > 200 and g > 200 and b < 60
        return green, yellow
    return blue * 16, w * h, square(130)[0], square(180)[1]
for i in range(300):
    time.sleep(0.1)
    try:
        blue, total, green, yellow = colours()
    except (OSError, struct.error):
        continue
    if blue > total // 2:
        break
time.sleep(1)
blue, total, green, yellow = colours()
print('[smoke] tally: %d of %d pixels blue, green square %d, yellow %d'
      % (blue, total, green, yellow))
sys.exit(0 if blue > total // 2 and green > 1500 and yellow < 100 else 1)
EOF
cp "$work/fb24/Xvfb_screen0" "$work/id24.xwd"
kill -0 "$game_pid" 2>/dev/null \
    || die "the engine did not stay up through the intermission; see $work/id24.log"
say "E1M1 ended by itself, and its tally was the INTERLEVEL's, conditions kept"
kill "$game_pid" 2>/dev/null
wait_game 10
stop_x

# ---------------------------------------------------------------------- ogg
if [ -z "$soundfont" ] || ! command -v oggenc >/dev/null 2>&1; then
    say "ogg: skipped, needs a soundfont and oggenc (vorbis-tools)"
else
    say "ogg: E1M1's music an Ogg Vorbis lump, as SIGIL II's are"
    mkdir -p "$work/ogg"
    python3 - "$work/ogg/tone.wav" <<'EOF' || die "could not write the tone"
import math, struct, sys, wave
w = wave.open(sys.argv[1], 'wb')
w.setnchannels(2)
w.setsampwidth(2)
w.setframerate(44100)
w.writeframes(b''.join(struct.pack('<hh', v, v) for v in
    (int(12000 * math.sin(2 * math.pi * 440 * i / 44100))
     for i in range(44100 * 3))))
w.close()
EOF
    oggenc -Q -q 2 -o "$work/ogg/tone.ogg" "$work/ogg/tone.wav" \
        || die "oggenc could not encode the tone"
    printf 'MAP E1M1 { music = "D_TONE" }\n' >"$work/ogg/umapinfo.txt"
    add_lumps "$wad" "$work/ogg/doom1.wad" \
        UMAPINFO "$work/ogg/umapinfo.txt" D_TONE "$work/ogg/tone.ogg" \
        || die "could not add the Ogg Vorbis lump"
    start_x 24
    start_mixer
    printf 'music_volume 15\nsfx_volume 0\n' >"$work/.doomrc"
    DOOM_SFX_SOCKET="$work/sfx.sock" DOOM_MUSIC_PIPE="$work/music.pipe" \
    DOOM_AUDIO_RATE=22050 DOOM_SOUNDFONT="$soundfont" \
    game_iwad="$work/ogg/doom1.wad" start_game ogg.log -warp 1 1 -nojoy
    game_iwad=
    python3 "$client" ogg --audio "$audio_port" \
        || { tail -20 "$work/ogg.log" >&2; die "the ogg phase; see $work/ogg.log"; }
    grep -q "I_RegisterSong" "$work/ogg.log" \
        && die "the engine complained about the Ogg Vorbis track; see $work/ogg.log"
    kill "$game_pid" 2>/dev/null
    wait_game 10
    stop_mixer
    stop_x
fi

# ------------------------------------------------------------------ recover
# What Load WAD leaves behind when it restarts the engine on a new file is
# the arguments it was running on, in DOOM_PREVIOUS_ARGS; given here by
# hand. The WAD is the shareware one with a second E1M1 whose first wall
# wants a texture there is none of, so it fails at the level, once the
# controller's port is open: the engine it goes back to has to be able to
# open it again.
say "recover: a WAD the engine cannot start on, as if from Load WAD"
mkdir -p "$work/recover"
python3 - "$wad" "$work/recover" <<'EOF' || die "could not write the broken map"
import struct, sys
data = open(sys.argv[1], 'rb').read()
count, diroff = struct.unpack('<ii', data[4:12])
lumps = [struct.unpack('<ii8s', data[diroff + 16*i:diroff + 16*i + 16])
         for i in range(count)]
names = [n.rstrip(b'\0').decode() for p, s, n in lumps]
i = names.index('E1M1')
for k in range(1, 11):
    pos, size, name = lumps[i + k]
    lump = bytearray(data[pos:pos + size])
    if names[i + k] == 'SIDEDEFS':
        lump[12:20] = b'NOSUCHTX'       # the first sidedef's middle texture
    open('%s/%s' % (sys.argv[2], names[i + k]), 'wb').write(lump)
open(sys.argv[2] + '/E1M1', 'wb').write(b'')
EOF
set -- E1M1 "$work/recover/E1M1"
for l in THINGS LINEDEFS SIDEDEFS VERTEXES SEGS SSECTORS NODES SECTORS REJECT BLOCKMAP; do
    set -- "$@" "$l" "$work/recover/$l"
done
add_lumps "$wad" "$work/recover/doom1.wad" "$@" \
    || die "could not make the broken WAD"
set --
start_x 24
rm -f "$work/.doomrc"
DOOM_PREVIOUS_ARGS=$(printf '%s\037%s\037%s\037%s' \
    "$engine" -2 -iwad "$work/wads/doom1.wad") \
DOOM_LOADING=broken.wad DOOM_PAD_PORT="$pad_port" \
game_iwad="$work/recover/doom1.wad" start_game recover.log -warp 1 1
game_iwad=
i=0
# It fails loading the level, before its window opens, so the one window
# there is belongs to the game it went back to.
until grep -q "Going back" "$work/recover.log" 2>/dev/null \
      && grep -q "I_InitGraphics" "$work/recover.log"; do
    i=$((i + 1))
    [ "$i" -gt 200 ] && { tail -20 "$work/recover.log" >&2;
        die "the engine did not go back to the game before; see $work/recover.log"; }
    sleep 0.1
done
sleep 2
kill -0 "$game_pid" 2>/dev/null \
    || die "the engine did not stay up after going back; see $work/recover.log"
grep -q "Error: R_TextureNumForName: NOSUCHTX" "$work/recover.log" \
    || die "the broken WAD failed some other way; see $work/recover.log"
[ "$(grep -c "^Controller: from the browser, port $pad_port" "$work/recover.log")" = 2 ] \
    || die "after going back the controller port was not listened on again; see $work/recover.log"
kill "$game_pid" 2>/dev/null
wait_game 10
stop_x
say "it went back to the game before, and listens for the controller again"

# ------------------------------------------------------------------ loadwad
# Load WAD restarts the engine with exec. Everything it had open went with
# it into the new one, the controller's listening socket included, so the
# new engine could not listen on the same port: "cannot listen on port ...
# (Address already in use)", and no controller from the browser until the
# container was restarted. The connection to the mixer went too, open and
# unused, and the mixer went on listening to it instead of the new engine:
# no sound effects after a Load WAD, and once the new engine had filled its
# own connection's buffer, a game frozen writing to it (up to 1.14.0). The
# menu is walked with keys, as a player would; after the restart, working
# the menu has to be heard.
if command -v xdotool >/dev/null 2>&1; then
    say "loadwad: Load WAD restarts the engine, controller, sound and all"
    start_x 24
    start_mixer
    printf 'music_volume 0\nsfx_volume 15\n' >"$work/.doomrc"
    DOOM_SFX_SOCKET="$work/sfx.sock" DOOM_MUSIC_PIPE="$work/music.pipe" \
    DOOM_AUDIO_RATE=22050 DOOM_PAD_PORT="$pad_port" start_game loadwad.log
    i=0
    until grep -q "I_InitGraphics" "$work/loadwad.log" 2>/dev/null; do
        i=$((i + 1))
        [ "$i" -gt 200 ] && die "the engine never opened its window; see $work/loadwad.log"
        sleep 0.1
    done
    sleep 2
    # Escape, Options, Setup, Load WAD, and the first WAD there
    for k in Escape Down Return t Return w Return Return; do
        DISPLAY="$disp" xdotool key "$k"
        sleep 0.5
    done
    i=0
    until [ "$(grep -c "I_InitGraphics" "$work/loadwad.log")" -ge 2 ]; do
        i=$((i + 1))
        [ "$i" -gt 200 ] && { tail -20 "$work/loadwad.log" >&2;
            die "Load WAD did not restart the engine; see $work/loadwad.log"; }
        sleep 0.1
    done
    grep -q "cannot listen" "$work/loadwad.log" \
        && die "after Load WAD the controller's port was still held; see $work/loadwad.log"
    [ "$(grep -c "^Controller: from the browser, port $pad_port" "$work/loadwad.log")" = 2 ] \
        || die "after Load WAD nothing listened for the controller; see $work/loadwad.log"
    sleep 2
    python3 "$client" peak --audio "$audio_port" --secs 4 &
    listener=$!
    sleep 1
    for k in Escape Down Down Up Escape Escape Down Escape; do
        DISPLAY="$disp" xdotool key "$k"
        sleep 0.3
    done
    wait "$listener" \
        || die "after Load WAD the menu's sounds were not heard; see $work/loadwad.log"
    kill "$game_pid" 2>/dev/null
    wait_game 10
    stop_mixer
    stop_x
    say "the engine restarted, listens for the controller again, and is heard"
else
    say "loadwad: skipped, xdotool is not installed"
fi

# ------------------------------------------------------------------ newgame
# New Game asks what to play before how hard: the WAD folder's files, in
# alphabetical order. Choosing one that is not running restarts the engine
# on it, and the new engine opens straight onto the difficulty. A second
# copy of the shareware WAD, a different file, is the other game here.
if command -v xdotool >/dev/null 2>&1; then
    say "newgame: New Game, another WAD, and straight on to the new game"
    cp "$wad" "$work/wads/doom1b.wad"
    start_x 24
    rm -f "$work/.doomrc"
    start_game newgame.log
    i=0
    until grep -q "I_InitGraphics" "$work/newgame.log" 2>/dev/null; do
        i=$((i + 1))
        [ "$i" -gt 200 ] && die "the engine never opened its window; see $work/newgame.log"
        sleep 0.1
    done
    sleep 2
    # the menu, New Game, then the second file in the list
    for k in Escape Return Down Return; do
        DISPLAY="$disp" xdotool key "$k"
        sleep 0.5
    done
    i=0
    # the shareware episode menu lists all three -- id's advert for the
    # rest -- so it is the episodes here, the difficulty for a mod
    until grep -q "^New game: on to the \(episodes\|difficulty\)" "$work/newgame.log"; do
        i=$((i + 1))
        [ "$i" -gt 200 ] && { tail -20 "$work/newgame.log" >&2;
            die "New Game did not restart on to the episodes or difficulty; see $work/newgame.log"; }
        sleep 0.1
    done
    grep -q "^ adding .*/doom1b.wad" "$work/newgame.log" \
        || die "New Game restarted, but not on the WAD chosen; see $work/newgame.log"
    kill "$game_pid" 2>/dev/null
    wait_game 10
    stop_x
    rm -f "$work/wads/doom1b.wad"
    say "the list, the restart on the file chosen, and on with the new game"
else
    say "newgame: skipped, xdotool is not installed"
fi

# ----------------------------------------------------------------- savegame
# A savegame is its level by number, and did not say which WADs it was made
# on: saved in SIGIL II and loaded with DOOM alone, it named sectors past the
# end of the level's and the engine died in P_UnArchiveSpecials (up to
# 1.17.1). Saves now list their WADs, and loading one restarts the engine on
# them; one that does not list them is checked against its level first.
wait_window() {  # log
    i=0
    until grep -q "I_InitGraphics" "$work/$1" 2>/dev/null; do
        i=$((i + 1))
        [ "$i" -gt 200 ] && die "the engine never opened its window; see $work/$1"
        sleep 0.1
    done
    sleep 2
}

if command -v xdotool >/dev/null 2>&1; then
    say "savegame: a save loads on its own WADs, and a stranger is refused"
    cp "$wad" "$work/wads/doom1b.wad"
    rm -f "$work"/doomsav*.dsg
    game_dir=$work
    start_x 24

    # on the second file: Save Game, the first slot, a name, and saved
    game_iwad="$work/wads/doom1b.wad" start_game savegame1.log -warp 1 1 -nojoy
    wait_window savegame1.log
    for k in F2 Return s Return; do
        DISPLAY="$disp" xdotool key "$k"
        sleep 0.5
    done
    i=0
    until grep -q "^G_DoSaveGame: doomsav0.dsg" "$work/savegame1.log"; do
        i=$((i + 1))
        [ "$i" -gt 100 ] && die "the game was not saved; see $work/savegame1.log"
        sleep 0.1
    done
    kill "$game_pid" 2>/dev/null
    wait_game 10
    [ "$(tail -c 4 "$work/doomsav0.dsg")" = WADS ] \
        || die "the save does not list its WADs"

    # on the first: Load Game and the first slot
    start_game savegame2.log -nojoy
    wait_window savegame2.log
    for k in F3 Return; do
        DISPLAY="$disp" xdotool key "$k"
        sleep 0.5
    done
    i=0
    until grep -q "^G_DoLoadGame: doomsav0.dsg, E1M1" "$work/savegame2.log"; do
        i=$((i + 1))
        [ "$i" -gt 200 ] && { tail -20 "$work/savegame2.log" >&2;
            die "the save was not loaded on its own WAD; see $work/savegame2.log"; }
        sleep 0.1
    done
    grep -q "^ adding .*/doom1b.wad" "$work/savegame2.log" \
        || die "the save was loaded, but not on its own WAD; see $work/savegame2.log"
    kill "$game_pid" 2>/dev/null
    wait_game 10
    say "saved on one WAD, loaded from another: restarted on the first, and loaded"

    # The same save as one from before the list, and naming E1M2: E1M1's
    # sectors and things do not fit E1M2's, and it has to be refused.
    python3 - "$work/doomsav0.dsg" "$work/doomsav1.dsg" <<'EOF'
import struct, sys
d = open(sys.argv[1], 'rb').read()
n = struct.unpack('<i', d[-8:-4])[0]
d = bytearray(d[:-8 - n])
d[24 + 16 + 2] = 2      # the map, after description, version, skill, episode
open(sys.argv[2], 'wb').write(d)
EOF
    start_game savegame3.log -nojoy -loadgame 1
    i=0
    until grep -q "^G_DoLoadGame: doomsav1.dsg is not a game on this E1M2" \
            "$work/savegame3.log" 2>/dev/null; do
        i=$((i + 1))
        [ "$i" -gt 200 ] && { tail -20 "$work/savegame3.log" >&2;
            die "a save that does not fit its map was not refused; see $work/savegame3.log"; }
        sleep 0.1
    done
    sleep 2
    grep -q "DOOM died" "$work/savegame3.log" \
        && die "refusing a save, the engine died; see $work/savegame3.log"
    kill -0 "$game_pid" 2>/dev/null \
        || die "refusing a save, the engine went away; see $work/savegame3.log"
    kill "$game_pid" 2>/dev/null
    wait_game 10

    # A save from before saves listed their WADs, made on a game whose E1M1
    # is not the running game's -- E1M2's lumps under E1M1's name, last in
    # the file -- is looked for in the WAD folder by its map, found, and
    # loaded on it.
    python3 - "$wad" "$work/wads/doom1c.wad" <<'EOF'
import struct, sys
d = open(sys.argv[1], 'rb').read()
n, ofs = struct.unpack('<ii', d[4:12])
lumps = [struct.unpack('<ii8s', d[ofs + 16*i:ofs + 16*i + 16]) for i in range(n)]
names = [l[2].rstrip(b'\0') for l in lumps]
m = names.index(b'E1M2')
# the old lumps, then E1M1 again, from E1M2's
out, dirs = b'', []
for fp, sz, nm in lumps:
    dirs.append((12 + len(out), sz, nm))
    out += d[fp:fp + sz]
for i in range(m, m + 11):
    fp, sz, nm = lumps[i]
    dirs.append((12 + len(out), sz, b'E1M1' if i == m else nm))
    out += d[fp:fp + sz]
open(sys.argv[2], 'wb').write(b'IWAD' + struct.pack('<ii', len(dirs), 12 + len(out))
    + out + b''.join(struct.pack('<ii8s', *x) for x in dirs))
EOF
    game_iwad="$work/wads/doom1c.wad" start_game savegame4.log -warp 1 1 -nojoy
    wait_window savegame4.log
    for k in F2 Down Down Return c Return; do
        DISPLAY="$disp" xdotool key "$k"
        sleep 0.5
    done
    i=0
    until grep -q "^G_DoSaveGame: doomsav2.dsg" "$work/savegame4.log"; do
        i=$((i + 1))
        [ "$i" -gt 100 ] && die "the game was not saved; see $work/savegame4.log"
        sleep 0.1
    done
    kill "$game_pid" 2>/dev/null
    wait_game 10
    python3 - "$work/doomsav2.dsg" <<'EOF'
import struct, sys
d = open(sys.argv[1], 'rb').read()
n = struct.unpack('<i', d[-8:-4])[0]
open(sys.argv[1], 'wb').write(d[:-8 - n])     # as saves were before
EOF
    start_game savegame5.log -nojoy -loadgame 2
    i=0
    until grep -q "^G_DoLoadGame: doomsav2.dsg, E1M1" "$work/savegame5.log"; do
        i=$((i + 1))
        [ "$i" -gt 200 ] && { tail -20 "$work/savegame5.log" >&2;
            die "a save from before was not loaded on the game it fits; see $work/savegame5.log"; }
        sleep 0.1
    done
    grep -q "^G_DoLoadGame: doomsav2.dsg was not saved on these, but fits .*/doom1c.wad" \
            "$work/savegame5.log" \
        && grep -q "^ adding .*/doom1c.wad" "$work/savegame5.log" \
        || die "a save from before was loaded, but not on the game it fits; see $work/savegame5.log"
    kill "$game_pid" 2>/dev/null
    wait_game 10
    say "a save from before, found its game in the WAD folder, and loaded on it"

    stop_x
    game_dir=
    rm -f "$work/wads/doom1b.wad" "$work/wads/doom1c.wad" "$work"/doomsav*.dsg
    say "a save that does not fit its map is refused, and the engine goes on"
else
    say "savegame: skipped, xdotool is not installed"
fi

# ------------------------------------------------------------ pointer, crash
start_x 24
start_game crash.log -warp 1 1 -nojoy -grabmouse
i=0
until grep -q "I_InitGraphics" "$work/crash.log" 2>/dev/null; do
    i=$((i + 1))
    [ "$i" -gt 200 ] && die "the engine never opened its window; see $work/crash.log"
    sleep 0.1
done
sleep 2

# Where X says the pointer is, after putting it in the corner.
pointer_after_corner() {
    DISPLAY="$disp" xdotool mousemove 10 10
    sleep 0.4
    DISPLAY="$disp" xdotool getmouselocation | sed 's/ screen.*//'
}

if command -v xdotool >/dev/null 2>&1; then
    say "pointer: held while playing, let go in the menu"
    at=$(pointer_after_corner)
    [ "$at" = "x:320 y:200" ] \
        || die "in a level with capture on, the pointer should be held at the centre; it is at $at"
    DISPLAY="$disp" xdotool key Escape
    sleep 0.5
    at=$(pointer_after_corner)
    [ "$at" = "x:10 y:10" ] \
        || die "with the menu up the pointer should be free; it was put back to $at"
    DISPLAY="$disp" xdotool key Escape
    sleep 0.5
    at=$(pointer_after_corner)
    [ "$at" = "x:320 y:200" ] \
        || die "back in the level the pointer should be held again; it is at $at"
    say "the pointer is the game's in a level, and free in the menu"
else
    say "pointer: skipped, xdotool is not installed"
fi

say "crash: what a segmentation fault leaves in the log"
kill -SEGV "$game_pid"
wait_game 10
[ "$st" = 139 ] || die "a segfault should end with 139; got $st"
grep -q "DOOM died on SIGSEGV" "$work/crash.log" \
    || die "no 'DOOM died on SIGSEGV' in the log; see $work/crash.log"
grep -q "D_DoomLoop\|D_DoomMain" "$work/crash.log" \
    || die "the backtrace has no function names in it; see $work/crash.log"
say "a segfault says so, with a backtrace that has names in it"
stop_x

say "all passed"
