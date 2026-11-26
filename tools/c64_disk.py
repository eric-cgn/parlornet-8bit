#!/usr/bin/env python3
"""Makes the Commodore 64 client's disk image, build/c64/parlornet.d64, from build.sh's output (build/c64):
a 1541 disk (35 tracks, 170K) named PARLORNET with a C128 boot sector (boot128.bin, on track 1, sector 0: a
C128 boots the disk and goes to 64 mode to start the program) and

  PARLORNET     stub.bin, which starts the rest: LOAD"*",8,1 starts it with no RUN (it loads over BASIC's
                idle-loop vector), and LOAD"*",8 then RUN starts it too
  PN.BOOT       the loader (loader.bin, at $C800)
  PN.CHARS      the character set (chars.bin, at $C000)
  PN.MAIN       the program (parlor.main up to its zeroed data, for $1500): what starts it and runs the start
                screen, the settings and the console
  PN.PARLOR     the lobby, the tables, the parlor's protocol, the catalog and the bitmap's buttons and
                pointer (the rest of parlor.main, from PCODE, after the zeroed data), loaded before the
                first sign-in
  PN.<NAME>     each overlay (parlor.<name>, for the overlay address), in the order of overlays.txt
  PARLOR.CFG    the settings as they start (a SEQ file; the program writes it again when they change)

The program and the overlays are packed (c64_pack.py): each file loads where its load address says, at the top
of the memory it is for, and ends with that address again; the program unpacks it from there to where it
runs. Each file is laid out as the 1541's DOS lays files out: from the tracks next to the directory outward, ten
sectors apart on a track, so that the drive reads one while the C64 takes the last. The same files always
give the same image.

Usage: c64_disk.py [build/c64]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from c64_pack import pack, place  # noqa: E402

TRACKS = 35


def spt(t):
    return 21 if t <= 17 else 19 if t <= 24 else 18 if t <= 30 else 17


def offset(t, s):
    return (sum(spt(k) for k in range(1, t)) + s) * 256


class Disk:
    def __init__(self, name, ident):
        self.img = bytearray(sum(spt(t) for t in range(1, TRACKS + 1)) * 256)
        self.used = {t: set() for t in range(1, TRACKS + 1)}
        self.entries = []
        self.name, self.ident = name, ident
        self.used[18] |= {0, 1}
        self.used[1].add(0)             # the C128's boot sector

    def free_sector(self, t, start):
        n = spt(t)
        for k in range(n):
            s = (start + k) % n
            if s not in self.used[t]:
                return s
        return None

    def add(self, name, data, kind=0x82):
        """A file: kind $82 PRG, $81 SEQ. Returns its size in blocks."""
        chunks = [data[i:i + 254] for i in range(0, len(data), 254)] or [b""]
        order = [17 - k for k in range(17)] + list(range(19, TRACKS + 1))
        chain = []
        t_i, s = 0, 0
        for _ in chunks:
            while True:
                t = order[t_i]
                found = self.free_sector(t, s)
                if found is not None:
                    break
                t_i += 1
                s = 0
            self.used[t].add(found)
            chain.append((t, found))
            s = found + 10
        for k, (t, s) in enumerate(chain):
            block = bytearray(256)
            if k + 1 < len(chain):
                block[0], block[1] = chain[k + 1]
            else:
                block[0], block[1] = 0, len(chunks[k]) + 1
            block[2:2 + len(chunks[k])] = chunks[k]
            self.img[offset(t, s):offset(t, s) + 256] = block
        self.entries.append((name, kind, chain[0], len(chain)))
        return len(chain)

    def finish(self):
        # the directory: 18/1, then three sectors on
        per = 8
        secs = [1]
        while len(secs) * per < len(self.entries):
            s = (secs[-1] + 3) % spt(18)
            while s in self.used[18] or s in secs:
                s = (s + 1) % spt(18)
            secs.append(s)
            self.used[18].add(s)
        for i, s in enumerate(secs):
            block = bytearray(256)
            if i + 1 < len(secs):
                block[0], block[1] = 18, secs[i + 1]
            else:
                block[0], block[1] = 0, 0xFF
            for j, (name, kind, (t0, s0), size) in enumerate(self.entries[i * per:(i + 1) * per]):
                e = j * 32
                block[e + 2] = kind
                block[e + 3], block[e + 4] = t0, s0
                nm = name.encode("ascii")
                block[e + 5:e + 21] = nm + b"\xA0" * (16 - len(nm))
                block[e + 30], block[e + 31] = size & 0xFF, size >> 8
            self.img[offset(18, s):offset(18, s) + 256] = block
        # the BAM
        bam = bytearray(256)
        bam[0], bam[1], bam[2] = 18, 1, 0x41
        for t in range(1, TRACKS + 1):
            free = [s for s in range(spt(t)) if s not in self.used[t]]
            bits = sum(1 << s for s in free)
            bam[4 * t:4 * t + 4] = bytes([len(free), bits & 0xFF, (bits >> 8) & 0xFF, bits >> 16])
        nm = self.name.encode("ascii")
        bam[0x90:0xA0] = nm + b"\xA0" * (16 - len(nm))
        bam[0xA0:0xA2] = b"\xA0\xA0"
        bam[0xA2:0xA4] = self.ident.encode("ascii")
        bam[0xA4] = 0xA0
        bam[0xA5:0xA7] = b"2A"
        bam[0xA7:0xAB] = b"\xA0" * 4
        self.img[offset(18, 0):offset(18, 0) + 256] = bam
        return bytes(self.img)


def prg(addr, data):
    return bytes([addr & 0xFF, addr >> 8]) + data


def packed(data, start, end):
    """data packed, as a file that loads at the top of start..end and unpacks to start in place (checked
    here, as the C64 does it)."""
    p = pack(data)
    load = place(p, start, end - 2)
    f = p + bytes([load & 0xFF, load >> 8])
    mem = bytearray(65536)
    mem[load:load + len(f)] = f
    src, dst = load, start
    while mem[src]:
        t = mem[src]
        src += 1
        if t < 0x80:
            mem[dst:dst + t] = mem[src:src + t]
            src += t
            dst += t
            continue
        if t < 0xC0:
            m, d = (t & 0x3F) + 2, mem[src] + 1
            src += 1
        else:
            m, d = (t & 0x3F) + 3, mem[src] | (mem[src + 1] << 8)
            src += 2
        for _ in range(m):
            mem[dst] = mem[dst - d]
            dst += 1
    assert bytes(mem[start:start + len(data)]) == data, "packed file does not unpack in place"
    return prg(load, f)


def default_cfg():
    """PARLOR.CFG as c64/setup.c packs the settings they start with: SwiftLink compatible at 19200, no flow
    control, no echo, color, the title screen, tone dialing, the init string ATZ, the number
    parlornet.org:6501, no mouse, the SwiftLink at $DE00 (byte 71: 0)."""
    b = bytearray(72)
    b[0:10] = bytes([ord("P"), ord("C"), 4, 2, 5, 0, 0, 0, 1, 0])     # (version 4, as setup.c's CFG_VER)
    b[10:13] = b"ATZ"
    num = b"parlornet.org:6501"
    b[10 + 24:10 + 24 + len(num)] = num
    return bytes(b)


def labels(path):
    """ld65's label file (-Ln): name to address."""
    out = {}
    for line in open(path):
        parts = line.split()
        if len(parts) == 3:
            out[parts[2].lstrip(".")] = int(parts[1], 16)
    return out


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "build/c64"
    syms = labels(os.path.join(d, "parlor.lbl"))
    names = open(os.path.join(d, "overlays.txt")).read().split()
    ovl = int(open(os.path.join(d, "ovl_addr.txt")).read().strip(), 16)
    disk = Disk("PARLORNET", "PN")
    layout = []
    def add(name, data, kind=0x82):
        layout.append("%-16s %3d blocks" % (name, disk.add(name, data, kind)))
    boot = open(os.path.join(d, "boot128.bin"), "rb").read()
    disk.img[offset(1, 0):offset(1, 0) + 256] = boot
    add("PARLORNET", prg(0x02A7, open(os.path.join(d, "stub.bin"), "rb").read()))
    add("PN.BOOT", prg(0xC800, open(os.path.join(d, "loader.bin"), "rb").read()))
    add("PN.CHARS", prg(0xC000, open(os.path.join(d, "chars.bin"), "rb").read()))
    # parlor.main: the program to its zeroed data, then (the zeroed data not in the file) PCODE and PRODATA
    prog = open(os.path.join(d, "parlor.main"), "rb").read()
    cut = syms["__BSS_RUN__"] - 0x1500
    late = syms["__PCODE_RUN__"]
    assert late == syms["__BSS_RUN__"] + syms["__BSS_SIZE__"]
    assert len(prog) == cut + syms["__PRODATA_RUN__"] + syms["__PRODATA_SIZE__"] - late
    add("PN.MAIN", packed(prog[:cut], 0x1500, 0xC000))
    add("PN.PARLOR", packed(prog[cut:], late, 0xC000))
    for n in names:
        p = os.path.join(d, "parlor." + n)
        if os.path.exists(p) and os.path.getsize(p):
            add("PN." + n.upper(), packed(open(p, "rb").read(), ovl, 0xC000))
    for k in range(12):                 # the help pages (tools/help.py), loaded into the reply buffer
        add("PN.HELP%d" % k, prg(0x0800, open(os.path.join(d, "help", "help%d" % k), "rb").read()))
    add("PARLOR.CFG", default_cfg(), 0x81)
    img = disk.finish()
    free = sum(spt(t) - len(disk.used[t]) for t in range(1, TRACKS + 1) if t != 18)
    layout.append("%d blocks free" % free)
    open(os.path.join(d, "parlornet.d64"), "wb").write(img)
    open(os.path.join(d, "layout.txt"), "w").write("\n".join(layout) + "\n")


if __name__ == "__main__":
    main()
