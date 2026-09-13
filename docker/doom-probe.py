#!/usr/bin/env python3
"""Ask x11vnc for pictures and time how long it takes to answer.

Run this inside the container while the picture is moving. It asks for frames
the way the browser does, twice: once straight to x11vnc, and once through
websockify and the proxy. The difference between the two says which of them is
slow, and running it on the machine that stutters says whether either of them
is slow there at all -- on a developer laptop x11vnc answers in about 10 ms
and never exceeds 60, so a very different answer here is the finding.

No browser is involved and no input is sent: the game's own attract-mode demo
moves the picture quite enough, and sending keys would play the game for you.

    docker exec <container> doom-probe

It also runs from another machine, which is the more interesting case: give it
a host and everything between that machine and the container is in the path
too, which is where a browser actually sits.

    python3 doom-probe.py nas.local
    python3 doom-probe.py 192.168.10.37:380
    python3 doom-probe.py http://192.168.10.37:380

The port is the one the web page is on, whatever it was published as -- the
same thing that is in the address bar when you play.

What it cannot do is conclude anything about a still picture. VNC sends what
changed and nothing else, so a motionless screen produces silences of any
length that are not faults -- this refuses to report rather than call that a
result, which is a mistake this project has made more than once.
"""

import base64
import os
import select
import socket
import struct
import sys
import threading
import time

SECONDS   = float(os.environ.get('DOOM_PROBE_SECONDS', '30'))
QUIET_S   = 0.015        # a picture is over once nothing has come for this long
MOVING_KB = 40           # below this the picture is not moving enough to judge


class Link:
    """The bytes of the RFB conversation, over a socket or a WebSocket."""

    def __init__(self, sock):
        self.s = sock
        self.s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.app = b''

    def send(self, data):
        self.s.sendall(data)

    def recv(self, timeout):
        if self.app:
            d, self.app = self.app, b''
            return d
        r, _, _ = select.select([self.s], [], [], timeout)
        if not r:
            return b''
        d = self.s.recv(1 << 20)
        if not d:
            raise RuntimeError('the connection closed')
        return d

    def exactly(self, n, timeout=10.0):
        out = b''
        while len(out) < n:
            d = self.recv(timeout)
            if not d:
                raise RuntimeError('timed out wanting %d bytes, got %d'
                                   % (n, len(out)))
            out += d
        if len(out) > n:            # the surplus goes back, not on the floor
            self.app = out[n:] + self.app
            out = out[:n]
        return out


class WSLink(Link):
    """The same, wrapped in WebSocket frames, which is how the browser is
    served: this is the path with websockify and the proxy in it."""

    def __init__(self, host, port, path):
        Link.__init__(self, socket.create_connection((host, port), 10))
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall((
            'GET %s HTTP/1.1\r\nHost: %s:%d\r\n'
            'Upgrade: websocket\r\nConnection: Upgrade\r\n'
            'Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n'
            'Sec-WebSocket-Protocol: binary\r\n\r\n'
            % (path, host, port, key)).encode())

        buf = b''
        while b'\r\n\r\n' not in buf:
            d = self.s.recv(4096)
            if not d:
                raise RuntimeError('the web server closed during the handshake')
            buf += d

        head, _, rest = buf.partition(b'\r\n\r\n')
        if b'101' not in head.split(b'\r\n')[0]:
            raise RuntimeError(head.split(b'\r\n')[0].decode('latin-1'))

        self.raw = rest
        self._unframe()

    def send(self, data):
        n = len(data)
        if n < 126:
            hdr = struct.pack('>BB', 0x82, 0x80 | n)
        elif n < 65536:
            hdr = struct.pack('>BBH', 0x82, 0x80 | 126, n)
        else:
            hdr = struct.pack('>BBQ', 0x82, 0x80 | 127, n)
        m = os.urandom(4)
        self.s.sendall(hdr + m
                       + bytes(b ^ m[i % 4] for i, b in enumerate(data)))

    def _unframe(self):
        while True:
            b = self.raw
            if len(b) < 2:
                return
            op, ln, i = b[0] & 0x0F, b[1] & 0x7F, 2
            if ln == 126:
                if len(b) < 4:
                    return
                ln, i = struct.unpack('>H', b[2:4])[0], 4
            elif ln == 127:
                if len(b) < 10:
                    return
                ln, i = struct.unpack('>Q', b[2:10])[0], 10
            if b[1] & 0x80:
                i += 4
            if len(b) < i + ln:
                return
            payload, self.raw = b[i:i + ln], b[i + ln:]
            if op in (0, 2):
                self.app += payload
            elif op == 8:
                raise RuntimeError('the proxy closed the connection')

    def recv(self, timeout):
        self._unframe()
        if self.app:
            d, self.app = self.app, b''
            return d
        r, _, _ = select.select([self.s], [], [], timeout)
        if not r:
            return b''
        d = self.s.recv(1 << 20)
        if not d:
            raise RuntimeError('the connection closed')
        self.raw += d
        self._unframe()
        d, self.app = self.app, b''
        return d


