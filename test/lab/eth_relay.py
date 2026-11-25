#!/usr/bin/env python3
"""The host side of the W5100 test double (mame/w5100.lua): it opens the TCP connection to the lab
parlor when the card's socket 0 connects (xchg/eth_ctl "open"), sends what the Apple writes
(xchg/eth_out, appended to) and appends what the parlor sends to xchg/eth_in; when the parlor closes,
it creates xchg/eth_closed. usage: eth_relay.py XCHG [port]"""
import os
import socket
import sys
import time

X = sys.argv[1]
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 16501
for f in ("eth_ctl", "eth_out", "eth_in", "eth_closed"):
    try:
        os.remove(os.path.join(X, f))
    except OSError:
        pass
open(os.path.join(X, "eth_in"), "wb").close()
ctl_pos = out_pos = 0
s = None
log = open(os.path.join(X, "relay.log"), "a", buffering=1)
while True:
    try:
        with open(os.path.join(X, "eth_ctl"), "rb") as f:
            f.seek(ctl_pos)
            d = f.read()
            ctl_pos += len(d)
        for cmd in d.decode().split():
            log.write("ctl %s\n" % cmd)
            if cmd == "open":
                if s:
                    s.close()
                s = socket.create_connection(("127.0.0.1", PORT))
                s.setblocking(False)
            elif cmd == "close" and s:
                s.close()
                s = None
    except FileNotFoundError:
        pass
    try:
        with open(os.path.join(X, "eth_out"), "rb") as f:
            f.seek(out_pos)
            d = f.read()
            out_pos += len(d)
        if d and s:
            log.write("out %r\n" % d)
            s.sendall(d)
    except FileNotFoundError:
        pass
    if s:
        try:
            d = s.recv(4096)
            if d:
                log.write("in %r\n" % d)
                with open(os.path.join(X, "eth_in"), "ab") as f:
                    f.write(d)
            else:
                s.close()
                s = None
                open(os.path.join(X, "eth_closed"), "w").close()
        except BlockingIOError:
            pass
    time.sleep(0.02)
