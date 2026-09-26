#!/usr/bin/env python3
#
# The browser's side of the smoke test, and its judge.
#
# tools/smoke-test.sh starts Xvfb, the mixer and the engine; this plays the part
# of play.html against them -- a controller going in, vibration and sound coming
# out -- and reads the picture straight out of Xvfb's framebuffer file (-fbdir),
# so nothing between the engine and the check can be the thing that passed.
#
#   smoke-client.py level   --fb FILE --pad PORT --audio PORT --out DIR
#   smoke-client.py music   --audio PORT
#   smoke-client.py compare --fb FILE --wad WAD --src v_video.c --out DIR
#
# Prints "[smoke] ..." as it goes and exits non-zero at the first thing that
# is wrong, saying what.
#

import argparse
import os
import socket
import struct
import sys
import threading
import time
import zlib


def say(msg):
    print('[smoke] ' + msg, flush=True)


def die(msg):
    print('[smoke] FAILED: ' + msg, file=sys.stderr, flush=True)
    sys.exit(1)


# -------------------------------------------------------------------- pictures
#
# Xvfb -fbdir keeps the screen in an XWD file: a header, a colour table, and
# the pixels. At depth 24 they are 32-bit little-endian BGRX; at depth 8 they
# are palette indices, and the colour table in the file is the screen's
# default colormap rather than the game's own, so it is no use -- compare()
# maps the indices through the WAD's PLAYPAL instead.
#

SCALE = 2                       # the engine runs at -2: 640x400
STATUSBAR_Y = 168 * SCALE       # the status bar's top edge
FACE = (143 * SCALE, 178 * SCALE)   # the face looks about; leave it out


