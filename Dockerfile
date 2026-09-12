# DOOM (linuxdoom-1.10) in a container, playable in a browser.
#
# The engine only ever learned to talk to an 8-bit PseudoColor X visual, which
# no modern X server still offers. The container supplies one of its own with
# Xvfb, then exports it over VNC and noVNC, so the game is reachable from a
# browser on any host.
#
#   docker build -t doom .
#   docker run --rm -p 6080:6080 -v "$PWD/wads:/wads:ro" doom
#   # then open http://localhost:6080/vnc.html
#
# See DOCKER.md for the full set of options.

##############################################################################
# Build stage: compile the engine against X11.
##############################################################################
FROM ubuntu:24.04 AS build

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        gcc \
        libc6-dev \
        make \
        libx11-dev \
        libxext-dev \
        libpulse-dev \
        libfluidsynth-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY linuxdoom-1.10/ ./linuxdoom-1.10/
COPY sndserv/ ./sndserv/
COPY audiostream/ ./audiostream/

# Music is built in: mus2mid.c converts the WAD's MUS lumps to Standard MIDI
# and FluidSynth renders them. Build with MUSIC=none to leave it out.
RUN make -C linuxdoom-1.10 -j"$(nproc)" \
 && strip linuxdoom-1.10/linux/linuxxdoom \
 && test -x linuxdoom-1.10/linux/linuxxdoom

# The engine drives sound through this separate process, which is how the
# original release worked and, per its own README, the arrangement that
# sounds best. Built twice: once talking to PulseAudio (sndserv/pulse.c), for
# when the host's audio socket is shared with the container, and once writing
# raw PCM to a pipe (sndserv/stream.c), for when there is no audio system at
# all and the sound is going to the browser. The entrypoint picks.
RUN make -C sndserv -j"$(nproc)" SNDBACKEND=pulse \
 && strip sndserv/linux/sndserver \
 && test -x sndserv/linux/sndserver \
 && mv sndserv/linux/sndserver /tmp/sndserver-pulse \
 && make -C sndserv clean \
 && make -C sndserv -j"$(nproc)" SNDBACKEND=stream \
 && strip sndserv/linux/sndserver \
 && test -x sndserv/linux/sndserver \
 && mv sndserv/linux/sndserver /tmp/sndserver-stream \
 && mv /tmp/sndserver-pulse sndserv/linux/sndserver

# Mixes the sound server's effects with the engine's music and serves the
# result to the browser, because the container has no sound card and cannot
# be given one: the only packaged PulseAudio brings systemd, GStreamer and a
# set of video codecs with it. See audiostream/audiostream.c.
RUN make -C audiostream -j"$(nproc)" \
 && strip audiostream/linux/audiostream \
 && test -x audiostream/linux/audiostream

##############################################################################
# Runtime stage: the engine plus a private 8-bit X server and a web client.
##############################################################################
FROM ubuntu:24.04

# Which General MIDI soundfont to install. FluidR3 is much the better one and
# is the default; build with --build-arg SOUNDFONT_PACKAGE=timgm6mb-soundfont
# to trade it for 6 MB instead of 142 MB. The engine finds whichever is there.
ARG SOUNDFONT_PACKAGE=fluid-soundfont-gm

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        libx11-6 \
        libxext6 \
        libpulse0 \
        libfluidsynth3 \
        "$SOUNDFONT_PACKAGE" \
        xvfb \
        x11vnc \
        websockify \
        tini \
# noVNC is static HTML and JavaScript served to the browser, but the distro
# package depends on Node and net-tools for tooling this image never runs.
# Unpack just the files instead; websockify above is what actually serves them.
 && apt-get download novnc \
 && dpkg-deb -x novnc_*.deb / \
 && rm -f novnc_*.deb \
# Drop what nothing depends on any more, chiefly libxml2 and the 36 MB of
# ICU behind it. This has to happen before the forced removals below, which
# leave dpkg with unmet dependencies that apt then refuses to work around.
 && apt-get autoremove -y --purge \
# Xvfb is linked against libGL.so.1 so the dispatch library has to stay, but
# nothing in this image ever renders through GLX and the Mesa driver behind it
# costs about 180 MB, most of it LLVM. Drop the driver, keep the dispatch.
 && dpkg --remove --force-depends \
        libglx-mesa0 mesa-libgallium libllvm20 libgl1-mesa-dri \
