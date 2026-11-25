"""The client's disk tools (tools/apple2_disk.py, tools/apple2gs.py): the ProDOS volume they write reads
back as ProDOS would read it, a built client (tools/build.sh, when build/ is there) fits its disk
and, under ProDOS, below ProDOS's global page, and the IIgs program's load file is OMF as the System
Loader reads it."""
import importlib.util
import os
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_spec = importlib.util.spec_from_file_location("apple2_disk", os.path.join(ROOT, "tools", "apple2_disk.py"))
disk = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(disk)
BUILD = os.path.join(ROOT, "build")


def read_volume(img):
    """{name: (type, aux, data)} of a ProDOS volume's top directory, read block by block."""
    def block(n):
        return img[n * 512:(n + 1) * 512]

    files = {}
    b = 2
    first = True
    while b:
        blk = block(b)
        for i in range(13):
            e = blk[4 + 39 * i:4 + 39 * (i + 1)]
            if first and i == 0:
                assert e[0] >> 4 == 0xF, "no volume header"
                continue
            storage, nlen = e[0] >> 4, e[0] & 15
            if not storage:
                continue
            name = e[1:1 + nlen].decode()
            key = int.from_bytes(e[0x11:0x13], "little")
            eof = int.from_bytes(e[0x15:0x18], "little")
            if storage == 1:
                data = block(key)[:eof]
            else:
                assert storage == 2
                idx = block(key)
                data = b"".join(block(idx[k] | (idx[256 + k] << 8)) for k in range((eof + 511) // 512))[:eof]
            files[name] = (e[0x10], int.from_bytes(e[0x1F:0x21], "little"), data)
        first = False
        b = int.from_bytes(blk[2:4], "little")
    return files


class ProdosVolume(unittest.TestCase):
    def test_files_read_back(self):
        small = bytes(range(200))
        big = bytes((i * 7) & 255 for i in range(70000))
        img = disk.prodos_volume("TESTVOL", [("A.SYSTEM", 0xFF, 0x2000, small), ("B.DAT", 0x06, 0, big)])
        self.assertEqual(len(img), 280 * 512)
        files = read_volume(img)
        self.assertEqual(files["A.SYSTEM"], (0xFF, 0x2000, small))
        self.assertEqual(files["B.DAT"], (0x06, 0, big))

    def test_free_map_marks_used_blocks(self):
        img = disk.prodos_volume("V", [("X", 0x06, 0, bytes(1500))])
        bitmap = img[6 * 512:7 * 512]

        def free(b):
            return bool(bitmap[b // 8] & (0x80 >> (b % 8)))

        # blocks 0-6 (boot, directory, map), the index block and three data blocks are in use
        self.assertEqual([free(b) for b in range(12)], [False] * 11 + [True])
        self.assertTrue(all(free(b) for b in range(11, 280)))


@unittest.skipUnless(os.path.exists(os.path.join(BUILD, "layout.txt")), "the Apple IIe client is not built")
class Built(unittest.TestCase):
    def test_fits(self):
        layout = [ln.split() for ln in open(os.path.join(BUILD, "layout.txt"))]
        used = {int(f[1]): int(f[2]) for f in layout if f[0] == "used"}
        self.assertTrue(all(n <= 560 for n in used.values()), used)
        end = [int(f[2], 16) for f in layout if f[:2] == ["prodos", "end"]]
        self.assertTrue(end and end[0] <= 0xBF00, end)

    def test_prodos_files(self):
        img = open(os.path.join(BUILD, "prodos", "parlor-files.po"), "rb").read()
        files = read_volume(img)
        loader = open(os.path.join(BUILD, "prodos", "PARLOR.SYSTEM"), "rb").read()
        self.assertEqual(files["PARLOR.SYSTEM"], (0xFF, 0x2000, loader))
        self.assertEqual(files["PARLOR.DAT"][2], open(os.path.join(BUILD, "prodos", "PARLOR.DAT"), "rb").read())
        self.assertEqual(loader[3:5], b"PN")


def omf_segments(data):
    """[(kind, length, body records)] of an OMF 2.0 load file."""
    segs = []
    at = 0
    while at < len(data):
        h = data[at:]
        bytecnt = int.from_bytes(h[0:4], "little")
        assert h[0x0E] == 4 and h[0x0F] == 2, "not OMF 2.0"
        kind = int.from_bytes(h[0x14:0x16], "little")
        length = int.from_bytes(h[0x08:0x0C], "little")
        body = h[int.from_bytes(h[0x2A:0x2C], "little"):bytecnt]
        records = []
        i = 0
        while body[i] != 0:
            op = body[i]
            n = int.from_bytes(body[i + 1:i + 5], "little")
            assert op in (0xF1, 0xF2), "record %02X" % op
            records.append((op, body[i + 5:i + 5 + n] if op == 0xF2 else n))
            i += 5 + (n if op == 0xF2 else 0)
        assert i == len(body) - 1
        segs.append((kind, length, records))
        at += bytecnt
    return segs


GS = os.path.join(BUILD, "gs")


@unittest.skipUnless(os.path.exists(os.path.join(GS, "PARLORNET")), "the Apple IIgs program is not built")
class BuiltGS(unittest.TestCase):
    def test_load_file(self):
        app = open(os.path.join(GS, "PARLORNET"), "rb").read()
        (k1, n1, r1), (k2, n2, r2) = omf_segments(app)
        self.assertEqual((k1, k2), (0x0000, 0x0012))        # static code; direct page and stack
        self.assertEqual(r2, [(0xF1, n2)])
        (op, code), = r1
        self.assertEqual((op, len(code)), (0xF2, n1))
        shell = open(os.path.join(GS, "shell.bin"), "rb").read()
        main = open(os.path.join(GS, "parlor.main"), "rb").read()
        self.assertEqual(code[:len(shell)], shell)
        at = len(shell)
        self.assertEqual(int.from_bytes(code[at:at + 2], "little"), len(main))
        page8 = code[at + 2:at + 258]
        self.assertEqual(code[at + 258:], main)
        self.assertEqual(page8[0x0A], 0)                    # from files, as under ProDOS
        # each overlay's place in PARLOR.DAT, one after another
        dat = open(os.path.join(GS, "PARLOR.DAT"), "rb").read()
        names = open(os.path.join(GS, "overlays.txt")).read().split()
        page = 0
        for i, n in enumerate(names):
            e = page8[0x21 + 4 * i:0x25 + 4 * i]
            ovl = open(os.path.join(GS, "parlor." + n.split(":")[0]), "rb").read()
            self.assertEqual(e[0] | e[1] << 8, page)
            self.assertEqual(dat[page * 256:page * 256 + len(ovl)], ovl)
            page += e[2]
        self.assertEqual(page * 256, len(dat))

    def test_fits_its_bank(self):
        ends = [int(f[2], 16) for f in (ln.split() for ln in open(os.path.join(GS, "parlor.map")))
                if len(f) > 2 and f[0] == "BSS" and len(f[2]) == 6]
        self.assertTrue(ends and ends[0] < 0xC000, ends)

    def test_disk(self):
        img = open(os.path.join(GS, "parlornet-gs.po"), "rb").read()
        self.assertEqual(len(img), 1600 * 512)
        files = read_volume(img)
        app = open(os.path.join(GS, "PARLORNET"), "rb").read()
        self.assertEqual(files["PARLORNET"], (0xB3, 0, app))
        self.assertEqual(files["PARLOR.DAT"][2], open(os.path.join(GS, "PARLOR.DAT"), "rb").read())
        self.assertEqual(sorted(files), ["PARLOR.DAT", "PARLORNET"])


if __name__ == "__main__":
    unittest.main()
