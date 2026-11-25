#!/bin/sh
# Builds ParlorNet for the Apple IIe and the Apple IIgs with cc65, into build/. Run by
# tools/build_apple2.sh in the toolchain container (Dockerfile), from the top of the repository.
#
# The program is the resident part (%O.main at $4000), the vectors (%O.vec, the IIe only) and overlays
# loaded at $D000 one at a time: the sign-in, then one per game, numbered in the order of OVERLAYS (ovl.h
# and the catalog in games.c use the same numbers). An overlay's C file is compiled with every segment
# named OVL_<name>, and its struct (marked OVHDR in the source) comes first.
#
# The IIgs program (build/gs/) is the same source built again with IIGS defined: the machine layer is
# gs.s with sys.s and hgr.s assembled for the 65816, Marinetti (mt.c) takes the Uthernet II's place
# (eth.c, w5100.s), GS/OS calls (gsos.c) take ProDOS's (mli.s), and gsshell.s is the GS/OS application
# that starts it. tools/apple2gs.py makes the application and its disk from these.
set -eu
SRC=src
OUT=build
OBJ=$OUT/obj
GS=$OUT/gs
GOBJ=$GS/obj
OVERLAYS="signin:parlor reversi:reversi backgammon:backgammon gridwords:gridwords checkers:checkers mancala:mancala fiveinarow:fiveinarow dotsandboxes:dotsandboxes navalcombat:navalcombat cannonfire:cannonfire rummytile:rummytile setup:setup splash:splash"
rm -rf "$OBJ" "$GS"
mkdir -p "$OBJ" "$GOBJ"

python3 tools/apple2_art.py "$OBJ"

CFLAGS="-t none --cpu 6502 -O -Oirs -Cl -I $SRC"
ovsrc=""
for o in $OVERLAYS; do
	ovsrc="$ovsrc ${o#*:}"
done

# program OBJDIR MAPOUT PROGRAMOUT CONFIG DEFINES SKIP ASM CRT0
#   compiles every C file but the overlays and those named in SKIP, assembles the files in ASM and the
#   font, then the overlays, and links them after CRT0 (an object in OBJDIR) with ld65 and CONFIG.
program() {
	pobj=$1 pmap=$2 pout=$3 pcfg=$4 pdef=$5 pskip=$6 pasm=$7 pcrt=$8
	objs=""
	for c in "$SRC"/*.c; do
		b=$(basename "$c" .c)
		case " $ovsrc $pskip " in *" $b "*) continue ;; esac
		cc65 $CFLAGS $pdef -o "$pobj/$b.s" "$c"
		ca65 --cpu 6502 -o "$pobj/$b.o" "$pobj/$b.s"
		objs="$objs $pobj/$b.o"
	done
	for s in $pasm; do
		ca65 $pdef -I "$SRC" -o "$pobj/$s.o" "$SRC/$s.s"
		objs="$objs $pobj/$s.o"
	done
	ca65 --cpu 6502 -o "$pobj/font.o" "$OBJ/font.s"
	objs="$objs $pobj/font.o"

	mem=""
	seg=""
	for o in $OVERLAYS; do
		name=${o%%:*}
		b=${o#*:}
		# Code, read-only data, data and the zeroed data each in a segment of its own (in one segment
		# cc65 would put a function's static variables before its first instruction).
		cc65 $CFLAGS $pdef --code-name "OVC_$name" --rodata-name "OVR_$name" --data-name "OVD_$name" \
			--bss-name "OVB_$name" -o "$pobj/$b.s" "$SRC/$b.c"
		sed -i "s/\"OVHDR\"/\"OVH_$name\"/" "$pobj/$b.s"
		ca65 --cpu 6502 -o "$pobj/$b.o" "$pobj/$b.s"
		objs="$objs $pobj/$b.o"
		if [ -f "$OBJ/${name}_art.s" ]; then
			sed -i "s/\"RODATA\"/\"OVR_$name\"/" "$OBJ/${name}_art.s"
			ca65 --cpu 6502 -o "$pobj/${name}_art.o" "$OBJ/${name}_art.s"
			objs="$objs $pobj/${name}_art.o"
		fi
		mem="$mem    OV_$name: file = \"%O.$name\", start = \$D000, size = \$2900;\\n"
		for s in OVH OVC OVR OVD OVB; do
			seg="$seg    ${s}_$name: load = OV_$name, type = rw, optional = yes;\\n"
		done
	done
	sed -e "s/^#OVLMEM/$mem/" -e "s/^#OVLSEG/$seg/" "$SRC/$pcfg" > "$pobj/$pcfg"
	ld65 -C "$pobj/$pcfg" -m "$pmap" -o "$pout" "$pobj/$pcrt" $objs /usr/share/cc65/lib/none.lib
}

# The Apple IIe: the self-booting disk's boot block, PARLOR.SYSTEM, and the program.
ca65 --cpu 6502 -o "$OBJ/crt0.o" "$SRC/crt0.s"
ca65 --cpu 6502 -o "$OBJ/boot.o" "$SRC/boot.s"
ld65 -C "$SRC/boot.cfg" -o "$OUT/boot.bin" "$OBJ/boot.o"
ca65 --cpu 6502 -o "$OBJ/prodsys.o" "$SRC/prodsys.s"
ld65 -C "$SRC/prodsys.cfg" -o "$OUT/prodsys.bin" "$OBJ/prodsys.o"
program "$OBJ" "$OUT/parlor.map" "$OUT/parlor" parlor.cfg "" "gsos mt" "sys hgr disk mli w5100" crt0.o
echo "$OVERLAYS" > "$OUT/overlays.txt"

# The Apple IIgs: PARLORNET's start, and the program.
ca65 --cpu 65816 -o "$GOBJ/gsshell.o" "$SRC/gsshell.s"
ld65 -C "$SRC/gsshell.cfg" -o "$GS/shell.bin" "$GOBJ/gsshell.o"
ca65 -D IIGS -o "$GOBJ/gs.o" "$SRC/gs.s"
program "$GOBJ" "$GS/parlor.map" "$GS/parlor" parlorgs.cfg "-D IIGS" "eth" "sys hgr" gs.o
echo "$OVERLAYS" > "$GS/overlays.txt"
