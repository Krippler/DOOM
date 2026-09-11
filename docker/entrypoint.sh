#!/bin/sh
#
# Bring up a private 8-bit X server, export it over VNC and noVNC, and run
# DOOM on it. Any arguments given to the container are passed straight to the
# engine, e.g. `docker run ... doom -warp 1 1 -skill 4`.
#
set -eu

SCALE="${DOOM_SCALE:-2}"
WADDIR="${DOOM_WADDIR:-/wads}"
STATE="${DOOM_STATE:-/doom/state}"
VNC_PORT="${DOOM_VNC_PORT:-5900}"
WEB_PORT="${DOOM_WEB_PORT:-6080}"
DISP="${DOOM_DISPLAY:-:99}"
DOOM_BIN="${DOOM_BIN:-/usr/local/games/linuxxdoom}"
NOVNC_ROOT="${DOOM_NOVNC_ROOT:-/usr/share/novnc}"

log() { printf '[doom] %s\n' "$*" >&2; }
die() { printf '[doom] error: %s\n' "$*" >&2; exit 1; }

case "$SCALE" in
    1|2|3|4) ;;
    *) die "DOOM_SCALE must be 1, 2, 3 or 4 (got '$SCALE')" ;;
esac

WIDTH=$((320 * SCALE))
HEIGHT=$((200 * SCALE))

##############################################################################
# Locate an IWAD.
#
# The engine looks for these exact lowercase names in $DOOMWADDIR and nowhere
# else, so build a directory of symlinks pointing at whatever the user
# actually mounted, whatever case they used.
##############################################################################
IWADS="doom2f.wad doom2.wad plutonia.wad tnt.wad doomu.wad doom.wad doom1.wad"
LINKDIR="$STATE/.iwads"

[ -d "$WADDIR" ] || die "WAD directory '$WADDIR' does not exist. Mount one with -v /path/to/wads:$WADDIR:ro"

rm -rf "$LINKDIR"
mkdir -p "$LINKDIR"

found=""
for want in $IWADS; do
    match=$(find "$WADDIR" -maxdepth 1 -type f -iname "$want" 2>/dev/null | head -n 1)
    if [ -n "$match" ]; then
        ln -sf "$match" "$LINKDIR/$want"
        found="$found $want"
        log "found IWAD $(basename "$match") -> $want"
    fi
done

if [ -z "$found" ]; then
    log "no IWAD found in $WADDIR"
    log "the engine recognises these names (any capitalisation):"
    for want in $IWADS; do log "    $want"; done
    log "mount a directory containing one, e.g.:"
    log "    docker run --rm -p $WEB_PORT:$WEB_PORT -v \"\$PWD/wads:$WADDIR:ro\" doom"
    die "no game data to run"
fi

export DOOMWADDIR="$LINKDIR"

##############################################################################
# Background services. Everything is torn down together.
##############################################################################
XVFB_PID=""
VNC_PID=""
WEB_PID=""
DOOM_PID=""

cleanup() {
    trap - EXIT INT TERM

    # The engine installs a SIGINT handler that saves the config file and
    # exits, so ask it to stop that way before pulling the display away.
    if [ -n "$DOOM_PID" ] && kill -0 "$DOOM_PID" 2>/dev/null; then
        kill -INT "$DOOM_PID" 2>/dev/null || true
        i=0
        while kill -0 "$DOOM_PID" 2>/dev/null && [ "$i" -lt 30 ]; do
            i=$((i + 1))
            sleep 0.1
        done
        kill -TERM "$DOOM_PID" 2>/dev/null || true
    fi

    for pid in "$WEB_PID" "$VNC_PID" "$XVFB_PID"; do
        if [ -n "$pid" ]; then
            kill "$pid" 2>/dev/null || true
        fi
    done
    wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# DOOM only supports an 8-bit PseudoColor visual, which is exactly what Xvfb
# gives us here and what a modern desktop X server no longer offers.
log "starting Xvfb on $DISP at ${WIDTH}x${HEIGHT}x8"
Xvfb "$DISP" -screen 0 "${WIDTH}x${HEIGHT}x8" -nolisten tcp >"$STATE/xvfb.log" 2>&1 &
XVFB_PID=$!
export DISPLAY="$DISP"

sock="/tmp/.X11-unix/X${DISP#:}"
i=0
while [ ! -e "$sock" ]; do
    i=$((i + 1))
    [ "$i" -gt 100 ] && die "Xvfb failed to start; see $STATE/xvfb.log"
    kill -0 "$XVFB_PID" 2>/dev/null || die "Xvfb exited; see $STATE/xvfb.log"
    sleep 0.1
done

# -8to24 converts the colormapped display to true colour for the VNC client;
# without it the palette comes out wrong in most viewers.
vnc_auth=""
if [ -n "${DOOM_VNC_PASSWORD:-}" ]; then
    x11vnc -storepasswd "$DOOM_VNC_PASSWORD" "$STATE/.vncpasswd" >/dev/null 2>&1
    vnc_auth="-rfbauth $STATE/.vncpasswd"
    log "VNC password authentication enabled"
else
    vnc_auth="-nopw"
    log "no VNC password set (export DOOM_VNC_PASSWORD to require one)"
fi

log "starting x11vnc on port $VNC_PORT"
# shellcheck disable=SC2086
x11vnc -display "$DISP" -rfbport "$VNC_PORT" -forever -shared -8to24 -quiet \
       $vnc_auth >"$STATE/x11vnc.log" 2>&1 &
VNC_PID=$!

log "starting noVNC on port $WEB_PORT"
websockify --web="$NOVNC_ROOT" "$WEB_PORT" "localhost:$VNC_PORT" \
       >"$STATE/websockify.log" 2>&1 &
WEB_PID=$!

log ""
log "  play at  http://localhost:$WEB_PORT/vnc.html?autoconnect=1&resize=scale"
log ""

##############################################################################
# Run the game. Scale flag is added unless the caller picked one.
##############################################################################
case " $* " in
    *" -2 "*|*" -3 "*|*" -4 "*)
        ;;
    *)
        if [ "$SCALE" -gt 1 ]; then
            set -- "-$SCALE" "$@"
        fi
        ;;
esac

cd "$STATE"
log "running: linuxxdoom $*"

# Run in the background and wait: a foreground child would block every trap
# until it exited, so `docker stop` could not shut the stack down.
"$DOOM_BIN" "$@" &
DOOM_PID=$!

status=0
wait "$DOOM_PID" || status=$?
DOOM_PID=""

log "DOOM exited with status $status"
exit "$status"
