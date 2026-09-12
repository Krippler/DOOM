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
SNDSERVER_BIN="${DOOM_SNDSERVER_BIN:-/usr/local/games/sndserver}"
NOVNC_ROOT="${DOOM_NOVNC_ROOT:-/usr/share/novnc}"

log() { printf '[doom] %s\n' "$*" >&2; }
die() { printf '[doom] error: %s\n' "$*" >&2; exit 1; }

##############################################################################
# Drop privileges.
#
# Started as root -- which is how Unraid and most NAS front ends run a
# container -- take ownership of the writable directory as PUID:PGID and
# then run everything else as that user. On Unraid those are 99:100
# (nobody:users), which is what its appdata share is owned by.
#
# Started as an ordinary user already, via --user or the image default,
# there is nothing to do and PUID/PGID are ignored.
##############################################################################
if [ "$(id -u)" = "0" ]; then
    PUID="${PUID:-1001}"
    PGID="${PGID:-1001}"

    case "$PUID$PGID" in
        *[!0-9]*) die "PUID and PGID must be numeric (got '$PUID' and '$PGID')" ;;
    esac

    mkdir -p "$STATE" 2>/dev/null || true
    chown "$PUID:$PGID" "$STATE" 2>/dev/null || true

    if [ "$PUID" = "0" ] && [ "$PGID" = "0" ]; then
        # Asking to stay root. Dropping to root is not a drop, and re-executing
        # would arrive back here as root and do it again, for ever.
        log "running as root (PUID=0)"
    else
        log "running as ${PUID}:${PGID}"
        exec setpriv --reuid "$PUID" --regid "$PGID" --clear-groups "$0" "$@"
    fi
fi

case "$SCALE" in
    1|2|3|4) ;;
    *) die "DOOM_SCALE must be 1, 2, 3 or 4 (got '$SCALE')" ;;
esac

WIDTH=$((320 * SCALE))
HEIGHT=$((200 * SCALE))

##############################################################################
# The state directory holds the config file, savegames, logs and the IWAD
# links, so it has to be writable. A bind-mounted host directory arrives
# owned by whoever created it, which is usually not the container's user.
##############################################################################
if ! mkdir -p "$STATE" 2>/dev/null || [ ! -w "$STATE" ]; then
    log "state directory '$STATE' is not writable by uid $(id -u)"
    log ""
    log "that happens when a host directory is bind-mounted there. Either run"
    log "the container as yourself:"
    log "    docker run --user \"\$(id -u):\$(id -g)\" ..."
    log "or hand the directory to the container's user:"
    log "    chown $(id -u):$(id -g) <that directory>"
    log ""
    log "a named volume, which is what docker-compose.yml uses, needs neither."
    die "cannot write to $STATE"
fi

##############################################################################
# Locate an IWAD.
#
# The engine looks for these exact lowercase names in $DOOMWADDIR and nowhere
# else, so build a directory of symlinks pointing at whatever the user
# actually mounted, whatever case they used.
##############################################################################
IWADS="doom2f.wad doom2.wad plutonia.wad tnt.wad doomu.wad doom.wad doom1.wad"
LINKDIR="$STATE/.iwads"

# Which one to start when several are mounted. The engine's own search order
# picks Doom II first, so a directory holding both games always started the
# sequel; Doom comes first here instead. Set DOOM_IWAD to a filename or a path
# to choose directly.
IWAD_PREFERENCE="doomu.wad doom.wad doom2.wad doom2f.wad tnt.wad plutonia.wad doom1.wad"

# The bundled shareware IWAD, used only when nothing was mounted.
BUNDLED_WAD="${DOOM_BUNDLED_WAD:-/usr/share/doom/doom1.wad}"
BUNDLED_MD5="f0cefca49926d00903cf57551d901abe"

# Not fatal any more: without a mount there is still the shareware WAD.
[ -d "$WADDIR" ] || log "no WAD directory at $WADDIR"

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

if [ -z "$found" ] && [ -r "$BUNDLED_WAD" ]; then
    # Check the bundled file really is the shareware IWAD before trusting it.
    # A truncated or substituted copy would otherwise fail much later, inside
    # the engine, with something far less obvious than this.
    actual_md5=$(md5sum "$BUNDLED_WAD" 2>/dev/null | cut -d' ' -f1)

    if [ "$actual_md5" != "$BUNDLED_MD5" ]; then
        log "bundled shareware WAD is not the file it should be"
        log "    expected md5 $BUNDLED_MD5"
        log "    got          ${actual_md5:-unreadable}"
    else
        ln -sf "$BUNDLED_WAD" "$LINKDIR/doom1.wad"
        found=" doom1.wad"
        log "no IWAD mounted; using the bundled shareware DOOM1.WAD (episode 1)"
        log "mount your own at $WADDIR to play the full game:"
        log "    docker run --rm -p $WEB_PORT:$WEB_PORT -v \"\$PWD/wads:$WADDIR:ro\" doom"
    fi
