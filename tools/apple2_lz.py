#!/usr/bin/env python3
"""The Apple IIe's packing of its overlays (apple2_disk.py), so that they take fewer sectors on the disk and
in PARLOR.DAT; src/apple2/lz.s unpacks them. Made for this program. docs/overlay-packing.md describes the
format.

Packed data is a series of sequences, each a token byte, then literal bytes, then a copy:

  token     bit 7: the copy's distance is two bytes (clear: one byte)
            bits 6-4: how many literals (0 to 6, or 7 for 7 or more, the count going on in a byte)
            bits 3-0: the copy's length minus 3 (0 to 14, or 15 for 18 or more, going on in a byte)
  count     where a field is full (7, or 15), one more byte e: the count is the field plus e when e is under
            255; when e is 255, the next two bytes (low first) are the whole count (the literals', or the
            copy's length minus 3)
  literals  that many bytes, as they are
  distance  one byte d for a distance of d + 1 (1 to 256), or two bytes (low first) for the distance
  copy      length bytes from distance bytes back in the output, made a byte at a time from the first,
            so a copy may overlap what it makes

The data ends where its bytes end: the unpacker stops before a token, or after a sequence's literals, when
it has read the last byte. A last sequence of literals only has no distance and no copy.

Packing is a shortest path over every place, with the longest copy (at either distance size) and every
shorter length, so the result is close to the best this format allows.

Unpacking is done in place: the packed data ends at the top of a region, and the output goes up from the
region's start. `ahead` says how far the output gets ahead of the input still to be read, so that the region
must be at least the packed size plus that.
"""

MINM = 3
CHAIN = 128             # earlier places tried for each copy
LONG = 64               # a copy longer than this goes on at the next place without a search


def _ext(v, cap):
    """The bytes a count costs past its token field."""
    if v < cap:
        return 0
    return 1 if v - cap < 255 else 3


def _matches(d):
    """For each place, (length, distance) of the longest copy at any distance and of the longest within 256."""
    n = len(d)
    head, prev = {}, [-1] * n
    out = [None] * n
    for i in range(n):
        far = near = (0, 0)
        if i + MINM <= n:
            k = d[i:i + MINM]
            last = out[i - 1] if i else []
            if last and last[0][0] > LONG:
                # inside a long copy: the same distances one byte on (as good as a search, and quicker)
                far, near = [(m[0] - 1, m[1]) for m in last] + [(0, 0)] * (2 - len(last))
            else:
                j, steps = head.get(k, -1), 0
                while j >= 0 and steps < CHAIN:
                    if i - j > 65535:
                        break
                    l = MINM
                    while i + l < n and d[j + l] == d[i + l]:
                        l += 1
                    if l > far[0]:
                        far = (l, i - j)
                    if i - j <= 256 and l > near[0]:
                        near = (l, i - j)
                    if i + l == n:
                        break
                    j, steps = prev[j], steps + 1
            prev[i] = head.get(k, -1)
            head[k] = i
        out[i] = [m for m in (far, near) if m[0] >= MINM]
    return out


def _copy_cost(length, dist):
    return 1 + (1 if dist <= 256 else 2) + _ext(length - MINM, 15)


def _parse(d):
    """[(literals, length, distance)], the last with length 0 when the data ends in literals."""
    n = len(d)
    best = _matches(d)
    inf = 1 << 60
    cost = [inf] * (n + 1)
    step = [None] * (n + 1)
    cost[0] = 0
    for i in range(n):
        c0 = cost[i]
        if c0 + 1 < cost[i + 1]:
            cost[i + 1], step[i + 1] = c0 + 1, None
        for length, dist in best[i]:
            for ln in list(range(MINM, min(length, 20) + 1)) + ([length] if length > 20 else []):
                c = c0 + _copy_cost(ln, dist)
                if c < cost[i + ln]:
                    cost[i + ln], step[i + ln] = c, (ln, dist)
    seq, i = [], n
    while i > 0:
        s = step[i]
        if s is None:
            seq.append(("L", d[i - 1]))
            i -= 1
        else:
            seq.append(("M",) + s)
            i -= s[0]
    seq.reverse()
    out, lit = [], bytearray()
    for s in seq:
        if s[0] == "L":
            lit.append(s[1])
        else:
            out.append((bytes(lit), s[1], s[2]))
            lit = bytearray()
    if lit or not out:
        out.append((bytes(lit), 0, 0))
    return out


