#!/usr/bin/env python3
"""The lab's orchestrator: when the MAME runner asks (xchg/need: "GAME NAME PASSWORD"), it signs in to the
lab parlor as that player on a connection of its own, reads the view of the player's running table with its
legal moves, and writes the runner's next steps (xchg/steps.txt): clicks on the targets that make one
legal move, as the Apple program numbers them (the C64 program numbers them the same). A game's name with + after
it (ckplay+) plays on until the game is over, where the name alone stops after a few moves. Runs until killed.
usage: orch.py XCHG_DIR [port]"""
import os
import socket
import sys
import time

X = sys.argv[1]
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 16501
STATE = {}
MINSTEPS = 4


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
                if line.startswith("VUE "):
                    # the view's V line (its type and event number), to tell a table that has moved on
                    STATE["vseq"] = ([l for l in out if l.startswith("V ")] or [""])[0]
                return out


def view(name, pw, typ):
    c = Conn()
    c.req("USER " + name)
    c.req("AUT password " + pw)
    tabs = [l.split() for l in c.req("LST") if l.startswith("TAB ")]
    tabs = [t for t in tabs if t[2].startswith(typ) and name in t[6].split(",")]
    t = tabs[-1][1]
    v = c.req("VUE %s TM" % t)
    c.s.close()
    return t, v


