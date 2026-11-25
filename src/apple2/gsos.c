/* ParlorNet for the Apple IIgs: files through GS/OS, and the mouse.

   The program's file code is the ProDOS version's (prodos.c, and the settings in setup.c), which makes
   ProDOS MLI calls. On the IIgs mli() takes the few calls it makes and makes the GS/OS calls that do the
   same: OPEN, READ, WRITE, CLOSE, SET_MARK, SET_EOF, CREATE and QUIT. A name is looked for beside the
   application (GS/OS's prefix 9), so PARLOR.DAT and PARLOR.CFG sit in its folder. */
#include <string.h>
#include "machine.h"
#include "apple2.h"
#include "gs.h"
#include "prodos.h"

unsigned char pd_main_sp;               /* (ProDOS's; unused here) */

static unsigned char gp[20];            /* a GS/OS parameter block */
static struct {
    unsigned int len;
    char s[24];
} gname;

#define W(o) (*(unsigned int *)(gp + (o)))

/* GS/OS's name for a counted ProDOS name: "9:" and the name. */
static void name_from(const char *pname)
{
    unsigned char n = pname[0];
    if (n > sizeof gname.s - 2)
        n = sizeof gname.s - 2;
    gname.s[0] = '9';
    gname.s[1] = ':';
    memcpy(gname.s + 2, pname + 1, n);
    gname.len = n + 2;
}

static void put_ptr(unsigned char o, const void *p)
{
    W(o) = (unsigned int)p;
    W(o + 2) = gs_bank;
}

unsigned char __fastcall__ mli(unsigned char cmd, void *params)
{
    unsigned char *p = params;
    unsigned int e;
    memset(gp, 0, sizeof gp);
    switch (cmd) {
    case 0xC8:                          /* OPEN */
        name_from(*(const char **)(p + 1));
        W(0) = 2;
        put_ptr(4, &gname);
        e = gs_call(0x2010, gp);
        p[5] = gp[2];
        break;
    case 0xCA:                          /* READ */
    case 0xCB:                          /* WRITE */
        W(0) = 4;
        W(2) = p[1];
        put_ptr(4, *(void **)(p + 2));
        W(8) = *(unsigned int *)(p + 4);
        e = gs_call(cmd == 0xCA ? 0x2012 : 0x2013, gp);
        *(unsigned int *)(p + 6) = W(12);
        break;
    case 0xCC:                          /* CLOSE */
        W(0) = 1;
        W(2) = p[1];
        e = gs_call(0x2014, gp);
        break;
    case 0xCE:                          /* SET_MARK */
    case 0xD0:                          /* SET_EOF */
        W(0) = 3;
        W(2) = p[1];
        W(6) = p[2] | (p[3] << 8);
        gp[8] = p[4];
        e = gs_call(cmd == 0xCE ? 0x2016 : 0x2018, gp);
        break;
    case 0xC0:                          /* CREATE */
        name_from(*(const char **)(p + 1));
        W(0) = 5;
        put_ptr(2, &gname);
        W(6) = p[3];                    /* access */
        W(8) = p[4];                    /* file type */
        W(10) = p[5] | (p[6] << 8);     /* aux type */
        W(14) = p[7];                   /* storage type */
        e = gs_call(0x2001, gp);
        break;
    case 0x65:                          /* QUIT: back to the program that launched us */
        mt_end();
        gs_video(0);
        W(0) = 0;
        gs_call(0x2029, gp);
        /* fall through: GS/OS does not come back */
    default:
        e = 1;
        break;
    }
    return (unsigned char)e;
}

/* The mouse: the IIgs's own, through the Miscellaneous Tool Set (which PARLORNET started), standing in
   for the AppleMouse II firmware calls ui.c makes. The answers go where that card's firmware puts them,
   the screen holes for its slot, which here are only memory in our bank. */
#define HOLE(a) (*(unsigned char *)(a))
#define MOUSE_SLOT 4
unsigned char mouse_slot;
static unsigned int clamp[4];

unsigned char mouse_find(void)
{
    mouse_slot = MOUSE_SLOT;
    return MOUSE_SLOT;
}

void __fastcall__ mouse_call(unsigned int call)
{
    unsigned char a = call >> 8;
    switch (call & 0xFF) {
    case 0x17:                          /* CLAMPMOUSE: x (A = 0), then y (A = 1) */
        clamp[a * 2] = HOLE(0x478) | (HOLE(0x578) << 8);
        clamp[a * 2 + 1] = HOLE(0x4F8) | (HOLE(0x5F8) << 8);
        if (a) {
            tl_nin = 4;
            tl_nout = 0;
            memcpy(tl_in, clamp, sizeof clamp);
            tl_call(0x1C03);            /* ClampMouse */
        }
        break;
    case 0x12:                          /* SETMOUSE */
        tl_nin = 1;
        tl_nout = 0;
        tl_in[0] = a;
        tl_call(0x1903);                /* SetMouse */
        break;
    case 0x14:                          /* READMOUSE */
        tl_nin = 0;
        tl_nout = 3;
        tl_call(0x1703);                /* ReadMouse: the mode and status, y, x */
        HOLE(0x478 + MOUSE_SLOT) = tl_out[2];
        HOLE(0x578 + MOUSE_SLOT) = tl_out[2] >> 8;
        HOLE(0x4F8 + MOUSE_SLOT) = tl_out[1];
        HOLE(0x5F8 + MOUSE_SLOT) = tl_out[1] >> 8;
        HOLE(0x778 + MOUSE_SLOT) = tl_out[0] >> 8;
        break;
    }
}
