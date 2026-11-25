#!/bin/sh
# Builds ParlorNet for the Apple IIe, the Apple IIgs and the Commodore 64 into build/:
#
#   build/parlornet.dsk            the self-booting disk (140K, DOS 3.3 sector order, no operating system)
#   build/parlornet-2.dsk        the second disk, only when the games do not all fit on the first
#   build/layout.txt           where each part is on the disks (test/test_disk.py reads it)
#   build/prodos/                  PARLOR.SYSTEM and PARLOR.DAT for the player's own ProDOS, and
#                                  parlor-files.po, a ProDOS volume holding just those two files
#   build/gs/                      the IIgs program: PARLORNET (a GS/OS application, S16),
#                                  PARLOR.DAT, and parlornet-gs.po, an 800K ProDOS volume holding
#                                  just those two files
#   build/c64/parlornet.d64        the C64 program's disk (a 1541's, 170K; tools/c64_disk.py)
#   build/c64cart/parlornet.crt    the C64 program's cartridge (Magic Desk, 128K; tools/c64_crt.py)
#
# The compiler is cc65 2.19 in a Debian image pinned by digest (Dockerfile), built here and
# tagged by the hash of the Dockerfile's instructions; build.sh compiles and links in it, and tools/apple2_disk.py
# lays out the disks (tools/apple2gs.py the IIgs program's, tools/c64_disk.py the C64's). The same source
# always gives the same disks.
# APPLE2_IMAGE names another image with cc65 2.19 and python3 to use instead.
set -eu
cd "$(dirname "$0")/.."
if [ -n "${APPLE2_IMAGE:-}" ]; then
	IMAGE=$APPLE2_IMAGE
else
	# tagged by the hash of the Dockerfile's instructions (its comments do not change the image)
	tag=$(python3 -c "import hashlib; print(hashlib.sha256(b''.join(l for l in open('Dockerfile','rb') if not l.startswith(b'#'))).hexdigest()[:12])")
	IMAGE=parlornet-8bit:$tag
	docker image inspect "$IMAGE" >/dev/null 2>&1 || docker build -q -t "$IMAGE" . >/dev/null
fi
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/src" -w /src "$IMAGE" sh build.sh
python3 tools/apple2_disk.py build
python3 tools/apple2gs.py build/gs
python3 tools/c64_disk.py build/c64
python3 tools/c64_crt.py build/c64cart
