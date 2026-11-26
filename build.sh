#!/bin/sh
# Builds ParlorNet's 8-bit programs with cc65, into build/. Run by tools/build.sh in the toolchain container
# (Dockerfile), from the top of the repository.
#
# The source is in src/core (the protocol, the sign-in and dialer, the lobby, the table and the games' rules,
# the same on every machine), src/common (what more than one machine uses) and one folder per machine:
# src/apple2 (the IIe and the IIgs), src/c64, and src/template (the stubbed machine, built only to check the
# interface). The core reaches a machine only through src/core/machine.h, which includes the machine's
# machdefs.h; a machine's folder comes first on the include path, so its machdefs.h and game drawing
# (draw/NAME.c) are the ones the core is built with.
#
# Each program is a resident part, linked from the files named below in that order, and overlays loaded one
# at a time: the sign-in, then one per game, numbered in the order of OVERLAYS (ovl.h and the catalog in
# games.c use the same numbers), then the machine's start screen and title screen. An overlay's C file is
# compiled with every segment named OV<kind>_<name>, and its struct (marked OVHDR in the source) comes first.
#
# The IIgs program (build/gs/) is the IIe's source built again with IIGS defined: the machine layer is
# gs.s with sys.s and hgr.s assembled for the 65816, Marinetti (mt.c) takes the Uthernet II's place
# (eth.c, w5100.s), GS/OS calls (gsos.c) take ProDOS's (mli.s), and gsshell.s is the GS/OS application
# that starts it. tools/apple2gs.py makes the application and its disk from these.
set -eu
OUT=build
OBJ=$OUT/obj
GS=$OUT/gs
GOBJ=$GS/obj
A2=src/apple2
CORE=src/core
COMMON=src/common
COMMON6502=src/common/6502
OVERLAYS="signin reversi backgammon gridwords checkers mancala fiveinarow dotsandboxes navalcombat cannonfire rummytile setup splash"
rm -rf "$OBJ" "$GS" "$OUT/c64" "$OUT/template"
mkdir -p "$OBJ" "$GOBJ"
# the version, from VERSION, for the title screens
printf '#define PN_VERSION "%s"\n' "$(cat VERSION)" > "$OBJ/version.h"

python3 tools/apple2_art.py "$OBJ"

# An overlay's sources: the sign-in is the core's parlor.c and the machine's signin_net.c (its network), a
# game is the core's or (not yet split into its rules and the machine's drawing) the machine's games/NAME.c,
# the setup overlay is the machine's setup.c with src/common's setting_line.c, the rest are the machine's own. A game with neither (not yet on that machine) is an empty overlay.
ovl_src() {
	case $2 in
	signin) echo "$CORE/parlor.c $1/signin_net.c" ;;
	setup) echo "$1/setup.c $COMMON/setting_line.c" ;;
	splash) echo "$1/splash.c" ;;
	*) if [ -f "$CORE/games/$2.c" ]; then echo "$CORE/games/$2.c"
	   elif [ -f "$1/games/$2.c" ]; then echo "$1/games/$2.c"; fi ;;
	esac
}

