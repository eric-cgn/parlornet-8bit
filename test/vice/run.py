#!/usr/bin/env python3
"""The step runner for the C64 program in VICE: reads a plan, one step per line, and carries it out through
VICE's binary monitor (127.0.0.1:6502), as test/mame/run.lua does in MAME. Runs in the VICE container
(run.sh starts it).

  wait N             N frames (60 a second)
  waittext TEXT [N]  until the text screen shows TEXT (at most N frames, default 1800)
  waittitle [N]      until the title screen is up (at most N frames)
  title [N]          the title screen (the bitmap, at start-up), which waits for a key: Return each second
                     while it is up, until the text screen is back; at most N frames (default 1800)
  type TEXT          type it (\\r for Return)
  basic TEXT         type it into BASIC's keyboard buffer (\\r for Return), as before the program runs
  key NAMES          keys by name: left right up down ret esc del tab space, a count before one repeats it
                     ("3down")
  oa C               the Commodore key and C (as Open-Apple and C on the Apple II)
  arrows UDLR...     the cursor keys, one after another
  click ID           point at the target with this id (as the cursor keys would) and press Return
  point ID           make the target with this id the current one, without a key
  joy BITS [N]       the joystick in port 2 held (bits: 1 up, 2 down, 4 left, 8 right, 16 fire) for N frames
                     (default 10), then let go
  snap NAME          save the screen as /out/NAME.png (with the border)
  dumptext NAME      write the text screen to /out/NAME.txt
  hold NAME V N      keep the byte at the program's variable NAME at V for the next N frames
  set NAME V         write the byte at the program's variable NAME once
  peek NAME[:N]...   log the bytes at the program's labels
  need GAME NAME PW  ask the lab's orchestrator (orch.py) for the next steps
  exit

Keys are put in the program's own key queue (kq_key, kq_mod, kq_head in sys.s), as its keyboard scan
puts them; `joy` goes through the emulated joystick port. The program's addresses come from the
linker's label file (/xchg/labels.txt).

usage: run.py PLAN OUTDIR XCHG
"""
import os
import socket
import struct
import sys
import time
import zlib

PLAN, OUT, XCHG = sys.argv[1], sys.argv[2], sys.argv[3]
FRAME = 1 / 60.0


class Mon:
    def __init__(self):
        for _ in range(120):
            try:
                self.s = socket.create_connection(("127.0.0.1", 6502), 5)
                break
            except OSError:
                time.sleep(0.5)
        else:
            raise SystemExit("no binary monitor")
        self.rid = 0

    def recv(self, n):
        b = b""
        while len(b) < n:
            c = self.s.recv(n - len(b))
            if not c:
                raise EOFError
            b += c
        return b

    def cmd(self, ctype, body=b""):
        self.rid += 1
        self.s.sendall(struct.pack("<BBIIB", 2, 2, len(body), self.rid, ctype) + body)
        while True:
            hdr = self.recv(12)
            _, _, blen, rtype, err, r = struct.unpack("<BBIBBI", hdr)
            data = self.recv(blen)
            if r == self.rid:
                if err:
                    raise RuntimeError("monitor error %d for %02x" % (err, ctype))
                return data

    def get(self, addr, n, bank=0):
        d = self.cmd(0x01, struct.pack("<BHHBH", 0, addr, addr + n - 1, 0, bank))
        return d[2:]

    def bank(self, name):
        d = self.cmd(0x82)
        n = struct.unpack("<H", d[:2])[0]
        i = 2
        for _ in range(n):
            size = d[i]
            bid, nl = struct.unpack("<HB", d[i + 1:i + 4])
            if d[i + 4:i + 4 + nl].decode() == name:
                return bid
            i += 1 + size
        raise KeyError(name)

    def bitmap(self):
        """1 while the VIC shows the bitmap (the title screen, the lobby, a table)."""
        if not hasattr(self, "io"):
            self.io = self.bank("io")
        return bool(self.get(0xD011, 1, self.io)[0] & 0x20)

    def put(self, addr, data):
        self.cmd(0x02, struct.pack("<BHHBH", 0, addr, addr + len(data) - 1, 0, 0) + bytes(data))

    def resume(self):
        self.cmd(0xAA)

    def joy(self, port, value):
        self.cmd(0xA2, struct.pack("<HH", port, value))

    def snap(self, path):
        d = self.cmd(0x84, bytes([1, 0]))
        info = struct.unpack("<I", d[:4])[0]
        dw, dh, ox, oy, iw, ih, depth = struct.unpack("<HHHHHHB", d[4:4 + 13])
        buf = d[4 + info + 4:]
        p = self.cmd(0x91, bytes([1]))
        n = struct.unpack("<H", p[:2])[0]
        pal = [tuple(p[2 + k * 4 + 1:2 + k * 4 + 4]) for k in range(n)]
        # the inner screen with 32 pixels of border around it, 384 by 264
        x0, y0 = max(ox - 32, 0), max(oy - 32, 0)
        w, h = min(iw + 64, dw - x0), min(ih + 64, dh - y0)
        rows = []
        for y in range(y0, y0 + h):
            line = bytes(buf[y * dw + x0:y * dw + x0 + w]).ljust(w, b"\0")
            rows.append(b"\0" + b"".join(bytes(pal[c % len(pal)]) for c in line))
        png(path, w, h, b"".join(rows))