def steps_for(game, name, pw):
    if game == "rv" and STATE.get("nmax"):
        # rv+: our moves (the first legal square) until the game is over
        t, v = view(name, pw, "reversi")
        again = "need rv+ %s %s" % (name, pw)
        turn = ([l.split()[1] for l in v if l.startswith("W ")] or ["-"])[0]
        m = [l[2:] for l in v if l.startswith("M ")]
        STATE["n"] = STATE.get("n", 0) + 1
        if turn in ("-", "-1") or STATE["n"] > STATE["nmax"]:
            return []
        if turn != "0" or not m:
            return ["wait 200", again]
        sq = m[0]
        return ["click %d" % ((int(sq[1:]) - 1) * 16 + ord(sq[0]) - 97), "wait 300", again]
    if game == "rv":
        t, v = view(name, pw, "reversi")
        m = [l[2:] for l in v if l.startswith("M ")]
        if not m:
            return ["wait 300"]
        sq = m[0]
        return ["click %d" % ((int(sq[1:]) - 1) * 16 + ord(sq[0]) - 97), "wait 400"]
    if game == "bg":
        t, v = view(name, pw, "backgammon")
        q = [l.split()[1] for l in v if l.startswith("Q ")]
        q = q[0] if q else ""
        m = [l[2:] for l in v if l.startswith("M ")]
        d = [l for l in v if l.startswith("D")]
        if q == "roll":
            return ["click 30", "wait 300"]
        if q == "move" and m:
            a, b = m[0].split("-")
            return ["click %d" % (25 if a == "bar" else int(a)), "wait 60",
                    "click %d" % (26 if b == "off" else int(b)), "wait 400"]
        if q == "move":
            return ["click 30", "wait 400"]
        return ["wait 300"]
    if game.startswith("bgsnap"):
        # bgsnapX: on our move, the first legal step, then snapshots X01 to X30 three frames apart (a
        # checker gliding); before that, roll or wait for our turn.
        t, v = view(name, pw, "backgammon")
        again = "need %s %s %s" % (game, name, pw)
        q = ([l.split()[1] for l in v if l.startswith("Q ")] or [""])[0]
        turn = ([l.split()[1] for l in v if l.startswith("W ")] or ["-"])[0]
        m = [l[2:] for l in v if l.startswith("M ")]
        if turn != "0":
            return ["wait 300", again]
        if q == "roll":
            return ["click 30", "wait 300", again]
        if q != "move" or not m:
            return ["wait 300", again]
        a, b = m[0].split("-")
        out = ["click %d" % (25 if a == "bar" else int(a)), "wait 60", "click %d" % (26 if b == "off" else int(b))]
        for k in range(1, 31):
            out += ["wait 3", "snap %s%02d" % (game[6:], k)]
        return out + ["wait 300"]
    if game in ("bgplay", "bgdbl", "bgbarme"):
        # Play on, preferring hits, until a checker is on the bar, at least MINSTEPS steps of ours have
        # been played and it is our move with dice showing; then return no steps (the plan goes on).
        # bgbarme: without hitting, until one of ours is on the bar.
        t, v = view(name, pw, "backgammon")
        STATE["n"] = STATE.get("n", 0) + 1
        again = "need %s %s %s" % (game, name, pw)
        q = ([l.split()[1] for l in v if l.startswith("Q ")] or [""])[0]
        turn = ([l.split()[1] for l in v if l.startswith("W ")] or ["-"])[0]
        pts = [0] * 25
        for l in v:
            if l.startswith("P "):
                ws = l.split()
                b = int(ws[1])
                for k, c in enumerate(ws[2:]):
                    pts[b + k] = int(c)
        bar = [int(x) for x in ([l.split()[1:3] for l in v if l.startswith("B ")] or [["0", "0"]])[0]]
        seat = STATE.setdefault("seat", 0)
        if STATE["n"] > STATE.get("nmax", 200 if game == "bgbarme" else 60) or turn == "-":
            return []
        if turn != str(seat):
            return ["wait 300", again]
        dr = ([l.split()[1:] for l in v if l.startswith("DR ")] or [[]])[0]
        dl = ([l.split()[1:] for l in v if l.startswith("D")and l.split()[0] == "D"] or [[]])[0]
        playon = STATE.get("nmax", 0) >= 10 ** 6         # bgplay+: on to the end of the game
        if game == "bgplay" and not playon and bar[0] + bar[1] > 0 and STATE.get("steps", 0) >= MINSTEPS \
                and q == "move":
            return []
        if game == "bgbarme" and bar[seat] > 0 and q == "move":
            return []
        if game == "bgdbl" and q == "move" and len(dr) == 2 and dr[0] == dr[1] and len(dl) in (2, 3)                 and STATE.get("steps", 0) >= MINSTEPS:
            return []
        m = [l[2:] for l in v if l.startswith("M ")]
        if q == "roll":
            return ["click 30", "wait 300", again]
        if q == "move" and m:
            opp = -1 if seat == 0 else 1
            hits = [x for x in m if x.split("-")[1] not in ("off",) and pts[int(x.split("-")[1])] == opp]
            if game == "bgbarme":
                hits = []
            a, b = (hits or m)[0].split("-")
            STATE["steps"] = STATE.get("steps", 0) + 1
            return ["click %d" % (25 if a == "bar" else int(a)), "wait 60",
                    "click %d" % (26 if b == "off" else int(b)), "wait 400", again]
        if q == "move":
            return ["click 30", "wait 400", again]
        return ["wait 300", again]
    if game == "ckplay":
        # One step of a checkers turn as the Apple takes it (piece, then square; mid-jump only the
        # square), Done when the turn is complete; until STEPS of ours, then no steps.
        t, v = view(name, pw, "checkers")
        again = "need ckplay %s %s" % (name, pw)
        q = ([l.split()[1] for l in v if l.startswith("Q ")] or [""])[0]
        turn = ([l.split()[1] for l in v if l.startswith("W ")] or ["-"])[0]
        seat = STATE.setdefault("seat", 0)
        STATE["n"] = STATE.get("n", 0) + 1
        if turn == "-" or STATE["n"] > STATE.get("nmax", 80):
            return []
        if turn != str(seat):
            return ["wait 240", again]
        m = [l[2:] for l in v if l.startswith("M ")]
        if STATE.get("steps", 0) >= STATE.get("want", 6) and q == "move":
            return ["click %d" % int(m[0].replace("x", "-").split("-")[0]), "wait 90"] if m else []
        j = [l for l in v if l.startswith("J ")]
        if q == "done":
            return ["click 40", "wait 300", again]
        if q == "move" and m:
            caps = [x for x in m if "x" in x] or m
            import re
            a, b = [int(x) for x in re.split("[-x]", caps[0])[:2]]
            STATE["steps"] = STATE.get("steps", 0) + 1
            st = [] if j else ["click %d" % a, "wait 60"]
            return st + ["click %d" % b, "wait 300", again]
        return ["wait 240", again]
    if game in ("cfplay", "rtplay"):
        typ = "cannonfire" if game == "cfplay" else "rummytile"
        t, v = view(name, pw, typ)
        again = "need %s %s %s" % (game, name, pw)
        turn = ([l.split()[1] for l in v if l.startswith("W ")] or ["-"])[0]
        STATE["n"] = STATE.get("n", 0) + 1
        if turn in ("-", "-1") or STATE["n"] > STATE.get("nmax", 60):
            return []
        if turn != "0":
            return ["wait 200", again]
        if STATE.get("steps", 0) >= STATE.get("want", 3):
            return []
        STATE["steps"] = STATE.get("steps", 0) + 1
        if game == "cfplay":
            st = []
            if STATE["steps"] == 1:
                st = ["click 7", "wait 300"]
            return st + ["click 4", "wait 30", "click 4", "wait 30", "click 2", "wait 30", "click 5", "wait 400", again]
        rack = []
        for l in v:
            if l.startswith("H "):
                rack += l.split()[1:]
        tiles = [(r[0], int(r[1:]) if r != "J" else 0, i) for i, r in enumerate(rack)]
        best = None
        for n in range(1, 14):
            g = {}
            for c, k, i in tiles:
                if k == n and c not in g:
                    g[c] = i
            if len(g) >= 3:
                s = list(g.values())[:4]
                if not best or n * len(s) > best[0]:
                    best = (n * len(s), s)
        for c in "RBKY":
            nums = {}
            for cc, k, i in tiles:
                if cc == c and k not in nums:
                    nums[k] = i
            for a in range(1, 12):
                run = []
                k = a
                while k in nums:
                    run.append(nums[k])
                    k += 1
                if len(run) >= 3:
                    pts = sum(range(a, a + len(run)))
                    if not best or pts > best[0]:
                        best = (pts, run)
        st = []
        if best:
            for i in best[1]:
                st += ["click %d" % (110 + i), "wait 30"]
            st += ["click 203", "wait 300"]
            if not STATE.get("snapped"):
                STATE["snapped"] = 1
                st += ["snap rt_staged"]
        return st + ["click 200", "wait 400", again]
    if game in ("dbplay", "ncplay", "ncplace"):
        typ = "dotsandboxes" if game == "dbplay" else "navalcombat"
        t, v = view(name, pw, typ)
        again = "need %s %s %s" % (game, name, pw)
        turn = ([l.split()[1] for l in v if l.startswith("W ")] or ["-"])[0]
        STATE["n"] = STATE.get("n", 0) + 1
        if turn in ("-", "-1") or STATE["n"] > STATE.get("nmax", 80):
            return []
        m = [l[2:] for l in v if l.startswith("M ")]
        if game == "ncplace":
            # random placing, one ship at a time by hand first (two ships), then Random and Ready
            q = ([l.split()[1] for l in v if l.startswith("Q ")] or [""])[0]
            if q != "place":
                return []
            n = [l for l in v if l.startswith("N ")]
            k = STATE.get("placed", 0)
            if n and k < 2:
                STATE["placed"] = k + 1
                a, b = m[0 if k == 0 else len(m) // 2].split("-")
                ids = [(int(s[1:]) - 1) * 10 + ord(s[0]) - 97 for s in (a, b)]
                return ["click %d" % ids[0], "wait 60", "click %d" % ids[1], "wait 300"] +                     (["snap nc_placing"] if k == 1 else []) + [again]
            if n and k == 2:
                STATE["placed"] = 3
                a = m[len(m) // 3].split("-")[0]
                return ["click %d" % ((int(a[1:]) - 1) * 10 + ord(a[0]) - 97), "wait 120", "snap nc_mark",
                        "click 100", "wait 300", again]
            if n:
                return ["click 100", "wait 300", again]
            if "ready" in m:
                return ["click 103", "wait 300"]
            return []
        if turn != "0":
            return ["wait 200", again]
        if STATE.get("steps", 0) >= STATE.get("want", 8):
            return []
        STATE["steps"] = STATE.get("steps", 0) + 1
        if game == "ncplay":
            sh = m[(STATE["steps"] * 37) % len(m)]
            return ["click %d" % ((int(sh[1:]) - 1) * 10 + ord(sh[0]) - 97), "wait 300", again]
        bw = int([l.split()[1] for l in v if l.startswith("BRD ")][0]) if any(l.startswith("BRD ") for l in v) else 5
        bw = STATE.setdefault("bw", 5)
        ln = m[(STATE["steps"] * 7) % len(m)]
        a, b = ln.split("-")
        ids = [(int(s[1:]) - 1) * (bw + 1) + ord(s[0]) - 97 for s in (a, b)]
        return ["click %d" % ids[0], "wait 60", "click %d" % ids[1], "wait 300", again]
    if game in ("mcplay", "frplay"):
        # Play until WANT moves of ours: mancala the first legal pit, five in a row a point next to the
        # center's stones (a short row of our own, across); then no steps.
        typ = "mancala" if game == "mcplay" else "fiveinarow"
        t, v = view(name, pw, typ)
        again = "need %s %s %s" % (game, name, pw)
        turn = ([l.split()[1] for l in v if l.startswith("W ")] or ["-"])[0]
        STATE["n"] = STATE.get("n", 0) + 1
        if turn in ("-", "-1") or STATE["n"] > STATE.get("nmax", 60):
            return []
        if turn != "0":
            return ["wait 200", again]
        if STATE.get("steps", 0) >= STATE.get("want", 5):
            return []
        STATE["steps"] = STATE.get("steps", 0) + 1
        if game == "mcplay":
            m = [l[2:] for l in v if l.startswith("M ")]
            return ["click %s" % m[len(m) // 2], "wait 300", again]
        taken = set()
        for l in v:
            if l.startswith("P "):
                for p in l.split()[2:]:
                    taken.add((ord(p[0]) - 97, int(p[1:]) - 1))
        k = STATE["steps"] - 1
        for (x, y) in [(5 + k, 7), (5 + k, 6), (5 + k, 8), (4, 4 + k), (10, 4 + k)] + \
                [(a, b) for a in range(15) for b in range(15)]:
            if (x, y) not in taken:
                return ["click %d" % (y * 15 + x), "wait 300", again]
        return []
    if game == "gwhover":
        # point at the square left of a played tile (a blank if there is one), then Right onto it
        c = Conn()
        c.req("USER " + name)
        c.req("AUT password " + pw)
        tabs = [l.split() for l in c.req("LST") if l.startswith("TAB ") and "gridwords" in l]
        v = c.req("VUE %s TM" % tabs[-1][1])
        c.s.close()
        tiles = []
        for l in v:
            if l.startswith("X "):
                ws = l.split()[1:]
                tiles += [(int(ws[k]) - 1, int(ws[k + 1]) - 1, ws[k + 2]) for k in range(0, len(ws), 3)]
        tiles = [t for t in tiles if t[0] > 0]
        tiles.sort(key=lambda t: (not t[2].islower(), t[1], t[0]))
        if not tiles:
            return []
        x, y, _ = tiles[0]
        return ["point %d" % (y * 15 + x - 1), "wait 10", "arrows R", "wait 60"]
    if game == "gwlay":
        # Find the best-scoring play of 1-3 rack tiles the way the Apple lays them (a start square, then
        # the letters fill the empty squares along the line) and return the clicks and keys that lay it
        # (without Return).
        import itertools
        c = Conn()
        c.req("USER " + name)
        c.req("AUT password " + pw)
        tabs = [l.split() for l in c.req("LST") if l.startswith("TAB ") and "gridwords" in l]
        t = tabs[-1][1]
        v = c.req("VUE %s TM" % t)
        rack = [l.split()[1] for l in v if l.startswith("R ")][0]
        board = {}
        for l in v:
            if l.startswith("X "):
                ws = l.split()[1:]
                for k in range(0, len(ws), 3):
                    board[(int(ws[k]) - 1, int(ws[k + 1]) - 1)] = ws[k + 2]
        letters = [ch for ch in rack if ch != "_"]
        best = None
        tries = 0
        starts = set()
        for (x, y) in board:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (-2, 0), (0, -2)):
                q = (x + dx, y + dy)
                if 0 <= q[0] < 15 and 0 <= q[1] < 15 and q not in board:
                    starts.add(q)
        if not board:
            starts = {(7, 7)}
        found = 0
        for n, (sx, sy), down in [(n, q, d) for n in (2, 1, 3) for q in sorted(starts) for d in (0, 1)]:
                if found > 25 or tries > 6000:
                    break
                if True:
                    for p in sorted(set(itertools.permutations(letters, n))):
                        # lay the letters
                        x, y, placed, ok = sx, sy, {}, True
                        for ch in p:
                            while (x, y) in board:
                                x, y = (x, y + 1) if down else (x + 1, y)
                            if x > 14 or y > 14:
                                ok = False
                                break
                            placed[(x, y)] = ch
                            x, y = (x, y + 1) if down else (x + 1, y)
                        if not ok:
                            continue
                        x, y = sx, sy
                        while (x - (0 if down else 1), y - (1 if down else 0)) in board:
                            x, y = (x, y - 1) if down else (x - 1, y)
                        bx, by, w = x, y, ""
                        while x < 15 and y < 15 and ((x, y) in board or (x, y) in placed):
                            w += board.get((x, y)) or placed[(x, y)]
                            x, y = (x, y + 1) if down else (x + 1, y)
                        tries += 1
                        r = c.req("CHK %s PLAY %d %d %s %s" % (t, bx + 1, by + 1, "V" if down else "H", w))[-1]
                        if r.startswith("OK"):
                            found += 1
                            pts = int(r.split()[1])
                            if not best or pts > best[0]:
                                best = (pts, sx, sy, down, "".join(p))
        c.s.close()
        print("orch: best", best, "after", tries, flush=True)
        if not best:
            return ["wait 30"]
        pts, sx, sy, down, word = best
        st = ["click %d" % (sy * 15 + sx), "wait 30"]
        if down:
            st += ["click %d" % (sy * 15 + sx), "wait 30"]
        for ch in word:
            st += ["type " + ch.lower(), "wait 90"]
        return st + ["wait 60"]
    if game in ("gw", "gwswap", "gwpass"):
        import itertools
        c = Conn()
        c.req("USER " + name)
        c.req("AUT password " + pw)
        tabs = [l.split() for l in c.req("LST") if l.startswith("TAB ") and "gridwords" in l]
        t = tabs[-1][1]
        v = c.req("VUE %s TM" % t)
        rack = [l.split()[1] for l in v if l.startswith("R ")][0]
        tiles = [l for l in v if l.startswith("X ")]
        if game == "gwswap":
            return ["click 234", "wait 60", "click 225", "wait 60", "click 226", "wait 60", "click 234", "wait 600"]
        if game == "gwpass" and STATE.get("nmax"):
            # gwpass+: pass on each of our turns until the game is over
            again = "need gwpass+ %s %s" % (name, pw)
            turn = ([l.split()[1] for l in v if l.startswith("W ")] or ["-"])[0]
            c.s.close()
            STATE["n"] = STATE.get("n", 0) + 1
            if turn in ("-", "-1") or STATE["n"] > STATE["nmax"]:
                return []
            if turn != "0":
                return ["wait 300", again]
            return ["click 235", "wait 30", "type y", "wait 600", again]
        if game == "gwpass":
            return ["click 235", "wait 30", "type y", "wait 600"]
        letters = [ch for ch in rack if ch != "_"]
        if not tiles:
            for n in (2, 3, 4):
                for p in itertools.permutations(letters, n):
                    w = "".join(p)
                    if c.req("CHK %s PLAY 8 8 H %s" % (t, w))[-1].startswith("OK"):
                        c.s.close()
                        return ["click 112", "wait 30", "type " + w.lower(), "wait 120", "key ret", "wait 600"]
        c.s.close()
        return ["click 234", "wait 60", "click 225", "wait 30", "click 234", "wait 600"]
    return ["wait 60"]


while True:
    p = os.path.join(X, "need")
    if os.path.exists(p):
        words = open(p).read().split()
        os.remove(p)
        plus = words[0].endswith("+")
        if plus:
            # GAME+: the same play, on until the game is over
            words[0] = words[0][:-1]
            STATE["want"] = STATE["nmax"] = 10 ** 6
        try:
            st = steps_for(words[0], words[1], " ".join(words[2:]))
        except Exception as e:
            st = ["wait 60"]
            print("orch:", e, flush=True)
        if plus or STATE.get("nmax"):
            # a snapshot in the middle of the game (mid.png), on the 12th turn's steps of each player's game
            # (GAME+ asks again as GAME+ or as GAME, with nmax set)
            k = "plus " + words[1]
            STATE[k] = STATE.get(k, 0) + 1
            if STATE[k] == 12:
                st = ["snap mid"] + st
            # the same clicks again and again on a table whose view has not moved on: the move is not taken;
            # show the screen (stuck.png, stuck.txt) on the fifth time and stop on the tenth
            clicks = [s for s in st if s.startswith(("click", "type"))]
            r = "repeat " + words[1]
            key = (clicks, STATE.get("vseq"))   # the same clicks on a table that has not moved on
            n = STATE[r][1] + 1 if clicks and STATE.get(r, (None,))[0] == key else 1
            STATE[r] = (key, n)
            if clicks and n == 5:
                st = ["snap stuck", "dumptext stuck"] + st
            if clicks and n >= 10:
                st = []
        print("orch:", words[0], st, flush=True)
        with open(os.path.join(X, "steps.tmp"), "w") as f:
            f.write("\n".join(st) + "\n")
        os.rename(os.path.join(X, "steps.tmp"), os.path.join(X, "steps.txt"))
    time.sleep(0.2)
