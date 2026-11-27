#!/usr/bin/env python3
"""Gives a player of the lab parlor a table list (LST) longer than the program's reply buffer, so that it
comes in pages: it signs up NAME (password Swordfish 1997), then six helpers (NAMEh0 to NAMEh5) who each
open two tables of every
game, sit there and invite NAME, so that NAME's own tables and invitations, which LST lists in full, run past
3.3K, as they can for a player of a busy parlor. plans/table_entry_long.txt then signs in as NAME.
usage: longlist.py NAME [port]"""
import socket
import sys

NAME = sys.argv[1]
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 16501
PW = "Swordfish 1997"
GAMES = ["backgammon standard", "cannonfire classic", "checkers american", "dotsandboxes standard",
         "fiveinarow standard", "gridwords standard", "mancala kalah", "navalcombat classic", "reversi standard",
         "rummytile standard"]


class Conn:
    def __init__(self):
        self.s = socket.create_connection(("127.0.0.1", PORT))
        self.f = self.s.makefile("rb")
        self.s.sendall(b"PARLORNET/1\r\n")
        self.f.readline()

    def req(self, line):
        self.s.sendall(line.encode() + b"\r\n")
        out = []
        while True:
            ln = self.f.readline().decode().rstrip("\r\n")
            out.append(ln)
            if ln.startswith(("OK", "ERR", "ASK")):
                return out


def signup(name):
    c = Conn()
    r = c.req("REG " + name)
    while r[-1].startswith("ASK"):
        r = c.req("AUT %s %s" % (r[-1].split()[1], PW))
    if not r[-1].startswith("OK"):
        sys.exit("%s: %s" % (name, r[-1]))
    return c


signup(NAME)
for h in range(6):
    c = signup("%sh%d" % (NAME[:9], h))
    for g in GAMES:
        for _ in range(2):
            r = c.req("NEW " + g)
            if r[-1].startswith("OK"):
                c.req("JON " + r[-1].split()[1])
                c.req("INV %s %s" % (r[-1].split()[1], NAME))
c = Conn()
c.req("USER " + NAME)
c.req("AUT password " + PW)
r = c.req("LST")
print("%s: LST of %d lines, %d bytes as the Apple program keeps them" % (NAME, len(r), sum(len(l) + 1 for l in r)))
