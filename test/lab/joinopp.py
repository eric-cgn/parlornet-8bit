"""The other player of plans/join.txt: signs up bob on the lab parlor (localhost:16501), names
GameMaster on a Macintosh Plus (CLI), waits for pat's open Reversi table and sits down; later a second
connection of bob's names another program and chats, so the table hears that bob now plays with it."""
import socket
import sys
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 16501     # the lab parlor (parlor.sh)


class Conn:
    def __init__(self):
        self.s = socket.create_connection(("127.0.0.1", PORT))
        self.f = self.s.makefile("rb")
        self.s.sendall(b"PARLORNET/1\r\n")
        print(self.line())

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

    def sign(self, name):
        r = self.req("HELO " + name)
        while r[-1].startswith("ASK"):
            f = r[-1].split()
            r = self.req("AUT %s %s" % (f[1], "y" if "yn" in f else "Swordfish 1997"))
        print(name, r[-1])


a = Conn()
a.req("CLI PROG GameMaster 1.0.0 + ParlorNet 0.5")
a.req("CLI MACH Macintosh Plus")
a.sign("bob")
tid = None
for _ in range(600):
    for l in a.req("LST open"):
        p = l.split()
        if p[:1] == ["TAB"] and p[6].startswith("pat,"):
            tid = p[1]
    if tid:
        break
    time.sleep(0.5)
print("table", tid, a.req("JON %s" % tid))
time.sleep(float(sys.argv[2]) if len(sys.argv) > 2 else 25)
b = Conn()
b.req("CLI PROG RSCARDS 5.02")
b.req("CLI MACH IBM PC")
b.sign("bob")
print(b.req("CHT %s Hello from the PC." % tid))
time.sleep(60)
