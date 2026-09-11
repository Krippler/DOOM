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
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY linuxdoom-1.10/ ./linuxdoom-1.10/
COPY sndserv/ ./sndserv/

RUN make -C linuxdoom-1.10 -j"$(nproc)" \
 && strip linuxdoom-1.10/linux/linuxxdoom \
 && test -x linuxdoom-1.10/linux/linuxxdoom

# The engine drives sound through this separate process, which is how the
# original release worked and, per its own README, the arrangement that
# sounds best. It talks to PulseAudio directly; see sndserv/pulse.c.
RUN make -C sndserv -j"$(nproc)" SNDBACKEND=pulse \
 && strip sndserv/linux/sndserver \
 && test -x sndserv/linux/sndserver

##############################################################################
# Runtime stage: the engine plus a private 8-bit X server and a web client.
##############################################################################
FROM ubuntu:24.04

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        libx11-6 \
        libxext6 \
        libpulse0 \
        xvfb \
        x11vnc \
        novnc \
        websockify \
        tini \
 && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/linuxdoom-1.10/linux/linuxxdoom /usr/local/games/linuxxdoom
COPY --from=build /src/sndserv/linux/sndserver /usr/local/games/sndserver
COPY docker/entrypoint.sh /usr/local/bin/doom-entrypoint

RUN chmod +x /usr/local/bin/doom-entrypoint \
 && useradd --create-home --home-dir /doom/state --uid 1001 doomer \
 && mkdir -p /wads /doom/state \
 && chown -R doomer:doomer /doom

# Savegames (doomsavN.dsg) and the config file (.doomrc) are written to the
# working directory and $HOME respectively, so both point at the state volume.
ENV DOOM_SCALE=2 \
    DOOM_WADDIR=/wads \
    DOOM_STATE=/doom/state \
    DOOM_VNC_PORT=5900 \
    DOOM_WEB_PORT=6080 \
    DOOM_DISPLAY=:99 \
    HOME=/doom/state

VOLUME ["/doom/state"]
EXPOSE 6080 5900

USER doomer
WORKDIR /doom/state

ENTRYPOINT ["/usr/bin/tini", "--", "/usr/local/bin/doom-entrypoint"]
