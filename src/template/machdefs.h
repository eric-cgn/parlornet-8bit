/* ParlorNet's template machine: the constants the machine interface (core/machine.h) leaves to the machine.
   Copy src/template to src/<your machine> and set these for it. */
#ifndef MACHDEFS_H
#define MACHDEFS_H

/* 1: a serial line, with the console and the dialer (most machines); 0: only a network connection. */
#define MACHINE_SERIAL 1
/* 1: the console does not load the sign-in, and its command-P sends the opening before the sign-in is
   loaded (for a slow drive); 0: the console loads the sign-in as it opens (machine.h). */
#define MACHINE_LATE_SIGNIN 0
/* Define MACHINE_LOWBSS to put the table's move tokens in a LOWBSS segment of the linker configuration. */

#define OA_NAME "Command"               /* the command key, as the text names it */
#define MACHINE_NAME "this computer"    /* "This game is not on this computer yet." */

/* The font's characters for the command key and a second one, and the key-legend marks. */
#define OA "\x7F"
#define CA "\x80"
#define K1 "\x01"
#define K3 "\x03"
#define K6 "\x06"

/* What kbd_get returns for these keys. */
#define K_LEFT  8
#define K_RIGHT 21
#define K_UP    11
#define K_DOWN  10
#define K_RET   13
#define K_ESC   27
#define K_TAB   9
#define K_DEL   127

/* Where overlays are loaded and run (template.cfg's OVL area). */
#define OVL_ADDR 0x9000

/* The table screen's frame, in lines of the graphics screen (192 lines here, as on the Apple II). */
#define GAME_TOP 9
#define MSG_Y 167
#define STATUS_Y 176
#define BUTTON_Y 184

#endif
