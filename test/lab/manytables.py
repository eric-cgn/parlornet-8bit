#!/usr/bin/env python3
"""Gives a player of the lab parlor a lobby longer than one screen, of every game: it signs up NAME
(password Swordfish 1997), who opens four tables (Backgammon, Cannon Fire, Checkers, Dots and Boxes), then
four helpers (NAMEh0 to NAMEh3) who open four tables each that wait for a player, through the games and
their rules, and the first two each invite NAME to a fifth. NAME's lobby then lists 4 tables of their own,
2 invitations and 18 tables waiting for a player. plans/lobby_names.txt then signs in as NAME.
usage: manytables.py NAME [port]"""
import socket
import sys

NAME = sys.argv[1]
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 16501
PW = "Swordfish 1997"
GAMES = ["backgammon standard", "cannonfire classic", "checkers american", "dotsandboxes standard",
         "fiveinarow standard", "gridwords standard", "mancala kalah", "navalcombat classic", "reversi standard",
         "rummytile standard", "dotsandboxes majority", "navalcombat salvo", "fiveinarow freestyle",
         "gridwords standard", "mancala oware"]


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


def new(c, game):
    r = c.req("NEW " + game)
    if not r[-1].startswith("OK"):
        sys.exit("NEW %s: %s" % (game, r[-1]))
    t = r[-1].split()[1]
    c.req("JON " + t)
    return t


me = signup(NAME)
for g in GAMES[:4]:
    new(me, g)
k = 4
for h in range(4):
    c = signup("%sh%d" % (NAME[:9], h))
    for _ in range(4):
        new(c, GAMES[k % len(GAMES)])
        k += 1
    if h < 2:
        c.req("INV %s %s" % (new(c, GAMES[k % len(GAMES)]), NAME))
        k += 1
r = me.req("LST")
print("%s: LST of %d lines" % (NAME, len(r)))
