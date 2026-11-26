#!/usr/bin/env python3
"""ParlorNet's 8-bit help pages (src/core/help): each place's text, wrapped at 40 columns, with the machine's
own key names, as the program reads it (frame.c's pages): its lines, each ended by a 0, a line "\\f" before
each page after the first, and a 4 after the last line.

usage: help.py apple2 OUT   writes OUT, one file of every help, each starting on a page of its own, after a
                            first page that is the index: for help k, its first page and its pages (2 bytes)
       help.py c64 DIR      writes DIR/help<k>, one file each (tools/c64_disk.py makes them PN.HELP<k>)

The helps are numbered as the program asks for them: 0 the lobby, 1 to 10 the games in the order of the
catalog (games.c), 11 the tutorial."""
import os
import sys

HELP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "core", "help")
ORDER = ["lobby", "reversi", "backgammon", "gridwords", "checkers", "mancala", "fiveinarow", "dotsandboxes",
         "navalcombat", "cannonfire", "rummytile", "tutorial"]
WIDTH = 40
ROWS = {"apple2": 18, "c64": 20}       # the rows of a page, from line 16 to the message line

KEYS = {"apple2": {"RET": "Return", "ESC": "Esc"},
        "c64": {"RET": "RETURN", "ESC": "RUN/STOP"}}


def tokens(text, machine):
    """The text as (shown, width) pieces: a key's name is one piece, marked as a key."""
    out = []
    i = 0
    while i < len(text):
        if text.startswith("{OA}", i):
            out.append((bytes([2, 0x7F]) + text[i + 4].encode(), 2))
            i += 5
        elif text[i] == "{":
            j = text.index("}", i)
            name = KEYS[machine][text[i + 1:j]]
            out.append((bytes([len(name)]) + name.encode(), len(name)))
            i = j + 1
        else:
            out.append((text[i].encode(), 1))
            i += 1
    return out


def wrap(text, machine):
    """A paragraph as lines of at most WIDTH columns, cut at spaces."""
    lines, line, width, word, wwidth = [], b"", 0, b"", 0
    for piece, w in tokens(text, machine) + [(b" ", 1)]:
        if piece == b" ":
            if width + (1 if width else 0) + wwidth > WIDTH:
                lines.append(line)
                line, width = word, wwidth
            elif wwidth:
                line, width = (line + b" " + word, width + 1 + wwidth) if width else (word, wwidth)
            word, wwidth = b"", 0
        else:
            word, wwidth = word + piece, wwidth + w
    if width:
        lines.append(line)
    return lines or [b""]


def help_bytes(name, machine):
    pages = open(os.path.join(HELP, name + ".txt")).read().split("\n---\n")
    out = b""
    for n, page in enumerate(pages):
        rows = page.rstrip("\n").split("\n")
        title, rows = rows[0], rows[1:]
        if n == 0:
            out += title.encode() + b"\0"
        else:
            out += b"\f\0"
        lines = [l for r in rows for l in wrap(r, machine)]
        assert len(lines) <= ROWS[machine], "%s, page %d: %d rows" % (name, n + 1, len(lines))
        out += b"".join(l + b"\0" for l in lines)
    return out + b"\x04"


def main():
    machine, out = sys.argv[1], sys.argv[2]
    helps = [help_bytes(n, machine) for n in ORDER]
    if machine == "apple2":
        index = bytearray(256)
        data = b""
        for k, h in enumerate(helps):
            n = (len(h) + 255) // 256
            assert n <= 11, "%s is %d bytes" % (ORDER[k], len(h))
            index[k * 2:k * 2 + 2] = bytes([1 + len(data) // 256, n])
            data += h.ljust(n * 256, b"\0")
        open(out, "wb").write(bytes(index) + data)
    else:
        os.makedirs(out, exist_ok=True)
        for k, h in enumerate(helps):
            assert len(h) <= 11 * 256, "%s is %d bytes" % (ORDER[k], len(h))
            open(os.path.join(out, "help%d" % k), "wb").write(h)


if __name__ == "__main__":
    main()
