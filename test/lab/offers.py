#!/usr/bin/env python3
"""The other player of plans/offers.txt: ann signs up, opens a table of the game, invites the player and waits
for them to sit down. Then she makes an offer that the player answers in its dialog: in Backgammon she doubles
at the start of her turn (the player's own first turn, if it comes first, is played for them on a connection of
their own), in Checkers she offers a draw. She stays a while for the answer.
usage: offers.py PORT GAME PLAYER PASSWORD   (GAME: backgammon or checkers)"""
import socket
import sys
import time

PORT, GAME, WHO, PW = int(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]


class Conn:
    def __init__(self):
        self.s = socket.create_connection(("127.0.0.1", PORT))
        self.f = self.s.makefile("rb")
        self.s.sendall(b"PARLORNET/1\r\n")
        self.line()

    def line(self):
        return self.f.readline().decode().rstrip("\r\n")

    def req(self, text):
        self.s.sendall(text.encode() + b"\r\n")
        out = []
        while True:
            l = self.line()
            out.append(l)
            if l.startswith(("OK", "ERR", "ASK")):
                return out

    def sign(self, verb, name, pw):
        r = self.req("%s %s" % (verb, name))
        while r[-1].startswith("ASK"):
            f = r[-1].split()
            r = self.req("AUT %s %s" % (f[1], "y" if "yn" in f else pw))
        return r[-1]

    def view(self, tid):
        v = {}
        m = []
        for l in self.req("VUE %s M" % tid):
            p = l.split(" ", 1)
            if p[0] == "M":
                m.append(p[1])
            elif len(p) > 1:
                v[p[0]] = p[1]
        return v, m


ann = Conn()
print("ann", ann.sign("REG", "ann", PW), flush=True)
tid = ann.req("NEW %s %s" % (GAME, "american" if GAME == "checkers" else "standard"))[-1].split()[1]
ann.req("JON %s" % tid)
while not ann.req("INV %s %s" % (tid, WHO))[-1].startswith("OK"):
    time.sleep(1)
print("invited", WHO, "to", tid, flush=True)
while True:
    t = [l.split() for l in ann.req("LST table=%s" % tid) if l.startswith("TAB ")]
    if t and WHO in t[0][6].split(","):
        break
    time.sleep(1)
seat = t[0][6].split(",").index("ann")
print("seated, ann at seat", seat, flush=True)
time.sleep(2)
if GAME == "checkers":
    print(ann.req("MOV %s draw" % tid), flush=True)
else:
    them = None
    for _ in range(120):
        v, m = ann.view(tid)
        turn = v.get("W", "-")
        if turn == str(seat) and v.get("Q") == "roll":
            print(ann.req("MOV %s double" % tid), flush=True)
            break
        if turn == str(1 - seat) and v.get("Q") in ("roll", "move"):
            # the player's own first turn, played for them
            if them is None:
                them = Conn()
                them.sign("USER", WHO, PW)
            if v.get("Q") == "roll":
                them.req("MOV %s roll" % tid)
            while True:
                _, m = them.view(tid)
                steps = [x for x in m if "-" in x]
                if not steps or not them.req("MOV %s %s" % (tid, steps[0]))[-1].startswith("OK"):
                    break
            print(them.req("END %s" % tid), flush=True)
        time.sleep(1)
time.sleep(600)
