# What this actually is

id Software published these sources in December 1997.
[`README.TXT`](README.TXT) is Carmack's note from that release and is left
exactly as it was — it describes the code, not this repository.

What it does not describe is that the code no longer compiles, and did not run
once it did. Getting from there to a playable game took:

- **19 fixes before it would build and run correctly** — a one-byte heap overflow
  in the IWAD search, a WAD structure whose on-disk layout shifted under
  64-bit pointers, pointer arrays allocated at half their size, a 4× scaler
  that assumed big-endian and mirrored every pixel pair, undefined behaviour
  in the input queue, and a sound path that called `exit(-1)` on any system
  without OSS — which is every system now.
- **Sound**, through the mixer that shipped with the release. The original ran
  it in a separate process writing to `/dev/dsp`, which no current kernel
  provides, and PulseAudio's own OSS shim was removed upstream in PulseAudio
  16. Sound bound for the browser is mixed by `audiostream` directly, from the
  same code; a container sharing the host's PulseAudio socket still runs the
  separate server, with a PulseAudio backend in place of the OSS one.
- **Music**, which was never implemented at all: every music function in the
  1997 sources is an empty stub. The WAD's MUS lumps are converted to Standard
  MIDI and rendered by FluidSynth.
- **A way for any of that to reach you.** The container has no sound card and
  no sound daemon — the only packaged PulseAudio brings systemd, GStreamer and
  a set of video codecs with it — and VNC carries a picture and nothing else.
  So `audiostream` mixes the two and sends raw PCM to the page, which plays it
  through an `AudioWorklet` where the browser allows one and a
  `ScriptProcessorNode` where it does not. Nothing to mount, which matters
  when the container is on a server in another room.
- **A display the engine will accept.** It only ever supported an 8-bit
  PseudoColor X visual, which no current X server offers. It now draws in
  truecolour, turning its palette into pixels itself, and the container brings
  its own Xvfb and exports it over noVNC.
- **A browser client that captures the mouse.** noVNC is a remote desktop
  client and reports where the pointer is; a game needs to know how far it
  moved. `play.html` locks the pointer instead, so turning never runs out of
  screen and the cursor cannot wander off into the rest of your desktop —
  fullscreen or in the window, whichever you pick.
- **A picture that does not stutter.** x11vnc ships with `-wireframe` and
  `-scrollcopyrect` on, which watch for a window being dragged or a pane
  scrolled while a mouse button is held and hold the screen back while they
  decide. A game holds the fire button down, so the picture stopped for about
  300 ms at a time whenever you shot at anything. Both are off here: with the
  button held that is the difference between 257 KB/s at a 41 ms median and
  554 KB/s at 12 ms.
- **A game controller, on a phone as well as a desktop, and it vibrates.** An
  Xbox pad, a Backbone One, anything the browser calls a standard gamepad. The
  pad is plugged into the machine running the browser, so the page reads it
  and passes its state to the engine over the same port as the picture; the
  engine does the rest (`i_pad.c`), with its buttons set in the game's own
  menus and its sticks as speeds rather than keys. The same code reads a pad
  through SDL2 when the engine runs on a desktop. Vibration comes through
  `I_Tactile`, a force-feedback hook id left in the 1997 code and never filled
  in.

[`PORTING-NOTES.md`](PORTING-NOTES.md) documents every change, with the
original code and why it broke — including [what the stutter turned out to
be](PORTING-NOTES.md#the-stutter-while-the-fire-button-is-down), the dozen
suspects measured and cleared before it, and the four instruments that exist
because a confident answer turned out to be an artefact of how it was measured.

## What was added

Four pages under **Options → Setup**, none of which the 1997 release had any
equivalent of:

- **Controls** — rebind the eleven movement, action and menu keys. Saved to
  `.doomrc`.
- **Mouse** — turn the mouse on and off, assign its buttons, and capture the
  pointer so it cannot slide out of the window while you turn.
- **Controller** — what each button does, the turn speed, vibration, swapping
  the sticks, and whether a full push runs. Saved to `.doomrc`.
- **Load WAD** — list the `.wad` files you mounted, marked `GAME` or `MOD`,
  and load one. The engine builds its textures, sprites and sound cache once
  at startup, so choosing a file restarts it: a couple of seconds back to the
  title screen.

How to use them is in [DOCKER.md](DOCKER.md); how they were built, in
[PORTING-NOTES.md](PORTING-NOTES.md#added).