def handshake(link):
    link.exactly(12)
    link.send(b'RFB 003.008\n')

    count = link.exactly(1)[0]
    if count == 0:
        reason = link.exactly(struct.unpack('>I', link.exactly(4))[0])
        raise RuntimeError(reason.decode('latin-1'))

    kinds = link.exactly(count)
    if 1 not in kinds:
        raise RuntimeError(
            'this x11vnc wants a password (security types %s), and the probe '
            'only speaks to one that does not. Start the container without '
            'DOOM_VNC_PASSWORD to measure it.' % list(kinds))

    link.send(bytes([1]))
    if struct.unpack('>I', link.exactly(4))[0] != 0:
        raise RuntimeError('x11vnc refused the connection')

    link.send(bytes([1]))                       # ClientInit, shared
    head = link.exactly(24)
    width, height = struct.unpack('>HH', head[:4])
    link.exactly(struct.unpack('>I', head[20:24])[0])

    # The list the browser offers, so x11vnc makes the same choices for us.
    encs = [1, 7, -260, 5, 2, 0, -224, -239, -308]
    link.send(struct.pack('>BBH', 2, 0, len(encs))
              + b''.join(struct.pack('>i', e) for e in encs))
    return width, height


def measure(link, width, height, seconds):
    def ask(incremental=1):
        link.send(struct.pack('>BBHHHH', 3, incremental, 0, 0, width, height))

    ask(0)
    waits, total, first = [], 0, True
    when = []                       # (wall clock, ms) for the long ones
    end = time.time() + seconds

    while time.time() < end:
        started, got = time.time(), 0
        while True:
            data = link.recv(2.0 if got == 0 else QUIET_S)
            if not data:
                break
            if got == 0:
                ms = (time.time() - started) * 1000
                if not first:                   # the first is the full screen
                    waits.append(ms)
                    if ms > 200:
                        when.append((started, ms))
                first = False
            got += len(data)
        total += got
        ask()

    return waits, total, when


def report(name, waits, total, seconds):
    if not waits:
        print('%-22s nothing arrived at all' % (name + ':'))
        return None

    waits.sort()

    def pct(p):
        return waits[min(len(waits) - 1, int(len(waits) * p))]

    kb = total / 1024.0 / seconds
    print('%-22s %4d answers, %4.0f KB/s   median %3.0f ms   p90 %3.0f   '
          'p99 %4.0f   worst %4.0f   over 200 ms: %d'
          % (name + ':', len(waits), kb, pct(0.5), pct(0.9), pct(0.99),
             waits[-1], sum(1 for w in waits if w > 200)))
    return kb


def parse_where(arg, default_port):
    """Take the address however it was typed.

    The published port is rarely 6080 -- it is whatever the container was
    given -- and asking for it in an environment variable means knowing that
    fish spells that differently from bash. Anything that looks like the
    address bar works instead: a host, a host and port, or the whole URL.
    """
    where = arg.strip()

    for scheme in ('http://', 'https://', 'ws://', 'wss://'):
        if where.lower().startswith(scheme):
            where = where[len(scheme):]
            break

    where = where.split('/', 1)[0]          # drop /play.html and anything after

    # host:port, but not an IPv6 address, which is full of colons and is
    # written in brackets when it carries a port.
    if where.startswith('['):
        host, _, rest = where.partition(']')
        host = host[1:]
        if rest.startswith(':') and rest[1:].isdigit():
            return host, int(rest[1:])
        return host, default_port

    if where.count(':') == 1:
        host, _, port = where.partition(':')
        if port.isdigit():
            return host, int(port)

    return where, default_port