def read_fb(path):
    d = open(path, 'rb').read()
    hdr = struct.unpack('>25I', d[:100])
    header_size, depth, w, h = hdr[0], hdr[3], hdr[4], hdr[5]
    bpp, bpl, ncolors = hdr[11], hdr[12], hdr[19]
    off = header_size + ncolors * 12
    rows = [d[off + y * bpl: off + y * bpl + w * bpp // 8] for y in range(h)]
    return w, h, depth, bpp, rows


def pixel(fb, x, y):
    w, h, depth, bpp, rows = fb
    if bpp == 32:
        r = rows[y]
        return (r[x * 4 + 2], r[x * 4 + 1], r[x * 4])
    return rows[y][x]


def region(fb, y0, y1, skip=None):
    w = fb[0]
    out = []
    for y in range(y0, y1):
        for x in range(w):
            if skip and skip[0] <= x < skip[1]:
                continue
            out.append(pixel(fb, x, y))
    return out


def changed(a, b):
    return sum(1 for p, q in zip(a, b) if p != q) / max(1, len(a))


def save_png(fb, path):
    w, h = fb[0], fb[1]
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            p = pixel(fb, x, y)
            raw.extend(p if isinstance(p, tuple) else (p, p, p))

    def chunk(t, b):
        return (struct.pack('>I', len(b)) + t + b
                + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff))

    open(path, 'wb').write(
        b'\x89PNG\r\n\x1a\n'
        + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
        + chunk(b'IDAT', zlib.compress(bytes(raw)))
        + chunk(b'IEND', b''))


def wait_for_level(path, timeout=40):
    # The level is up once the status bar has been drawn and holds still: the
    # screen melts in from the title first, and a check made during the melt
    # would be of half of each.
    end = time.time() + timeout
    last = None
    while time.time() < end:
        try:
            fb = read_fb(path)
        except (OSError, struct.error):
            time.sleep(0.2)
            continue
        bar = region(fb, STATUSBAR_Y, fb[1], FACE)
        if len(set(bar)) > 10 and bar == last:
            return fb
        last = bar
        time.sleep(0.5)
    die('the level never appeared: no steady status bar in %d seconds' % timeout)


def gamma0(source):
    # The engine does not show PLAYPAL as it is: every colour goes through
    # gammatable[usegamma], and even level 0 is not quite the identity -- it
    # starts 1, 2, 3, so most channels come out one brighter. The table is
    # read out of the source rather than copied here, so the two cannot
    # drift apart.
    import re
    text = open(source).read()
    start = text.index('gammatable[5][256]')
    nums = re.findall(r'\d+', text[text.index('{', start):])
    return [int(n) for n in nums[:256]]


def playpal(wad):
    d = open(wad, 'rb').read()
    n, diroff = struct.unpack('<II', d[4:12])
    for i in range(n):
        pos, size, name = struct.unpack('<II8s', d[diroff + i * 16: diroff + i * 16 + 16])
        if name.rstrip(b'\0') == b'PLAYPAL':
            pal = d[pos:pos + 768]
            return [tuple(pal[j * 3:j * 3 + 3]) for j in range(256)]
    die('no PLAYPAL in ' + wad)


# ------------------------------------------------------------------ controller
#
# The same 16-byte state message doom-gamepad.js sends, and the 7-byte
# vibration message the engine sends back (linuxdoom-1.10/i_pad.c).
#

BUTTONS = 'A B X Y LB RB LT RT VIEW START LS RS UP DOWN LEFT RIGHT GUIDE'.split()


class Pad:
    def __init__(self, port):
        end = time.time() + 10
        while True:
            try:
                self.s = socket.create_connection(('127.0.0.1', port), timeout=5)
                break
            except OSError:
                if time.time() > end:
                    die('nothing listening for the controller on port %d' % port)
                time.sleep(0.2)
        self.rumbles = []
        self.buttons = 0
        self.axes = [0.0, 0.0, 0.0, 0.0]
        self.trig = [0, 0]
        threading.Thread(target=self._read, daemon=True).start()
        name = b'Smoke Test Pad'
        self.s.sendall(b'N' + bytes([len(name)]) + name)
        self.hold(0.3)

    def _read(self):
        buf = b''
        while True:
            try:
                d = self.s.recv(256)
            except OSError:
                return
            if not d:
                return
            buf += d
            while len(buf) >= 7:
                if buf[0:1] != b'R':
                    buf = b''
                    break
                self.rumbles.append(struct.unpack('<HHH', buf[1:7]))
                buf = buf[7:]

    def _send(self):
        self.s.sendall(b'P' + bytes([1])
                       + struct.pack('<Ihhhh', self.buttons,
                                     *(int(a * 32767) for a in self.axes))
                       + bytes(self.trig))

    def hold(self, secs):
        end = time.time() + secs
        while True:
            self._send()
            if time.time() >= end:
                return
            time.sleep(1 / 60)

    def press(self, name, secs=0.15):
        i = BUTTONS.index(name)
        if name in ('LT', 'RT'):
            self.trig[i - 6] = 255
        else:
            self.buttons |= 1 << i
        self.hold(secs)
        if name in ('LT', 'RT'):
            self.trig[i - 6] = 0
        else:
            self.buttons &= ~(1 << i)
        self.hold(0.2)

    def stick(self, lx, ly, rx, ry, secs):
        self.axes = [lx, ly, rx, ry]
        self.hold(secs)
        self.axes = [0.0, 0.0, 0.0, 0.0]
        self.hold(0.2)


# ----------------------------------------------------------------------- sound
#
# audiostream's own stream, as the page receives it once websockify has
# unwrapped it: a 16-byte header, then signed 16-bit stereo.
#

class Audio:
    def __init__(self, port):
        end = time.time() + 10
        while True:
            try:
                self.s = socket.create_connection(('127.0.0.1', port), timeout=5)
                break
            except OSError:
                if time.time() > end:
                    die('nothing serving sound on port %d' % port)
                time.sleep(0.2)
        hdr = b''
        while len(hdr) < 16:
            c = self.s.recv(16 - len(hdr))
            if not c:
                die('the sound stream closed before its header')
            hdr += c
        if hdr[:8] != b'DOOMAUD1':
            die('the sound stream does not start DOOMAUD1: %r' % hdr[:8])
        self.rate = struct.unpack('<I', hdr[8:12])[0]
        self.lock = threading.Lock()
        self.samples = []       # (time, peak) per chunk
        self.total = 0
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        rest = b''
        while True:
            try:
                d = self.s.recv(8192)
            except OSError:
                return
            if not d:
                return
            d = rest + d
            n = len(d) // 2 * 2
            rest = d[n:]
            vals = struct.unpack('<%dh' % (n // 2), d[:n])
            peak = max((abs(v) for v in vals), default=0)
            with self.lock:
                self.samples.append((time.time(), peak))
                self.total += n

    def peak(self, t0, t1):
        with self.lock:
            return max((p for t, p in self.samples if t0 <= t <= t1), default=0)


# ---------------------------------------------------------------------- phases

def level(a):
    fb0 = wait_for_level(a.fb)
    save_png(fb0, os.path.join(a.out, 'level.png'))
    colours = len(set(region(fb0, 0, fb0[1])))
    if colours < 64:
        die('the level is drawn in %d colours; a DOOM screen has well over 64' % colours)
    say('E1M1 is on screen, %d colours, depth %d' % (colours, fb0[2]))

    # Kept for the depth 8 comparison: the status bar before anything has
    # touched it.
    with open(os.path.join(a.out, 'statusbar24.bin'), 'wb') as f:
        for p in region(fb0, STATUSBAR_Y, fb0[1], FACE):
            f.write(bytes(p))

    audio = Audio(a.audio)
    time.sleep(1.5)
    if audio.total < audio.rate * 4 // 2:
        die('the sound stream is not flowing: %d bytes in 1.5 s at %d Hz'
            % (audio.total, audio.rate))
    say('sound is streaming at %d Hz' % audio.rate)

    pad = Pad(a.pad)
    say('controller connected')

    view0 = region(fb0, 0, STATUSBAR_Y)
    pad.stick(0, -1, 0, 0, 1.0)
    fb1 = read_fb(a.fb)
    moved = changed(view0, region(fb1, 0, STATUSBAR_Y))
    if moved < 0.2:
        die('pushing the left stick forward for a second changed %.0f%% of the '
            'view; walking changes most of it' % (moved * 100))
    say('the left stick walks: %.0f%% of the view changed' % (moved * 100))

    view1 = region(fb1, 0, STATUSBAR_Y)
    pad.stick(0, 0, 0.8, 0, 0.6)
    fb2 = read_fb(a.fb)
    turned = changed(view1, region(fb2, 0, STATUSBAR_Y))
    if turned < 0.2:
        die('the right stick changed %.0f%% of the view; turning changes most of it'
            % (turned * 100))
    say('the right stick turns: %.0f%% of the view changed' % (turned * 100))

    # Fire: the pistol goes off, its sound reaches the stream, and the pad is
    # asked to kick. Music is at volume 0 for this phase (smoke-test.sh writes
    # the config), so the only sound there is is the game's.
    quiet = audio.peak(time.time() - 1.0, time.time())
    bar0 = region(fb2, STATUSBAR_Y, fb2[1], FACE)
    t0 = time.time()
    pad.press('RT', 0.3)
    time.sleep(0.8)
    loud = audio.peak(t0, time.time())
    if not pad.rumbles:
        die('fired, and the engine asked the pad for no vibration')
    lo, hi, ms = pad.rumbles[0]
    say('RT fired, and the pad was asked to vibrate: %.2f/%.2f for %d ms'
        % (lo / 65535, hi / 65535, ms))
    if loud < 2000 or loud < quiet * 4:
        die('the shot was not heard: peak %d after firing, %d before' % (loud, quiet))
    say('the shot was heard: peak %d, against %d before it' % (loud, quiet))
    bar1 = region(read_fb(a.fb), STATUSBAR_Y, fb2[1], FACE)
    if bar1 == bar0:
        die('fired, and the status bar did not change: no ammunition was used')
    say('the ammunition count went down')

    view2 = region(read_fb(a.fb), 0, STATUSBAR_Y)
    pad.press('START')
    time.sleep(0.5)
    menu = changed(view2, region(read_fb(a.fb), 0, STATUSBAR_Y))
    if menu < 0.02:
        die('Start did not open the menu: %.1f%% of the view changed' % (menu * 100))
    say('Start opens the menu')
    pad.press('START')
    time.sleep(0.3)


def music(a):
    audio = Audio(a.audio)
    time.sleep(6)
    peak = audio.peak(0, time.time())
    if peak < 1000:
        die('six seconds of the title screen with music on, and the loudest '
            'sample was %d' % peak)
    say('the title music plays: peak %d' % peak)


def compare(a):
    fb = wait_for_level(a.fb)
    if fb[3] != 8:
        die('expected an 8-bit screen, got %d bits a pixel' % fb[3])
    g = gamma0(a.src)
    pal = [tuple(g[c] for c in rgb) for rgb in playpal(a.wad)]
    want = open(os.path.join(a.out, 'statusbar24.bin'), 'rb').read()
    got = region(fb, STATUSBAR_Y, fb[1], FACE)
    if len(want) != len(got) * 3:
        die('the two screens are different sizes')
    bad = sum(1 for i, p in enumerate(got) if pal[p] != tuple(want[i * 3:i * 3 + 3]))
    save_png(fb, os.path.join(a.out, 'depth8-indices.png'))
    if bad:
        die('%d of %d status bar pixels differ between depth 8 and depth 24'
            % (bad, len(got)))
    say('depth 8 and depth 24 agree on all %d status bar pixels' % len(got))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('phase', choices=['level', 'music', 'compare'])
    p.add_argument('--fb')
    p.add_argument('--pad', type=int)
    p.add_argument('--audio', type=int)
    p.add_argument('--wad')
    p.add_argument('--src', help="the engine's v_video.c, for its gamma table")
    p.add_argument('--out', default='.')
    a = p.parse_args()
    {'level': level, 'music': music, 'compare': compare}[a.phase](a)


if __name__ == '__main__':
    main()
