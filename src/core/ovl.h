/* ParlorNet: overlays. The sign-in and each game are loaded from the disk into memory at OVL_ADDR (machdefs.h:
   the Apple's language card RAM at $D000, $9800 on the C64) by the machine's ovl_load (machine.h) when they
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

#endif
