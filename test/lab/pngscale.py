#!/usr/bin/env python3
"""Nearest-neighbor scaling of MAME's PNG snapshots (8-bit RGB or RGBA, not interlaced).
usage: pngscale.py IN OUT SX SY [x0 y0 w h]   (an optional crop, in source pixels, first)"""
import struct
import sys
import zlib


def read(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos, idat, w = 8, b"", None
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", body[:10])
            assert depth == 8 and ctype in (2, 6)
            bpp = 3 if ctype == 2 else 4
        elif kind == b"IDAT":
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([bytes(line[x * bpp:x * bpp + 3]) for x in range(w)])
        prev = line
    return rows


def write(path, rows):
    h, w = len(rows), len(rows[0])
    raw = b"".join(b"\0" + b"".join(r) for r in rows)
    chunk = lambda k, b: struct.pack(">I", len(b)) + k + b + struct.pack(">I", zlib.crc32(k + b) & 0xFFFFFFFF)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


rows = read(sys.argv[1])
sx, sy = int(sys.argv[3]), int(sys.argv[4])
if len(sys.argv) > 8:
    x0, y0, cw, ch = map(int, sys.argv[5:9])
    rows = [r[x0:x0 + cw] for r in rows[y0:y0 + ch]]
out = []
for r in rows:
    line = [p for p in r for _ in range(sx)]
    out += [line] * sy
write(sys.argv[2], out)
