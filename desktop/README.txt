DOOM for the Linux desktop
==========================

id Software's DOOM, from the source code id released in 1997, ported to
current Linux: truecolour on any X display, sound effects and music, a game
controller with vibration, and menus for keys, the mouse, the controller and
loading mods. The shareware episode comes with it; for the full game, use the
game files from a copy you own (Steam, GOG, or the original disks).

Install
-------

    ./install.sh                  for you alone, into ~/.local
    sudo ./install.sh /usr/local  for everybody on the machine

Then start DOOM from your desktop's menu, or run `doom` (~/.local/bin/doom if
~/.local/bin is not on your PATH).

It runs on Ubuntu 22.04, Debian 12, Fedora 36 or anything newer, under X11
or XWayland, and needs these libraries:

    Debian, Ubuntu:  sudo apt install libx11-6 libxext6 zlib1g libfluidsynth3 libpulse0
    Fedora:          sudo dnf install libX11 libXext zlib fluidsynth-libs pulseaudio-libs
    Arch:            sudo pacman -S libx11 libxext zlib fluidsynth libpulse

Music needs a General MIDI soundfont (fluid-soundfont-gm on Debian, Ubuntu
and Fedora, soundfont-fluid on Arch). Game controllers need SDL2
(libsdl2-2.0-0, SDL2, sdl2), which is installed wherever Steam is. Without
either the game runs, without music or without a controller.

Game files
----------

The first time it starts, DOOM looks for a Steam or GOG copy in the usual
places. If it does not find yours:

    doom --data /path/to/DOOM.WAD

or DOOM2.WAD, TNT.WAD, PLUTONIA.WAD, or the directory that holds one. It is
remembered after that. Until then it plays the shareware episode. Your game
files are only read; settings and saves go in ~/.local/share/doom.

Mods go in ~/.local/share/doom/wads. Options -> Setup -> Load WAD lists them
and loads the one you pick; `doom -file mod.wad` does the same from the
command line.

Playing
-------

The window is the largest multiple of 320x200 that fits your screen; -2, -3
or -4 picks one. The mouse turns you and is held by the window while you
play, and let go in the menus and when another window has the keyboard;
Options -> Setup -> Mouse -> Grab pointer turns that off.

W, A, S and D move you and E opens doors, on a first run; the mouse wheel
changes weapon and Backspace opens the menu. DOOM_MODERN_CONTROLS=0 on that
first run keeps id's keys instead (the arrows, comma and period, Space).
Keys are set under Options -> Setup -> Controls, a controller under
Options -> Setup -> Controller -- what each button does, turn speed,
vibration and more -- and a crosshair, a minimal HUD, changing weapon on a
pickup and Max FPS under Options -> Setup -> Gameplay. Max FPS starts at 60:
the game runs at 35 tics a second, as it always did, and the frames between
are drawn with everything part of the way to where it is going. 35 draws
only at the tics, as id's did; a faster monitor can take 120 or 144. In the menus, A chooses, B goes
back and Start opens and closes them.

Sound effects go through PulseAudio, or PipeWire's stand-in for it, which is
what almost every desktop runs.
