#!/bin/sh
# Runs the IIgs program in MAME's Apple IIgs (ROM 03) in Docker, on a network of its own: System 6.0.1 with
# Marinetti on a hard disk (CFFA2 card, slot 7), the program's 800K disk in the first 3.5-inch drive, and the
# modem port's line, at 9600 baud, to slirp (SLIP in user space) in a container beside it. slirp connects
# on to the lab parlor, a container named parlor on the same network with parlord, the bot and the
# orchestrator (orch.py). The network is internal: nothing leaves it, and no container has extra privileges.
# test/mame/gsrun.lua follows the plan; screenshots (704 by 462), text dumps and the logs go to OUTDIR.
#
# usage: GSROMS=dir GSHD=disk.2mg PARLORNET=checkout test/lab/gs.sh PLAN OUTDIR
# (GSLINE=host:port sends the modem line somewhere else on the network, such as a relay that logs it.)
# GSROMS has MAME's apple2gs and a2cffa2 ROM sets, and GSHD is a 32MB hard disk image with System 6.0.1 and
# Marinetti, set up for SLIP (the Direct Connect link layer) on the modem port at 9600 baud: address 10.0.2.15, DNS
# 10.0.2.3. You supply both; neither is part of this repository. The plan runs on a copy of GSHD.
set -eu
PLAN=$1
OUTDIR=$2
: "${GSROMS:?set GSROMS to the directory of your MAME ROM sets}"
: "${GSHD:?set GSHD to a hard disk image with System 6.0.1 and Marinetti}"
: "${PARLORNET:?set PARLORNET to a ParlorNet checkout}"
LAB=$(cd "$(dirname "$0")" && pwd)
TEST=$(dirname "$LAB")
ROOT=$(dirname "$TEST")
WORK=$TEST/work/gs
XCHG=$WORK/xchg
NET=parlornet-gslab
IMAGE=parlornet-8bit-mame
docker image inspect "$IMAGE" >/dev/null 2>&1 || docker build -q -t "$IMAGE" "$TEST/mame" >/dev/null
docker image inspect parlornet-gslab-slirp >/dev/null 2>&1 || docker build -q -t parlornet-gslab-slirp "$LAB/slirp" >/dev/null
docker network inspect "$NET" >/dev/null 2>&1 || docker network create --internal "$NET" >/dev/null
mkdir -p "$OUTDIR/cfg" "$XCHG" "$WORK/disks"
OUTDIR=$(cd "$OUTDIR" && pwd)
rm -f "$XCHG/need" "$XCHG/steps.txt"

