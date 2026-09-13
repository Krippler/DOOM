# Changelog

Notable changes to the containerised DOOM. Versions are the image tags
published to `ghcr.io/krippler/doom`, so `1.10.0` here is `:1.10.0` there.
`:latest` always tracks the newest release and `:edge` the tip of `master`.

The version follows the engine this is built from, linuxdoom-1.10.

## [Unreleased]

### Added
- **The container says who is waiting for a CPU rather than using one.** A
  process that is ready to run but not scheduled looks exactly like a slow
  process from the outside, and everything here shares the host's cores with
  whatever else that host is running. `/proc/PID/schedstat` says which: its
  second field is nanoseconds spent on the run queue wanting to run. A line
  every five seconds when something waited more than 2% of the interval, and
  nothing when nothing did:

  ```
  [doom] waiting for a CPU in the last 5s: doom 450ms x11vnc 430ms
  [doom] waiting for a CPU in the last 5s: doom 420ms x11vnc 286ms Xvfb 255ms audiostream 114ms
  ```

  This is the question left after the first trustworthy picture-side reading:

  ```
  Picture, while you were playing: best 36 frames a second of the 35 the game
      draws, arriving at 457 KB/s, longest gap 613 ms, 165 late.
  ```

  Best 36 of 35 means the path can carry the full rate, so it is not bandwidth
  and not throughput — 457 KB/s at 320×200 is the whole stream. What it is, is
  full speed punctuated by stalls: 165 frames of about 1600 arriving late, one
  gap of 613 ms. The sound over the same link in the same session underran
  three times while holding 20 ms, which a 613 ms stall would have destroyed,
  so the stall is not the network either. That leaves the one process in this
  container that does real work per frame, and whether it is being starved or
  stalling on its own is exactly what the new line answers.

## [1.10.31] — 2026-09-13

### Fixed
- **The picture figures were being wiped while you read them, which is how they
  came to say the opposite of the truth.** They aged out on a 15-second timer
  that kept running while the start screen was up — and the start screen can
  only be read once you have stopped playing, so what you read was whatever had
  survived the last wipe, measured against a motionless picture.

  That made every reading of them incomparable, and one of those readings is
  what I used to conclude the picture was bandwidth-limited. It is not: a
  quarter-size run reported **8 frames a second where full size reported 22**,
  which is the wrong way round for a bandwidth limit and should have been the
  end of that theory rather than a puzzle.

  The figures now describe the last spell of play and hold until play starts
  again, so what is on screen is always the run just finished:

  ```
  Picture, while you were playing: best 35 frames a second of the 35 the game
      draws, arriving at 1130 KB/s, longest gap 67 ms, 16 late.
  ```

  The advice about `DOOM_SCALE=1` that went with the old figures is gone with
  them.

### Known

- **Removing `-8to24` from x11vnc cuts the picture's bandwidth by 2.8x and
  breaks it.** Worth writing down, because it looks like the obvious answer to
  a link that cannot carry the picture. x11vnc is given a depth-8 display and
  presents it to clients as 32-bit truecolor, so four times as much has to be
  compressed as the engine actually produced. Serving the colour-mapped
  framebuffer instead measured **457 KB/s against 1268**, at the same 35 frames
  a second — and noVNC, which has no colour-map support, renders it as a green
  and black mess. The bandwidth was real; the picture was not.

  `DOOM_SCALE=1` does measure smaller here — 474 KB/s against 1265, at 34.4
  frames a second against 35.0 — but it does not follow that it helps, and in
  the field it did not: see the fix above. The bandwidth numbers on this page
  are measurements of this machine and nothing more.

## [1.10.30] — 2026-09-13

### Fixed
- **The sound could be left permanently out of step when the listener's socket
  filled, which is what "super loud, then cut out, then back to normal" is.**
  `write` on a socket takes as many bytes as it feels like — any number, not
  necessarily a whole 16-bit stereo frame. audiostream dropped whatever was
  left of the period when the socket came back `EAGAIN`, wherever in a frame
  that happened to be. The listener is then holding half a frame, and every
  sample after it is assembled from the wrong pair of bytes: not a glitch, but
  full-scale noise that never recovers until the page's ring buffer gives up
  and resynchronises.

  What is owed is now kept and offered again next period, and when the backlog
  is too old to be worth sending, **whole frames** are dropped from the front
  of it. A gap in the sound is a click; a gap that is not a whole number of
  frames is noise for ever.

  Driven against a socket that takes an arbitrary number of bytes at a time,
  the old code handed over **73,701 bytes — not a multiple of four**, so the
  listener's stream was genuinely not a whole number of frames. The new code
  delivered the whole test stream in order. The audible noise itself was not
  reproduced end to end: several attempts foundered on faults in the test
  rather than the program, and what is demonstrated is the misalignment, not
  the sound it makes.

### Added
- **The page says how fast the picture is arriving, not just how much of it
  gets painted.** A container reporting a steady 35 frames a second to a page
  painting 11 means two thirds are going nowhere, and only the byte rate says
  whether that is the link or something before it:

  ```
  Picture: 11 frames a second painted (the game draws 35), best lately 11,
      arriving at 402 KB/s, longest gap 179 ms, 13 late.
  ```

  Well short of 35 now also says what a full-rate picture costs, because the
  answer is usually a setting rather than a bug. Measured here, turning
  continuously at `screenblocks 10`:

  | | bandwidth | painted |
  | --- | --- | --- |
  | `DOOM_SCALE=2` (default, 640×400) | 1265 KB/s | 35.0/s |
  | `DOOM_SCALE=1` (320×200) | **474 KB/s** | 34.4/s |

  A link that can carry 400 KB/s gets a third of the frames at the default
  size and very nearly all of them at half it.

### Known

- **noVNC's compression and quality settings do nothing here.** Tried against
  the same 20-second turn, since a constrained link was the suspicion:
  defaults 1263 KB/s, compression 9 1259, compression 9 with quality 8 1634,
  with quality 5 1175. x11vnc is not honouring them in any useful way, so
  there is no bandwidth to be had from that direction and the picture costs
  what it costs.

