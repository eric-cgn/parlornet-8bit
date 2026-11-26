"""Reads the run.log of plans/dhr_timing.txt, played by a build with DHR_TIMING (run.lua's watch on $FEFF,
where draw/backgammon.c marks its drawing: 3 a full board, 1 a partial one, 2 a checker's step, 0 done),
and prints how many frames each drawing took. Usage: python3 marks.py OUTDIR/run.log"""
import sys
ev = []
for line in open(sys.argv[1]):
    w = line.split()
    if len(w) == 3 and w[1] == "mark":
        ev.append((int(w[0]), int(w[2])))
full, part, steps = [], [], []
start = None
step_at = None
for f, v in ev:
    if v == 3:
        start = ("full", f)
    elif v == 1 and (start is None or start[0] != "full"):
        if step_at is not None:
            steps.append(f - step_at); step_at = None
        if start is None:
            start = ("part", f)
    elif v == 1 and step_at is not None:
        steps.append(f - step_at); step_at = None
    elif v == 2:
        if start is None:
            start = ("part", f)
        step_at = f
    elif v == 0 and start:
        (full if start[0] == "full" else part).append(f - start[1])
        if step_at is not None:
            steps.append(f - step_at); step_at = None
        start = None
def show(name, xs):
    if xs:
        print("%-8s n=%2d frames: min %d median %d max %d   %s" % (name, len(xs), min(xs), sorted(xs)[len(xs) // 2], max(xs), xs))
show("full", full); show("partial", part); show("step", steps)
