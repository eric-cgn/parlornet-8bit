#!/bin/sh
# Builds ParlorNet for the Apple IIe and the Apple IIgs into build/:
#
#   build/parlornet.dsk            the self-booting disk (140K, DOS 3.3 sector order, no operating system)
#   build/parlornet-2.dsk        the second disk, only when the games do not all fit on the first
#   build/layout.txt           where each part is on the disks (test/test_disk.py reads it)
#   build/prodos/                  PARLOR.SYSTEM and PARLOR.DAT for the player's own ProDOS, and
#                                  parlor-files.po, a ProDOS volume holding just those two files
#   build/gs/                      the IIgs program: PARLORNET (a GS/OS application, S16),
#                                  PARLOR.DAT, and parlornet-gs.po, an 800K ProDOS volume holding
#                                  just those two files
#
# The compiler is cc65 2.19 in a Debian image pinned by digest (Dockerfile), built here and
# tagged by the Dockerfile's hash; build.sh compiles and links in it, and tools/apple2_disk.py
# lays out the disks (tools/apple2gs.py the IIgs program's). The same source always gives the same disks.
# APPLE2_IMAGE names another image with cc65 2.19 and python3 to use instead.
set -eu
cd "$(dirname "$0")/.."
if [ -n "${APPLE2_IMAGE:-}" ]; then
	IMAGE=$APPLE2_IMAGE
else
	tag=$(python3 -c "import hashlib; print(hashlib.sha256(open('Dockerfile','rb').read()).hexdigest()[:12])")
	IMAGE=parlornet-8bit:$tag
	docker image inspect "$IMAGE" >/dev/null 2>&1 || docker build -q -t "$IMAGE" . >/dev/null
fi
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/src" -w /src "$IMAGE" sh build.sh
python3 tools/apple2_disk.py build
python3 tools/apple2gs.py build/gs
