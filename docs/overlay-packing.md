# The Apple IIe's overlay packing

The Apple IIe program keeps its overlays packed, on the self-booting disk and in PARLOR.DAT, so that they take fewer sectors and blocks. `tools/apple2_lz.py` packs them when the disks are made (`tools/apple2_disk.py`), and `src/apple2/lz.s` unpacks them as they load. The program itself, its vectors and the Double Hi-Res kit are not packed, so the boot block and PARLOR.SYSTEM load them as they are. The IIgs program's overlays are not packed either. This format was made for this program.

## The packed data

Packed data is a series of sequences. Each sequence is a token byte, then some literal bytes, then a copy of bytes already unpacked.

| Part | Bytes | What it holds |
|---|---|---|
| token | 1 | Bit 7 is set when the copy's distance takes two bytes. Bits 6-4 are the number of literals, 0 to 6, or 7 when there are 7 or more. Bits 3-0 are the copy's length minus 3, 0 to 14, or 15 when the length is 18 or more. |
| literal count | 0, 1 or 3 | Present when bits 6-4 are 7 (see below). |
| literals | as many as the count | Bytes that go to the output as they are. |
| distance | 1 or 2 | One byte d means a distance of d + 1 (1 to 256). Two bytes, low first, are the distance itself (1 to 65535). |
| length count | 0, 1 or 3 | Present when bits 3-0 are 15 (see below). |

A full field (7 for literals, 15 for a length) goes on in the next byte e. When e is under 255, the count is the field plus e. When e is 255, the two bytes after it, low first, are the whole count: the number of literals, or the length minus 3.

The copy takes its length in bytes from the distance back in the output, one byte at a time from the first. A copy can overlap what it makes, so a distance of 1 repeats the last byte.

The data ends where its bytes end. The unpacker stops before a token, or after a sequence's literals, once it has read the last byte. So the last sequence can be literals alone, with no distance and no copy.

## A packed piece

A piece is read in whole pages, so it starts with a byte that says where the packed data starts in its first page, and the packed data ends at the end of its last page. That byte is 0 when the data starts at the second page. The bytes between that first byte and the data are zeros.

## Unpacking in place

An overlay's pieces are read into the top pages of $D000-$F8FF and unpacked from there to $D000. The output goes up behind the input, so it must never reach input that is still to be read. `tools/apple2_lz.py` works out how far the output can get ahead of the input for each piece, and the build fails when a piece would not fit its region that way.

Help is packed one help at a time. The help overlay's first page is its index: each help's first page in the overlay and its number of packed pages (2 bytes each), and from byte 128 each help's number of pages unpacked. The program reads the index into the reply buffer, then the help's packed pages to the top of the pages it will take there, and unpacks them in place.

## Packing

`tools/apple2_lz.py` looks for copies through chains of earlier places with the same three bytes, and then finds the shortest packing by a shortest-path search over every place, trying the longest copy at each distance size and every shorter length.
