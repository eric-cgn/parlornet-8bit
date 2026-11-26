#!/usr/bin/env python3
"""Checks Cannon Fire's shell against its path, after a plan with track steps (plans/cannonfire_path.txt).

It reads what the parlor sent the C64 from the modem stand-in's log (modem.log, or bridge.log on a direct
line): the views (ARN, Z and K lines) and the shots' events (FIRE, TRJ, HIT or OUT, ZCH, MOVE). From them it
works out where the program puts the shell's sprite at each step of each shot, as draw/cannonfire.c does:
the muzzle, then a third, two thirds and all of the way to each point of the path in turn, then to the
burst, kept on the arena's top line while it is above it. The flights in the track files (NAME.csv: the
sprite on, from when it comes on until it goes off) are the shots in the order they were fired (the bot's
shot often comes before the player's has flown), and each place the VIC showed the sprite in is checked
against its shot's steps. A place more than a pixel from
every step is off the path.

It prints a line for each flight, and draws each one as NAME_N_path.png (N for the flight) in the output directory: the shot's
steps in green and the sprite's places in white when on the path and red when off, at twice the size, in
the VIC's sprite coordinates (the screen is x 24 to 343 and y 50 to 249). It exits with 1 when a place
is off the path or a flight has no shot.

usage: cftrack.py OUTDIR [NAME...]   (the track names; all the .csv files in OUTDIR when none are named)
"""
import ast
import glob
import os
import struct
import sys
import zlib

BASE, TOP = 151, 8                      # draw/cannonfire.c: the line of height 0, the arena's first line


def stream(path):
    """The parlor's lines to the C64, each with the time its first byte came."""
    buf, t0, out = b"", None, []
    for ln in open(path, encoding="latin-1"):
        parts = ln.split(" ", 2)
        if len(parts) < 3 or parts[1] != "A2<":
            continue
        t = float(parts[0])
        buf += ast.literal_eval(parts[2].strip())
        if t0 is None:
            t0 = t
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            out.append((t0, line.rstrip(b"\r").decode("latin-1")))
            t0 = t if buf else None
    return out


def trunc_div(a, b):
    """C's division of an int, towards zero."""
    q = abs(a) // b
    return q if a >= 0 else -q


