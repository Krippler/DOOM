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
AUDIOSTREAM_BIN="${DOOM_AUDIOSTREAM_BIN:-/usr/local/games/audiostream}"
WSPROXY_BIN="${DOOM_WSPROXY_BIN:-/usr/local/bin/doom-wsproxy}"
AUDIO_PORT="${DOOM_AUDIO_PORT:-5901}"
AUDIO_RATE="${DOOM_AUDIO_RATE:-22050}"
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

# Which build this is, printed after the drop so the re-exec above does not
# say it twice. The client carries the same stamp on its start screen, and the
# two disagreeing is the whole diagnosis when a browser is quietly running an
# older client than the container it is talking to -- which the container's
# own log cannot tell you, because nothing about it is wrong.
log "DOOM ${DOOM_VERSION:-dev}"

#
# How many cores this container is allowed, said out loud at startup.
#
# Not a diagnosis: starving these processes was tested and does not cause the
# stutter. Pinned to a single core with a busy process competing for it,
# x11vnc spent 2225 ms of every 5000 waiting for a core -- three times worse
# than the machine this was chased on -- and answered every request inside
# 48 ms with nothing over 200. The run-queue figures further down are worth
# reading, but they are not this.
#
# It is here because it is the one thing about how the container was started
# that the log could not tell you, and the first question anyone asks about a
# slow container is how much of the machine it was given.
#
cpu_allowance () {
    cores=$(nproc 2>/dev/null || echo '?')

    # Pinning shows up in nproc, because it is an affinity mask. A quota does
    # not: it is a share of time across whatever cores are visible, so it has
    # to be read from the cgroup -- v2 first, then v1.
    quota=''

    if [ -r /sys/fs/cgroup/cpu.max ]; then
        read -r q p _ < /sys/fs/cgroup/cpu.max 2>/dev/null || q=max
        [ "$q" = max ] || quota=$(( q * 100 / p ))
    elif [ -r /sys/fs/cgroup/cpu/cpu.cfs_quota_us ]; then
        q=$(cat /sys/fs/cgroup/cpu/cpu.cfs_quota_us 2>/dev/null || echo -1)
        p=$(cat /sys/fs/cgroup/cpu/cpu.cfs_period_us 2>/dev/null || echo 100000)
        [ "$q" -le 0 ] 2>/dev/null || quota=$(( q * 100 / p ))
    fi

    if [ -n "$quota" ]; then
        log "${cores} core(s) visible, limited to ${quota}% of one"
    else
        log "${cores} core(s) available"
    fi
}

cpu_allowance

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
# Music it renders itself, with FluidSynth.
#
# Those two streams go one of two ways. By default they go to the browser:
# audiostream reads both, mixes them and serves the result over the same port
# the picture arrives on, because a container on a server somewhere has no
# speakers and nobody is sitting next to it. Share the host's PulseAudio
# socket instead and they go there, which is what you want when the container
# is on the machine you are sitting at.
##############################################################################
AUDIO_TO_BROWSER=0

if [ "${DOOM_SOUND:-1}" != "1" ]; then
    log "sound: disabled"
    rm -f "$LINKDIR/sndserver"
elif [ -n "${PULSE_SERVER:-}" ] || \
     [ -S "${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/pulse/native" ]; then
    # An audio server was handed to us, so use it: this is the same container
    # on the same desktop as the speakers.
    if [ -x "$SNDSERVER_BIN" ]; then
        ln -sf "$SNDSERVER_BIN" "$LINKDIR/sndserver"
    fi

    if [ -n "${PULSE_SERVER:-}" ]; then
        log "sound: to PulseAudio, PULSE_SERVER=$PULSE_SERVER"
    else
        log "sound: to PulseAudio at ${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/pulse/native"
    fi
elif [ -x "$AUDIOSTREAM_BIN" ]; then
    AUDIO_TO_BROWSER=1
    # No sound server to start: audiostream does the mixing itself and the
    # engine sends it the same commands down a socket.
    rm -f "$LINKDIR/sndserver"

    # Both producers write raw PCM into these; audiostream reads them on a
    # real-time schedule, which is also what stops either running ahead.
    AUDIO_PIPES="$STATE/audio"
    rm -rf "$AUDIO_PIPES"
    mkdir -p "$AUDIO_PIPES"

    export DOOM_SFX_SOCKET="$AUDIO_PIPES/commands"
    export DOOM_MUSIC_PIPE="$AUDIO_PIPES/music"
    export DOOM_AUDIO_RATE="$AUDIO_RATE"

    log "sound: to the browser, ${AUDIO_RATE} Hz stereo"