# the disks: a copy of the hard disk, and the program's disk with a 2MG header (the 3.5-inch drives take .2mg)
rm -f "$WORK/disks/hd.2mg"
cp "$GSHD" "$WORK/disks/hd.2mg"
chmod u+w "$WORK/disks/hd.2mg"
python3 - "$ROOT/build/gs/parlornet-gs.po" "$WORK/disks/parlornet-gs.2mg" <<'PY'
import struct, sys
d = open(sys.argv[1], "rb").read()
h = b"2IMG" + b"PNET" + struct.pack("<HHIIIII", 64, 1, 1, 0, len(d) // 512, 64, len(d))
open(sys.argv[2], "wb").write(h.ljust(64, b"\0") + d)
PY
# where the runner finds the program (its first bytes) and the pointer's targets (the link map)
{
	printf 'sig %s\n' "$(head -c 4 "$ROOT/build/gs/parlor.main" | od -An -tx1 | tr -d ' \n')"
	for s in t_n t_cur t_id; do
		printf '%s %s\n' "$s" "$(grep -o "_$s  *[0-9A-F]*" "$ROOT/build/gs/parlor.map" | head -1 | awk '{print $2}')"
	done
} > "$XCHG/addr.txt"
# the modem line's speed, which MAME reads at reset: 9600 baud (at 19200 MAME's IIgs loses incoming packets)
cat > "$OUTDIR/cfg/apple2gs.cfg" <<'CFG'
<?xml version="1.0"?>
<mameconfig version="10">
    <system name="apple2gs">
        <input>
            <port tag=":modem:null_modem:RS232_TXBAUD" type="CONFIG" mask="255" defvalue="7" value="7" />
            <port tag=":modem:null_modem:RS232_RXBAUD" type="CONFIG" mask="255" defvalue="7" value="7" />
        </input>
    </system>
</mameconfig>
CFG

# slirp, and the lab parlor with its bot and the orchestrator
docker rm -f parlornet-gsslirp parlornet-gsparlor >/dev/null 2>&1 || true
docker run -d --rm --name parlornet-gsslirp --network "$NET" parlornet-gslab-slirp >/dev/null
# the parlor's files, in a directory new each run (Docker Desktop's file sharing can refuse to create a file
# under a name the host has just deleted)
rm -rf "$WORK"/srv.*
SRV=$(mktemp -d "$WORK/srv.XXXXXX")
# the bot's key for the parlor's frontend channel
docker run --rm -u "$(id -u):$(id -g)" -e PYTHONDONTWRITEBYTECODE=1 -v "$PARLORNET:/pn:ro" -v "$SRV:/srv" \
	-w /pn python:3.13-slim python3 -c "from parlord import frontends; frontends.write_key('/srv/keys', 'bot', 'frontend')"
docker run -d --rm --name parlornet-gsparlor --network "$NET" --network-alias parlor -u "$(id -u):$(id -g)" \
	-e PYTHONDONTWRITEBYTECODE=1 -e HOME=/srv -v "$PARLORNET:/pn:ro" -v "$LAB:/lab:ro" -v "$XCHG:/xchg" \
	-v "$SRV:/srv" -w /pn python:3.13-slim sh -c '
	python3 -m parlord --host 0.0.0.0 --port 6501 --name labparlor --auth password --signup password \
		--frontend-port 16590 --keys /srv/keys --db /srv/lab.db --admin-telnet 0 --rate 1000 --burst 1000 \
		> /srv/core.log 2>&1 &
	sleep 3
	python3 -m bot --core 127.0.0.1:16590 --key /srv/keys/bot.key --think 0,0 --pace 0 --poll 0.3 \
		> /srv/bot.log 2>&1 &
	exec python3 /lab/orch.py /xchg 6501 > /srv/orch.log 2>&1' >/dev/null
sleep 4

docker run --rm --network "$NET" -e GSPLAN=/plan.txt -v "$GSROMS:/roms:ro" -v "$TEST/mame:/scripts:ro" \
	-v "$(cd "$(dirname "$PLAN")" && pwd)/$(basename "$PLAN"):/plan.txt:ro" -v "$OUTDIR:/out" \
	-v "$WORK/disks:/disks" -v "$XCHG:/xchg" "$IMAGE" sh -c "cd /out && /usr/games/mame apple2gs -rompath /roms \
		-cfg_directory /out/cfg -sl7 cffa2 -hard1 /disks/hd.2mg -flop3 /disks/parlornet-gs.2mg \
		-modem null_modem -bitb socket.${GSLINE:-parlornet-gsslirp:2323} -nothrottle -video none -sound none -skip_gameinfo \
		-autoboot_script /scripts/gsrun.lua 2>&1 | grep -v -E 'XDG_RUNTIME|ALSA|seconds_to_run|REDUMP|might not run' || true"
cp "$SRV/core.log" "$SRV/bot.log" "$SRV/orch.log" "$OUTDIR/" 2>/dev/null || true
docker rm -f parlornet-gsslirp parlornet-gsparlor >/dev/null 2>&1 || true
# MAME's frames are 704 by 231 (the IIgs's 640 by 200 and its border): doubled down, for a 4:3 picture
for f in "$OUTDIR"/*.png; do
	[ -f "$f" ] && python3 "$LAB/pngscale.py" "$f" "$f" 1 2
done
cat "$OUTDIR/run.log"
