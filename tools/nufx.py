#!/usr/bin/env python3
"""Writes ShrinkIt archives (NuFX, ProDOS file type $E0, aux type $8002) of ProDOS files.

The format is the one in Apple II File Type Note $E0/8002 and its NuFX addendum. An archive is a master
header (48 bytes) followed by one record a file. Each record is a version 3 record header with the file's
ProDOS file type, aux type, access and storage type, then two thread headers, then the threads' bytes.
The first thread holds the file's name and the second its data fork.

The threads are stored as they are, in thread format 0 (uncompressed), so that nothing can go wrong in a
compressor and every version of ShrinkIt and NuLib2 can read them. Each data thread carries the CRC of
its bytes, as version 3 records do.

The dates are left empty (all zeros, which ShrinkIt shows as no date), as they are on the ProDOS volumes
that tools/apple2_disk.py writes. That way the same files always give the same archive.
"""

MASTER_ID = bytes([0x4E, 0xF5, 0x46, 0xE9, 0x6C, 0xE5])     # "NuFile", every other letter with its high bit set
RECORD_ID = bytes([0x4E, 0xF5, 0x46, 0xD8])                 # "NuFX", the same way
PRODOS = 1                      # file_sys_id
SEPARATOR = ord("/")            # file_sys_info: ProDOS's separator between a path's names
NO_DATE = bytes(8)


def crc16(data, crc=0):
    """The CRC-16 that NuFX uses (CCITT, polynomial $1021, high bit first)."""
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


def storage_type(size):
    """The ProDOS storage type of a file of this many bytes: a seedling, a sapling or a tree."""
    blocks = max(1, (size + 511) // 512)
    return 1 if blocks == 1 else 2 if blocks <= 256 else 3


def thread(klass, kind, data, crc=0):
    """A thread header (16 bytes) for uncompressed data."""
    return (klass.to_bytes(2, "little") + (0).to_bytes(2, "little") + kind.to_bytes(2, "little")
            + crc.to_bytes(2, "little") + len(data).to_bytes(4, "little") + len(data).to_bytes(4, "little"))


def record(name, ftype, aux, access, data):
    """One file's record: its header, its thread headers, then its name and its data."""
    fname = name.encode("ascii")
    attrs = bytearray()
    attrs += (3).to_bytes(2, "little")                  # version_number
    attrs += (2).to_bytes(4, "little")                  # total_threads
    attrs += PRODOS.to_bytes(2, "little")
    attrs += SEPARATOR.to_bytes(2, "little")
    attrs += access.to_bytes(4, "little")
    attrs += ftype.to_bytes(4, "little")
    attrs += aux.to_bytes(4, "little")
    attrs += storage_type(len(data)).to_bytes(2, "little")
    attrs += NO_DATE + NO_DATE + NO_DATE                # create_when, mod_when, archive_when
    attrs += (0).to_bytes(2, "little")                  # option_size: no option list
    attrs += (0).to_bytes(2, "little")                  # filename_length: the name is in its own thread
    # attrib_count runs from the record's ID through filename_length
    attrib_count = len(RECORD_ID) + 2 + 2 + len(attrs)
    threads = thread(3, 0, fname) + thread(2, 0, data, crc16(data, 0xFFFF))    # filename, data fork
    covered = attrib_count.to_bytes(2, "little") + bytes(attrs) + threads
    return RECORD_ID + crc16(covered).to_bytes(2, "little") + covered + fname + data


def archive(files, access):
    """A ShrinkIt archive of files [(name, type, aux, data)], each with the ProDOS access bits given."""
    records = b"".join(record(name, ftype, aux, access, data) for name, ftype, aux, data in files)
    body = bytearray()
    body += len(files).to_bytes(4, "little")            # total_records
    body += NO_DATE + NO_DATE                           # archive_create_when, archive_mod_when
    body += (2).to_bytes(2, "little")                   # master_version
    body += bytes(8)
    body += (48 + len(records)).to_bytes(4, "little")   # master_eof: the archive's length
    body += bytes(6)
    return MASTER_ID + crc16(body).to_bytes(2, "little") + bytes(body) + records
