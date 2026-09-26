#!/bin/sh
#
# install.sh -- installs the Linux build from this directory.
#
#   ./install.sh                 for you alone, into ~/.local
#   sudo ./install.sh /usr/local for everybody on the machine
#   ./install.sh --uninstall [PREFIX]
#
# What goes where, under PREFIX:
#   bin/doom                                  the launcher: start the game with this
#   lib/doom/linuxxdoom                       the engine
#   lib/doom/sndserver                        the sound effects server
#   lib/doom/doom1.wad                        the shareware episode
#
# Not share/doom: with the default prefix that is ~/.local/share/doom, which
# is where the game keeps your settings and saves.
#   share/applications/doom.desktop           the entry in the desktop's menu
#   share/icons/hicolor/128x128/apps/doom.png
#
# Your settings, saves and mods are in ~/.local/share/doom either way, and
# uninstalling leaves them there.
#

set -eu

here=$(cd "$(dirname "$0")" && pwd)

uninstall=0
if [ "${1:-}" = --uninstall ]; then
    uninstall=1
    shift
fi

prefix="${1:-$HOME/.local}"

files="bin/doom lib/doom/linuxxdoom lib/doom/sndserver lib/doom/doom1.wad
lib/doom/SHAREWARE.md share/applications/doom.desktop share/icons/hicolor/128x128/apps/doom.png"

if [ "$uninstall" = 1 ]; then
    for f in $files; do
        rm -f "$prefix/$f"
    done
    rmdir "$prefix/lib/doom" 2>/dev/null || true
    echo "Removed DOOM from $prefix. Settings, saves and mods are still in"
    echo "${XDG_DATA_HOME:-$HOME/.local/share}/doom."
    exit 0
fi

# Everything the engine and the sound server need from the system, said by
# name when it is missing rather than as a loader error the first time.
missing=$(ldd "$here/lib/doom/linuxxdoom" "$here/lib/doom/sndserver" 2>/dev/null \
          | awk '/not found/ {print $1}' | sort -u)
if [ -n "$missing" ]; then
    echo "These libraries are needed and not installed:"
    for m in $missing; do
        echo "    $m"
    done
    echo "On Debian or Ubuntu: sudo apt install libx11-6 libxext6 libfluidsynth3 libpulse0"
    echo "On Fedora:           sudo dnf install libX11 libXext fluidsynth-libs pulseaudio-libs"
    echo "On Arch:             sudo pacman -S libx11 libxext fluidsynth libpulse"
    exit 1
fi

# The music needs a General MIDI soundfont, and the game runs without one;
# so this is a word rather than a refusal.
found=0
for sf in /usr/share/sounds/sf2/default-GM.sf2 /usr/share/sounds/sf2/FluidR3_GM.sf2 \
          /usr/share/soundfonts/FluidR3_GM.sf2 /usr/share/soundfonts/default.sf2 \
          /usr/share/sounds/sf2/TimGM6mb.sf2; do
    [ -r "$sf" ] && found=1
done
if [ "$found" = 0 ]; then
    echo "Note: no General MIDI soundfont is installed, so there will be no music."
    echo "      (fluid-soundfont-gm on Debian/Ubuntu, fluid-soundfont-gm on Fedora,"
    echo "      soundfont-fluid on Arch)"
fi

# Controllers come through SDL2, loaded when the game starts if it is there.
if ! ldconfig -p 2>/dev/null | grep -q 'libSDL2-2.0.so.0'; then
    echo "Note: SDL2 is not installed, so game controllers will not work."
    echo "      (libsdl2-2.0-0 on Debian/Ubuntu, SDL2 on Fedora, sdl2 on Arch)"
fi

install -d "$prefix/bin" "$prefix/lib/doom" \
           "$prefix/share/applications" "$prefix/share/icons/hicolor/128x128/apps"
install -m 755 "$here/lib/doom/linuxxdoom" "$prefix/lib/doom/linuxxdoom"
install -m 755 "$here/lib/doom/sndserver" "$prefix/lib/doom/sndserver"
install -m 755 "$here/bin/doom" "$prefix/bin/doom"
install -m 644 "$here/lib/doom/doom1.wad" "$prefix/lib/doom/doom1.wad"
install -m 644 "$here/lib/doom/SHAREWARE.md" "$prefix/lib/doom/SHAREWARE.md"
install -m 644 "$here/share/icons/hicolor/128x128/apps/doom.png" \
               "$prefix/share/icons/hicolor/128x128/apps/doom.png"

# The menu entry runs the launcher by its full path: ~/.local/bin is not on
# every desktop session's PATH.
sed "s|^Exec=doom|Exec=\"$prefix/bin/doom\"|" \
    "$here/share/applications/doom.desktop" \
    > "$prefix/share/applications/doom.desktop"
chmod 644 "$prefix/share/applications/doom.desktop"

command -v update-desktop-database >/dev/null 2>&1 \
    && update-desktop-database -q "$prefix/share/applications" 2>/dev/null || true

echo "Installed DOOM into $prefix."
echo
echo "Start it from your desktop's menu, or run: $prefix/bin/doom"
echo "It plays the shareware episode until it finds your own copy of the game."
echo "If it does not find yours by itself:"
echo "    $prefix/bin/doom --data /path/to/DOOM.WAD"