# websockify only uses numpy to unmask client-to-server WebSocket frames.
# Per RFC 6455 the server never masks what it sends, so for a VNC session
# that is just the keystrokes, not the video. It warns and carries on.
 && dpkg --remove --force-depends \
        python3-numpy liblapack3 libblas3 libgfortran5 \
 && rm -rf /var/lib/apt/lists/* /usr/share/doc/* /usr/share/man/*

COPY --from=build /src/linuxdoom-1.10/linux/linuxxdoom /usr/local/games/linuxxdoom
COPY --from=build /src/sndserv/linux/sndserver /usr/local/games/sndserver
COPY --from=build /tmp/sndserver-stream /usr/local/games/sndserver-stream
COPY --from=build /src/audiostream/linux/audiostream /usr/local/games/audiostream
COPY docker/entrypoint.sh /usr/local/bin/doom-entrypoint

# websockify, taught to carry the sound alongside the picture on one port.
COPY docker/doom-wsproxy.py /usr/local/bin/doom-wsproxy

# Which build this is. Stamped into the client and printed at startup, so a
# report of "no sound" can be told apart from a browser quietly running the
# client from two releases ago -- the container's log looks identical either
# way, and everything that decides whether sound arrives happens in the page.
ARG DOOM_VERSION=dev

# A client that captures the mouse. Stock noVNC reports absolute pointer
# positions, which a game cannot use: see the comment at the top of the file.
COPY docker/play.html /usr/share/novnc/play.html
COPY docker/doom-ring.js /usr/share/novnc/doom-ring.js
COPY docker/doom-audio.js /usr/share/novnc/doom-audio.js
COPY docker/index.html /usr/share/novnc/index.html

# Not sed: the stamp is whatever the build was told, and a branch name with a
# slash in it ends the s/// early -- which is exactly how this broke first
# time. Python replaces the placeholder literally, and narrows the value to
# characters that cannot escape either the HTML text or the JavaScript string
# literal it lands in.
RUN python3 -c 'import os,re,pathlib; p=pathlib.Path("/usr/share/novnc/play.html"); v=re.sub(r"[^A-Za-z0-9._+-]","-",os.environ.get("DOOM_VERSION") or "dev"); s=p.read_text().replace("__DOOM_VERSION__",v); p.write_text(s); assert "__DOOM_VERSION__" not in s, "version placeholder left in the client"'

# The shareware IWAD, so the container is playable with nothing mounted.
# id distributes it freely; see shareware/README.md. A mounted IWAD still
# wins over it. Commercial game data is not here and never will be.
COPY shareware/doom1.wad /usr/share/doom/doom1.wad

# The directories come first so useradd does not warn about a home it
# cannot chown yet.
RUN chmod +x /usr/local/bin/doom-entrypoint /usr/local/bin/doom-wsproxy \
 && mkdir -p /wads /doom/state \
 && useradd --create-home --home-dir /doom/state --uid 1001 doomer \
 && chown -R doomer:doomer /doom

# Savegames (doomsavN.dsg) and the config file (.doomrc) are written to the
# working directory and $HOME respectively, so both point at the state volume.
# DOOM_SOUNDFONT is deliberately unset: the engine searches for an installed
# General MIDI soundfont on its own, so whichever SOUNDFONT_PACKAGE was built
# in gets used. Set it to override with your own file.
ENV DOOM_VERSION=${DOOM_VERSION} \
    DOOM_SCALE=2 \
    DOOM_WADDIR=/wads \
    DOOM_STATE=/doom/state \
    DOOM_VNC_PORT=5900 \
    DOOM_AUDIO_PORT=5901 \
    DOOM_WEB_PORT=6080 \
    DOOM_DISPLAY=:99 \
    HOME=/doom/state

VOLUME ["/doom/state"]
EXPOSE 6080 5900

# No USER: the entrypoint starts as root only long enough to take ownership
# of the state directory as PUID:PGID, then drops to that user for the rest.
# Defaults to the doomer account created above. Pass --user to skip that and
# run as somebody specific from the outset.
WORKDIR /doom/state

ENTRYPOINT ["/usr/bin/tini", "--", "/usr/local/bin/doom-entrypoint"]
