#!/bin/sh
# Runs the client in MAME's Apple IIe (enhanced) in Docker: a Super Serial Card in slot 2 whose line goes to
# a TCP port on the host (the bridge, bridge.py, which relays it to a parlor), an AppleMouse II card in slot
# 4, the Apple joystick in the game port and the disk image in drive 1, following a plan (run.lua describes
# the steps). Screenshots, text dumps and the logs go to OUTDIR. The orchestrator (orch.py) runs beside it
# for plans that ask for moves. With ETH=1, the Uthernet II test double (w5100.lua) and its relay
# (eth_relay.py) take part too. With LINE=host:port the serial line goes there instead (and with NET=name
# the MAME container joins that Docker network): the Unix shell test uses it. P dials a modem first, which a
# plain relay to the parlor cannot answer, so on the bridge's port (16601), or with DIRECT=1, the plan
# empties the dialer's number once the title screen has gone, as a player on a direct line does in the
# settings.
#
# usage: A2ROMS=/path/to/roms run.sh PORT DISK PLAN OUTDIR
# A2ROMS is a directory with MAME's apple2ee, a2ssc, a2mouse, m68705p3 and a2diskiing ROM sets (and apple2c for
# MACH=apple2c), which you supply; none are part of this repository.
set -eu
PORT=$1
DISK=$2
PLAN=$3
OUTDIR=$4
: "${A2ROMS:?set A2ROMS to the directory of your MAME ROM sets}"
LAB=$(cd "$(dirname "$0")" && pwd)
TEST=$(dirname "$LAB")
WORK=$TEST/work
XCHG=$WORK/xchg
SCRIPTS=$WORK/scripts
IMAGE=parlornet-8bit-mame
docker image inspect "$IMAGE" >/dev/null 2>&1 || docker build -q -t "$IMAGE" "$TEST/mame" >/dev/null
mkdir -p "$OUTDIR" "$XCHG" "$SCRIPTS"
OUTDIR=$(cd "$OUTDIR" && pwd)
DISKDIR=$(cd "$(dirname "$DISK")" && pwd)
rm -f "$XCHG/need" "$XCHG/steps.txt" "$XCHG/use_w5100"
for s in t_n t_cur t_id mono cfg_number cfg_tutor; do
	printf '%s %s\n' "$s" "$(grep -o "_$s  *[0-9A-F]*" "$DISKDIR/parlor.map" | head -1 | awk '{print $2}')"
done > "$XCHG/addr.txt"
cp "$TEST/mame/run.lua" "$TEST/mame/w5100.lua" "$TEST/mame/joy.lua" "$SCRIPTS/"
if [ "$PORT" = 16601 ] || [ "${DIRECT:-}" = 1 ]; then
	awk '{ print } /^title/ && !done { print "hold cfg_number 0 30"; done = 1 }' "$PLAN" > "$SCRIPTS/plan.txt"
else
	cp "$PLAN" "$SCRIPTS/plan.txt"
fi
python3 "$LAB/orch.py" "$XCHG" > "$OUTDIR/orch.log" 2>&1 &
ORCH=$!
RELAY=
if [ -n "${ETH:-}" ]; then
	touch "$XCHG/use_w5100"
	python3 "$LAB/eth_relay.py" "$XCHG" > "$OUTDIR/relay.out" 2>&1 &
	RELAY=$!
fi
LINE=${LINE:-host.docker.internal:$PORT}
# MACH=apple2c runs it on a IIc, whose modem port takes the line; MAMEARGS adds MAME options, such as
# "-ramsize 64K -aux std80" for a IIe without the auxiliary 64K (plans/k64.txt).
if [ "${MACH:-apple2ee}" = apple2c ]; then
	PORTS="-modem null_modem -bitb socket.$LINE -gameio joy"
else
	PORTS="-sl2 ssc -sl2:ssc:rs232 null_modem -bitb socket.$LINE -sl4 mouse -gameio joy"
fi
docker run --rm ${NET:+--network "$NET"} -v "$A2ROMS:/roms:ro" -v "$SCRIPTS:/scripts:ro" -v "$OUTDIR:/out" -v "$DISKDIR:/disk:ro" \
	-v "$XCHG:/xchg" "$IMAGE" sh -c "cd /tmp && /usr/games/mame ${MACH:-apple2ee} -rompath /roms $PORTS \
		-flop1 /disk/$(basename "$DISK") ${MAMEARGS:-} -video none -sound none -skip_gameinfo \
		-autoboot_script /scripts/run.lua 2>&1 | grep -v -E 'XDG_RUNTIME|ALSA|seconds_to_run' || true"
kill $ORCH $RELAY 2>/dev/null || true
