/* ParlorNet on the Commodore 64: loading overlays (ovl.h), each a file of its own on the disk ("PN." and its
   name, as build.sh and tools/c64_disk.py make them), packed (tools/c64_pack.py): loaded through the KERNAL
   (disk.s) where the file says, at the top of the overlays' memory, and unpacked to OVL_ADDR. A game that is
   not on the disk (not yet made for the C64) is not there; when a load fails otherwise (no disk in the
   drive, say), the player is asked for the disk.

   The lobby, the tables, the parlor's protocol, the catalog and the bitmap's buttons and pointer
   (ui_targets.c and most of ui.c) are PN.PARLOR (build.sh's PLATE, segment PCODE), which is loaded once,
   before the sign-in is first loaded: in the console, or for P on the start screen. It unpacks to where the
   linker put it, after the program's zeroed data; on its way it passes through the memory above, where the
   overlay is, so that overlay is gone after it. */
#include <string.h>
#include "machine.h"
#include "ovl.h"
#include "gfx.h"
#include "c64.h"

static unsigned char loaded = 255;
static unsigned char parlor_in;
extern unsigned char _PCODE_RUN__[];    /* (the linker's: where PN.PARLOR goes) */

/* The overlays' file names, in the order of OVERLAYS in build.sh. */
static const char *const ovl_name[] = {
    "PN.SIGNIN", "PN.REVERSI", "PN.BACKGAMMON", "PN.GRIDWORDS", "PN.CHECKERS", "PN.MANCALA",
    "PN.FIVEINAROW", "PN.DOTSANDBOXES", "PN.NAVALCOMBAT", "PN.CANNONFIRE", "PN.RUMMYTILE", "PN.SETUP",
    "PN.SPLASH", "PN.NEWGAME"};

/* What each is, for "Loading ..." */
static const char *const ovl_what[] = {
    "the sign-in", "Reversi", "Backgammon", "Grid Words", "Checkers", "Mancala", "Five in a Row",
    "Dots and Boxes", "Naval Combat", "Cannon Fire", "Rummy Tile", "the settings", "the title screen", "New game"};

/* "Loading Backgammon..." on the screen's bottom row, which the progress bar then fills (disk.s): the text
   screen's spare 25th row, or the bitmap's status row. On the title screen that is the row of "Press a key",
   which comes back when the load is done (ovl_load), so the title's caption above it is left alone. */
static void loading(const char *what)
{
    static char b[40];
    strcpy(b, "Loading ");
    strcat(b, what);
    strcat(b, "...");
    if (txt_screen) {
        txt_clrrow(24);
        txt_goto(0, 24);
        txt_puts(b);
    } else
        ui_status(b);
}

/* Ask for the disk on whichever screen is showing. */
static void ask_disk(void)
{
    static const char msg[] = "Put the ParlorNet disk in, press Return";
    if (txt_screen) {
        txt_goto(0, 23);
        txt_puts(msg);
    } else
        ui_status(msg);
    while (kbd_get() != K_RET)
        ;
    if (txt_screen) {
        txt_goto(0, 23);
        txt_clreol();
    } else
        ui_status("");
}

/* The file loaded with "Loading what..." and the bar, and unpacked to dst; 0 when it is not there. */
static unsigned char load_unpack(const char *name, const char *what, void *dst)
{
    unsigned char tries, e;
    for (tries = 0; tries < 4; ++tries) {
        loading(what);
        k_bar = 1;
        e = k_load(name, 0);
        if (txt_screen)
            txt_clrrow(24);
        if (e == 0) {
            k_unpack(dst);
            return 1;
        }
        if (e == 0x84)                  /* FILE NOT FOUND */
            return 0;
        ask_disk();
    }
    return 0;
}

unsigned char __fastcall__ ovl_load(unsigned char k)
{
    unsigned char e, title;
    if (k == loaded)
        return 1;
    if (k >= sizeof ovl_name / sizeof ovl_name[0])
        return 0;
    if ((k == OVL_SIGNIN || k == OVL_FRAME) && !parlor_in) {
        loaded = 255;
        if (!load_unpack("PN.PARLOR", "the lobby", _PCODE_RUN__))
            return 0;
        parlor_in = 1;
    }
    if (k == OVL_FRAME)                 /* (part of PN.PARLOR, which is in now) */
        return 1;
    title = loaded == OVL_SPLASH && !txt_screen;    /* (the title screen up, as splash.c draws it) */
    loaded = 255;
    e = load_unpack(ovl_name[k], ovl_what[k], (void *)OVL_ADDR);
    if (title) {
        ui_status("");
        ui_text(14, STATUS_Y, "Press a key");
    }
    if (!e)
        return 0;
    loaded = k;
    return 1;
}
