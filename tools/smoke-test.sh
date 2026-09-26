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
        "$engine" -2 -iwad "$work/wads/doom1.wad" "$@" >"$work/$log" 2>&1 &
    game_pid=$!
}

# Wait up to $1 seconds for the engine to exit, and leave its status in $st,
# or "timeout". A watchdog and a real wait, not a loop of kill -0: an engine
# that has exited stays a zombie until it is waited for, and kill -0 goes on
# succeeding on a zombie -- which is how the first version of this reported a
# two-second shutdown as a ten-second hang.
wait_game() {
    ( sleep "$1"; kill -KILL "$game_pid" 2>/dev/null; touch "$work/.timeout" ) &
    dog=$!
    rm -f "$work/.timeout"
    wait "$game_pid"
    st=$?
    kill "$dog" 2>/dev/null
    wait "$dog" 2>/dev/null
    [ -e "$work/.timeout" ] && st=timeout
    game_pid=
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

# -------------------------------------------------------------------- crash
say "crash: what a segmentation fault leaves in the log"
start_x 24
start_game crash.log -warp 1 1 -nojoy
i=0
until grep -q "I_InitGraphics" "$work/crash.log" 2>/dev/null; do
    i=$((i + 1))
    [ "$i" -gt 200 ] && die "the engine never opened its window; see $work/crash.log"
    sleep 0.1
done
sleep 1
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
