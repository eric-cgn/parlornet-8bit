"""The IIe's overlay packing (tools/apple2_lz.py): packed data unpacks to what was packed, and a packed piece
unpacks in place, read into the top of its region as src/apple2/lz.s reads it, without the output ever
reaching input still to be read. With a built client (tools/build.sh), every overlay and every help is
checked that way too."""
import importlib.util
import os
import random
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _load(name):
    spec = importlib.util.spec_from_file_location(name, os.path.join(ROOT, "tools", name + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


lz = _load("apple2_lz")
disk = _load("apple2_disk")
BUILD = os.path.join(ROOT, "build")


def unpack_in_place(piece, region):
    """What lz.s leaves at the start of a region of region bytes with piece in its top pages: it reads its
    input from the same memory it writes, so input it overwrote would come out wrong."""
    mem = bytearray(region)
    top = region - len(piece)
    mem[top:] = piece
    i = top + (piece[0] or 256)
    o = 0

    def count(cap):
        nonlocal i
        e = mem[i]
        i += 1
        if e < 255:
            return cap + e
        v = mem[i] | mem[i + 1] << 8
        i += 2
        return v

    while i < region:
        t = mem[i]
        i += 1
        n = t >> 4 & 7
        if n == 7:
            n = count(7)
        for _ in range(n):
            mem[o] = mem[i]
            o += 1
            i += 1
        if i >= region:
            break
        if t & 0x80:
            dist = mem[i] | mem[i + 1] << 8
            i += 2
        else:
            dist = mem[i] + 1
            i += 1
        length = t & 15
        if length == 15:
            length = count(15)
        for _ in range(length + 3):
            mem[o] = mem[o - dist]
            o += 1
        assert o <= i, "the output passed the input"
    return bytes(mem[:o])


def noise(seed, n):
    r = random.Random(seed)
    return bytes(r.randrange(256) for _ in range(n))


SAMPLES = {
    "empty": b"",
    "one byte": b"A",
    "zeros": bytes(5000),
    "a pattern": b"ParlorNet " * 700,
    "random": noise(1, 3000),
    "long literals then a far copy": noise(2, 900) * 2,
    "text": open(os.path.join(ROOT, "README.md"), "rb").read()[:9000],
}


class Packing(unittest.TestCase):
    def test_round_trip(self):
        for name, data in SAMPLES.items():
            with self.subTest(name):
                self.assertEqual(lz.unpack(lz.pack(data)), data)

    def test_in_place(self):
        for name, data in SAMPLES.items():
            if not data:
                continue
            with self.subTest(name):
                region = disk.OVL_REGION
                self.assertEqual(unpack_in_place(lz.piece(data, region), region), data)

    def test_margin_is_checked(self):
        """Data that packs to nearly its own size fills its region, and a piece that would overrun it fails."""
        data = noise(3, disk.OVL_REGION)
        with self.assertRaises(AssertionError):
            lz.piece(data, disk.OVL_REGION)


@unittest.skipUnless(os.path.exists(os.path.join(BUILD, "overlays.txt")), "the Apple IIe client is not built")
class Built(unittest.TestCase):
    def test_every_overlay(self):
        names = [o.split(":")[0] for o in open(os.path.join(BUILD, "overlays.txt")).read().split()]
        for n in names:
            if n == "help":
                continue
            with self.subTest(n):
                raw = open(os.path.join(BUILD, "parlor." + n), "rb").read()
                self.assertEqual(unpack_in_place(lz.piece(raw, disk.OVL_REGION), disk.OVL_REGION), raw)

    def test_every_help(self):
        raw = open(os.path.join(BUILD, "parlor.help"), "rb").read()
        packed = disk.pack_help(raw)
        k = 0
        while raw[k * 2 + 1]:
            with self.subTest(k):
                at, n = packed[k * 2], packed[k * 2 + 1]
                region = packed[128 + k] * 256
                text = raw[raw[k * 2] * 256:(raw[k * 2] + raw[k * 2 + 1]) * 256].rstrip(b"\0")
                self.assertEqual(unpack_in_place(packed[at * 256:(at + n) * 256], region), text)
            k += 1


if __name__ == "__main__":
    unittest.main()
