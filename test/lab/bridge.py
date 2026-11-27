#!/usr/bin/env python3
"""The serial line's other end, for testing ParlorNet on the Apple IIe in MAME.

MAME's Super Serial Card (null modem, -bitb socket.HOST:PORT) connects here. In bridge mode the bytes go
straight to the parlor's native port, as a PC bridge (tcpser, ser2net) would. In modem mode it acts like a
WiFi modem: it echoes what is typed, answers AT with OK, and ATDT host:port (or ATDP, or ATD, the number quoted or not) dials (CONNECT 9600), after
which bytes are relayed; the other end closing gives NO CARRIER. While connected, +++ with a second's
quiet before and after it goes back to command mode (OK), where ATH hangs up and ATO goes back online.
Some dial strings stand for a modem's other answers: a number with BUSY in it gives BUSY, NOCARRIER gives
NO CARRIER, SILENT gives nothing until the next byte from the Apple (which, as on a modem, stops the dial
with NO CARRIER), and STAY connects but does not notice the parlor hanging up, so that only +++ and ATH
end the call. Everything it sees is logged. In bridge mode, CUT (seconds) hangs up the line that long after
the connection to the parlor opens, as when the parlor restarts: the dialer at the other end (ATZ in the Unix
shell) then says NO CARRIER. DROP (bytes) loses one byte on the way to the Apple, once, as noise on the line
would: the first O of an OK line after that many bytes from the parlor, so that a reply has no final line
and the program asks again (net.c).

usage: bridge.py LISTENPORT PARLORHOST PARLORPORT [modem|bridge] [log] [CUT] [DROP]
"""
import select
import socket
import sys
import threading
import time

listen, phost, pport = int(sys.argv[1]), sys.argv[2], int(sys.argv[3])
modem = len(sys.argv) > 4 and sys.argv[4] == "modem"
logf = open(sys.argv[5] if len(sys.argv) > 5 else "/dev/null", "a", buffering=1)
cut = float(sys.argv[6]) if len(sys.argv) > 6 else 0
drop = [int(sys.argv[7]) if len(sys.argv) > 7 else 0, 0]      # after so many bytes, and the bytes so far


def log(what, data):
    logf.write("%.3f %s %r\n" % (time.time(), what, data))


def lose(b):
    """b without the O of its first OK line, once drop's bytes have gone by (once)."""
    if not drop[0]:
        return b
    drop[1] += len(b)
    if drop[1] < drop[0]:
        return b
    i = b.find(b"\nOK")
    if i < 0:
        return b
    drop[0] = 0
    log("dropped", b[i + 1:i + 2])
    return b[:i + 1] + b[i + 2:]


def relay(src, dst, what, done):
    try:
        while True:
            b = src.recv(4096)
            if not b:
                break
            log(what, b)
            dst.sendall(lose(b) if what == "A2<" else b)
    except OSError:
        pass
    done.set()


def serve(c):
    if not modem:
        # A bridge that opens the connection to the parlor when the Apple first sends something, and
        # again after the parlor closes it.
        while True:
            first = c.recv(4096)
            if not first:
                return
            log("A2>", first)
            p = socket.create_connection((phost, pport))
            log("open", b"")
            p.sendall(first)
            done = threading.Event()
            threading.Thread(target=relay, args=(p, c, "A2<", done), daemon=True).start()
            c.settimeout(0.2)
            end = time.time() + cut
            while not done.is_set():
                if cut and time.time() >= end:
                    log("cut", b"")
                    for x in (p, c):
                        try:
                            x.shutdown(socket.SHUT_RDWR)
                        except OSError:
                            pass
                        x.close()
                    return
                try:
                    b = c.recv(4096)
                except socket.timeout:
                    continue
                except OSError:
                    b = b""
                if not b:
                    p.close()
                    return
                log("A2>", b)
                try:
                    p.sendall(b)
                except OSError:
                    break
            c.settimeout(None)
            p.close()
            log("closed", b"")
    line = b""
    p = None                                # a call held in command mode after +++
    while True:
        b = c.recv(1)
        if not b:
            return
        log("A2>", b)
        c.sendall(b)                        # echo
        if b in b"\r\n":
            cmd = line.strip().upper()
            line = b""
            if not cmd:
                continue
            if cmd.startswith(b"ATD"):
                # ATDT, ATDP (pulse) or ATD, and the number, quoted or not
                target = cmd[4:] if cmd[3:4] in (b"T", b"P") else cmd[3:]
                target = target.strip(b'"')
                log("dial", cmd[:4] + b" " + target if cmd[3:4] in (b"T", b"P") else cmd[:3] + b" " + target)
                if b"BUSY" in target:
                    c.sendall(b"\r\nBUSY\r\n")
                    continue
                if b"NOCARRIER" in target:
                    c.sendall(b"\r\nNO CARRIER\r\n")
                    continue
                if b"SILENT" in target:
                    b = c.recv(1)
                    log("A2>", b)
                    c.sendall(b"\r\nNO CARRIER\r\n")
                    continue
                try:
                    p = socket.create_connection((phost, pport))   # whatever was dialed: the lab parlor
                except OSError:
                    c.sendall(b"\r\nNO ANSWER\r\n")
                    continue
                c.sendall(b"\r\nCONNECT 9600\r\n")
                p = online(c, p, b"STAY" in target)
            elif cmd.startswith(b"ATH"):
                if p:
                    p.close()
                    p = None
                log("hangup", b"")
                c.sendall(b"\r\nOK\r\n")
            elif cmd.startswith(b"ATO") and p:
                c.sendall(b"\r\nCONNECT 9600\r\n")
                p = online(c, p, False)
            elif cmd.startswith(b"AT"):
                c.sendall(b"\r\nOK\r\n")
            else:
                c.sendall(b"\r\nERROR\r\n")
        else:
            line += b


def online(c, p, stay):
    """Relays a call. Returns the parlor's socket when +++ went back to command mode, None when the call
    ended (NO CARRIER said, unless stay)."""
    last = time.time()                      # when the Apple last sent a byte
    plus, plus_at = b"", 0.0
    while True:
        r, _, _ = select.select([c] + ([p] if p else []), [], [], 0.1)
        now = time.time()
        if plus == b"+++" and now - plus_at >= 1.0:
            log("escape", b"")
            c.sendall(b"\r\nOK\r\n")
            return p
        if p in r:
            try:
                b = p.recv(4096)
            except OSError:
                b = b""
            if b:
                log("A2<", b)
                c.sendall(b)
            else:
                p.close()
                p = None
                log("parlor closed", b"")
                if not stay:
                    c.sendall(b"\r\nNO CARRIER\r\n")
                    return None
        if c in r:
            b = c.recv(4096)
            if not b:
                if p:
                    p.close()
                return None
            log("A2>", b)
            out = b""
            for ch in b:
                ch = bytes([ch])
                if ch == b"+" and len(plus) < 3 and (plus or now - last >= 1.0):
                    plus += ch
                    plus_at = now
                else:
                    out += plus + ch
                    plus = b""
            last = now
            if out and p:
                try:
                    p.sendall(out)
                except OSError:
                    pass

s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", listen))
s.listen(4)
while True:
    c, _ = s.accept()
    log("connect", b"")
    threading.Thread(target=serve, args=(c,), daemon=True).start()