# program MACHINEDIR OBJDIR MAPOUT PROGRAMOUT CONFIG CFLAGS CSRC ASM CRT0 LIB OVLSTART OVLSIZE
#   compiles the C files in CSRC (paths, in link order; those named in $PLATE with their code and read-only
#   data in the segments PCODE and PRODATA), assembles the files in ASM (in MACHINEDIR) and the
#   font ($PFONT, when set), then the overlays with their art ($PART/NAME_art.s), and links them after CRT0
#   (an object in OBJDIR) with ld65 and CONFIG (a path; its #OVLMEM and #OVLSEG lines get the overlays).
program() {
	pm=$1 pobj=$2 pmap=$3 pout=$4 pcfg=$5 pcflags=$6 pcsrc=$7 pasm=$8 pcrt=$9
	shift 9
	plib=$1 povl=$2 povlsize=$3
	objs=""
	for c in $pcsrc; do
		b=$(basename "$c" .c)
		late=
		case " $PLATE " in *" $b "*) late="--code-name PCODE --rodata-name PRODATA" ;; esac
		cc65 $pcflags $late -o "$pobj/$b.s" "$c"
		ca65 --cpu 6502 -o "$pobj/$b.o" "$pobj/$b.s"
		objs="$objs $pobj/$b.o"
	done
	for s in $pasm; do
		ca65 $pdef -I "$pm" -I "$COMMON6502" -o "$pobj/$s.o" "$pm/$s.s"
		objs="$objs $pobj/$s.o"
	done
	if [ -n "$PFONT" ]; then
		ca65 --cpu 6502 -o "$pobj/font.o" "$PFONT"
		objs="$objs $pobj/font.o"
	fi

	mem=""
	seg=""
	for name in $OVERLAYS; do
		for c in $(ovl_src "$pm" "$name"); do
			b=$(basename "$c" .c)
			# Code, read-only data, data and the zeroed data each in a segment of its own (in one
			# segment cc65 would put a function's static variables before its first instruction).
			cc65 $pcflags --code-name "OVC_$name" --rodata-name "OVR_$name" --data-name "OVD_$name" \
				--bss-name "OVB_$name" -o "$pobj/$b.s" "$c"
			sed -i "s/\"OVHDR\"/\"OVH_$name\"/" "$pobj/$b.s"
			ca65 --cpu 6502 -o "$pobj/$b.o" "$pobj/$b.s"
			objs="$objs $pobj/$b.o"
		done
		if [ -f "$PART/${name}_art.s" ]; then
			sed -i "s/\"RODATA\"/\"OVR_$name\"/" "$PART/${name}_art.s"
			ca65 --cpu 6502 -o "$pobj/${name}_art.o" "$PART/${name}_art.s"
			objs="$objs $pobj/${name}_art.o"
		fi
		mem="$mem    OV_$name: file = \"%O.$name\", start = $povl, size = $povlsize;\\n"
		for s in OVH OVC OVR OVD OVB; do
			seg="$seg    ${s}_$name: load = OV_$name, type = rw, optional = yes;\\n"
		done
	done
	cfgout=$pobj/$(basename "$pcfg")
	sed -e "s/^#OVLMEM/$mem/" -e "s/^#OVLSEG/$seg/" "$pcfg" > "$cfgout.tmp"
	mv "$cfgout.tmp" "$cfgout"
	ld65 -C "$cfgout" -m "$pmap" $LDEXTRA -o "$pout" "$pobj/$pcrt" $objs "$plib"
}

LIB=/usr/share/cc65/lib/none.lib
A2FLAGS="-t none --cpu 6502 -O -Oirs -Cl -I $A2 -I $CORE -I $OBJ"

# The Apple IIe: the self-booting disk's boot block, PARLOR.SYSTEM, and the program.
ca65 --cpu 6502 -o "$OBJ/crt0.o" "$A2/crt0.s"
ca65 --cpu 6502 -o "$OBJ/boot.o" "$A2/boot.s"
ld65 -C "$A2/boot.cfg" -o "$OUT/boot.bin" "$OBJ/boot.o"
ca65 --cpu 6502 -o "$OBJ/prodsys.o" "$A2/prodsys.s"
ld65 -C "$A2/prodsys.cfg" -o "$OUT/prodsys.bin" "$OBJ/prodsys.o"
pdef=""
LDEXTRA=
PFONT=$OBJ/font.s
PART=$OBJ
PLATE=
program "$A2" "$OBJ" "$OUT/parlor.map" "$OUT/parlor" "$A2/parlor.cfg" "$A2FLAGS" \
	"$A2/eth.c $CORE/games.c $A2/line.c $CORE/lobby.c $CORE/main.c $CORE/net.c $A2/ovl.c $A2/prodos.c $CORE/table.c $COMMON/ui_targets.c $COMMON/ui_hgr.c $A2/ui.c" \
	"sys hgr disk mli w5100 joy" crt0.o "$LIB" '$D000' '$2900'
echo "$OVERLAYS" | tr ' ' '\n' | awk '{ printf "%s%s:%s", (NR > 1 ? " " : ""), $1, ($1 == "signin" ? "parlor" : $1) } END { print "" }' > "$OUT/overlays.txt"

# The Apple IIgs: PARLORNET's start, and the program.
ca65 --cpu 65816 -o "$GOBJ/gsshell.o" "$A2/gsshell.s"
ld65 -C "$A2/gsshell.cfg" -o "$GS/shell.bin" "$GOBJ/gsshell.o"
ca65 -D IIGS -o "$GOBJ/gs.o" "$A2/gs.s"
pdef="-D IIGS"
program "$A2" "$GOBJ" "$GS/parlor.map" "$GS/parlor" "$A2/parlorgs.cfg" "$A2FLAGS -D IIGS" \
	"$CORE/games.c $A2/gsos.c $A2/line.c $CORE/lobby.c $CORE/main.c $A2/mt.c $CORE/net.c $A2/ovl.c $A2/prodos.c $CORE/table.c $COMMON/ui_targets.c $COMMON/ui_hgr.c $A2/ui.c" \
	"sys hgr joy" gs.o "$LIB" '$D000' '$2900'
