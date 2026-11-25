/* ParlorNet for the Apple IIe: running under ProDOS (prodos.c, mli.s). PARLOR.SYSTEM loads the
   program and marks the boot block's disk number ($080A) 0; the games are then read from PARLOR.DAT
   and the settings kept in PARLOR.CFG, beside it. */
#ifndef PRODOS_H
#define PRODOS_H

#define PRODOS (*(unsigned char *)0x080A == 0)

unsigned char __fastcall__ mli(unsigned char cmd, void *params);
extern unsigned char pd_main_sp;

/* Files (the setup overlay reads and writes PARLOR.CFG with these). */
extern unsigned char pd_p[16], pd_ref;
const char *pd_path(const char *name);  /* a counted name, copied where ProDOS can read it */
unsigned char pd_open(const char *name);    /* a counted name; 1 when open, as pd_ref */
void pd_close(void);
unsigned int pd_rw(unsigned char cmd, void *buf, unsigned int n);  /* READ ($CA) or WRITE ($CB) */
#define PD_CHUNK ((unsigned char *)0x0D00)

unsigned char pd_read(unsigned int page, unsigned char n, unsigned char dest);  /* PARLOR.DAT -> RAM */
void pd_quit(void);                     /* back to ProDOS */

#endif
