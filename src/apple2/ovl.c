/* ParlorNet for the Apple IIe: loading overlays (ovl.h). The disk maker writes where each one is into
   the boot block's table at $0821: linear sector, sector count and which disk. Before a read the first
   sector of the disk in the drive is checked, and if it is the other disk the player is asked to swap.
   Under ProDOS, PARLOR.SYSTEM fills in the same table with each one's first page in PARLOR.DAT.

   The first sector is read into the memory the overlay goes to, whose first page it takes anyway. Low
   memory is not free for it: the lobby's game names, read when the lobby starts and kept while the player
   goes to a table and back, are there with the reply buffer. The help pages are one overlay (help.py), read
   a help at a time into the reply buffer (ovl_help). */
#include "machine.h"
#include "apple2.h"
#include "ovl.h"
#include "prodos.h"

#define OVLTAB ((unsigned char *)0x0821)

static unsigned char loaded = 255;

/* Ask for a disk on whichever screen is showing. */
static void ask_disk(unsigned char d)
{
    static char msg[] = "Put ParlorNet disk 1 in the drive, press Return";
    msg[19] = '0' + d;
    if (*(unsigned char *)0xC01A & 0x80) {     /* text mode */
        txt_goto(0, 23);
        txt_puts(msg);
    } else
        ui_status(msg);
    while (kbd_get() != K_RET)
        ;
    if (*(unsigned char *)0xC01A & 0x80) {
        txt_goto(0, 23);
        txt_clreol();
    } else
        ui_status("");
}

static unsigned char __fastcall__ load(unsigned char k);
#ifndef IIGS
void bank1_copy(void);
static unsigned char frame_in;
#endif

/* The command line, the dialogs and the pages of text (OVL_FRAME): on the IIe, loaded once and copied into
   the language card's bank 1; on the IIgs, part of the program. */
unsigned char __fastcall__ ovl_load(unsigned char k)
{
    if (k != OVL_FRAME)
        return load(k);
#ifndef IIGS
    if (!frame_in) {
        if (!load(k))
            return 0;
        bank1_copy();
        frame_in = 1;
    }
#endif
    return 1;
}

/* n pages of overlay e's place on the disk (or in PARLOR.DAT), from its page at, into memory from page
   page. On the self-booting disk the disk's first sector is checked first, read into that page too. */
static unsigned char __fastcall__ read(const unsigned char *e, unsigned char at, unsigned char n,
                                       unsigned char page)
{
    unsigned char tries;
    if (PRODOS)                         /* (where the table gives the page it starts on) */
        return pd_read((e[0] | (e[1] << 8)) + at, n, page);
    for (tries = 0; tries < 8; ++tries) {
        RD_LIN = 0;
        RD_COUNT = 1;
        RD_PAGE = page;
        if (disk_read() == 0 && ((unsigned char *)(page << 8))[0x0B] == 'P'
            && ((unsigned char *)(page << 8))[0x0A] == e[3]) {
            RD_LIN = (e[0] | (e[1] << 8)) + at;
            RD_COUNT = n;
            RD_PAGE = page;
            if (disk_read() == 0) {
                disk_motor_off();
                return 1;
            }
        }
        disk_motor_off();
        ask_disk(e[3]);
    }
    return 0;
}

#ifndef IIGS
/* Packed pages lz_n of them from page lz_from, unpacked in place to page lz_to (lz.s). */
extern unsigned char lz_from, lz_n, lz_to;
void lz_unpack(void);
#endif

static unsigned char __fastcall__ load(unsigned char k)
{
    unsigned char *e = OVLTAB + k * 4;
    if (k == loaded)
        return 1;
    if (!e[2])
        return 0;
    loaded = 255;                       /* (a failed read leaves the overlay in memory half there) */
#ifdef IIGS
    if (!read(e, 0, e[2], OVL_ADDR >> 8))
        return 0;
#else
    /* on the IIe the overlay is packed (tools/apple2_lz.py): read to the top of its memory, and unpacked
       from there to its start */
    lz_n = e[2];
    lz_from = (OVL_TOP >> 8) - lz_n;
    if (!read(e, 0, lz_n, lz_from))
        return 0;
    lz_to = OVL_ADDR >> 8;
    lz_unpack();
#endif
    loaded = k;
    return 1;
}

#ifdef IIGS
/* Help k (help.py): its first page and pages from the index, the help overlay's first page, then them. */
unsigned char __fastcall__ ovl_help(unsigned char k, unsigned char page)
{
    const unsigned char *e = OVLTAB + OVL_HELP * 4, *x = (unsigned char *)(page << 8) + k * 2;
    return e[2] && read(e, 0, 1, page) && read(e, x[0], x[1], page);
}
#else
/* Help k, packed on its own (apple2_disk.py): the help overlay's first page is the index, with each help's
   first page and pages packed (2 bytes) and from byte 128 its pages unpacked. The packed pages are read to
   the top of those, and unpacked from there to page. */
unsigned char __fastcall__ ovl_help(unsigned char k, unsigned char page)
{
    static const unsigned char *ix;
    static unsigned char at;
    ix = (unsigned char *)(page << 8);
    if (!OVLTAB[OVL_HELP * 4 + 2] || !read(OVLTAB + OVL_HELP * 4, 0, 1, page))
        return 0;
    at = ix[k * 2];
    lz_n = ix[k * 2 + 1];
    lz_from = page + ix[128 + k] - lz_n;
    if (!read(OVLTAB + OVL_HELP * 4, at, lz_n, lz_from))
        return 0;
    lz_to = page;
    lz_unpack();
    return 1;
}
#endif