else
    log "sound: no audio output available, the game will be silent"
    rm -f "$LINKDIR/sndserver"
fi

if [ "${DOOM_SOUND:-1}" = "1" ]; then
    # Music is rendered inside the engine by FluidSynth. With no
    # DOOM_SOUNDFONT set it finds the soundfont installed in the image and
    # reports its choice on the "using soundfont" line below.
    if [ -n "${DOOM_SOUNDFONT:-}" ] && [ ! -r "$DOOM_SOUNDFONT" ]; then
        log "music: DOOM_SOUNDFONT $DOOM_SOUNDFONT is not readable,"
        log "music: falling back to whichever soundfont is installed"
    fi
fi

##############################################################################
# Background services. Everything is torn down together.
##############################################################################
XVFB_PID=""
VNC_PID=""
WEB_PID=""
AUDIO_PID=""
DOOM_PID=""
CPUWATCH_PID=""
HELPERWATCH_PID=""

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

    # First, so the teardown below cannot be reported as a fault. A normal
    # quit killed the helpers and the watchdog announced "noVNC has exited --
    # nothing works without it" on the way out, which is true and useless.
    if [ -n "$HELPERWATCH_PID" ]; then
        kill "$HELPERWATCH_PID" 2>/dev/null || true
        HELPERWATCH_PID=""
    fi

    for pid in "$CPUWATCH_PID" "$WEB_PID" "$AUDIO_PID" "$VNC_PID" "$XVFB_PID"; do
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

#
# -wireframe and -scrollcopyrect are both on by default, and both are for a
# desktop: they watch for a window being dragged or a pane being scrolled while
# a mouse button is held, and hold back the picture while they decide. A game
# holds the fire button down. There is one window, it never moves, and there is
# nothing to scroll.
#
# Measured with the fire button held and the player turning, 80 seconds each:
#
#                            answers   KB/s   median   p90   p99   worst   >200ms
#   both on (the default)       1271    257    41 ms    62    91     118        0
#   -nowireframe only           1079    250    43 ms    62    94     118        0
#   -noscrollcopyrect only      1029    121    15 ms   140   306     341       41
#   both off                    2774    554    12 ms    15    24      44        0
#
# Without the button held, the default is fine -- 2752 answers, 539 KB/s,
# median 12 ms. So this costs nothing until someone fires, and then it halves
# the frame rate and triples the median. The two do different damage:
# -scrollcopyrect slows everything down evenly, which hides the other one, and
# -wireframe holds the picture for about 300 ms at a time -- t2 in its own
# default timing string, "0.15+0.30+5.0+0.125", is how long it waits for a
# window to start moving after a button goes down, and it repaints nothing
# while it waits. Those 300 ms holds are the stutter this was chased for.
#
# x11vnc's timing defaults are tuned for a desktop, where nothing is waiting
# on the next frame: -nap lengthens the poll interval when activity is low,
# which is the state of a DOOM screen in the moment before you press fire, and
# -wait and -defer each hold 20 ms in reserve for a slow link.
#
# None of that measured as worth anything here -- keypress to pixels is about
# 140 ms either way, and moving these from 20 to 5 to 1 changed nothing
# outside the noise. They are set anyway because the defaults are wrong in
# principle for something being played rather than administered, and left
# adjustable because the next person to look at this will want to try them.
# What the delay actually is, and everything that was ruled out, is in
# PORTING-NOTES.md.
#
# Catch the option that is not one before x11vnc does.
#
# x11vnc answers anything it does not recognise by printing "unrecognized
# option(s)" into its own log and exiting, which leaves the game running, the
# container looking healthy, and a browser saying "connection lost" with no
# explanation anywhere anyone thinks to look. A value of "noxdamage" instead
# of "-noxdamage" is enough to do it, and that is an easy thing to type into a
# template field that does not show you the dash.
#
for vnc_arg in ${DOOM_VNC_ARGS:-}; do
    case "$vnc_arg" in
        -*) ;;
        *)  die "DOOM_VNC_ARGS has '$vnc_arg', which x11vnc will not accept:
       options need their leading dash, as in '-$vnc_arg'. x11vnc exits on an
       option it does not know, and then nothing can connect." ;;
    esac
done

log "starting x11vnc on port $VNC_PORT"
# shellcheck disable=SC2086
#
# -8to24 is how a depth 8 display is presented as truecolor, and it is the most
# expensive thing x11vnc does here: its own manual says the mode walks the
# window tree, polls it with XGetImage, transforms the whole screen, and "does
# hog resources". Turning it off is a diagnostic, not a supported way to play
# -- the picture goes to a colormapped depth 8 that not every client renders
# properly -- so it is an environment variable rather than an option anyone is
# steered towards.
#
if [ "${DOOM_VNC_8TO24:-1}" = "0" ]; then
    log "  -8to24 off by request: the picture will show 64 colours, not 256"
    log "  (noVNC cannot use a colour map, so at depth 8 it asks for two bits"
    log "  per channel). For counting stalls only -- set it back to play."
    vnc_8to24=""
