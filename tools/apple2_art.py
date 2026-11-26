#!/usr/bin/env python3
"""Makes the Apple IIe client's Hi-Res art: art/font.txt -> font.s, art/*.spr -> art.s and art/splash.txt ->
splash_art.s.

Font: each glyph is a line naming the character in quotes, then 8 rows of 5 pixels; a row becomes one
byte with the pixels in bits 1 to 5 (bit 0 is the leftmost pixel on the screen, as Hi-Res has it).

Sprites (*.spr): a line `color NAME [both]` or `mono NAME [both]`, then its rows, then a blank line.
- `mono` rows are pixels at full resolution (280 across): '#' on, '.' off. 'h' as the first character
  of a row's first byte sets nothing; the palette bit is never set for mono sprites.
- `color` rows are at color resolution (140 across), one character for two pixels: '.' black,
  'W' white, 'G' green, 'V' violet, 'O' orange, 'B' blue. Within one screen byte the colors must all
  come from one group (green and violet, or orange and blue), since one bit picks it for all 7 pixels.
A sprite is drawn at a byte column; on an odd column the colors of a color sprite move by a pixel,
so `both` also makes the version for odd columns (NAME_o). The data is the width in bytes, the height,
then the rows, top first.

Title letters (splash.txt): a line `letter X`, then its rows of blocks ('#' filled). splash.c draws each
block 4 color pixels across and 6 lines down, with a black gap on its right and bottom edge. A letter's data (_sg_X) is its width in blocks, then a
byte a row, the blocks in bits 7 down.
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art")
A2ART = os.path.join(ART, "apple2")


def font(path):
    glyphs = {}
    lines = [ln.rstrip("\n") for ln in open(path) if not ln.startswith("# ")]
    i = 0
    while i < len(lines):
        ln = lines[i].strip()
        if not ln:
            i += 1
            continue
        ch = chr(int(ln[1:], 16)) if ln.startswith("$") else ln[1]
        rows = lines[i + 1:i + 9]
        assert len(rows) == 8 and all(len(r) == 5 for r in rows), ch
        glyphs[ch] = [sum(1 << (b + 1) for b, c in enumerate(r) if c == "#") for r in rows]
        i += 9
    out = ["; Made by tools/apple2_art.py from art/font.txt; do not edit.",
           "        .export _font", '        .segment "RODATA"', "_font:"]
    for code in range(32, 129):        # ASCII, then $7F Open-Apple and $80 Closed-Apple
        g = glyphs[chr(code)]
        out.append("        .byte   " + ",".join("$%02X" % b for b in g) + "  ; %r" % chr(code))
    return "\n".join(out) + "\n"


COLOURS = {".": (0, None), "W": (3, None), "G": (1, 0), "V": (2, 0), "O": (1, 1), "B": (2, 1)}


def colour_rows(rows, odd):
    """Rows of 140-res color letters -> rows of bytes, for a sprite whose first pixel is at an even (or
    odd) screen column."""
    out = []
    for r in rows:
        px = []      # (on, group) per 280-res pixel
        for k, c in enumerate(r):
            kind, group = COLOURS[c]
            if kind == 0:
                px += [(0, None), (0, None)]
            elif kind == 3:
                px += [(1, None), (1, None)]
            else:
                # Palette 0: an on pixel at an even screen column is violet, at an odd one green.
                # Palette 1: even blue, odd orange. kind 1 (green/orange) wants the odd column.
                first_even = not odd
                want_odd = kind == 1
                left_on = (first_even and not want_odd) or (not first_even and want_odd)
                px += [(1 if left_on else 0, group), (0 if left_on else 1, group)]
        nbytes = (len(px) + 6) // 7
        px += [(0, None)] * (nbytes * 7 - len(px))
        row = []
        for b in range(nbytes):
            cell = px[b * 7:b * 7 + 7]
            groups = {g for on, g in cell if g is not None}
            if len(groups) > 1:
                raise ValueError("mixed color groups in one byte: %r" % r)
            v = sum(on << i for i, (on, g) in enumerate(cell))
            if groups == {1}:
                v |= 0x80
            row.append(v)
        out.append(row)
    return out


def mono_rows(rows):
    out = []
    for r in rows:
        n = (len(r) + 6) // 7
        r = r.ljust(n * 7, ".")
        out.append([sum(1 << i for i, c in enumerate(r[b * 7:b * 7 + 7]) if c == "#") for b in range(n)])
    return out


def emit(name, rows):
    w = max(len(r) for r in rows)
    rows = [r + [0] * (w - len(r)) for r in rows]
    lines = ["_spr_%s:" % name, "        .byte   %d,%d" % (w, len(rows))]
    for r in rows:
        lines.append("        .byte   " + ",".join("$%02X" % b for b in r))
    return lines, "_spr_%s" % name


def sprites(paths):
    out = ["; Made by tools/apple2_art.py from art/*.spr; do not edit."]
    body, names = [], []
    for path in paths:
        lines = [ln.rstrip("\n") for ln in open(path)]
        i = 0
        while i < len(lines):
            ln = lines[i].strip()
            if not ln or ln.startswith("#"):
                i += 1
                continue
            words = ln.split()
            kind, name, flags = words[0], words[1], words[2:]
            rows = []
            i += 1
            while i < len(lines) and lines[i].strip():
                rows.append(lines[i].strip())
                i += 1
            seg = "RODATA"
            for f in flags:
                if f.startswith("seg="):
                    seg = f[4:]
            body.append('        .segment "%s"' % seg)
            if kind == "mono":
                b, n = emit(name, mono_rows(rows))
                body += b
                names.append(n)
            else:
                b, n = emit(name, colour_rows(rows, False))
                body += b
                names.append(n)
                if "both" in flags:
                    b, n = emit(name + "_o", colour_rows(rows, True))
                    body += b
                    names.append(n)
    out += ["        .export " + n for n in names]
    return "\n".join(out + body) + "\n"


def splash(path):
    letters, lines, i = {}, [ln.rstrip("\n") for ln in open(path)], 0
    while i < len(lines):
        ln = lines[i].strip()
        if not ln.startswith("letter "):
            i += 1
            continue
        rows = []
        i += 1
        while i < len(lines) and lines[i].strip():
            rows.append(lines[i].strip())
            i += 1
        letters[ln.split()[1]] = rows
    out = ["; Made by tools/apple2_art.py from art/splash.txt; do not edit.",
           "        .export " + ", ".join("_sg_" + k for k in letters), '        .segment "RODATA"']
    for k, rows in letters.items():
        assert len({len(r) for r in rows}) == 1 and len(rows[0]) <= 8, k
        out.append("_sg_%s:  .byte   %d," % (k, len(rows[0])) + ",".join(
            "$%02X" % sum(0x80 >> j for j, c in enumerate(r) if c == "#") for r in rows))
    return "\n".join(out) + "\n"


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build")
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "font.s"), "w") as f:
        f.write(font(os.path.join(ART, "font.txt")))
    with open(os.path.join(outdir, "splash_art.s"), "w") as f:
        f.write(splash(os.path.join(ART, "splash.txt")))
    for spr in sorted(p for p in os.listdir(A2ART) if p.endswith(".spr")):
        with open(os.path.join(outdir, spr[:-4] + "_art.s"), "w") as f:
            f.write(sprites([os.path.join(A2ART, spr)]))


if __name__ == "__main__":
    main()
