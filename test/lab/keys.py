#!/usr/bin/env python3
"""The other side of plans/stay_signed_in.txt: it watches who is online, and once the player has a sign-in
key, has left and has come back with it, it signs in as the player with the password and forgets every one of
their keys (KEY DEL all). That ends the player's session, and the program's next sign-in with its key gets
ERR NOKEY.
usage: keys.py PORT PLAYER PASSWORD"""
import socket
import sys
import time

PORT, WHO, PW = int(sys.argv[1]), sys.argv[2], sys.argv[3]


class Conn:
    def __init__(self):
        self.s = socket.create_connection(("127.0.0.1", PORT))
        self.f = self.s.makefile("rb")
        self.s.sendall(b"PARLORNET/1\r\n")
        self.f.readline()

    def req(self, text):
        self.s.sendall(text.encode() + b"\r\n")
        out = []
        while True:
            l = self.f.readline().decode().rstrip("\r\n")
            out.append(l)
            if l.startswith(("OK", "ERR", "ASK")):
                return out

    def sign(self, verb, name, pw):
        r = self.req("%s %s" % (verb, name))
        while r[-1].startswith("ASK"):
            f = r[-1].split()
            r = self.req("AUT %s %s" % (f[1], "y" if "yn" in f else pw))
        return r[-1]


def online(c):
    return any(WHO in l for l in c.req("CMD /who") if l.startswith("INF"))


watch = Conn()
print("watcher", watch.sign("REG", "kip", "Swordfish 1997"), flush=True)
seen = []
while seen[-3:] != [True, False, True]:
    on = online(watch)
    if not seen or seen[-1] != on:
        seen.append(on)
        print("online" if on else "offline", flush=True)
    time.sleep(1)
time.sleep(10)
me = Conn()
print("as the player", me.sign("USER", WHO, PW), flush=True)
print(me.req("KEY LST"), flush=True)
print(me.req("KEY DEL all"), flush=True)
time.sleep(600)
