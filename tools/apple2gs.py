#!/usr/bin/env python3
"""Makes the Apple IIgs program's files and disk from build.sh's output (build/gs/).

- PARLORNET: the GS/OS application (file type S16, $B3), an OMF load file with two segments. The
  first is static code: gsshell.s (shell.bin, position independent) followed by the program it starts,
  as a length word, the page for $0800 and the program for $4000 (parlor.main). The second is the
  direct page and stack GS/OS gives the application (4K in bank 0).
- PARLOR.DAT: every overlay, each starting on a page; the page for $0800 holds where each one is, as
  the overlay table that PARLOR.SYSTEM fills in under ProDOS (ovl.c).
- parlornet-gs.po: an 800K ProDOS volume (ProDOS block order) named PARLORNET holding those two files,
  for a 3.5-inch disk or for copying them to the player's own GS/OS disk. GS/OS and Marinetti are not
  on it.

The OMF is version 2.0, as the System Loader reads it (Apple IIgs GS/OS Reference, appendix F): a
segment header, then an LCONST record with the bytes (or a DS record of zeros) and an END record.

Usage: apple2gs.py [build/gs]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from apple2_disk import pad, pages, prodos_volume  # noqa: E402

OVLTAB, DISKID = 0x21, 0x0A     # in the page at $0800, as in the boot block
STACK = 0x1000                  # the direct page and stack segment


def omf_segment(segnum, kind, name, body, length, banksize=0x10000):
    """One OMF 2.0 segment: the header (with its load name, blank, and segment name) and body."""
    loadname = b" " * 10
    segname = bytes([len(name)]) + name.encode()
    disp_name = 0x2C
    disp_data = disp_name + len(loadname) + len(segname)
    h = bytearray(disp_data)
    total = disp_data + len(body)
    h[0x00:0x04] = total.to_bytes(4, "little")      # BYTECNT
    h[0x04:0x08] = (0).to_bytes(4, "little")        # RESSPC
    h[0x08:0x0C] = length.to_bytes(4, "little")     # LENGTH in memory
    h[0x0D] = 0                                     # LABLEN: names are counted strings
    h[0x0E] = 4                                     # NUMLEN
    h[0x0F] = 2                                     # VERSION
    h[0x10:0x14] = banksize.to_bytes(4, "little")   # BANKSIZE
    h[0x14:0x16] = kind.to_bytes(2, "little")       # KIND
    h[0x18:0x1C] = (0).to_bytes(4, "little")        # ORG
    h[0x1C:0x20] = (0).to_bytes(4, "little")        # ALIGN
    h[0x20] = 0                                     # NUMSEX
    h[0x22:0x24] = segnum.to_bytes(2, "little")     # SEGNUM
    h[0x24:0x28] = (0).to_bytes(4, "little")        # ENTRY
    h[0x28:0x2A] = disp_name.to_bytes(2, "little")  # DISPNAME
    h[0x2A:0x2C] = disp_data.to_bytes(2, "little")  # DISPDATA
    h[disp_name:disp_name + 10] = loadname
    h[disp_name + 10:disp_data] = segname
    return bytes(h) + body


def lconst(data):
    return b"\xF2" + len(data).to_bytes(4, "little") + data + b"\x00"


def ds(n):
    return b"\xF1" + n.to_bytes(4, "little") + b"\x00"


def build(d):
    shell = open(os.path.join(d, "shell.bin"), "rb").read()
    main = open(os.path.join(d, "parlor.main"), "rb").read()
    names = [o.split(":")[0] for o in open(os.path.join(d, "overlays.txt")).read().split()]
    ovls = [open(os.path.join(d, "parlor." + n), "rb").read() for n in names]
    for n, o in zip(names, ovls):
        assert pages(o) <= 0x2F, "overlay %s is too big: %d bytes" % (n, len(o))
    assert len(main) <= 0x8000 and len(ovls) <= 16

    dat = b""
    page8 = bytearray(256)
    for i, o in enumerate(ovls):
        at = len(dat) // 256
        page8[OVLTAB + i * 4:OVLTAB + i * 4 + 4] = bytes([at & 255, at >> 8, pages(o), 0])
        dat += pad(o)
    page8[DISKID] = 0                   # from files: PRODOS in prodos.h

    code = shell + len(main).to_bytes(2, "little") + bytes(page8) + main
    assert len(code) < 0x10000
    app = (omf_segment(1, 0x0000, "main", lconst(code), len(code))
           + omf_segment(2, 0x0012, "stack", ds(STACK), STACK))
    with open(os.path.join(d, "PARLORNET"), "wb") as f:
        f.write(app)
    with open(os.path.join(d, "PARLOR.DAT"), "wb") as f:
        f.write(dat)
    vol = prodos_volume("PARLORNET", [("PARLORNET", 0xB3, 0x0000, app),
                                      ("PARLOR.DAT", 0x06, 0x0000, dat)], blocks=1600)
    with open(os.path.join(d, "parlornet-gs.po"), "wb") as f:
        f.write(vol)


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build", "gs"))
