#!/usr/bin/env python3
"""Makes the Apple IIe client's disk images from build.sh's output (build/).

The disks hold no operating system. Linear sector n is track n // 16, physical sector n % 16:
- sectors 0-5: the boot block (boot.bin), which the Disk II boot ROM loads at $0800 and which reads the
  rest itself;
- then the program (parlor.main, loaded at $4000), the vectors (parlor.vec, at $FF00) and the Double
  Hi-Res kit (parlor.dhr, at $2800, which the program copies into the auxiliary memory);
- then the overlays (parlor.<name>, each loaded at $D000 when needed), in the order of overlays.txt.

The boot block's load table (at $080D) and overlay table (at $0821) are filled in here. When the
overlays do not all fit on one disk, those named in DISK2 go on a second disk, whose boot block only
says to boot disk 1; the program asks for the other disk when it needs it.

The images are .dsk files in DOS 3.3 sector order, as emulators and disk tools expect: physical sector p
of a track is the file's sector DOS_SKEW[p] of that track.

It also makes the files for running the program under ProDOS, in build/prodos/: PARLOR.SYSTEM
(prodsys.bin with its table filled in), PARLOR.DAT (the program, its vectors, the kit and every overlay,
each starting on a page) and parlor-files.po, a 140K ProDOS volume (ProDOS block order) named PARLORNET that
holds just those two files, for copying them to a ProDOS disk. ProDOS itself is not on it.

Usage: apple2_disk.py [build]   (writes parlornet.dsk, and parlornet-2.dsk when needed, there,
and layout.txt)
"""
import os
import re
import sys

TRACKS = 35
SECTORS = TRACKS * 16
DOS_SKEW = [0x0, 0x7, 0xE, 0x6, 0xD, 0x5, 0xC, 0x4, 0xB, 0x3, 0xA, 0x2, 0x9, 0x1, 0x8, 0xF]
BOOT_SECTORS = 6
LOADTAB = 0x0D          # offsets in the boot block (from $0800)
START = 0x1D
OVLTAB = 0x21
DISKID = 0x0A
# Overlays that go on disk 2 when disk 1 is full, the least played first.
DISK2 = ["cannonfire", "rummytile", "navalcombat", "dotsandboxes", "fiveinarow", "mancala"]


def pages(data):
    return (len(data) + 255) // 256


def pad(data):
    return data + bytes(pages(data) * 256 - len(data))


def image(sectors):
    """Linear sectors (bytes of 256) -> a .dsk image."""
    out = bytearray(SECTORS * 256)
    for lin, data in sectors.items():
        t, p = divmod(lin, 16)
        off = t * 4096 + DOS_SKEW[p] * 256
        out[off:off + 256] = data
    return bytes(out)


