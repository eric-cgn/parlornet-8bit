"""The ShrinkIt archives (tools/nufx.py): NuLib2, a reader written apart from this repository, reads them
back with their file types and every CRC checked. The built archives (tools/build.sh, when build/ is
there) hold the same files as the ProDOS volumes, byte for byte, with the same types and access bits.

The tests need nulib2 (the Debian and Ubuntu package of the same name). Without it they are skipped."""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nufx  # noqa: E402
from test_disk import BUILD, read_volume  # noqa: E402

NULIB2 = shutil.which("nulib2")


def nulib2(*args, cwd=None):
    """NuLib2's output, failing the test if NuLib2 fails. Its questions are answered by an empty stdin."""
    r = subprocess.run([NULIB2] + list(args), cwd=cwd, stdin=subprocess.DEVNULL,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = r.stdout.decode("latin-1")
    assert r.returncode == 0, out
    return out


def extracted(shk):
    """{name: (type, aux, data)} of the files that NuLib2 extracts from an archive. With -e it puts each
    file's ProDOS type and aux type at the end of its name, as in PARLOR.SYSTEM#ff2000."""
    with tempfile.TemporaryDirectory() as d:
        nulib2("-xe", os.path.abspath(shk), cwd=d)
        files = {}
        for n in os.listdir(d):
            name, _, types = n.rpartition("#")
            with open(os.path.join(d, n), "rb") as f:
                files[name] = (int(types[:2], 16), int(types[2:], 16), f.read())
        return files


def access_in_archive(data):
    """{name: access bits} from an archive's record headers. NuLib2 shows no access bits, so this reads
    them itself."""
    out = {}
    at = 48
    for _ in range(int.from_bytes(data[8:12], "little")):
        attrib = int.from_bytes(data[at + 6:at + 8], "little")
        nthreads = int.from_bytes(data[at + 10:at + 14], "little")
        access = data[at + 18]
        threads = [data[at + attrib + 16 * i:at + attrib + 16 * (i + 1)] for i in range(nthreads)]
        at += attrib + 16 * nthreads
        name = None
        for t in threads:
            n = int.from_bytes(t[12:16], "little")
            if t[0:2] == (3).to_bytes(2, "little"):
                name = data[at:at + int.from_bytes(t[8:12], "little")].decode()
            at += n
        out[name] = access
    return out


def access_on_volume(img):
    """{name: access bits} of the files in a ProDOS volume's first directory block."""
    out = {}
    for i in range(1, 13):
        e = img[2 * 512 + 4 + 39 * i:2 * 512 + 4 + 39 * (i + 1)]
        if e[0] >> 4:
            out[e[1:1 + (e[0] & 15)].decode()] = e[0x1E]
    return out


@unittest.skipUnless(NULIB2, "nulib2 is not installed")
class Archive(unittest.TestCase):
    def test_reads_back(self):
        files = [("A.SYSTEM", 0xFF, 0x2000, bytes(range(200))),
                 ("B.DAT", 0x06, 0x1234, bytes((i * 7) & 255 for i in range(70000))),
                 ("C.S16", 0xB3, 0xDB07, bytes((i * 13) & 255 for i in range(140000))),
                 ("EMPTY", 0x04, 0x0000, b"")]
        with tempfile.TemporaryDirectory() as d:
            shk = os.path.join(d, "t.shk")
            with open(shk, "wb") as f:
                f.write(nufx.archive(files, 0xC3))
            nulib2("-i", shk)
            listing = nulib2("-v", shk)
            self.assertEqual(extracted(shk), {n: (t, a, data) for n, t, a, data in files})
            for name, ftype, aux, data in files:
                line = [ln for ln in listing.splitlines() if ln.split()[:1] == [name]]
                self.assertEqual(len(line), 1, listing)
                self.assertIn("$%04X" % aux, line[0])
                self.assertIn("unc", line[0])
                self.assertEqual(int(line[0].split()[-1]), len(data))

    def test_storage_types(self):
        self.assertEqual([nufx.storage_type(n) for n in (0, 512, 513, 131072, 131073)], [1, 1, 2, 2, 3])


BUILT = [(os.path.join(BUILD, "prodos", "parlornet.shk"), os.path.join(BUILD, "prodos", "parlor-files.po")),
         (os.path.join(BUILD, "gs", "parlornet-gs.shk"), os.path.join(BUILD, "gs", "parlornet-gs.po"))]


@unittest.skipUnless(NULIB2, "nulib2 is not installed")
@unittest.skipUnless(all(os.path.exists(p) for b in BUILT for p in b), "the archives are not built")
class Built(unittest.TestCase):
    def test_same_files_as_the_volumes(self):
        for shk, po in BUILT:
            with self.subTest(shk=os.path.basename(shk)):
                nulib2("-i", shk)
                img = open(po, "rb").read()
                self.assertEqual(extracted(shk), read_volume(img))
                self.assertEqual(access_in_archive(open(shk, "rb").read()), access_on_volume(img))

    def test_names(self):
        self.assertEqual(sorted(extracted(BUILT[0][0])), ["PARLOR.DAT", "PARLOR.SYSTEM"])
        self.assertEqual(sorted(extracted(BUILT[1][0])), ["PARLOR.DAT", "PARLORNET"])


if __name__ == "__main__":
    unittest.main()