## [1.10.29] — 2026-09-13

### Fixed
- **The engine no longer waits for the X server to say it has finished with
  each frame.** `I_FinishUpdate` handed the picture over with `XShmPutImage`
  asking for a completion event, then blocked until it arrived. That is correct
  if you are about to overwrite the shared image — but the next frame is a tic
  away, 28 ms, and the server copies it in microseconds. What the wait actually
  did was couple the engine to how busy the X server is, and the X server is
  busy because x11vnc is reading the same framebuffer as fast as it is allowed
  to.

  Measured in the field at **74 ms in that wait for one frame**, with the
  drawing itself under 2 ms. It is gone; the frame is handed over and the
  engine carries on. The loop was also pumping input events, which `I_StartTic`
  does every tic through `NetUpdate` anyway, so nothing is lost — verified by
  playing through it.

  Nothing else moved: delivery here is median 33 ms between painted frames, p90
  34, p99 52, and keypress-to-picture 154 ms with sound 13 ms behind it, all
  unchanged.

### Changed
- **The frame report names where a slow frame went, instead of lumping four
  things under "drawing".** The field logs had frames spending 36 ms in
  `drawing` with the picture handover at zero, which said only that the time
  went somewhere else. A frame is now split into the tic wait, the sound
  update, and the three parts of the draw — handing the picture to X, pumping
  input, and the rendering itself:

  ```
  frames: 176 in the last 5s, 12 late, 5 over 50 ms, 18% of a core; worst 59 ms
      (tic 59, sound 0, draw 0 = picture to X 0 + input 0 + render 0);
      woken 30 ms late at worst, holding the last 8 ms of each tic
  ```

  `input` is the other place a frame touches the X server — `I_StartTic` pumps
  the event queue and warps the pointer through there — so between it and the
  handover, a stall in the X path now has a name.

## [1.10.28] — 2026-09-13

### Fixed
- **Esc is handled by the page now, instead of hoping the browser does it.**
  Releasing the mouse on Esc was always the browser's own behaviour; the page
  only watched for the result. Two releases went by trying to fix Esc by
  reasoning about what browsers do, and both were wrong, because the whole
  mechanism lived somewhere neither I nor any test here could reach — a
  synthetic keypress does not trigger the browser's native pointer-lock
  release, so every test passed on a build that was broken in the field.

  The page now takes the key in the capture phase and asks for the release
  itself, with a fallback that shows the start screen anyway if neither the
  pointer lock nor fullscreen produced an event. That is the failure this
  exists for. Verified with a real Esc keypress in all four combinations —
  fullscreen and windowed, with and without Keyboard Lock — which is the first
  time this behaviour has been testable at all.

### Changed
- **The engine holds on to the CPU through the end of each tic, instead of
  sleeping and hoping to be woken on time.** 1.10.26 added a report of how
  late the kernel hands the CPU back, and the field answer was **22, 32 and 43
  ms** — against under a millisecond here. A machine consistently a few
  milliseconds late runs every frame late, and the same logs show stretches of
  126 to 142 frames per five seconds where 176 is the full rate: 28 a second
  instead of 35, which is a shudder rather than an occasional hitch.

  So the wait is now split. While there is time to spare it sleeps a
  millisecond at a time and costs nothing; through the last stretch — where
  being handed the CPU late is what costs a frame — it holds on. The stretch is
  as long as that machine has actually been late and no longer, capped at 8 ms,
  so a punctual machine spins one millisecond in twenty-eight:

  | | engine CPU, playing |
  | --- | --- |
  | 1997 (spin the whole wait) | 96.5% of a core |
  | 1.10.25–27 (sleep only) | 5% |
  | now, on a punctual machine | 9% |
  | now, worst case on a late one | ~28% |

  `DOOM_TIC_SPIN_MS=0` turns it off and restores the 1.10.25 behaviour.

  **This is not demonstrated to fix the stutter.** It cannot be: the fault is a
  property of the host's scheduler, and this machine does not have it — 24
  competing processes at raised priority against a container held to 0.6 of a
  core still produced not one late frame. What is measured is the mechanism, on
  the reporting machine, and that the countermeasure costs 4 points of CPU
  where it is not needed. Frame delivery here is unchanged: median gap 33 ms,
  p90 34, p99 52.

## [1.10.27] — 2026-09-13

### Fixed
- **Esc now releases the mouse and brings the start screen back, in both modes
  and every browser.** It did not, in fullscreen, on a browser that granted
  Keyboard Lock: the page deliberately kept Esc for the game there, and getting
  out meant *holding* it.

  That behaviour was correct for a premise that stopped being true in 1.10.14.
  Esc was taken from the browser because Esc was the game's own menu key and
  Pointer Lock hands Esc to the browser — but the menu moved to `` ` `` in
  1.10.14 and the lock stayed behind. Keyboard Lock also needs a secure
  context, so the same container answered Esc one way over HTTPS and another
  over plain HTTP on a LAN, which is not something anyone should have to know
  about. The lock is gone: `` ` `` opens the game menu, Esc gets you out, and
  the note on the start screen says so in one sentence instead of three
  depending on circumstances.

- **The picture readout added in 1.10.26 reported a player standing still as a
  disaster.** VNC sends what changed and nothing else, so a still screen paints
  nothing — and the line read `1 frames a second painted, worst wait 1467 ms`,
  which is true and completely misleading. It now says a still screen is still,
  counts gaps only while the picture is actually changing, and reports the best
  second lately, which is the honest answer to whether the path can carry 35 a
  second:

  ```
  Picture: 35 frames a second painted (the game draws 35), best lately 36, longest gap 67 ms, 6 late.
  Picture: still — nothing is changing on screen. Best lately 35 frames a second, of the 35 the game draws.
  ```

  This is the same mistake a test harness in this project made a release
  earlier, when it reported its own robot standing still as a 200 ms stall. It
  is worth naming twice: anything that measures a VNC screen has to know the
  difference between nothing arriving and nothing happening.