else
    vnc_8to24="-8to24"
fi

# shellcheck disable=SC2086
x11vnc -display "$DISP" -rfbport "$VNC_PORT" -forever -shared -quiet \
       $vnc_8to24 \
       -nowireframe -noscrollcopyrect \
       -nonap -wait "${DOOM_VNC_WAIT:-5}" -defer "${DOOM_VNC_DEFER:-5}" \
       ${DOOM_VNC_ARGS:-} \
       $vnc_auth >"$STATE/x11vnc.log" 2>&1 &
VNC_PID=$!

# audiostream has to be up before the engine, because the engine and its sound
# server open these pipes at startup and the reader is what creates them.
if [ "$AUDIO_TO_BROWSER" = "1" ]; then
    log "starting audiostream on port $AUDIO_PORT"
    "$AUDIOSTREAM_BIN" --commands "$DOOM_SFX_SOCKET" \
        --music "$DOOM_MUSIC_PIPE" \
        --rate "$AUDIO_RATE" --port "$AUDIO_PORT" \
        >"$STATE/audiostream.log" 2>&1 &
    AUDIO_PID=$!

    i=0
    while [ ! -p "$DOOM_MUSIC_PIPE" ]; do
        i=$((i + 1))
        [ "$i" -gt 100 ] && die "audiostream failed to start; see $STATE/audiostream.log"
        kill -0 "$AUDIO_PID" 2>/dev/null || die "audiostream exited; see $STATE/audiostream.log"
        sleep 0.1
    done
fi

# One WebSocket port, two streams behind it: the screen and, when the sound is
# going to the browser, the sound. See docker/doom-wsproxy.py -- a connection
# that asks for neither gets the screen, so stock /vnc.html still works.
{
    printf 'vnc: localhost:%s\n' "$VNC_PORT"
    [ "$AUDIO_TO_BROWSER" = "1" ] && printf 'audio: localhost:%s\n' "$AUDIO_PORT"
} > "$STATE/ws-targets"