def place(sectors, lin, data):
    data = pad(data)
    for i in range(0, len(data), 256):
        sectors[lin + i // 256] = data[i:i + 256]
    return lin + len(data) // 256


def build(d):
    boot = open(os.path.join(d, "boot.bin"), "rb").read()
    assert len(boot) == BOOT_SECTORS * 256 and boot[0] == BOOT_SECTORS
    main = open(os.path.join(d, "parlor.main"), "rb").read()
    vec = open(os.path.join(d, "parlor.vec"), "rb").read()
    kit = open(os.path.join(d, "parlor.dhr"), "rb").read()
    assert pages(kit) <= 0x18
    names = [o.split(":")[0] for o in open(os.path.join(d, "overlays.txt")).read().split()]
    ovls = [open(os.path.join(d, "parlor." + n), "rb").read() for n in names]
    for n, o in zip(names, ovls):
        assert pages(o) <= 0x2F or n == "help", "overlay %s is too big: %d bytes" % (n, len(o))
    assert pages(main) <= 0x80

    disk1, disk2 = {}, {}
    lin = BOOT_SECTORS
    main_at = lin
    lin = place(disk1, lin, main)
    vec_at = lin
    lin = place(disk1, lin, vec)
    kit_at = lin
    lin = place(disk1, lin, kit)
    # Overlays: on disk 1 while there is room, the rest (DISK2 names) on disk 2.
    total1 = lin + sum(pages(o) for o in ovls)
    on2 = set()
    if total1 > SECTORS:
        for n in DISK2:
            if n in names and total1 > SECTORS:
                on2.add(n)
                total1 -= pages(ovls[names.index(n)])
        assert total1 <= SECTORS, "the overlays do not fit on two disks"
    lin2 = BOOT_SECTORS
    table = []
    for n, o in zip(names, ovls):
        if n in on2:
            table.append((lin2, pages(o), 2))
            lin2 = place(disk2, lin2, o)
        else:
            table.append((lin, pages(o), 1))
            lin = place(disk1, lin, o)
    assert lin <= SECTORS and lin2 <= SECTORS
    assert len(table) <= 20

    b = bytearray(boot)
    b[LOADTAB:LOADTAB + 12] = bytes([main_at & 255, main_at >> 8, pages(main), 0x40,
                                     vec_at & 255, vec_at >> 8, 1, 0xFF,
                                     kit_at & 255, kit_at >> 8, pages(kit), 0x28])
    b[START:START + 2] = bytes([0x00, 0x40])
    for i, (at, n, disk) in enumerate(table):
        b[OVLTAB + i * 4:OVLTAB + i * 4 + 4] = bytes([at & 255, at >> 8, n, disk])
    b[DISKID] = 1
    place(disk1, 0, bytes(b))
    out = {"parlornet.dsk": image(disk1)}
    if on2:
        b2 = bytearray(b)
        b2[LOADTAB:LOADTAB + 16] = bytes(16)
        b2[DISKID] = 2
        place(disk2, 0, bytes(b2))
        out["parlornet-2.dsk"] = image(disk2)
    for name, data in out.items():
        with open(os.path.join(d, name), "wb") as f:
            f.write(data)
    with open(os.path.join(d, "layout.txt"), "w") as f:
        f.write("disk 1 boot 0 %d\n" % BOOT_SECTORS)
        f.write("disk 1 main %d %d %d\n" % (main_at, pages(main), len(main)))
        f.write("disk 1 vec %d 1 %d\n" % (vec_at, len(vec)))
        f.write("disk 1 dhr %d %d %d\n" % (kit_at, pages(kit), len(kit)))
        for n, o, (at, cnt, disk) in zip(names, ovls, table):
            f.write("disk %d %s %d %d %d\n" % (disk, n, at, cnt, len(o)))
        f.write("used 1 %d\n" % lin)
        if on2:
            f.write("used 2 %d\n" % lin2)
    return out


# ---- ProDOS ---------------------------------------------------------------------------------------
PRODOS_TOP = 0xBF00     # the program must end below ProDOS's global page
NPAGES_AT, OVLTAB_AT, KPAGES_AT = 5, 6, 86     # in prodsys.bin


def program_end(d):
    """The first address after the program's BSS, from the link map."""
    for line in open(os.path.join(d, "parlor.map")):
        m = re.match(r"^BSS\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s", line)
        if m:
            return int(m.group(2), 16) + 1
    raise ValueError("no BSS segment in parlor.map")


def prodos_files(d):
    main = open(os.path.join(d, "parlor.main"), "rb").read()
    vec = open(os.path.join(d, "parlor.vec"), "rb").read()
    kit = open(os.path.join(d, "parlor.dhr"), "rb").read()
    names = [o.split(":")[0] for o in open(os.path.join(d, "overlays.txt")).read().split()]
    ovls = [open(os.path.join(d, "parlor." + n), "rb").read() for n in names]
    end = program_end(d)
    assert end <= PRODOS_TOP, "the program ends at $%04X, past ProDOS's $BF00" % end
    dat = pad(main) + pad(vec) + pad(kit)
    table = bytearray(80)
    for i, o in enumerate(ovls):
        page = len(dat) // 256
        table[i * 4:i * 4 + 4] = bytes([page & 255, page >> 8, pages(o), 0])
        dat += pad(o)
    loader = bytearray(open(os.path.join(d, "prodsys.bin"), "rb").read())
    assert loader[3:5] == b"PN"
    loader[NPAGES_AT] = pages(main)
    loader[KPAGES_AT] = pages(kit)
    loader[OVLTAB_AT:OVLTAB_AT + 80] = table
    out = os.path.join(d, "prodos")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "PARLOR.SYSTEM"), "wb") as f:
        f.write(loader)
    with open(os.path.join(out, "PARLOR.DAT"), "wb") as f:
        f.write(dat)
    vol = prodos_volume("PARLORNET", [("PARLOR.SYSTEM", 0xFF, 0x2000, bytes(loader)),
                                      ("PARLOR.DAT", 0x06, 0x0000, dat)])
    with open(os.path.join(out, "parlor-files.po"), "wb") as f:
        f.write(vol)
    return end