## [1.10.26] — 2026-09-13

### Added
- **The page says how many frames it actually painted, next to what the sound
  is doing.** The engine can report a healthy 35 frames a second while the
  browser paints far fewer — VNC sends only what changed, websockify relays it
  in Python, and the tab still has to decode and draw it. Nothing in the
  container can see the far end of that. The overlay now reads:

  ```
  Picture: 35 frames a second painted (the game draws 35), worst wait 100 ms, 12 arrived late.
  ```

  It counts the canvas actually changing, which is the one measure a frame that
  was sent and never arrived cannot fool, and it stops counting while the
  overlay is up — otherwise reading the figures records the reader as the worst
  stutter of the session, which is what the first version of this did.

  With the engine's own line in the container log, the two together say which
  half of the path a stutter is in. Not knowing that has cost two releases.

### Changed
- **The engine's frame report now counts frames that missed their slot, not
  just outright slow ones.** A tic is 28.6 ms; a frame a quarter longer than
  that means the engine runs two tics and draws once, and the picture jumps.
  The old threshold was 50 ms, so a steady stream of 40 ms frames — a judder —
  read as a clean log.

  It also reports how late the kernel was handing the CPU back. The wait for
  the next tic sleeps about 28 times a tic, and every one of those asks to be
  woken at a particular moment. A frame that misses its slot while the engine
  is using three per cent of a core was not the engine's doing, and that
  number says so.

### Known

- **Two things were tried against the remaining stutter and both were worse.**
  Written down because the measurements cost more than the changes would have.

  | Tried | Result |
  | --- | --- |
  | Sleeping straight to the tic boundary — one sleep a tic instead of 28, so one moment for the kernel to be late with | **Much worse.** Lands a whole tic past the boundary half the time: 88 frames in every 176 took 57 ms instead of 28, against 0 with the millisecond poll. A judder, introduced deliberately. Not kept. |
  | x11vnc at its own `-wait 20 -defer 20` instead of the 5 ms 1.10.21 set | **Much worse.** Median gap between painted frames 44 ms against 33, p90 65 against 34, and 1471 frames delivered in 60 seconds against 2054. The aggressive polling is what delivers 35 a second. Kept. |

  The second is worth stating plainly: 1.10.21 set those flags on the strength
  of a latency figure that turned out to be wrong, and 1.10.22 said as much.
  They are right anyway, for a different reason than the one given.

## [1.10.25] — 2026-09-13

### Fixed
- **The engine was spinning on a core waiting for the next tic, and starving
  everything that had to get the picture out.** `TryRunTics` has a loop that
  waits for the tic it is about to run, and in the 1997 sources that loop has
  no sleep in it: between one tic and the next — five sixths of the time, at 35
  tics a second — the engine asked the clock whether it was time yet as fast as
  the machine could manage. Measured here at **97% of a core** to draw a
  320×168 view.

  On a 1997 machine that was free, and getting to the tic the instant it
  arrived was the point. In this container the same box is also running an X
  server, a VNC server reading that server's framebuffer, a websocket proxy in
  Python, a mixer and a synth. A millisecond sleep in that loop is 35 times
  finer than the tic being waited for, so nothing about the timing changes:

  | | engine CPU, playing |
  | --- | --- |
  | before | 96.5% of a core |
  | after | 5% of a core |

  What that costs when the CPU is contended, measured in the browser by
  watching the canvas actually change, 70 seconds of walking and turning with
  the container held to one core:

  | | frames painted | median gap | p90 | p99 |
  | --- | --- | --- | --- | --- |
  | before | 1915, 1953 | 33 ms | **67 ms** | **84 ms** |
  | after | 2441, 2449 | 33 ms | **34 ms** | **51 ms** |

  The old build drops a quarter of its frames — 27 a second reaching the
  browser instead of 35 — and the ones that arrive come unevenly. Two runs each,
  the figures repeat. Keypress-to-picture is unchanged at 156 ms and
  sound-behind-picture at 14 ms, so none of this was buying responsiveness.

  The melt between screens had the same spin in its own inner loop, costing a
  second of a core at every level start. Same fix.

### Added
- **The engine says when its frames run late, and when it is using more CPU
  than drawing one takes.** A line every five seconds if either is true and
  nothing when neither is, so a quiet log is a clean one — the same bargain
  audiostream's underrun reporting makes. It names where the time went, split
  between waiting for the tic, drawing, and handing the finished picture to the
  X server:

  ```
  frames: 167 in the last 5s, 2 over 50 ms, 4% of a core; worst 95 ms
      (tics 28, draw 67, of which handing over the picture 67)
  ```

  The screen melt is excluded, being slow on purpose. A spinning engine is
  invisible in a frame count — the picture keeps arriving, just not on time —
  which is why the CPU figure is in there. Diagnosing the fault above took
  per-process CPU sampling inside a container; it should take reading the log.

## [1.10.24] — 2026-09-12