log "starting noVNC on port $WEB_PORT"
#
# websockify's own chatter goes to its log; the lines it prints about the
# picture going quiet are lifted into this one, because that is where anyone
# reading a stutter report is looking.
#
"$WSPROXY_BIN" --web="$NOVNC_ROOT" \
       --token-plugin=websockify.token_plugins.ReadOnlyTokenFile \
       --token-source="$STATE/ws-targets" \
       "$WEB_PORT" 2>&1 |
    while IFS= read -r wsline; do
        case "$wsline" in
            "picture gap"*) log "$wsline" ;;
            *) printf '%s\n' "$wsline" >>"$STATE/websockify.log" ;;
        esac
    done &
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
#
# Who is waiting for a CPU rather than using one.
#
# Everything in here shares the host's cores with whatever else it is running,
# and a process that is ready but not scheduled looks exactly like a slow
# process from the outside. /proc/PID/schedstat says which: its second field is
# nanoseconds spent on the run queue wanting to run.
#
# This matters because the stall that shows up as a stuttering picture is not
# in the engine -- the engine reports itself healthy while the browser paints
# in fits -- and x11vnc is the one process here that does real work per frame.
# Quiet unless something actually waited, like every other report in this
# container.
#
cpu_wait_watch () {
    (
        # Sampled four times a second rather than once every five, because the
        # total on its own cannot tell 275 ms spent in one stall from the same
        # 275 ms spread over fifty. One of those stutters and the other does
        # not, and that is the whole question. The worst single quarter-second
        # is reported alongside the total.
        #
        # read is a builtin, so a sample is five file reads and no processes.
        i=0
        line=""

        for name in doom x11vnc Xvfb noVNC audiostream; do
            eval "prev_$name=''; tot_$name=0; worst_$name=0"
            eval "prevrun_$name=''; run_$name=0"
        done

        while sleep 0.25; do
            for entry in "doom:$DOOM_PID" "x11vnc:$VNC_PID" "Xvfb:$XVFB_PID" \
                         "noVNC:$WEB_PID" "audiostream:$AUDIO_PID"; do
                name=${entry%%:*}
                pid=${entry#*:}

                [ -n "$pid" ] && [ -r "/proc/$pid/schedstat" ] || continue

                read -r run_ns wait_ns _rest < "/proc/$pid/schedstat" || continue

                eval "old_ns=\$prev_$name; oldrun=\$prevrun_$name"
                eval "prev_$name=\$wait_ns; prevrun_$name=\$run_ns"

                [ -n "$old_ns" ] || continue

                ms=$(( (wait_ns - old_ns) / 1000000 ))
                runms=$(( (run_ns - oldrun) / 1000000 ))

                eval "tot_$name=\$(( \$tot_$name + ms ))"
                eval "run_$name=\$(( \$run_$name + runms ))"
                eval "worst=\$worst_$name"
                [ "$ms" -gt "$worst" ] && eval "worst_$name=$ms"
            done

            i=$((i + 1))
            [ "$i" -lt 20 ] && continue
            i=0
            line=""

            busyline=""

            for name in doom x11vnc Xvfb noVNC audiostream; do
                eval "tot=\$tot_$name; worst=\$worst_$name; used=\$run_$name"
                eval "tot_$name=0; worst_$name=0; run_$name=0"

                # More than 2% of the interval waiting for a core, or a single
                # quarter-second where it waited for most of one.
                if [ "$tot" -gt 100 ] || [ "$worst" -gt 60 ]; then
                    line="$line $name ${tot}ms (worst stretch ${worst}ms)"
                fi

                # Waiting is only half of it. A process using most of a core is
                # not starved and can still be the thing that stalls, and the
                # first version of this could not see that at all.
                if [ "$used" -gt 2500 ]; then
                    busyline="$busyline $name $(( used / 50 ))%"
                fi
            done

            [ -n "$line" ] && log "waiting for a CPU in the last 5s:$line"
            [ -n "$busyline" ] && log "using a lot of CPU in the last 5s:$busyline"
        done
    ) &
    CPUWATCH_PID=$!
}

#
# Notice when one of the supporting processes dies, and say so.
#
# Only the engine was waited on, so if x11vnc exited -- a mistyped
# DOOM_VNC_ARGS is enough, and Unraid's template will hand over the quotes as
# part of the value -- the container carried on looking healthy. The engine
# kept drawing, the log kept reporting frames, and the only sign was a browser
# saying "connection lost" with nothing in the container log to explain it.
# The reason was sitting in x11vnc's own log the whole time, which nobody knew
# to look at.
#
watch_helpers () {
    (
        while sleep 1; do
            for entry in "x11vnc:$VNC_PID:$STATE/x11vnc.log" \
                         "Xvfb:$XVFB_PID:$STATE/xvfb.log" \
                         "noVNC:$WEB_PID:$STATE/websockify.log" \
                         "audiostream:$AUDIO_PID:"; do
                name=${entry%%:*}
                rest=${entry#*:}
                pid=${rest%%:*}
                logfile=${rest#*:}

                [ -n "$pid" ] || continue
                kill -0 "$pid" 2>/dev/null && continue

                log "$name has exited -- nothing works without it"

                # The reason, from its own log: the first line that looks like
                # a complaint, or the last few if none of them do.
                if [ -n "$logfile" ] && [ -r "$logfile" ]; then
                    # The last complaint, not the first one in the file. These
                    # logs outlive a restart, so the first match can be minutes
                    # old and about something else entirely -- a stale "Failed
                    # to connect to localhost:5900" from an earlier crash got
                    # quoted as the reason for a later, unrelated exit.
                    said=$(tail -n 40 "$logfile" 2>/dev/null \
                           | grep -iE "unrecognized|invalid|error|fatal|cannot|refused|no such" \
                           | tail -n 1)
                    [ -n "$said" ] || said=$(tail -n 2 "$logfile" 2>/dev/null)

                    printf '%s\n' "$said" | while IFS= read -r said_line; do
                        [ -n "$said_line" ] && log "  $said_line"
                    done

                    log "  the rest is in $logfile"
                fi

                # Bring the whole thing down rather than sit here looking well.
                kill "$DOOM_PID" 2>/dev/null
                exit 0
            done
        done
    ) &
    HELPERWATCH_PID=$!
}

log "running: linuxxdoom $*"

# Run in the background and wait: a foreground child would block every trap
# until it exited, so `docker stop` could not shut the stack down.
"$DOOM_BIN" "$@" &
DOOM_PID=$!

cpu_wait_watch
watch_helpers

status=0
wait "$DOOM_PID" || status=$?
DOOM_PID=""

# The engine is gone on purpose, so nothing that follows is a fault.
if [ -n "$HELPERWATCH_PID" ]; then
    kill "$HELPERWATCH_PID" 2>/dev/null || true
    HELPERWATCH_PID=""
fi

log "DOOM exited with status $status"
exit "$status"
