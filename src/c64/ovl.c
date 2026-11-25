/* ParlorNet on the Commodore 64: loading overlays (ovl.h), each a file of its own on the disk ("PN." and its
   name, as build.sh and tools/c64_disk.py make them), packed (tools/c64_pack.py): loaded through the KERNAL
   (disk.s) where the file says, at the top of the overlays' memory, and unpacked to OVL_ADDR. A game that is
   not on the disk (not yet made for the C64) is not there; when a load fails otherwise (no disk in the
   drive, say), the player is asked for the disk. */
#include <string.h>
#include "machine.h"
#include "ovl.h"
#include "gfx.h"
#include "c64.h"

static unsigned char loaded = 255;

/* The overlays' file names, in the order of OVERLAYS in build.sh. */
static const char *const ovl_name[] = {
    "PN.SIGNIN", "PN.REVERSI", "PN.BACKGAMMON", "PN.GRIDWORDS", "PN.CHECKERS", "PN.MANCALA",
    "PN.FIVEINAROW", "PN.DOTSANDBOXES", "PN.NAVALCOMBAT", "PN.CANNONFIRE", "PN.RUMMYTILE", "PN.SETUP",
    "PN.SPLASH"};

/* What each is, for "Loading ..." */
static const char *const ovl_what[] = {
    "the sign-in", "Reversi", "Backgammon", "Grid Words", "Checkers", "Mancala", "Five in a Row",
    "Dots and Boxes", "Naval Combat", "Cannon Fire", "Rummy Tile", "the settings", "the title screen"};

/* "Loading Backgammon..." on the screen's bottom row, which the progress bar then fills (disk.s): the text
   screen's spare 25th row, or the bitmap's status row. */
static void loading(unsigned char k)
{
    static char b[40];
    strcpy(b, "Loading ");
    strcat(b, ovl_what[k]);
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

unsigned char __fastcall__ ovl_load(unsigned char k)
{
    unsigned char tries, e;
    if (k == loaded)
        return 1;
    if (k >= sizeof ovl_name / sizeof ovl_name[0])
        return 0;
    for (tries = 0; tries < 4; ++tries) {
        loaded = 255;
        loading(k);
        k_bar = 1;
        e = k_load(ovl_name[k], 0);
        if (txt_screen)
            txt_clrrow(24);
        if (e == 0) {
            k_unpack((void *)OVL_ADDR);
            loaded = k;
            return 1;
        }
        if (e == 0x84)                  /* FILE NOT FOUND */
            return 0;
        ask_disk();
    }
    return 0;
}