### Fixed
- **The engine was re-reading the WAD during play, which is what a pause when
  a door opens or a new monster appears actually is.** The zone allocator is
  the cache for every wall texture, every sprite and every level structure in
  play, and it was being given **two megabytes**. `i_system.c` initialises
  `mb_used` to 6, but `M_LoadDefaults` runs before `Z_Init` and overwrites it
  from the config table, where the default was 2 — and every config file this
  container has ever written says `mb_used 2`.

  Two megabytes does not hold a level's textures and the sprites of the things
  standing in it, so the zone spends the game purging what it is about to want
  again and reading it back. Measured on the shareware attract demos, counting
  every lump that had to be re-read after startup, over the same three minutes
  of the same demo each time:

  | zone heap | lumps re-read | bytes |
  | --- | --- | --- |
  | 1 MB | 1278 | 3094 KB |
  | **2 MB (what shipped)** | **230** | **825 KB** |
  | 16 MB | 41 | 320 KB |
  | **32 MB (now)** | **33** | **288 KB** |

  What is left at 32 MB is first-time loads and nothing more. And that is the
  smallest WAD and the smallest levels in existence — a retail IWAD has larger
  levels and many more monsters to hold sprites for, so the same 2 MB has more
  to thrash over.

  Those re-reads land exactly where the pause is reported: a door revealing a
  texture nothing was using, or a monster coming into view for the first time.
  `W_ReadLump` is a blocking read on the thread that draws the frame, and the
  IWAD is a symlink to whatever was mounted — on a NAS that is a share on the
  array, possibly behind a FUSE layer, possibly a disk that has spun down. The
  sound keeps playing while the picture waits, because the mixer is a separate
  process.

  The new value is a floor rather than a setting: a config file asking for
  less is a 1997 answer to a problem nobody has, so it is raised. Asking for
  more still works. The engine's resident memory went up by under a megabyte
  in three minutes of play — the zone is allocated, not touched.

  Measured on this machine the re-reads were too cheap to produce a visible
  stall, so the pause itself is not reproduced here; what is measured is that
  the reads are gone.

## [1.10.23] — 2026-09-12

### Changed
- **The sound effects are mixed where they are sent, and the separate sound
  server is gone from this path.** It was the last piece of the 1997
  arrangement still in the way: the engine started a child process, the child
  mixed 512 samples — 46 ms — at a time and wrote the result down a pipe, and
  `audiostream` read that pipe a period at a time and sent it on. A shot
  therefore waited half a block to be mixed at all and then waited its turn in
  a queue, all of it behind sound that had already been decided.

  `audiostream` now links the same mixer — `sndserv/soundsrv.c` compiled with
  `-DSNDMIX_LIB`, which takes its `main()` out and leaves `grabdata`,
  `initdata`, `addsfx` and `mix` — and calls it once per 12 ms period, and the
  engine sends the same `'p'` commands it always sent to a unix socket instead
  of to a child's standard input. Nothing about the mixing changed; what
  changed is who calls it and how often.

  Measured in the browser off the audio clock, twenty shots a run, music
  muted so the measurement starts from silence:

  | | keypress → picture | keypress → sound | sound behind picture |
  | --- | --- | --- | --- |
  | 1.10.22 | 155 ms | 259 ms | **102 ms** |
  | this release | 154 ms | 169 ms | **14 ms** |

  Repeated: 107 ms against 10 ms. The sound is now within a frame of the
  picture, which is where it was always supposed to be.

  This also explains 1.10.22's useful negative — shrinking the transport from
  93 ms to 23 ms changed nothing measurable. The queue was never the stage
  that mattered. The separate process was: a mixer that decides 46 ms at a
  time and runs ahead of the reader is 90 ms of delay no matter how short the
  pipe between them is.

- The sound server itself is unchanged and still built, for the one case that
  still uses it: a container sharing the host's PulseAudio socket. Its
  pipe-writing backend (`SNDBACKEND=stream`, added in 1.10.15) has no caller
  left and is removed.

- **The browser now asks for 25 ms of sound in hand rather than 40.** On a
  machine that can sustain it that is 15 ms less delay between a shot and
  hearing it; on one that cannot, the buffer grows to what that machine needs
  as it already did, so there is nothing to lose by asking for less first.

### Added
- **audiostream says when a producer could not keep up.** A period short of
  sound is padded with silence, and silence is exactly what an absent sound
  already sounds like — the one fault in the audio path that cannot be heard.
  It now prints a line when it happens, and nothing when it does not, so a
  quiet log means a clean one.

## [1.10.22] — 2026-09-12

### Fixed
- **The "140 ms video delay" in 1.10.21's notes was wrong, and is corrected.**
  It was measured from a keypress to the muzzle flash, and the pistol spends
  four tics in `S_PISTOL1` doing nothing before `A_FirePistol` runs — 114 ms
  of deliberate weapon animation, counted as delay. Timed against the menu
  key, which the game acts on the moment it arrives, the picture answers in
  **48 ms**: about one of the engine's 35 frames a second plus a browser
  repaint, which is the floor rather than a fault.

  The replacement video path those notes proposed was then built — the
  engine's frames pushed down the WebSocket beside the sound, x11vnc reduced
  to carrying the keyboard and mouse, the palette applied in the browser. It
  works and it is exactly as fast, 48 ms either way, while sending 1382 KB/s
  against VNC's 941 when walking. It is on the
  `claude/video-stream-experiment` branch and is deliberately not merged.

  What is left that is real: the sound arrives about 150 ms after the picture
  it belongs to.

## [1.10.21] — 2026-09-12

### Fixed
- **`sha-<short>` image tags pointed at whichever build finished last.** The
  same commit is built twice — once when a branch is pushed, once when it is
  merged — and since 1.10.17 gave images a version stamp those two builds
  differ, so both writing the same tag left it ambiguous. Only default-branch
  builds write it now; a branch build is still reachable by its branch name,
  and PR builds get a `pr-<n>` tag so they are never left with none.

### Changed
- x11vnc no longer naps, and polls and defers at 5 ms rather than 20. Its
  defaults are meant for a desktop nobody is waiting on — `-nap` deliberately
  slows polling when activity is low, which is the state of a DOOM screen in
  the instant before you press fire. `DOOM_VNC_WAIT`, `DOOM_VNC_DEFER` and
  `DOOM_VNC_ARGS` expose these.

  **It made no measurable difference**, and is kept only because the defaults
  are wrong in principle for something being played. See below.

### Known