def prodos_volume(volname, files, blocks=280):
    """A ProDOS volume image (512-byte blocks in order) holding files [(name, type, aux, data)]: the
    volume directory in blocks 2-5, the free-block map in block 6, then each file as a seedling (one
    block), a sapling (an index block and up to 256 data blocks) or a tree (a master index block, whose
    index blocks each hold 256 data blocks). Its boot blocks are empty."""
    img = bytearray(blocks * 512)
    used = set(range(7))
    nxt = [7]

    def alloc():
        b = nxt[0]
        nxt[0] += 1
        used.add(b)
        return b

    entries = []
    for name, ftype, aux, data in files:
        n = max(1, (len(data) + 511) // 512)
        if n == 1:
            key = alloc()
            img[key * 512:key * 512 + len(data)] = data
            storage, count = 1, 1
        else:
            def index(first, k):
                """An index block for data blocks first to first + k - 1."""
                key = alloc()
                for i in range(k):
                    b = alloc()
                    img[b * 512:b * 512 + 512] = data[(first + i) * 512:(first + i + 1) * 512].ljust(512, b"\0")
                    img[key * 512 + i] = b & 255
                    img[key * 512 + 256 + i] = b >> 8
                return key
            if n <= 256:
                key = index(0, n)
                storage, count = 2, n + 1
            else:
                key = alloc()
                m = (n + 255) // 256
                for j in range(m):
                    b = index(j * 256, min(256, n - j * 256))
                    img[key * 512 + j] = b & 255
                    img[key * 512 + 256 + j] = b >> 8
                storage, count = 3, n + m + 1
        e = bytearray(39)
        e[0] = (storage << 4) | len(name)
        e[1:1 + len(name)] = name.encode()
        e[0x10] = ftype
        e[0x11:0x13] = key.to_bytes(2, "little")
        e[0x13:0x15] = count.to_bytes(2, "little")
        e[0x15:0x18] = len(data).to_bytes(3, "little")
        e[0x1E] = 0xC3                  # read, write, rename, destroy
        e[0x1F:0x21] = aux.to_bytes(2, "little")
        e[0x25:0x27] = (2).to_bytes(2, "little")
        entries.append(e)
    assert nxt[0] <= blocks and len(entries) < 13
    # the volume directory: blocks 2 to 5, linked both ways
    for i, b in enumerate(range(2, 6)):
        img[b * 512:b * 512 + 2] = (b - 1 if i else 0).to_bytes(2, "little")
        img[b * 512 + 2:b * 512 + 4] = (b + 1 if b < 5 else 0).to_bytes(2, "little")
    h = bytearray(39)
    h[0] = 0xF0 | len(volname)
    h[1:1 + len(volname)] = volname.encode()
    h[0x1E] = 0xC3
    h[0x1F] = 0x27                      # entry length
    h[0x20] = 0x0D                      # entries a block
    h[0x21:0x23] = len(entries).to_bytes(2, "little")
    h[0x23:0x25] = (6).to_bytes(2, "little")
    h[0x25:0x27] = blocks.to_bytes(2, "little")
    at = 2 * 512 + 4
    img[at:at + 39] = h
    for i, e in enumerate(entries):
        img[at + 39 * (i + 1):at + 39 * (i + 2)] = e
    # the free-block map: a set bit is a free block, the high bit first
    for b in range(blocks):
        if b not in used:
            img[6 * 512 + b // 8] |= 0x80 >> (b % 8)
    return bytes(img)


if __name__ == "__main__":
    d = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build")
    build(d)
    end = prodos_files(d)
    with open(os.path.join(d, "layout.txt"), "a") as f:
        f.write("prodos end %04X\n" % end)
