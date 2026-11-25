/* ParlorNet for the Apple IIe: overlays. The sign-in and each game are loaded from the disk into the
   language card's RAM at $D000 when they are needed; one is in memory at a time. */
#ifndef OVL_H
#define OVL_H

#define OVL_ADDR 0xD000
#define OVL_SIGNIN 0
/* The games are 1 upwards, in the order of the catalog (games.c). */

unsigned char __fastcall__ ovl_load(unsigned char k);   /* 1 when it is in memory */

/* The sign-in overlay's header. */
struct signin {
    unsigned char (*run)(void);         /* 0 start screen, 1 console, 3 quit, 4 signed in */
#ifndef IIGS
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

/* Whether the line has been greeted (so the sign-in skips the opening and greeting). */
extern unsigned char connected;
extern unsigned char signin_note;       /* 1: say the player signed out; 2: the line was lost */

#endif