- **The picture is about 140 ms behind the keypress, and this release does not
  fix it.** That is separate from the sound, which is a further 150 ms and did
  come down. Measured from the browser dispatching the keydown to the pixels
  arriving back:

  | Suspected | Measured |
  | --- | --- |
  | The browser drawing | 11 ms of the total |
  | x11vnc `-nap`/`-wait`/`-defer`, 20 → 5 → 1/0 | no change outside noise |
  | x11vnc `-threads` | no change |
  | `-8to24 poll` | no change |
  | Pixel volume (`DOOM_SCALE=1`, a quarter) | 139 vs 143 ms |
  | The audio sharing the proxy | 143 with, 145 without |
  | Throughput | 35 frames a second, arriving in pairs 2 ms apart |

  Nothing is starved and no knob touches it. What remains is structural: VNC
  is request/response, so a frame goes out only once the client has asked for
  the next, which it does after decoding the last; the engine samples input
  and draws once a tic, 28 ms apart; and all of it crosses websockify twice.

  Getting it materially lower means not using VNC for the picture — pushing
  the engine's frames down the WebSocket that already carries the sound, with
  the palette applied in the browser. That replaces the video path rather than
  tuning it, and has not been done.

## [1.10.20] — 2026-09-12

### Fixed
- **The sound got later the longer you played.** 1.10.19 made the browser's
  buffer adaptive but gave it no way to give anything back except a hard
  ceiling far above, so every block the browser was too busy to fetch left
  more sound sitting in it — permanently. Measured over 55 seconds of play,
  the delay walked from 35 ms up to 195 and kept going. Reported from the
  field at 80–85 ms and climbing, which is what sent me looking.

  Two clocks are involved and nothing makes them agree: the container sends
  on its own schedule and the browser plays on its sound card's. So playback
  is now nudged instead — a fraction fast while there is too much in hand, a
  fraction slow while there is too little, never more than two per cent. That
  is inaudible on gunfire and door mechanisms, and far less audible than
  dropping the frames outright, which clicks. It corrects about 20 ms a
  second, comfortably faster than the drift arrives.

  Over the same 55 seconds it now holds a median of 38 ms and never passes 80,
  with no audible gap. The same measurement on 1.10.19 gave a median of 93 and
  no upper bound at all.

- **The buffer's target could sit still while underrunning for ever.** Growth
  on an underrun and decay over time were set to exactly cancel for a machine
  that stumbles every ten seconds. Decay is now half as fast, so the target
  climbs until the underruns stop and only then drifts back down to whatever
  that machine actually needs. Same run: five underruns before, two after.

  When it has grown, the start screen says so and why.

## [1.10.19] — 2026-09-12

### Changed
- **The sound is about 220 ms quicker off the mark.** Measured from the
  keypress to the first sample of the gunshot leaving the browser: 542 ms
  before, 324 ms now. Of what is left, 168 ms is the picture — the muzzle
  flash arrives that long after the key too, which is VNC and the input path,
  not the audio — so the sound now sits about 146 ms behind what you see,
  where it was well over twice that.

  Four things, found by measuring each stage rather than guessing at them:

  - **The music pipe was four times the size it was meant to be**, holding
    371 ms instead of the 93 ms the comment beside it claimed. An arithmetic
    slip: it was sized in frames where the rate was in bytes.
  - **The mixer's period was halved**, 512 frames to 256, which is 23 ms to
    12 ms of granularity for everything downstream.
  - **The browser's buffer is adaptive now** instead of a fixed 140 ms guess.
    It starts at 40 ms and asks for more only when it actually runs dry,
    giving the cushion back after eight quiet seconds. How much a machine
    needs depends on the machine, the browser and what else the page is
    doing, none of which can be known from here — a fixed figure is either
    too much for everyone or too little for somebody.
  - **The fallback's own buffer came down** from 4096 frames to 1024. A
    `ScriptProcessorNode` is handed its work roughly two buffers ahead of
    when it is heard, so that alone was about 190 ms.

  Verified over 32 seconds of walking, shooting and menus on both paths: no
  dropouts, no underruns, and the buffer never had to grow past its starting
  40 ms.

  The start screen now shows how much sound is being held, which is the same
  thing as how far behind it is.

## [1.10.18] — 2026-09-12

### Fixed
- **Sound never worked anywhere but localhost.** 1.10.15 played it through an
  `AudioWorklet`, which browsers expose only in a *secure context* — HTTPS, or
  localhost. This page is normally served over plain HTTP from a machine on
  the network, where `ctx.audioWorklet` is simply absent, and asking it to
  load the worklet threw. Which is every real installation of this container:
  the one arrangement where it did work was the one every test used.

  What made it hard to see from either end is that `AudioWorkletNode` *is*
  defined in an insecure context even though `ctx.audioWorklet` is not, so the
  support check passed and the failure came later — and until 1.10.17 it came
  silently, leaving a container whose log was perfectly healthy because
  nothing in the container was wrong.

  Where there is no worklet the sound now goes through a `ScriptProcessorNode`
  instead: deprecated for years, implemented everywhere, and needing no secure
  context. Both feed the same ring buffer, in `doom-ring.js`, which the page
  and the worklet load from the one file so they cannot drift apart. The
  fallback keeps a little more sound in hand, since it runs on the main thread
  alongside noVNC's decoding. Measured over 37 seconds of play on an insecure
  origin: no dropouts, and audio arriving at 22045 Hz against a nominal 22050.

  The start screen says which of the two is in use.
- **Branch builds failed on the version stamp 1.10.17 added.** It was
  substituted with `sed s/…/…/`, and the stamp for a non-release build carries
  the branch name — which in this repository contains a slash, ending the
  substitution early. Released images were never affected, since a release
  stamp is just the version, but every other build broke.

  The substitution is a literal replace now rather than a regular expression,
  and the value is narrowed to characters that cannot escape the HTML text or
  the JavaScript string it lands in. The branch name is gone from the stamp as
  well: it identified nothing the commit did not.

## [1.10.17] — 2026-09-12

