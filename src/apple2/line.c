/* ParlorNet on the Apple IIe and IIgs: the serial line's settings and opening (app.h), and the program's
   start-up and quitting. */
#include <string.h>
#include "machine.h"
#include "apple2.h"
#include "app.h"
#include "ovl.h"
#include "prodos.h"

unsigned char cfg_slot = 2, cfg_baud = 4, cfg_parity, cfg_flow;
unsigned char cfg_conn;                 /* 0 the Super Serial Card, 1 the Uthernet II */

const char *const baud_name[] = {"300", "1200", "2400", "4800", "9600", "19200"};
static const unsigned char baud_code[] = {6, 8, 10, 12, 14, 15};
const char *const parity_name[] = {"8N1", "7O1", "7E1"};

const char *line_desc(void)
{
    static char b[16];
    strcpy(b, baud_name[cfg_baud]);
    strcat(b, " ");
    strcat(b, parity_name[cfg_parity]);
    return b;
}

void serial_open(void)
{
    unsigned char control = 0x10 | baud_code[cfg_baud] | (cfg_parity ? 0x20 : 0);
    unsigned char command = 0x09 | (cfg_parity == 1 ? 0x20 : cfg_parity == 2 ? 0x60 : 0);
    ser_flow = cfg_flow;
    ser_open(cfg_slot, control, command);
}

/* The text screen; under ProDOS the settings from PARLOR.CFG; the mouse; the line. */
void sys_init(void)
{
    txt_detect();
    txt_mode();
    if (PRODOS) {
        pd_main_sp = *(unsigned char *)0x081F;
        if (ovl_load(OVL_SETUP))
            SETUP->load();
    }
    ui_mouse_init();
    serial_open();
}

/* Open-Apple-Q on the start screen: back to ProDOS (the self-booting disk has nowhere to go). */
void sys_quit(void)
{
    if (PRODOS)
        pd_quit();
}
