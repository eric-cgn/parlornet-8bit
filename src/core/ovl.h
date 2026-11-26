/* ParlorNet: overlays. The sign-in and each game are loaded from the disk into memory at OVL_ADDR (machdefs.h:
   the Apple's language card RAM at $D000, $9900 on the C64) by the machine's ovl_load (machine.h) when they
   are needed; one is in memory at a time. Each overlay starts with its header, below. */
#ifndef OVL_H
#define OVL_H

#define OVL_SIGNIN 0
/* The games are 1 upwards, in the order of the catalog (games.c). */

/* The sign-in overlay's header. */
struct signin {
    unsigned char (*run)(void);         /* 0 start screen, 1 console, 3 quit, 4 signed in */
#if MACHINE_SERIAL
    void (*hangup)(void);               /* a dialed line, after BYE */
    unsigned char (*console)(void);     /* the console: 1 ParlorNet mode, 0 start screen, 2 the settings */
#endif
};
#define SIGNIN ((const struct signin *)OVL_ADDR)

/* The setup overlay's header: the settings screen and the start screen. */
#define OVL_SETUP 11
struct setup {
    void (*settings)(void);
    void (*start)(void);
    void (*load)(void);                 /* under ProDOS: the settings from PARLOR.CFG */
};
#define SETUP ((const struct setup *)OVL_ADDR)

/* The title screen's overlay (splash.c). */
#define OVL_SPLASH 12
struct splash {
    void (*show)(void);                 /* draws it on Hi-Res page 1 and shows that */
};
#define SPLASH ((const struct splash *)OVL_ADDR)

/* The lobby's New game (newgame.c): returns the table made, or 0. */
#define OVL_NEWGAME 13
struct newgame {
    unsigned int (*run)(void);
};
#define NEWGAME ((const struct newgame *)OVL_ADDR)

/* The command line, the dialogs and the pages of text (frame.c). On the Apple IIe they are in the language
   card's other bank (bank 1, $D000-$DFFF), which ovl_load fills from this overlay the first time; on the
   IIgs and the C64 they are part of the program, and ovl_load(OVL_FRAME) only makes sure that the lobby and
   the tables are in memory (the C64's PN.PARLOR). The core asks for it before the lobby. */
#define OVL_FRAME 14

/* The help pages (tools/help.py), read a help at a time by ovl_help (machine.h). */
#define OVL_HELP 15

#endif