def shots(lines):
    """Each shot fired: its time, seat, aim and the sprite's places (as the VIC has them) at each step."""
    aw, ht, tx = 72, [0] * 200, [0] * 8
    seen, found, cur = set(), [], None

    def cx(x):
        scale = min(320 // aw, 4)
        ox = (320 - aw * scale) // 2
        return ox + x * scale + scale // 2

    def line_of(h):
        return BASE - 2 * h

    def steps(shot, hit):
        fs = shot["seat"]
        sx, sy = cx(tx[fs]), line_of(ht[tx[fs]] + 2)
        pts = [(cx(x), line_of(y)) for x, y in shot["trj"] if x < aw]
        if hit:
            pts.append((cx(hit[0]), line_of(hit[1])))
        out = []
        for ex, ey in pts:
            for k in (1, 2, 3):
                x = sx + trunc_div((ex - sx) * k, 3)
                y = max(sy + trunc_div((ey - sy) * k, 3), TOP)
                out.append((x + 24 - 2, y + 50 - 2))
            sx, sy = ex, ey
        return out

    for t, line in lines:
        w = line.split()
        if not w:
            continue
        if w[0] == "ARN" and len(w) > 1:
            aw = min(int(w[1]), 200)
        elif w[0] == "Z" and len(w) > 2:
            x = int(w[1])
            for c in w[2]:
                if x < aw:
                    ht[x] = ord(c) - 0x30
                x += 1
        elif w[0] == "K" and len(w) > 3 and int(w[1]) < 8:
            tx[int(w[1])] = int(w[2])
        elif w[0] == "EVT" and len(w) > 3:
            key = (w[1], w[2])
            if key in seen:
                continue
            seen.add(key)
            kind = w[3]
            if kind == "FIRE" and len(w) > 6:
                cur = {"time": t, "seat": int(w[4]), "angle": int(w[5]), "power": int(w[6]), "trj": []}
            elif kind == "TRJ" and cur:
                for i in range(4, len(w) - 1, 2):
                    if len(cur["trj"]) < 18:
                        cur["trj"].append((255 if w[i].startswith("-") else int(w[i]),
                                           0 if w[i + 1].startswith("-") else min(int(w[i + 1]), 255)))
            elif kind in ("HIT", "OUT") and cur:
                hit = (int(w[4]), int(w[5])) if kind == "HIT" and len(w) > 6 else None
                cur["end"] = "HIT %d %d" % hit if hit else "OUT"
                cur["steps"] = steps(cur, hit)
                found.append(cur)
                cur = None
            elif kind == "ZCH" and len(w) > 5:
                x = int(w[4])
                for c in w[5]:
                    if x < aw:
                        ht[x] = ord(c) - 0x30
                    x += 1
            elif kind == "MOVE" and len(w) > 6 and int(w[4]) < 8:
                tx[int(w[4])] = int(w[6])
    return found


def flights(path):
    """The sprite's flights in a track file: the time each began and its places, one after another."""
    out, cur = [], None
    for row in list(open(path))[1:]:
        f, t, on, x, y = row.strip().split(",")
        if on == "1":
            if cur is None:
                cur = {"time": float(t), "places": []}
                out.append(cur)
            p = (int(x), int(y))
            if not cur["places"] or cur["places"][-1] != p:
                cur["places"].append(p)
        else:
            cur = None
    return out


def png(path, w, h, px):
    raw = b"".join(b"\0" + bytes(px[y * w * 3:(y + 1) * w * 3]) for y in range(h))

    def chunk(t, b):
        c = struct.pack(">I", len(b)) + t + b
        return c + struct.pack(">I", zlib.crc32(t + b) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def draw(path, steps, places, off):
    w, h = 384 * 2, 272 * 2
    px = bytearray(w * h * 3)

    def mark(x, y, col, r):
        for yy in range(2 * y - r, 2 * y + r + 1):
            for xx in range(2 * x - r, 2 * x + r + 1):
                if 0 <= xx < w and 0 <= yy < h:
                    px[(yy * w + xx) * 3:(yy * w + xx) * 3 + 3] = bytes(col)
    for x in range(24, 344):                        # the screen's edge, and the arena's top line
        mark(x, 50, (60, 60, 60), 0)
        mark(x, 249, (60, 60, 60), 0)
        mark(x, TOP + 48, (40, 40, 100), 0)
    for y in range(50, 250):
        mark(24, y, (60, 60, 60), 0)
        mark(343, y, (60, 60, 60), 0)
    for x, y in steps:
        mark(x, y, (0, 200, 0), 3)
    for p in places:
        mark(p[0], p[1], (255, 40, 40) if p in off else (255, 255, 255), 1)
    png(path, w, h, px)


def main():
    outdir = sys.argv[1]
    names = sys.argv[2:] or sorted(os.path.basename(p)[:-4] for p in glob.glob(os.path.join(outdir, "*.csv")))
    log = os.path.join(outdir, "modem.log")
    if not os.path.exists(log):
        log = os.path.join(outdir, "bridge.log")
    all_shots = shots(stream(log))
    bad, used = 0, 0
    for name in names:
        for n, fl in enumerate(flights(os.path.join(outdir, name + ".csv")), 1):
            if used >= len(all_shots) or all_shots[used]["time"] > fl["time"]:
                print("%s flight %d: no shot fired before it" % (name, n))
                bad += 1
                continue
            s = all_shots[used]
            used += 1
            off = [p for p in fl["places"]
                   if min(max(abs(p[0] - q[0]), abs(p[1] - q[1])) for q in s["steps"]) > 1]
            worst = max(min(max(abs(p[0] - q[0]), abs(p[1] - q[1])) for q in s["steps"]) for p in fl["places"])
            print("%s flight %d: seat %d, angle %d, power %d, %d points, %s: %d places, %d off the path%s"
                  % (name, n, s["seat"], s["angle"], s["power"], len(s["trj"]), s["end"], len(fl["places"]),
                     len(off), " (at most %d pixels)" % worst if off else ""))
            draw(os.path.join(outdir, "%s_%d_path.png" % (name, n)), s["steps"], fl["places"], set(off))
            bad += bool(off)
    sys.exit(1 if bad else 0)


main()
