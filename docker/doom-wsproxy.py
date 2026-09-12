#!/usr/bin/env python3
#
# websockify, with two streams on the one port.
#
# The page needs both the VNC connection and, since 1.10.15, a second one
# carrying sound. Opening another port for it would mean another -p on every
# docker run and another mapping in the Unraid template, for what is one more
# socket to the same container.
#
# websockify can already route by a token in the query string, which is how
# multi-target deployments pick a backend. What it cannot do is carry on
# without one: a connection with no token is refused outright, and that would
# break /vnc.html, which is stock noVNC and knows nothing about tokens.
#
# So the token lookup is left alone and only the "no token" case is changed,
# to mean the screen -- the one thing every client that predates this wanted.
#
import sys
from urllib.parse import parse_qs, urlparse

from websockify.websocketproxy import ProxyRequestHandler, websockify_init

DEFAULT_TOKEN = 'vnc'


#
# Make the browser check whether the page has changed.
#
# websockify serves the client with Python's SimpleHTTPRequestHandler, which
# sends Last-Modified and nothing else -- no Cache-Control, no ETag. With no
# freshness given, a browser is entitled to guess one and reuse what it has
# without asking, and it does: an image can be updated underneath a tab that
# goes on running the client it downloaded two releases ago. That failure is
# invisible from the container, whose log shows a perfectly healthy server
# talking to nobody.
#
# no-cache does not mean do not store it. It means ask first, which is a
# conditional request answered by a 304 in the ordinary case: the same traffic
# as before, minus the chance of serving a stale client for ever.
#
_end_headers = ProxyRequestHandler.end_headers


def end_headers(self):
    if not self.headers.get('Upgrade'):
        self.send_header('Cache-Control', 'no-cache')

    _end_headers(self)


ProxyRequestHandler.end_headers = end_headers


def get_target(self, target_plugin):
    query = parse_qs(urlparse(self.path).query)
    token = (query.get('token') or [''])[0].strip() or DEFAULT_TOKEN

    pair = target_plugin.lookup(token)

    if pair is None:
        raise self.server.EClose("no stream called %r" % token)

    return pair


ProxyRequestHandler.get_target = get_target

sys.exit(websockify_init())