### Added
- **The page now says what the sound is doing, and which build it is.** Two
  releases running, "no sound" has been impossible to tell apart from "sound
  arriving and inaudible" from the outside: the container's log is identical
  in both cases, because everything that decides whether sound reaches you
  happens in the browser. The start screen now carries two lines — the sound's
  live state (`Sound: on — 22050 Hz in, 48000 Hz out, 12.4 s received`, or the
  reason there is none) and the build the page itself came from.

  The build stamp is printed at container startup too, as `DOOM <version>`.
  The two disagreeing is a complete diagnosis on its own: it means the browser
  is running an older client than the container it is talking to, which
  nothing in the container's log can reveal, because nothing in the container
  is wrong.

## [1.10.16] — 2026-09-12

### Fixed
- **1.10.15's sound never reached some browsers, and said nothing about it.**
  The container was mixing audio and serving it correctly; the page asking for
  it was the one from the release before, still in the browser cache.
  websockify serves the client with Python's `SimpleHTTPRequestHandler`, which
  sends `Last-Modified` and nothing else — no `Cache-Control`, no `ETag`. Given
  no freshness, a browser is entitled to guess one and reuse what it has
  without asking, so a tab could go on running a client from two releases back
  while the image underneath it was current. Nothing in the container's log
  looks wrong in that state, because nothing in the container is wrong.

  The client is now served `Cache-Control: no-cache`, which means ask first,
  not do not store: the ordinary answer is a 304 and the same traffic as
  before. This applies to every future release, not just this one — a hard
  reload is no longer needed after an update.

### Changed
- **Sound that does not work now says why.** It used to fail silently in every
  case — no Web Audio, a worklet the browser would not load, a refused
  connection, a container sending nothing — which left no way to tell a silent
  game from a broken one. Each of those now puts a line at the bottom of the
  page saying which it was. The audio watchdog also catches the case where the
  connection opens and nothing ever comes down it.
- The mouse-capture and sound messages no longer overwrite each other; they
  have a slot each.
- The audio worklet is served as a file rather than built from a `blob:` URL.
  Both are legal and the blob worked everywhere it was tried, but a worklet
  that fails to load takes the sound with it, and a plain URL has fewer ways
  to be refused.

## [1.10.15] — 2026-09-12

### Added
- **Sound, in the browser.** The game has had effects and music since 1.10.0,
  but only a container sharing the host's PulseAudio socket could play them —
  which is no use at all when the container is on a server in another room and
  you are looking at it through a browser. Now the audio arrives on the page,
  over the port that was already published. Nothing to install, nothing to
  mount, no flags: click to play and it plays.

  There was no shortcut to it. The container has no sound card and no sound
  daemon, and cannot practically be given one — the only packaged PulseAudio
  brings systemd, GStreamer, cairo and a set of video codecs with it, 172
  packages, to do a job that is adding two streams together. VNC was no help
  either; it carries a picture and nothing else.

  So the sound server gained a third output that writes raw PCM to a pipe, the
  engine learned to render music into another one, and a new `audiostream`
  mixes the two on a real-time schedule and sends 16-bit stereo to the page,
  where an `AudioWorklet` plays it through a ring buffer that absorbs the
  difference between the container's clock and your sound card's. Reading on
  that schedule is also what paces the game's audio, the same job a blocking
  write to `/dev/dsp` did in 1997.

  About 700 kbit/s at the default 22050 Hz; `DOOM_AUDIO_RATE` takes `11025` or
  `44100` instead. The image is no larger. Sharing the host's PulseAudio
  socket still works exactly as before and still takes precedence — set
  `PULSE_SERVER` and the sound goes there rather than to the browser. Stock
  `/vnc.html` is unchanged, and silent, as it always was.

## [1.10.14] — 2026-09-12

