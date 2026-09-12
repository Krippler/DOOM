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


def get_target(self, target_plugin):
    query = parse_qs(urlparse(self.path).query)
    token = (query.get('token') or [''])[0].strip() or DEFAULT_TOKEN

    pair = target_plugin.lookup(token)

    if pair is None:
        raise self.server.EClose("no stream called %r" % token)

    return pair


ProxyRequestHandler.get_target = get_target

sys.exit(websockify_init())