def _count(o, v, cap):
    if v >= cap:
        if v - cap < 255:
            o.append(v - cap)
        else:
            o += bytes([255, v & 255, v >> 8])


def pack(data):
    """data packed, as bytes."""
    data = bytes(data)
    assert len(data) < 65536
    o = bytearray()
    for lit, length, dist in _parse(data):
        far = 0x80 if length and dist > 256 else 0
        o.append(far | min(len(lit), 7) << 4 | (min(length - MINM, 15) if length else 0))
        _count(o, len(lit), 7)
        o += lit
        if length:
            o += bytes([dist & 255, dist >> 8]) if far else bytes([dist - 1])
            _count(o, length - MINM, 15)
    return bytes(o)


def _sequences(p):
    """The packed data's sequences, as the unpacker reads them: (input position after the literals' count,
    literals, input position after the copy's count, length, distance)."""
    i, n = 0, len(p)

    def count(cap):
        nonlocal i
        e = p[i]
        i += 1
        if e < 255:
            return cap + e
        v = p[i] | p[i + 1] << 8
        i += 2
        return v

    while i < n:
        t = p[i]
        i += 1
        lits = t >> 4 & 7
        if lits == 7:
            lits = count(7)
        at = i
        i += lits
        if i >= n:
            yield at, lits, i, 0, 0
            return
        if t & 0x80:
            dist = p[i] | p[i + 1] << 8
            i += 2
        else:
            dist = p[i] + 1
            i += 1
        length = t & 15
        if length == 15:
            length = count(15)
        yield at, lits, i, length + MINM, dist


def unpack(p):
    """The bytes p packs."""
    o = bytearray()
    for at, lits, _, length, dist in _sequences(p):
        o += p[at:at + lits]
        assert dist <= len(o) or not length, "a copy reaches before the start"
        for _ in range(length):
            o.append(o[-dist])
    return bytes(o)


def ahead(p):
    """How far the output gets ahead of the input still to be read while p unpacks in place: the region
    must be len(p) + ahead(p) bytes or more. Each output byte o is written while the input's next unread
    byte is k; it must land below where k is, so the region needs len(p) + o - k + 1."""
    worst, out = 0, 0
    for at, lits, after, length, _ in _sequences(p):
        if lits:
            # literal j (input at + j, read just before it is written) goes to out + j
            worst = max(worst, out + lits - 1 - (at + lits) + 1)
        out += lits
        if length:
            worst = max(worst, out + length - 1 - after + 1)
            out += length
    return max(0, worst)


def piece(data, region):
    """data packed for loading into whole pages at the top of a region of region bytes (a whole number of
    pages), with its output going up from the region's start. The first byte says where the packed data
    starts in the first page (0 for the next page's start), then come padding and the packed data, which
    ends on a page end. The build fails here when the data would not unpack in place in that region."""
    p = pack(data)
    assert unpack(p) == bytes(data), "the packing does not unpack to the data"
    need = len(p) + ahead(p)
    pages = (len(p) + 1 + 255) // 256
    assert len(data) <= region and need <= region and pages * 256 <= region, \
        "%d bytes packed to %d need %d bytes to unpack in place, and the region is %d" % (
            len(data), len(p), need, region)
    start = pages * 256 - len(p)        # 1 to 256
    out = bytes([start & 255]) + bytes(start - 1) + p
    assert len(out) == pages * 256
    return out