cp "$OUT/overlays.txt" "$GS/overlays.txt"

# The Commodore 64 (build/c64): PARLORNET (stub.s, which LOAD"*",8,1 starts by itself), the loader PN.BOOT,
# the C128's boot sector, the character set, the program and its overlays, which
# tools/c64_disk.py puts on parlornet.d64. The overlays are loaded at C64_OVL; a table's moves and chat, or
# the lobby's lists, are in the $600 below it; the program ends below those. The lobby, the tables, the
# parlor's protocol, the catalog and the bitmap's buttons (PLATE, and the parts of ui.c marked PCODE) are
# linked after the program's zeroed data, and tools/c64_disk.py makes them a file of their own, PN.PARLOR,
# which the program loads before the first sign-in (ovl.c): the start-up loads only what the title, the
# start screen, the settings and the console need, and the console loads nothing more. The opening line
# (net.c's net_opening) stays in the program, since the console sends it before PN.PARLOR is loaded.
C64=src/c64
COUT=$OUT/c64
COBJ=$COUT/obj
C64_OVL=9900
mkdir -p "$COBJ"
python3 tools/c64_art.py "$COBJ"
cp "$COBJ/chars.bin" "$COUT/chars.bin"
ca65 --cpu 6502 -I "$C64" -o "$COBJ/loader.o" "$C64/loader.s"
ld65 -C "$C64/loader.cfg" -o "$COUT/loader.bin" "$COBJ/loader.o"
for f in stub boot128; do
	ca65 --cpu 6502 -o "$COBJ/$f.o" "$C64/$f.s"
	ld65 -C "$C64/$f.cfg" -o "$COUT/$f.bin" "$COBJ/$f.o"
done
ca65 --cpu 6502 -o "$COBJ/crt0.o" "$C64/crt0.s"
high=$(printf '%04X' $((0x$C64_OVL - 0x600)))
mainsize=$(printf '%04X' $((0x$high - 0x1500)))
sed -e "s/@HIGH@/\$$high/g" -e "s/@MAINSIZE@/\$$mainsize/" -e "s/@OVL@/\$$C64_OVL/" "$C64/parlor.cfg" > "$COBJ/c64.cfg"
pdef="-D C64 -g"
LDEXTRA="-Ln $COUT/parlor.lbl"
PFONT=
PART=$COBJ
PLATE="games lobby net table ui_targets"
program "$C64" "$COBJ" "$COUT/parlor.map" "$COUT/parlor" "$COBJ/c64.cfg" \
	"-t none --cpu 6502 -O -Oirs -Cl -D C64 -D OVL_ADDR=0x$C64_OVL -I $C64 -I $CORE -I $OBJ" \
	"$CORE/games.c $C64/line.c $CORE/lobby.c $CORE/main.c $CORE/net.c $C64/ovl.c $CORE/table.c $COMMON/ui_targets.c $COMMON/ui_hgr.c $C64/ui.c" \
	"sys serial gfx disk" crt0.o "$LIB" "\$$C64_OVL" "\$$(printf '%04X' $((0xC000 - 0x$C64_OVL)))"
echo "$OVERLAYS" > "$COUT/overlays.txt"
echo "$C64_OVL" > "$COUT/ovl_addr.txt"

# The template machine (src/template, PORTING.md): every call of the machine interface stubbed, built and
# linked here so that the interface and the template keep to each other. Nothing runs it.
TPL=src/template
TOUT=$OUT/template
TOBJ=$TOUT/obj
mkdir -p "$TOBJ"
ca65 --cpu 6502 -o "$TOBJ/crt0.o" "$TPL/crt0.s"
pdef=""
LDEXTRA=
PFONT=
PART=$TOBJ
PLATE=
program "$TPL" "$TOBJ" "$TOUT/parlor.map" "$TOUT/parlor" "$TPL/template.cfg" \
	"-t none --cpu 6502 -O -Oirs -Cl -I $TPL -I $CORE -I $OBJ" \
	"$CORE/games.c $CORE/lobby.c $CORE/main.c $CORE/net.c $CORE/table.c $COMMON/ui_targets.c $TPL/template.c" \
	"" crt0.o "$LIB" '$9000' '$3000'
