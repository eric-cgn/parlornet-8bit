#!/usr/bin/env python3
"""The C64 client's own compression for its files on the disk (c64_disk.py), so that they load in fewer
blocks; src/c64/unpack.inc unpacks them on the C64. Made for this program.

The packed data is a series of tokens, each a byte:
  $00          the end
  $01-$7F      that many bytes as they are, which follow
  $80-$BF      a copy of (token & $3F) + 2 bytes (2 to 65) from d bytes back in the output, where d - 1 is
               the next byte (d 1 to 256)
  $C0-$FF      a copy of (token & $3F) + 3 bytes (3 to 66) from d bytes back, d the next two bytes (low
               first, 1 to 65535)
A copy may overlap what it makes (d less than its length): it is made a byte at a time, from the first.

Unpacking can be done in place: the packed data is loaded so that it ends at a given address (the end of
the region) and unpacked to the region's start; `place` says where it must start so that the output never
overtakes the input that is still to be read.
"""


def pack(data):
    n = len(data)
    out = bytearray()
    lit = bytearray()
    heads = {}
    i = 0

    def flush():
        nonlocal lit
        while lit:
            k = min(len(lit), 127)
            out.append(k)
            out.extend(lit[:k])
            lit = lit[k:]

    def best_at(i):
        """The copy that saves the most at i: (saved, length, d), or None."""
        best = None
        if i + 2 > n:
            return None
        for j in reversed(heads.get(bytes(data[i:i + 2]), [])[-1024:]):
            d = i - j
            if d > 65535:
                break
            m = 2
            while i + m < n and m < 66 and data[j + m] == data[i + m]:
                m += 1
            for length, cost in ((min(m, 65), 2), (m, 3)) if d <= 256 else ((m, 3),):
                if (cost == 3 and length < 3) or length <= cost:
                    continue
                if best is None or length - cost > best[0]:
                    best = (length - cost, length, d)
        return best

    def note(k):
        if k + 2 <= n:
            heads.setdefault(bytes(data[k:k + 2]), []).append(k)

    while i < n:
        b = best_at(i)
        if b:
            # a lazy look one byte on: a better copy there is worth a literal here
            note(i)
            b2 = best_at(i + 1)
            if b2 and b2[0] > b[0] + 1:
                lit.append(data[i])
                i += 1
                continue
            heads[bytes(data[i:i + 2])].pop()
            saved, length, d = b
            flush()
            if d <= 256 and length <= 65 and saved == length - 2:
                out.append(0x80 | (length - 2))
                out.append(d - 1)
            else:
                out.append(0xC0 | (length - 3))
                out.append(d & 0xFF)
                out.append(d >> 8)
            for k in range(length):
                note(i + k)
            i += length
        else:
            note(i)
            lit.append(data[i])
            i += 1
    flush()
    out.append(0)
    return bytes(out)


def unpack(p):
    out = bytearray()
    i = 0
    while True:
        t = p[i]
        i += 1
        if t == 0:
            return bytes(out)
        if t < 0x80:
            out.extend(p[i:i + t])
            i += t
        elif t < 0xC0:
            m, d = (t & 0x3F) + 2, p[i] + 1
            i += 1
            for _ in range(m):
                out.append(out[-d])
        else:
            m, d = (t & 0x3F) + 3, p[i] | (p[i + 1] << 8)
            i += 2
            for _ in range(m):
                out.append(out[-d])


def place(p, start, end):
    """Where packed data p must be loaded so that unpacking it to start, in place, is safe: at most end
    minus its length, and low enough that the output never passes the input still to be read."""
    load = end - len(p)
    # walk the tokens: before each is read, the output so far must end at or before where it starts
    out = 0
    i = 0
    worst = 0
    while True:
        worst = max(worst, out - i)
        t = p[i]
        if t == 0:
            break
        if t < 0x80:
            i += 1 + t
            out += t
        elif t < 0xC0:
            i += 2
            out += (t & 0x3F) + 2
        else:
            i += 3
            out += (t & 0x3F) + 3
        worst = max(worst, out - i)
    # input byte k is at load + k; output byte o at start + o; need start + out <= load + i at all times
    return min(load, start + worst + 1)


if __name__ == "__main__":
    import sys
    for f in sys.argv[1:]:
        d = open(f, "rb").read()
        p = pack(d)
        assert unpack(p) == d
        print(f, len(d), len(p), round(len(p) / len(d), 2))