def main():
    # Inside the container both ports are on localhost. From another machine
    # the web port is the one that is published, and x11vnc's usually is not --
    # which is fine: the run that cannot connect says so and the other still
    # happens.
    vnc  = int(os.environ.get('DOOM_VNC_PORT', '5900'))
    web  = int(os.environ.get('DOOM_WEB_PORT', '6080'))
    host = '127.0.0.1'

    if len(sys.argv) > 1:
        host, web = parse_where(sys.argv[1], web)

    # Not "in the container": run on the host but outside it, this is still
    # localhost and the container is not where the probe is.
    where = 'from here' if host in ('127.0.0.1', 'localhost') \
            else 'from here to %s' % host
    print('Timing how long x11vnc takes to answer, %s, %.0f seconds, both'
          ' paths at once.' % (where, SECONDS))
    print('Leave the game moving while this runs -- its own demo is enough.')
    print()

    #
    # Both paths at the same instant, in two threads -- not one after the other.
    #
    # Measured in sequence, the second one wears whatever the minute brings.
    # That produced two confident and opposite findings from the same container
    # under the same load: run x11vnc first and the proxy looked twenty times
    # worse, run the proxy first and the stall moved to x11vnc. Splitting each
    # into halves did not fix it either, because both halves still ran in the
    # same order and the proxy stayed in second place.
    #
    # Run together there is nothing left to confound. If a stall belongs to one
    # path it shows in that stream alone; if it belongs to x11vnc, both streams
    # stall at the same moment by the same amount, which is what actually
    # happens:
    #
    #     straight to x11vnc:  1387 ms at t+150.7s
    #     through the proxy:   1384 ms at t+150.7s
    #
    paths = (('straight to x11vnc',
              lambda: Link(socket.create_connection((host, vnc), 10))),
             ('through the proxy',
              lambda: WSLink(host, web, '/websockify')))

    gathered = {}
    failed = {}
    started_at = time.time()

    def run(name, make):
        link = None
        try:
            link = make()
            width, height = handshake(link)
            gathered[name] = measure(link, width, height, SECONDS)
        except ConnectionRefusedError:
            failed[name] = None
        except Exception as exc:
            failed[name] = str(exc)
        finally:
            if link is not None:
                try:
                    link.s.close()
                except Exception:
                    pass

    threads = [threading.Thread(target=run, args=(n, m)) for n, m in paths]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    results = {}
    stalls = {}

    for name, _make in paths:
        if name in failed:
            reason = failed[name]
            if reason is None:
                # The ordinary case from another machine: x11vnc's port is
                # almost never published, and does not need to be.
                print('%-22s nothing listening on %s:%d%s'
                      % (name + ':', host, vnc if 'x11vnc' in name else web,
                         ' (expected from another machine -- the line below is'
                         ' the one that matters)' if 'x11vnc' in name else ''))
            else:
                print('%-22s could not measure: %s' % (name + ':', reason))
            results[name] = None
            continue

        waits, total, when = gathered[name]
        stalls[name] = when
        results[name] = report(name, waits, total, SECONDS)

    for name in stalls:
        for at, ms in stalls[name]:
            print('      %-18s stall %5.0f ms at t+%.1fs'
                  % (name, ms, at - started_at))

    if len(stalls) == 2:
        a, b = (stalls[n] for n, _ in paths)
        together = sum(1 for at, _ms in a
                       if any(abs(at - bt) < 0.25 for bt, _bms in b))
        if a and b and together:
            print()
            print('%d of those hit both paths at the same moment, so they are'
                  % together)
            print('one thing upstream of the proxy rather than two problems.')


    print()

    moving = [kb for kb in results.values() if kb is not None]
    if not moving:
        print('Nothing was measured, so there is nothing to conclude.')
        return 1

    if max(moving) < MOVING_KB:
        print('The picture was barely changing (%.0f KB/s), so these numbers'
              % max(moving))
        print('describe a still screen, not a slow one. VNC sends what changed')
        print('and nothing else. Start the game moving and run it again.')
        return 1

    measured = sum(1 for kb in results.values() if kb is not None)
    if measured > 1:
        print('On a machine with nothing wrong both lines read about 10 ms in')
        print('the middle and never pass 60 at the worst, with nothing over')
    else:
        print('On a machine with nothing wrong this reads about 10 ms in the')
        print('middle and never passes 60 at the worst, with nothing over')
    print('200 ms. A worst of several hundred here is the stutter, measured')
    print('with no browser anywhere near it.')

    if host not in ('127.0.0.1', 'localhost'):
        print()
        print('This run had the network in it. If it stalls here and not when')
        print('run inside the container, the link and the proxy that feeds it')
        print('are the thing to fix, not x11vnc.')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
