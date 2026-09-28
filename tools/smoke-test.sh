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
#   umapinfo  a level starts with a UMAPINFO lump naming it, its sky and its
#             music, and the lump is read without complaint (the reader
#             itself is tools/umapinfo-test.c's)
#   ogg       a level whose music is an Ogg Vorbis lump, as SIGIL II's are,
#             is heard at the tone's own pitch (needs oggenc, from
#             vorbis-tools, and a soundfont; skipped without)
#   ogg       a level whose music is an Ogg Vorbis lump, as SIGIL II's are,
#             is heard at the tone's own pitch (needs oggenc, from
#             vorbis-tools, and a soundfont; skipped without)
#   recover   a WAD the engine cannot start on, as if chosen from Load WAD:
#             it goes back to the game before, still listening for the
#             controller, and says why
#   loadwad   Load WAD, walked through with keys, restarts the engine, and
#             the controller's port is listened on again (needs xdotool)
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
start_game() {  # log, then engine arguments
    log=$1
    shift
    DISPLAY="$disp" HOME="$work" DOOMWADDIR="$work/wads" \
        "$engine" -2 -iwad "${game_iwad:-$work/wads/doom1.wad}" "$@" \
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
# container was restarted. The menu is walked with keys, as a player would.
if command -v xdotool >/dev/null 2>&1; then
    say "loadwad: Load WAD restarts the engine, controller and all"
    start_x 24
    rm -f "$work/.doomrc"
    DOOM_PAD_PORT="$pad_port" start_game loadwad.log
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
    kill "$game_pid" 2>/dev/null
    wait_game 10
    stop_x
    say "the engine restarted, and listens for the controller again"
else
    say "loadwad: skipped, xdotool is not installed"
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
