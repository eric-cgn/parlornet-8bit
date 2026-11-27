#!/usr/bin/env python3
"""Makes the Commodore 64 client's art, into the directory given (build/c64/obj):

- chars.bin: the character set's first half, 128 characters of 8 bytes, as ASCII: art/font.txt's 5 by 8
  glyphs made bold for the C64's screen (each pixel two wide, unless that would close a one-pixel gap), with
  art/c64/glyphs.txt's 8 by 8 ones over them (the Commodore logo at $7F). The program makes the inverted
  half itself.
- splash_art.s: the title letters from art/splash.txt, for c64/splash.c: each letter's width in blocks,
  then a byte a row (9 rows), the blocks from bit 7 down.
- NAME_art.s for each art/c64/NAME.pic: pictures and sprites for an overlay. A line `pic NAME` then rows of
  '#' (set) and '.' (clear), a multiple of 8 wide: _pic_NAME, the width in bytes, the height, the rows (as
  gfx.s's hgr_blit draws them). A line `sprite NAME` then up to 21 rows of up to 24: _spr_NAME, 63 bytes (a
  hires sprite). A blank line ends each.
- gridwords_art.s, with Grid Words' tile letters from art/c64/tiles.txt: _tile_glyphs, A to Z and then ?,
  6 bytes each, made bold as the character set is. gridwords.c draws them in a tile's rows 1 to 6.
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art")
C64ART = os.path.join(ART, "c64")


def read_glyphs(path, width, height=8):
    glyphs = {}
    lines = [ln.rstrip("\n") for ln in open(path) if not ln.startswith("# ")]
    i = 0
    while i < len(lines):
        ln = lines[i].strip()
        if not ln:
            i += 1
            continue
        code = int(ln[1:], 16) if ln.startswith("$") else ord(ln[1])
        rows = lines[i + 1:i + 1 + height]
        assert len(rows) == height and all(len(r) == width for r in rows), ln
        glyphs[code] = rows
        i += 1 + height
    return glyphs


def bold(row):
    """5 pixels -> a byte: in bits 6 to 2, each widened one pixel to the right where that does not run into
    the next pixel along (which would close a gap a pixel wide)."""
    px = [c == "#" for c in row] + [False, False]
    out = [False] * 8
    for i in range(5):
        if px[i]:
            out[i + 1] = True
            if not px[i + 1] and not px[i + 2]:
                out[i + 2] = True
            elif not px[i + 1] and px[i + 2]:
                pass
    return sum(1 << (7 - i) for i, on in enumerate(out) if on)


def charset():
    font = read_glyphs(os.path.join(ART, "font.txt"), 5)
    own = read_glyphs(os.path.join(C64ART, "glyphs.txt"), 8)
    data = bytearray(128 * 8)
    for code in range(32, 127):
        for r, row in enumerate(font[code]):
            data[code * 8 + r] = bold(row)
    for code, rows in own.items():
        for r, row in enumerate(rows):
            data[code * 8 + r] = sum(1 << (7 - i) for i, c in enumerate(row) if c == "#")
    return bytes(data)


def splash(path):
    out = ["; Made by tools/c64_art.py from art/splash.txt; do not edit.", '        .segment "RODATA"']
    lines = [ln.rstrip("\n") for ln in open(path) if not ln.startswith("# ")]
    i = 0
    names = []
    while i < len(lines):
        ln = lines[i].strip()
        if not ln.startswith("letter "):
            i += 1
            continue
        name = ln.split()[1]
        rows = []
        i += 1
        while i < len(lines) and lines[i].strip():
            rows.append(lines[i].strip())
            i += 1
        assert len(rows) == 9, name
        w = max(len(r) for r in rows)
        out.append("_sg_%s:" % name)
        out.append("        .byte   %d," % w + ",".join(
            "$%02X" % sum(0x80 >> k for k, c in enumerate(r) if c == "#") for r in rows))
        names.append("_sg_" + name)
    out.insert(1, "        .export " + ", ".join(names))
    return "\n".join(out) + "\n"


def pictures(path):
    lines = [ln.rstrip("\n") for ln in open(path) if not ln.startswith("# ")] + [""]
    out = ['        .segment "RODATA"']
    exports = []
    i = 0
    while i < len(lines):
        ln = lines[i].strip()
        if not (ln.startswith("pic ") or ln.startswith("sprite ")):
            i += 1
            continue
        kind, name = ln.split()[:2]
        rows = []
        i += 1
        while lines[i].strip():
            rows.append(lines[i].strip())
            i += 1
        if kind == "pic":
            w = max(len(r) for r in rows)
            assert w % 8 == 0, name
            out.append("_pic_%s:" % name)
            out.append("        .byte   %d,%d" % (w // 8, len(rows)))
            for r in rows:
                r = r.ljust(w, ".")
                out.append("        .byte   " + ",".join(
                    "$%02X" % sum(0x80 >> k for k, c in enumerate(r[b:b + 8]) if c == "#")
                    for b in range(0, w, 8)))
            exports.append("_pic_" + name)
        else:
            assert len(rows) <= 21 and all(len(r) <= 24 for r in rows), name
            data = []
            for r in rows + [""] * (21 - len(rows)):
                r = r.ljust(24, ".")
                data += [sum(0x80 >> k for k, c in enumerate(r[b:b + 8]) if c == "#") for b in (0, 8, 16)]
            out.append("_spr_%s:" % name)
            for b in range(0, 63, 21):
                out.append("        .byte   " + ",".join("$%02X" % v for v in data[b:b + 21]))
            exports.append("_spr_" + name)
    out.insert(0, "        .export " + ", ".join(exports))
    out.insert(0, "; Made by tools/c64_art.py from art/c64/%s; do not edit." % os.path.basename(path))
    return "\n".join(out) + "\n"


TILE_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ?"


def tiles(path):
    glyphs = read_glyphs(path, 5, 6)
    assert sorted(glyphs) == sorted(map(ord, TILE_CHARS)), "tiles.txt needs A to Z and ?"
    out = ["; Made by tools/c64_art.py from art/c64/tiles.txt; do not edit.",
           "        .export _tile_glyphs", '        .segment "RODATA"', "_tile_glyphs:"]
    for ch in TILE_CHARS:
        out.append("        .byte   " + ",".join("$%02X" % bold(r) for r in glyphs[ord(ch)]) + "  ; " + ch)
    return "\n".join(out) + "\n"


def main():
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "chars.bin"), "wb") as f:
        f.write(charset())
    with open(os.path.join(outdir, "splash_art.s"), "w") as f:
        f.write(splash(os.path.join(ART, "splash.txt")))
    art = {p[:-4]: pictures(os.path.join(C64ART, p)) for p in sorted(os.listdir(C64ART)) if p.endswith(".pic")}
    art["gridwords"] = art.get("gridwords", "") + tiles(os.path.join(C64ART, "tiles.txt"))
    for name, text in art.items():
        with open(os.path.join(outdir, name + "_art.s"), "w") as f:
            f.write(text)


if __name__ == "__main__":
    main()
