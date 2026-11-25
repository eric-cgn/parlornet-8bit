/* ParlorNet for the Apple IIe: the program under ProDOS (prodos.h). */
#include <string.h>
#include "a2.h"
#include "ui.h"
#include "app.h"
#include "eth.h"
#include "prodos.h"

#define IOBUF ((unsigned char *)0x0900)    /* ProDOS's 1K buffer for an open file */
#define CHUNK ((unsigned char *)0x0D00)    /* a page read at a time, then copied */

static const char dat_name[] = "\012PARLOR.DAT";
unsigned char pd_p[16];                    /* the parameters of one call */
unsigned char pd_ref;

/* A counted name copied into main memory, where ProDOS can read it (an overlay's own data is in the
   auxiliary language card, out of ProDOS's sight). */
static char pd_name[16];
const char *pd_path(const char *name)
{
    memcpy(pd_name, name, name[0] + 1);
    return pd_name;
}

unsigned char pd_open(const char *name)
{
    pd_p[0] = 3;
    *(const char **)(pd_p + 1) = pd_path(name);
    *(unsigned char **)(pd_p + 3) = IOBUF;
    if (mli(0xC8, pd_p))
        return 0;
    pd_ref = pd_p[5];
    return 1;
}

void pd_close(void)
{
    pd_p[0] = 1;
    pd_p[1] = pd_ref;
    mli(0xCC, pd_p);
}

/* READ or WRITE n bytes at buf; the bytes moved. */
unsigned int pd_rw(unsigned char cmd, void *buf, unsigned int n)
{
    pd_p[0] = 4;
    pd_p[1] = pd_ref;
    *(void **)(pd_p + 2) = buf;
    *(unsigned int *)(pd_p + 4) = n;
    *(unsigned int *)(pd_p + 6) = 0;
    mli(cmd, pd_p);
    return *(unsigned int *)(pd_p + 6);
}

unsigned char pd_read(unsigned int page, unsigned char n, unsigned char dest)
{
    unsigned char ok = 1;
    if (!pd_open(dat_name))
        return 0;
    pd_p[0] = 2;
    pd_p[1] = pd_ref;
    pd_p[2] = 0;
    pd_p[3] = page;
    pd_p[4] = page >> 8;
    if (mli(0xCE, pd_p))                   /* SET_MARK */
        ok = 0;
    while (ok && n--) {
        if (pd_rw(0xCA, CHUNK, 256) != 256)
            ok = 0;
        else
            memcpy((unsigned char *)(dest++ << 8), CHUNK, 256);
    }
    pd_close();
    return ok;
}

void pd_quit(void)
{
    ser_close();
    if (eth_on)
        eth_close();
    txt_mode();
#ifndef IIGS
    if (!(*(unsigned char *)0x0820 & 0x80))    /* the character set PARLOR.SYSTEM found (prodsys.s) */
        *(unsigned char *)0xC00E = 0;
#endif
    pd_p[0] = 4;
    memset(pd_p + 1, 0, 6);
    mli(0x65, pd_p);
}