### Changed
- **The backquote key (`` ` ``) now opens the menu**, everywhere, out of the
  box. Escape still does whenever the browser lets it through — this only adds
  a key — but in a window, and on Firefox and Safari in any mode, the browser
  keeps Escape for releasing the pointer and it never reaches the game. The
  binding was already there under Options → Setup → Controls; it just
  defaulted to Escape, which made it do nothing until somebody found it. A
  config written by an older build is promoted on load, so this needs no
  editing.
- **Every menu is drawn at one size.** SETUP was the odd one out under
  Options: the original menus are built from graphic lumps 15 pixels tall, and
  there is no lump for a word id never shipped, so the added item was drawn in
  the small font next to nine large ones. Rather than leave the mismatch, the
  five original menus are now text too, and every menu moved to 13-pixel rows
  from 16. Everything matches, and the added pages have room for the options
  they needed.

## [1.10.13] — 2026-09-12

### Added
- **Play in this window**, alongside **Play fullscreen**, on the page you land
  on. Both capture the mouse; fullscreen is no longer the price of playing.
  Its one remaining advantage is that Keyboard Lock only works there, so only
  fullscreen can keep Escape for the game's menu — in the window Escape always
  releases the mouse, which no page can change.
- Once the capture is released, clicking the picture resumes it the way you
  started, so Escape then click cannot drop you into fullscreen unexpectedly.

## [1.10.12] — 2026-09-12

### Fixed
- **Pressing "Detail" in Options broke the game on the next launch.** Low
  detail does not work in this port — the 1997 source says so itself and the
  call that would apply it is commented out — but the menu item still flipped
  the setting, and the flipped value was written to `.doomrc`. The next start
  applied it for real, selecting a drawing routine that range-checks the
  column against the full screen width and *then* doubles it, so it indexes
  past the end of its column table and writes wherever that lands: garbled
  rendering, then a segfault, then a restart loop.

  The menu no longer changes the setting and says why, the renderer refuses
  the mode whatever a config file asks for, and a value left behind by an
  older build is corrected on load — so an already-broken config fixes itself
  with nothing to edit. The range check is corrected too, so the routine is
  not a trap if it is ever revived.

## [1.10.11] — 2026-09-12

### Changed
- The renderer's range checks no longer end the game. They are 1997 debug
  asserts that call `I_Error`, so one bad frame became a crash loop in a
  container that restarts. The offending column or span is skipped instead —
  nothing is written outside the framebuffer either way — and it is reported,
  up to eight times, with the view geometry that caused it.
- Startup logs the view size: `R_SetViewSize: blocks 11, detail 0 -> view
  320x200`. `blocks` above 11 is what breaks the renderer, so this line says
  immediately whether that is the problem — and whether a build with the
  clamp is running, since a clamped one can never print more than 11.

## [1.10.10] — 2026-09-12

### Fixed
- **A bad `screenblocks` in `.doomrc` broke the renderer**, with errors like
  `R_DrawColumn: 206 to 219 at 178` and, past 12, a segfault on the first
  frame — and since the container restarts, a crash loop. The view height is
  derived straight from that number and anything above 11 makes the view
  taller than the 200-line screen, so the renderer draws off the end of the
  framebuffer. The 1997 code trusted the value, which is fine for the menu
  that produces it and not for a text file. It is clamped now, both where the
  view size is set and on load, so a bad value is corrected and saved back
  rather than carried around.

## [1.10.9] — 2026-09-12

### Fixed
- **The mouse no longer walks you forward and back.** The original used its Y
  axis for movement, because there was nothing to aim at vertically; with the
  same hand turning you, it mostly moves you about by accident. Turning is
  unchanged. **Options → Setup → Mouse → MOVE WITH MOUSE** puts the original
  behaviour back, and the choice is saved as `novert` in `.doomrc`.
- `PUID=0` looped forever instead of starting. The entrypoint re-executes
  itself to drop privileges whenever it is root, and dropping to root left it
  root, so it did it again. It now stays root and says so.

## [1.10.8] — 2026-09-12

### Fixed
- **Holding a mouse button did not repeat the action, and releasing it did not
  always stop.** A 1997 bug: `Button1Mask` is `1<<8`, and the engine used it
  raw where buttons two and three are correctly turned into bits 1 and 2. So a
  release reported `256 ^ 1` = 257, whose bit 0 is still set — the game never
  saw the button come up and kept firing — while any mouse movement made while
  the button was held reported 256, whose bit 0 is clear, switching fire back
  off. Holding still stuck; holding and moving stopped. Both now report 1.

## [1.10.7] — 2026-09-12

### Fixed
- **Mouse look still jittered, and this is the cause the last two releases
  missed.** The page reported the pointer as *centre plus the movement*, so a
  steady drag named the same coordinates over and over — and x11vnc discards a
  pointer report that does not move the pointer. Most of the movement never
  reached X at all. The page walks the pointer around the screen now and puts
  it back in the middle only when it would run off the edge, which the engine
  ignores for movement. Measured end to end through a real browser, pointer
  lock and x11vnc, over an identical drag: 1.1° before, 83.3° now, the same in
  both directions, and no drift at all with the mouse still.
- **The picture did not fill the window.** noVNC writes its own width and
  height onto the canvas whenever the framebuffer size arrives, which replaced
  the page's sizing; the scale is applied with `!important` now, and the page
  watches for the canvas being resized rather than measuring it once.
- **The mouse capture could silently fail to start.** The click asked for
  fullscreen and awaited it before requesting pointer lock, by which time the
  click's user activation was spent and the browser refused — leaving an
  ordinary remote-desktop pointer with the engine recentring underneath it.
  Both requests go out inside the click now, and the page says so if the
  capture does not take.

### Changed
- `grab_mouse` is off by default again. The engine's own recentring is for
  running on a real X display; through VNC the page handles it, and the two
  fought.

## [1.10.6] — 2026-09-12

### Fixed
- **Mouse look still jittered**, and this was the real cause rather than the
  one fixed in 1.10.5. `G_Responder` *assigned* each mouse event's movement to
  `mousex` instead of adding it, so only the last event of each 35 Hz tic
  counted and every earlier one was discarded. Any mouse reports faster than
  that — all of them, and a pointer-locked browser sends one event per report —
  so most of the movement never reached the game. Measured on the player's
  facing over an identical drag: 1.1° in 1.10.4, 22.3° in 1.10.5, 147.7° now.
- **Mouse buttons could stick on.** Losing the capture with a button held —
  pressing Esc, switching window — left the game holding it down for ever,
  which in DOOM means firing for ever. The page now releases every button when
  the capture, the focus or the tab goes away. Button changes are also sent on
  their own and immediately, rather than riding along on a movement report that
  could be queued behind a frame's worth of motion.
- **Fullscreen did not fill the screen.** The picture was scaled by whole
  numbers only, so a 640×400 render could manage just 3× on a 2560×1440
  display and left a wide border. It is scaled to fit the window now, by the
  same factor on both axes so the shape is kept.

### Changed
- The page sends one pointer report per animation frame with the movement
  since the last one, instead of one per mouse event. A gaming mouse reporting
  several hundred times a second was flooding the VNC link.

## [1.10.5] — 2026-09-12

### Fixed
- **Mouse look jittered instead of turning.** With the pointer captured the
  browser reports several positions inside one 28 ms tic, and only the first
  was measured from the centre of the screen: the rest were measured against
  the previous report, so a steady drag became a run of near-cancelling
  deltas. The engine re-centres the pointer after each motion but was waiting
  for the warp's own event to say so, and that event is queued behind
  everything already received. It now records the centre at the moment it
  warps. Measured on the player's facing over an identical drag: 1.1 degrees
  before, 22.7 after.

## [1.10.4] — 2026-09-12

### Fixed
- **Escape belongs to DOOM again.** Capturing the mouse meant the browser
  claimed Escape to cancel the capture, which is precisely the key the game
  wants for its menu. `play.html` now goes fullscreen and takes a Keyboard Lock
  on Escape, so it reaches the game; holding Escape is still the way out, and
  no page can take that away.

### Added
- A second, bindable key for the menu — **Options → Setup → Controls → MENU**,
  saved with the rest of your controls. Keyboard Lock is Chromium-only, so on
  Firefox and Safari Escape still ends the capture; binding the menu to
  something else (backquote is unused by DOOM) avoids the problem entirely.

## [1.10.3] — 2026-09-12

### Fixed
- **The Ultimate Doom was being played as plain registered Doom**, with *Thy
  Flesh Consumed* sitting in the WAD where the menu could not reach it. The
  1997 code identifies the game from the IWAD's filename and expected the four
  episode version to be called `doomu.wad`; Steam, GOG and every other
  re-release install it as `DOOM.WAD`. The engine now looks for E4M1 inside the
  IWAD and identifies it from that, so the banner, the episode menu and the
  fourth episode all agree.
- **A directory holding both games always started Doom II.** That is the
  engine's own search order. The container now picks the IWAD itself, prefers
  Doom over Doom II, and names it on the command line rather than leaving it to
  the search. `DOOM_IWAD` chooses directly.
- **"Demo is from a different game version!" on repeat, and no attract loop.**
  Every released IWAD's demos were recorded by 1.9 and this engine announces
  itself as 1.10, so it rejected all of them and sat on the title screen. The
  1.10 source is 1.9's game code with the version bumped, so the demos play
  back correctly; they are accepted now.
- **"S_StartSoundAtVolume: 16bit and not pre-cached - wtf?" for every sound.**
  The guard around that warning is spelled `SNDSRV`; every other file, and the
  build, spells it `SNDSERV`. A one-character typo in the 1997 source meant the
  branch never compiled out.
- The bare URL now serves the client that captures the mouse instead of a
  directory listing, so reaching the plain viewer by accident is harder.
- A WAD the game cannot read — owned by someone else, or not world-readable —
  now says so, and which user it tried as, instead of failing obscurely.

## [1.10.2] — 2026-09-11

### Added
- The shareware IWAD ships in the image, so the container plays with nothing
  mounted — `docker run -p 6080:6080 ghcr.io/krippler/doom` is now the whole
  of it. Episode 1, nine levels, id's file under their shareware terms. A
  mounted IWAD still takes precedence, the entrypoint verifies the bundled
  file's checksum before using it, and commercial game data is still yours to
  supply. Adds 6 MB to the image.
- `play.html`, a browser client that captures the mouse. noVNC is a remote
  desktop client and reports where the pointer *is*; a game needs to know how
  far it *moved*. This page locks the pointer instead and sends movement, so
  turning never runs out of screen and the cursor cannot slide off into the
  rest of your desktop. It is what the WebUI and the printed URL now open;
  plain noVNC is still served at `/vnc.html`.

### Fixed
- Mouse look stopped partway through a turn. The pointer was only re-centred
  while the grab was on, and the grab was off by default, so the pointer
  walked to an edge of the screen and stayed there — measured after one sweep
  it sat at x=639 of 640. It is now re-centred after every motion the engine
  acts on, and the grab defaults on.
- The game never claimed the X input focus. With no window manager in the
  container, X leaves the focus at `PointerRoot`, which delivers keystrokes to
  whichever window the pointer is over, so the keyboard depended on where the
  pointer had drifted to.

### Changed
- Added a README. GitHub was showing `README.TXT` as the landing page, which
  is id Software's note from the 1997 source drop: accurate about the code it
  shipped with, and silent on the port, the container, the sound and music, or
  how to run any of it. `README.TXT` is left as it was, as the historical
  document it is.
- The Unraid template and Community Applications profile mention the in-game
  Setup menus, and the template's change list covers 1.10.1.
- `DOCKER.md` leads with pulling the published image rather than building one.

## [1.10.1] — 2026-09-11

### Added
- In-game **Options → Setup**, with three pages the 1997 release had no
  equivalent of:
  - **Controls** rebinds the ten movement and action keys. Bindings are
    written to `.doomrc` and survive a restart.
  - **Mouse** turns the mouse on and off, assigns its buttons, and toggles a
    pointer grab that confines the pointer to the window while playing.
  - **Load WAD** lists the `.wad` files in the mounted WAD directory, marks
    each one `GAME` or `MOD` by reading its signature, and loads the one you
    pick. Nothing about the loaded WADs can change while the engine runs, so
    this restarts it — a couple of seconds back to the title screen.
- `-iwad <file>` on the command line, to name the game data outright rather
  than take whichever of seven fixed filenames turns up first.

### Fixed
- The browser stretched the picture. noVNC was told to scale its canvas to the
  window; the engine renders a fixed 320×200. The link in the docs now passes
  `resize=off`, and `DOOM_SCALE` is the way to get a bigger picture — 3 and 4
  render 960×600 and 1280×800 natively, which beats upscaling.
- The mouse did not work at all. The window's event mask had every pointer
  event commented out, and the filter meant to drop the engine's own
  re-centring warp threw away any motion that merely shared a row or column
  with the centre.
- `I_ShutdownGraphics` detached a shared memory segment it had never attached
  when the MIT-SHM extension was missing, and dereferenced a null `Display*`
  when startup failed before graphics came up, turning a clean error message
  into a segfault.
- The engine wrote a NUL into its own `DISPLAY` environment variable while
  checking whether the X connection was local.

## [1.10.0] — 2026-09-11

### Added
- Sound effects, through the sound server that shipped with the original
  release, given a PulseAudio backend in place of the `/dev/dsp` one.
- Music. Every music function in the 1997 sources was an empty stub; the WAD's
  MUS lumps are now converted to Standard MIDI and rendered by FluidSynth.
- A container that builds the engine and serves it over noVNC on an 8-bit
  Xvfb, which is the only kind of display the engine supports.
- An Unraid Community Applications template, with PUID/PGID support.

### Fixed
- The 1997 sources did not compile, and did not run once they did. A one-byte
  heap overflow in the IWAD path search, a WAD structure whose layout shifted
  under 64-bit pointers, pointer arrays allocated at half their size, a 4x
  scaler that assumed big-endian and mirrored every pixel pair, undefined
  behaviour in the input queue, and a sound path that called `exit(-1)` on any
  system without OSS. `PORTING-NOTES.md` has the full list.
- Sound effects played at about an eighth of their intended volume: the game
  passes a 0..15 volume to mixers that index a 0..127 table.
