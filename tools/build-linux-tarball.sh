#!/bin/sh
#
# build-linux-tarball.sh VERSION [OUTDIR]
#
# Builds the engine and the PulseAudio sound server and packs them with the
# launcher, the menu entry, the shareware episode and install.sh into
# doom-linux-ARCH-VERSION.tar.gz.
#
# The engine is the same one the container runs: it draws on whatever X
# display it is given and reads a controller through SDL2 when there is no
# browser to hand it one. The binaries run on distributions with the same C
# library as the machine they are built on or newer, so the release is built
# on an old one: see the linux job in .github/workflows/docker-publish.yml.
#

set -eu

version="${1:?usage: build-linux-tarball.sh VERSION [OUTDIR]}"
out="${2:-.}"
here=$(cd "$(dirname "$0")/.." && pwd)
arch=$(uname -m)
name="doom-linux-$arch-$version"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

make -C "$here/linuxdoom-1.10" clean >/dev/null
make -C "$here/linuxdoom-1.10" -j"$(nproc)"
make -C "$here/sndserv" clean >/dev/null
make -C "$here/sndserv" -j"$(nproc)" SNDBACKEND=pulse

root="$stage/$name"
install -d "$root/bin" "$root/lib/doom" \
           "$root/share/applications" "$root/share/icons/hicolor/128x128/apps"
install -m 755 "$here/linuxdoom-1.10/linux/linuxxdoom" "$root/lib/doom/linuxxdoom"
install -m 755 "$here/sndserv/linux/sndserver" "$root/lib/doom/sndserver"
strip "$root/lib/doom/linuxxdoom" "$root/lib/doom/sndserver"
install -m 755 "$here/desktop/doom" "$root/bin/doom"
install -m 644 "$here/desktop/doom.desktop" "$root/share/applications/"
install -m 644 "$here/templates/doom-icon.png" \
               "$root/share/icons/hicolor/128x128/apps/doom.png"
install -m 644 "$here/shareware/doom1.wad" "$root/lib/doom/doom1.wad"
install -m 755 "$here/desktop/install.sh" "$root/install.sh"
install -m 644 "$here/desktop/README.txt" "$root/README.txt"
install -m 644 "$here/LICENSE.TXT" "$root/COPYING"
install -m 644 "$here/shareware/README.md" "$root/lib/doom/SHAREWARE.md"

mkdir -p "$out"
tar -C "$stage" -czf "$out/$name.tar.gz" "$name"
echo "$out/$name.tar.gz"
