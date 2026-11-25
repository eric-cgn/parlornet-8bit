#!/bin/sh
# Runs the C64 program in VICE (x64sc, a PAL C64 by default) in Docker, following a plan (run.py describes the
# steps; the program's addresses come from build.sh's label file, parlor.lbl, as labels.txt), with screenshots and the log in OUTDIR. The container joins the lab's network (c64lab, which
# parlor.sh makes, with the parlor as c64-parlor) and nothing else.
#
# The serial line: a SwiftLink cartridge at $DE00 (IFACE=swift, the default, as the program's settings start;
# ACIA=0xdf00 puts it at $DF00, the only other place VICE has for it on a C64)
# or VICE's user port RS-232 (IFACE=user, for the Supra 2400 compatible modem) goes to a TCP port in the
# container: there test/lab/bridge.py is a WiFi modem's stand-in (LINE=modem, the default: AT commands,
# ATDT connects to the parlor) or a plain relay to the parlor (LINE=direct, with the dialer's number
# emptied after the title, as a player on a direct line sets it). LINE=host:port sends the line to another
# container on c64lab instead (NET names another network), the Unix shell's for pndial.
# (VICE runs under script, on a terminal of its own: without one, its SDL interface stops at start.)
# NTSC=1 runs an NTSC machine; C128=1 runs a C128 (x128), which boots the disk and goes to 64 mode. The 1541 is emulated in full, at its own speed;
# FAST=1 runs the whole emulation in warp mode (for plans that do not depend on time). VICE works on a copy
# of the disk, which is left in OUTDIR afterwards (disk-after.d64), with what the program wrote on it.
#
# usage: test/vice/run.sh DISK PLAN OUTDIR
set -eu
DISK=$1
PLAN=$2
OUTDIR=$3
VICE=$(cd "$(dirname "$0")" && pwd)
TEST=$(dirname "$VICE")
WORK=$TEST/work
XCHG=${XCHG:-$WORK/c64xchg}     # (the orchestrator answers in c64xchg; another one lets plans without need steps run beside)
IMAGE=${VICE_IMAGE:-parlornet-8bit-vice}
mkdir -p "$OUTDIR" "$XCHG"
OUTDIR=$(cd "$OUTDIR" && pwd)
DISKDIR=$(cd "$(dirname "$DISK")" && pwd)
rm -f "$XCHG/need" "$XCHG/steps.txt"
cp "$DISKDIR/parlor.lbl" "$XCHG/labels.txt"
LINE=${LINE:-modem}
if [ "$LINE" = direct ]; then
	awk '{ print } /^title/ && !done { print "set cfg_number 0"; done = 1 }' "$PLAN" > "$XCHG/plan.txt"
else
	cp "$PLAN" "$XCHG/plan.txt"
fi
case "$LINE" in
	modem) RELAY="python3 -I /lab/bridge.py 16600 c64-parlor 16501 modem /out/modem.log"; DEV=127.0.0.1:16600 ;;
	direct) RELAY="python3 -I /lab/bridge.py 16600 c64-parlor 16501 bridge /out/bridge.log"; DEV=127.0.0.1:16600 ;;
	*) RELAY=true; DEV=$LINE ;;
esac
BAUD=${BAUD:-19200}
case "${IFACE:-swift}" in
	user) SER="-userportdevice 2 -rsuserdev 0 -rsuserbaud $BAUD +rsuserup9600" ;;
	swift) SER="-acia1 -acia1base ${ACIA:-0xde00} -acia1mode 1 -myaciadev 0" ;;
esac
# control port 2: VICE's I/O simulation device, which the monitor's joyport set drives (run.py's joy)
SER="$SER -controlport2device 37"
EMU=x64sc
[ -n "${C128:-}" ] && EMU=x128
# VICE's autostart (LOAD"*",8,1 and RUN), or ATTACH=1: the disk in drive 8 only, for a plan that types the
# LOAD itself (basic steps) or for a C128, which boots the disk by itself
START="-autostart /tmp/disk.d64"
[ -n "${ATTACH:-}${C128:-}" ] && START="-8 /tmp/disk.d64"
MODEL=
[ -n "${NTSC:-}" ] && MODEL="-ntsc"
DRIVE="-drive8truedrive +autostart-warp"
[ -n "${FAST:-}" ] && DRIVE="-drive8truedrive -warp"
docker run --rm --network "${NET:-c64lab}" --cap-drop ALL --security-opt no-new-privileges -e HOME=/tmp \
	-v "$TEST/lab:/lab:ro" -v "$VICE:/vice:ro" -v "$OUTDIR:/out" -v "$DISKDIR:/disk:ro" -v "$XCHG:/xchg" \
	"$IMAGE" sh -c "
	Xvfb :1 -screen 0 1024x768x24 -nolisten tcp > /tmp/xvfb.log 2>&1 &
	$RELAY > /out/relay.log 2>&1 &
	sleep 1
	export DISPLAY=:1 SDL_AUDIODRIVER=dummy XDG_RUNTIME_DIR=/tmp
	cp /disk/$(basename "$DISK") /tmp/disk.d64
	script -qfc \"$EMU $MODEL -sounddev dummy -binarymonitor -binarymonitoraddress ip4://127.0.0.1:6502 \
		-rsdev1 $DEV -rsdev1baud $BAUD $SER $DRIVE $START\" /dev/null \
		> /out/vice.log 2>&1 &
	python3 -I /vice/run.py /xchg/plan.txt /out /xchg
	pkill -f x64sc; pkill -f x128; sleep 1; cp /tmp/disk.d64 /out/disk-after.d64" || true
