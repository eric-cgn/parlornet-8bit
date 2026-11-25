#!/usr/bin/env python3
"""Puts the release files in build/release/, from a build by tools/build.sh, with their SHA-256 and MD5 sums
(SHA256SUMS, MD5SUMS):

  parlornet.dsk        the Apple IIe's self-booting disk (and parlornet-2.dsk, when the build makes one)
  parlor-files.po      PARLOR.SYSTEM and PARLOR.DAT for the player's own ProDOS, on a ProDOS volume
  parlornet-gs.po      the Apple IIgs program, PARLORNET, and its PARLOR.DAT, on an 800K ProDOS volume
  parlornet.d64        the Commodore 64's disk (a 1541's, 170K)
  parlornet.crt        the Commodore 64's cartridge (Magic Desk, 128K), the program and every game in ROM

The version is the one in VERSION; the release workflow tags it v<version>.
Usage: tools/release.py [build dir]"""
import hashlib
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = ["parlornet.dsk", "parlornet-2.dsk", "prodos/parlor-files.po", "gs/parlornet-gs.po", "c64/parlornet.d64",
         "c64cart/parlornet.crt"]


def version():
    with open(os.path.join(ROOT, "VERSION")) as f:
        return f.read().strip()


def main():
    build = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build")
    rel = os.path.join(build, "release")
    shutil.rmtree(rel, ignore_errors=True)
    os.makedirs(rel)
    names = []
    for f in FILES:
        src = os.path.join(build, f)
        if not os.path.exists(src):
            if f == "parlornet-2.dsk":      # only when the games do not all fit on the first disk
                continue
            sys.exit("%s is missing: tools/build.sh" % src)
        names.append(os.path.basename(f))
        shutil.copyfile(src, os.path.join(rel, names[-1]))
    for algo, out in (("sha256", "SHA256SUMS"), ("md5", "MD5SUMS")):
        with open(os.path.join(rel, out), "w") as s:
            for name in names:
                with open(os.path.join(rel, name), "rb") as g:
                    s.write("%s  %s\n" % (hashlib.new(algo, g.read()).hexdigest(), name))
    print("ParlorNet for the Apple IIe, IIgs and C64 %s:" % version())
    with open(os.path.join(rel, "SHA256SUMS")) as s:
        sys.stdout.write(s.read())


if __name__ == "__main__":
    main()
