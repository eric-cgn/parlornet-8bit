/* ParlorNet on the Apple IIe and IIgs: the constants the machine interface (core/machine.h) leaves to the
   machine. The IIgs program is this machine built with IIGS defined. */
#ifndef MACHDEFS_H
#define MACHDEFS_H

#ifdef IIGS
#define MACHINE_SERIAL 0                /* Marinetti only: no console, no dialer */
#define MACHINE_LOWBSS                  /* the move tokens and chat parts in low memory (parlorgs.cfg) */
#else
#define MACHINE_SERIAL 1                /* the Super Serial Card (or the Uthernet II) */
#endif
#define MACHINE_LATE_SIGNIN 0           /* the console loads the sign-in as it opens (machine.h) */

#define MACHINE_NAME "the Apple II"

/* The Open-Apple and Closed-Apple keys in text: MouseText's apples on an enhanced IIe, a IIc or a IIgs
   (txt_mt), "OA-" and "CA-" on an older IIe (txt_putc); the Hi-Res font has them on every machine. */
#define OA "\x7F"
#define CA "\x80"
/* Key legends on the Hi-Res screen: the next 1, 3 or 6 characters are a key, drawn in bold in monochrome
   (as the Open-Apple glyph and the character after it are). K1 "D" done, K3 "Esc", K6 "Return". */
#define K1 "\x01"
#define K3 "\x03"
#define K6 "\x06"

#define K_LEFT  8
#define K_RIGHT 21
#define K_UP    11
#define K_DOWN  10
#define K_RET   13
#define K_ESC   27
#define K_TAB   9
#define K_DEL   127

/* Where overlays are loaded (ovl.h): the language card's RAM. */
#define OVL_ADDR 0xD000

/* The table screen's frame (game.h): a title line at the top, then the game's area (lines 9 to 166), a
   message line (167), the status line (176) and the buttons (184). */
#define GAME_TOP 9
#define MSG_Y 167
#define STATUS_Y 176
#define BUTTON_Y 184

/* The middle of a screen line (280 pixels) as a target's x (pixel x / 2), for a target as wide as the
   screen (the lobby's rows). */
#define MID_X2 70

#endif
