/* ParlorNet for the Apple IIe: loading overlays (ovl.h). The disk maker writes where each one is into
   the boot block's table at $0821: linear sector, sector count and which disk. Before a read the first
   sector of the disk in the drive is checked, and if it is the other disk the player is asked to swap.
   Under ProDOS, PARLOR.SYSTEM fills in the same table with each one's first page in PARLOR.DAT. */
#include "machine.h"
#include "apple2.h"
#include "ovl.h"
#include "prodos.h"

#define OVLTAB ((unsigned char *)0x0821)
#define CHECK_PAGE 0x0F                 /* the reply buffer's first page, free between requests */

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

unsigned char __fastcall__ ovl_load(unsigned char k)
{
    unsigned char *e = OVLTAB + k * 4, tries;
    if (k == loaded)
        return 1;
    if (!e[2])
        return 0;
    if (PRODOS) {
        /* under ProDOS: from PARLOR.DAT, where the table gives the page it starts on */
        loaded = 255;
        if (!pd_read(e[0] | (e[1] << 8), e[2], OVL_ADDR >> 8))
            return 0;
        loaded = k;
        return 1;
    }
    for (tries = 0; tries < 8; ++tries) {
        RD_LIN = 0;
        RD_COUNT = 1;
        RD_PAGE = CHECK_PAGE;
        if (disk_read() == 0 && ((unsigned char *)(CHECK_PAGE << 8))[0x0B] == 'P'
            && ((unsigned char *)(CHECK_PAGE << 8))[0x0A] == e[3]) {
            loaded = 255;
            RD_LIN = e[0] | (e[1] << 8);
            RD_COUNT = e[2];
            RD_PAGE = OVL_ADDR >> 8;
            if (disk_read() == 0) {
                disk_motor_off();
                loaded = k;
                return 1;
            }
        }
        disk_motor_off();
        ask_disk(e[3]);
    }
    return 0;
}
