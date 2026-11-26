/* ParlorNet's template machine: the constants the machine interface (core/machine.h) leaves to the machine.
   Copy src/template to src/<your machine> and set these for it. */
#ifndef MACHDEFS_H
#define MACHDEFS_H

/* 1: a serial line, with the console and the dialer (most machines); 0: only a network connection. */
#define MACHINE_SERIAL 1
/* Define MACHINE_LOWBSS to put the table's move tokens in a LOWBSS segment of the linker configuration. */

#define MACHINE_NAME "this computer"    /* "This game is not on this computer yet." */
#define MACHINE_LABEL "Computer"       /* the computer, for its sign-in key (KEY NEW) */

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
#define K_BACK  28                      /* the second button of a joystick or a mouse */
#define KEY_RET K6 "Return"
#define KEY_ESC K3 "Esc"
#define COL_PX 8

/* Where overlays are loaded and run (template.cfg's OVL area). */
#define OVL_ADDR 0x9000

/* The table screen's frame, in lines of the graphics screen (192 lines here, as on the Apple II). */
#define GAME_TOP 9
#define MSG_Y 167
#define STATUS_Y 176
#define BUTTON_Y 184

/* The middle of a screen line (320 pixels) as a target's x (pixel x / 2), for a target as wide as the
   screen (the lobby's rows). */
#define MID_X2 80

#endif