def png(path, w, h, raw):
    def chunk(t, b):
        c = struct.pack(">I", len(b)) + t + b
        return c + struct.pack(">I", zlib.crc32(t + b) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


ADDR = {}
for line in open(os.path.join(XCHG, "labels.txt")):      # ld65's -Ln: "al 007DF5 ._ser_iface"
    parts = line.split()
    if len(parts) == 3:
        name = parts[2].lstrip(".")
        ADDR.setdefault(name[1:] if name.startswith("_") else name, int(parts[1], 16))

steps = [l.rstrip("\n") for l in open(PLAN) if l.strip() and not l.startswith("#")]
log = open(os.path.join(OUT, "run.log"), "w", buffering=1)
mon = Mon()
t0 = time.time()
holds = []


def frame():
    return int((time.time() - t0) / FRAME)


def note(s):
    log.write("%d %s\n" % (frame(), s))


def tick():
    """Between steps: the holds kept, the emulation running."""
    now = frame()
    for h in list(holds):
        if now >= h[2]:
            holds.remove(h)
        else:
            mon.put(h[0], [h[1]])
    mon.resume()


def wait(n):
    end = time.time() + n * FRAME
    while time.time() < end:
        if holds:
            tick()
        time.sleep(min(0.1, max(0, end - time.time())))


def screen_text():
    d = mon.get(0xC800, 1000)
    mon.resume()
    rows = []
    for r in range(25):
        s = "".join(chr(b & 0x7F) if 32 <= (b & 0x7F) < 127 else ("@" if (b & 0x7F) == 127 else " ")
                    for b in d[r * 40:r * 40 + 40])
        rows.append(s)
    return "\n".join(rows)


def push_key(code, mod=0):
    """A key into the program's queue, as the keyboard scan queues it (waiting while the queue is full)."""
    end = time.time() + 120
    while True:
        head, tail = mon.get(ADDR["kq_head"], 2)
        if (head + 1) & 7 != tail:
            break
        mon.resume()
        if time.time() > end:
            note("the key queue stays full")
            return
        time.sleep(0.05)
    mon.put(ADDR["kq_key"] + head, [code])
    mon.put(ADDR["kq_mod"] + head, [mod])
    mon.put(ADDR["kq_head"], [(head + 1) & 7])
    mon.resume()
    wait(3)


KEYS = {"left": 8, "right": 21, "up": 11, "down": 10, "ret": 13, "esc": 27, "del": 127, "tab": 9, "space": 32}


def run():
    i = 0
    while i < len(steps):
        step = steps[i]
        cmd, _, rest = step.partition(" ")
        rest = rest.strip()
        if cmd == "wait":
            wait(int(rest))
        elif cmd == "waittext":
            parts = rest.rsplit(" ", 1)
            text, n = (parts[0], int(parts[1])) if len(parts) == 2 and parts[1].isdigit() else (rest, 1800)
            end = time.time() + n * FRAME
            while True:
                if text in screen_text():
                    note("found " + text)
                    break
                if time.time() > end:
                    note("TIMEOUT waiting for " + text)
                    break
                wait(10)
        elif cmd == "waittitle":
            end = time.time() + (int(rest) if rest else 1800) * FRAME
            while time.time() < end:
                if mon.bitmap():
                    mon.resume()
                    note("title up")
                    wait(30)
                    break
                mon.resume()
                wait(30)
            else:
                note("TIMEOUT waiting for the title")
        elif cmd == "title":
            end = time.time() + (int(rest) if rest else 1800) * FRAME
            seen = False
            while time.time() < end:
                up = mon.bitmap()
                mon.resume()
                if up:
                    if not seen:
                        note("title")
                    seen = True
                    push_key(13)
                    wait(60)
                elif seen:
                    note("title gone")
                    break
                else:
                    wait(10)
            else:
                note("title stays" if seen else "no title")
        elif cmd == "basic":
            mon.cmd(0x72, bytes([len(rest.replace("\\r", "\r"))]) + rest.replace("\\r", "\r").upper().encode())
            mon.resume()
            wait(30)
        elif cmd == "type":
            for ch in rest.replace("\\r", "\r"):
                push_key(13 if ch == "\r" else ord(ch))
        elif cmd == "key":
            for word in rest.split():
                n = "".join(c for c in word if c.isdigit())
                name = word[len(n):]
                for _ in range(int(n or 1)):
                    push_key(KEYS[name])
        elif cmd == "oa":
            c = rest[0]
            push_key(ord(c.lower()) if c.isalpha() else ord(c), 2)
            wait(10)
        elif cmd == "arrows":
            for c in rest:
                push_key({"U": 11, "D": 10, "L": 8, "R": 21}[c])
                wait(8)
        elif cmd in ("click", "point"):
            ident = int(rest)
            n = mon.get(ADDR["t_n"], 1)[0]
            ids = mon.get(ADDR["t_id"], max(n, 1))
            found = [k for k in range(n) if ids[k] == ident]
            if found:
                mon.put(ADDR["t_cur"], [found[0]])
                mon.resume()
                if cmd == "click":
                    push_key(13)
                    note("click %d" % ident)
                    wait(10)
            else:
                mon.resume()
                note("NO TARGET %d" % ident)
        elif cmd == "joy":
            parts = rest.split()
            mon.joy(1, 0x1F & ~int(parts[0]))     # (VICE numbers the C64's port 2 as 1; lines low when on)
            mon.resume()
            wait(int(parts[1]) if len(parts) > 1 else 10)
            mon.joy(1, 0x1F)
            mon.resume()
            wait(10)
        elif cmd == "snap":
            mon.snap(os.path.join(OUT, rest + ".png"))
            mon.resume()
            note("snap " + rest)
        elif cmd == "dumptext":
            open(os.path.join(OUT, rest + ".txt"), "w").write(screen_text())
        elif cmd == "hold":
            name, v, n = rest.split()
            holds.append((ADDR[name], int(v), frame() + int(n)))
            tick()
        elif cmd == "set":
            name, v = rest.split()
            mon.put(ADDR[name], [int(v)])
            mon.resume()
        elif cmd == "need":
            need = os.path.join(XCHG, "need")
            got = os.path.join(XCHG, "steps.txt")
            open(need, "w").write(rest)
            end = time.time() + 60
            while not os.path.exists(got) and time.time() < end:
                wait(10)
            if os.path.exists(got):
                new = [l.rstrip("\n") for l in open(got) if l.strip()]
                os.remove(got)
                note("steps for %s: %d" % (rest, len(new)))
                steps[i + 1:i + 1] = new
            else:
                note("TIMEOUT need " + rest)
        elif cmd == "peek":
            # peek NAME... : log the bytes at each (NAME:N for N of them)
            out = []
            for w in rest.split():
                name, _, n = w.partition(":")
                out.append("%s=%s" % (name, mon.get(ADDR[name], int(n or 1)).hex()))
            mon.resume()
            note("peek " + " ".join(out))
        elif cmd == "exit":
            note("exit")
            return
        else:
            note("unknown step " + step)
        i += 1


try:
    mon.joy(1, 0x1F)                    # the joystick in port 2 let go (VICE's simulated port reads 0 as all on)
    mon.resume()
    run()
finally:
    log.close()