fi

if [ -z "$found" ]; then
    log "no IWAD found in $WADDIR and no usable bundled copy"
    log "the engine recognises these names (any capitalisation):"
    for want in $IWADS; do log "    $want"; done
    log "mount a directory containing one, e.g.:"
    log "    docker run --rm -p $WEB_PORT:$WEB_PORT -v \"\$PWD/wads:$WADDIR:ro\" doom"
    die "no game data to run"
fi

export DOOMWADDIR="$LINKDIR"

##############################################################################
# Choose which IWAD to start, and say so rather than leaving it to the
# engine's built-in search order.
##############################################################################
CHOSEN=""

if [ -n "${DOOM_IWAD:-}" ]; then
    # A path, or a bare filename to look for in the mounted directory.
    for cand in "$DOOM_IWAD" "$WADDIR/$DOOM_IWAD" "$LINKDIR/$DOOM_IWAD"; do
        [ -r "$cand" ] && { CHOSEN="$cand"; break; }
    done
    [ -n "$CHOSEN" ] || die "DOOM_IWAD='$DOOM_IWAD' not found or not readable"
    log "DOOM_IWAD set: starting $(basename "$CHOSEN")"
else
    for want in $IWAD_PREFERENCE; do
        if [ -e "$LINKDIR/$want" ]; then
            CHOSEN="$LINKDIR/$want"
            break
        fi
    done
fi

# By now this script has re-executed itself as PUID:PGID, so this is the
# access the engine will actually have. Finding a file needs only the
# directory; reading it needs the file itself, and a WAD that is readable by
# its owner alone fails here with nothing to explain why.
if [ -n "$CHOSEN" ] && [ ! -r "$CHOSEN" ]; then
    log "found $(basename "$CHOSEN") but cannot read it as $(id -u):$(id -g)"
    log "make it readable, e.g.  chmod a+r <your wad>"
    log "or run with -e PUID=<owner uid> -e PGID=<owner gid>"
    die "IWAD is not readable"
fi

# Only worth a line when there was actually a choice to make.
if [ -z "${DOOM_IWAD:-}" ] && [ "$(echo $found | wc -w)" -gt 1 ]; then
    log "several IWADs present ($(echo $found)); starting $(basename "$CHOSEN")"
    log "set DOOM_IWAD to start a different one"
fi

# The in-game WAD menu scans this instead, so it lists what the user actually
# mounted rather than the lowercase symlinks above. With nothing mounted there
# is no such directory, so point it at the links, where the bundled shareware
# WAD is the one entry.
if [ -d "$WADDIR" ]; then
    export DOOM_WADPATH="$WADDIR"
else
    export DOOM_WADPATH="$LINKDIR"
fi

##############################################################################
# Sound.
#
# The engine spawns a separate sound server process and looks for it at
# $DOOMWADDIR/sndserver, so it goes in the same directory as the IWAD links.
# The server plays through PulseAudio; if it cannot reach a server it says so
# and the game runs silent.
##############################################################################
if [ "${DOOM_SOUND:-1}" = "1" ] && [ -x "$SNDSERVER_BIN" ]; then
    ln -sf "$SNDSERVER_BIN" "$LINKDIR/sndserver"

    if [ -n "${PULSE_SERVER:-}" ]; then
        log "sound: PULSE_SERVER=$PULSE_SERVER"
    elif [ -S "${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/pulse/native" ]; then
        log "sound: using ${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/pulse/native"
    else
        log "sound: no PulseAudio server configured, the game will be silent"
        log "sound: to hear it, share the host's audio socket, e.g."
        log "sound:   -v /run/user/\$(id -u)/pulse/native:/tmp/pulse:ro \\"
        log "sound:   -e PULSE_SERVER=unix:/tmp/pulse"
    fi

    # Music is rendered inside the engine by FluidSynth. With no
    # DOOM_SOUNDFONT set it finds the soundfont installed in the image and
    # reports its choice on the "using soundfont" line below.
    if [ -n "${DOOM_SOUNDFONT:-}" ] && [ ! -r "$DOOM_SOUNDFONT" ]; then
        log "music: DOOM_SOUNDFONT $DOOM_SOUNDFONT is not readable,"
        log "music: falling back to whichever soundfont is installed"
    fi
else
    log "sound: disabled"
    rm -f "$LINKDIR/sndserver"
fi

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
log "  play at  http://localhost:$WEB_PORT/  (or /play.html)"
log ""
log "  plain noVNC (no mouse capture):  http://localhost:$WEB_PORT/vnc.html?autoconnect=1&resize=off"
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

# Name the IWAD outright unless the caller already did. The engine would
# otherwise fall back to searching, which is what picked the wrong game.
case " $* " in
    *" -iwad "*)
        ;;
    *)
        [ -n "$CHOSEN" ] && set -- -iwad "$CHOSEN" "$@"
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
