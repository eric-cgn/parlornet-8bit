/* ParlorNet on the Commodore 64: the constants the machine interface (core/machine.h) leaves to the machine. */
#ifndef MACHDEFS_H
#define MACHDEFS_H

#define MACHINE_SERIAL 1                /* the user port or a SwiftLink: the console and the dialer */

/* The Commodore key, which stands for commands as Open-Apple does on the Apple II: OA in the key legends
   is the font's Commodore logo (C=), on the text screen and the bitmap alike. */
#define OA "\x7F"
#define CA "\x80"
#define OA_NAME "Commodore"
#define MACHINE_NAME "the C64"
/* Key legends on the bitmap: the next 1, 3 or 6 characters are a key, drawn in the keys' color. */
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

/* Where overlays are loaded (ovl.h). build.sh sets it (C64_OVL) for the linker too. */
#ifndef OVL_ADDR
#define OVL_ADDR 0x9900
#endif

/* The table screen's frame (game.h), on whole rows of 8 lines: the title on row 0, the game on rows 1 to
   21, the message line on row 22, the status line on row 23 and the buttons on row 24. */
#define GAME_TOP 8
#define MSG_Y 176
#define STATUS_Y 184
#define BUTTON_Y 192

#endif
