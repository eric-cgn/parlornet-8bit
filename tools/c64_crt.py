#!/usr/bin/env python3
"""Makes the Commodore 64 client's cartridge, build/c64cart/parlornet.crt, from build.sh's output (build/c64cart):
a Magic Desk cartridge (the CRT file's hardware type 19) of 8K banks at $8000, the bank chosen by a write to
$DE00, $80 switching the cartridge off. It writes nothing else in the I/O areas, so a SwiftLink at $DF00 or $DF80
and the user port stay free for a modem, through a port expander.

  bank 0       cart.bin (the start and the loader, cart.s), and at $8800 the directory: five bytes a file, its
               bank, its offset in the bank and its length (with its load address; 0 for none)
  bank 1 on    the files one after another, each as a C64 file is (its load address, then its bytes), across
               banks as they come: the overlays in the order of overlays.txt (directory 0 to 12), the character
               set (13) and the program (14), packed as on the disk (c64_disk.py), to unpack where they run

The cartridge is 32K, 64K or 128K, the smallest the files fit, as Magic Desk cartridges came. The same files
always give the same cartridge.

Usage: c64_crt.py [build/c64cart]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from c64_disk import packed, prg  # noqa: E402

BANK = 0x2000
DIR_AT = 0x0800                 # in bank 0
FILES = 15                      # the directory's places: overlays 0 to 12, the character set, the program


def crt(banks, name):
    head = b"C64 CARTRIDGE   " + struct.pack(">IHHBB6x", 0x40, 0x0100, 19, 0, 1)
    head += name.encode().ljust(32, b"\0")
    out = bytearray(head)
    for n, b in enumerate(banks):
        out += b"CHIP" + struct.pack(">IHHHH", 0x10 + BANK, 0, n, 0x8000, BANK) + b
    return bytes(out)


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "build/c64cart"
    names = open(os.path.join(d, "overlays.txt")).read().split()
    ovl = int(open(os.path.join(d, "ovl_addr.txt")).read().strip(), 16)
    files = []
    for n in names:
        p = os.path.join(d, "parlor." + n)
        files.append(packed(open(p, "rb").read(), ovl, 0xC000) if os.path.getsize(p) else b"")
    assert len(files) == 13, "13 overlays"
    files.append(prg(0xC000, open(os.path.join(d, "chars.bin"), "rb").read()))
    files.append(packed(open(os.path.join(d, "parlor.main"), "rb").read(), 0x1500, 0xC000))
    data = bytearray()
    directory = bytearray()
    for f in files:
        at = BANK + len(data)                   # (from bank 1 on)
        directory += bytes([at // BANK, at % BANK & 0xFF, at % BANK >> 8, len(f) & 0xFF, len(f) >> 8]) if f \
            else bytes(5)
        data += f
    assert len(directory) == FILES * 5
    bank0 = bytearray(open(os.path.join(d, "cart.bin"), "rb").read())
    assert len(bank0) <= DIR_AT, "cart.bin runs into the directory"
    bank0 = bank0.ljust(DIR_AT, b"\xff") + directory
    rom = bytes(bank0.ljust(BANK, b"\xff")) + bytes(data)
    nbanks = next(n for n in (4, 8, 16) if n * BANK >= len(rom))
    rom = rom.ljust(nbanks * BANK, b"\xff")
    banks = [rom[k * BANK:(k + 1) * BANK] for k in range(nbanks)]
    open(os.path.join(d, "parlornet.crt"), "wb").write(crt(banks, "PARLORNET"))
    open(os.path.join(d, "layout.txt"), "w").write(
        "%dK cartridge, %d banks; the files take %d bytes of banks 1 on\n" % (nbanks * 8, nbanks, len(data)))


if __name__ == "__main__":
    main()
