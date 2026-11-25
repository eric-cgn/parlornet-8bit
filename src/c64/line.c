/* ParlorNet on the Commodore 64: the serial line's settings and opening (app.h), the program's start-up and
   quitting, and the network's place (eth.h) until there is a driver for it. */
#include <string.h>
#include "machine.h"
#include "app.h"
#include "ovl.h"
#include "c64.h"

/* The user port, 2400 baud, no flow control: what most WiFi modems for the user port start with. */
unsigned char cfg_slot = SER_USER, cfg_baud = 2, cfg_flow;
unsigned char cfg_conn;
unsigned char eth_on;
unsigned char cfg_saved[CFG_LEN];      /* the settings as PARLOR.CFG has them (setup.c) */

const char *const baud_name[] = {"300", "1200", "2400", "4800", "9600", "19200", "38400"};
static const char *const iface_name[] = {"user port", "UP9600", "SwiftLink", "SwiftLink $DF00"};

/* The speeds each way of connecting can do: the user port bit by bit 300 to 2400, UP9600 9600, the 6551
   300 to 38400 (with the SwiftLink's crystal). */
unsigned char __fastcall__ baud_ok(unsigned char iface, unsigned char b)
{
    if (iface == SER_USER)
        return b <= 2;
    if (iface == SER_UP9600)
        return b == 4;
    return 1;
}

const char *line_desc(void)
{
    static char b[24];
    strcpy(b, baud_name[cfg_baud]);
    strcat(b, " ");
    strcat(b, iface_name[cfg_slot & 3]);
    return b;
}

void serial_open(void)
{
    if (cfg_slot > SER_SWIFT_DF)
        cfg_slot = SER_USER;
    if (!baud_ok(cfg_slot, cfg_baud))
        cfg_baud = cfg_slot == SER_UP9600 ? 4 : 2;
    ser_flow = cfg_flow;
    ser_open(cfg_slot, cfg_baud);
}

/* The text screen; the settings from PARLOR.CFG on the program's disk; the pointer; the line. */
void sys_init(void)
{
    k_device = *(unsigned char *)0xBA;  /* the drive the loader came from */
    if (k_device < 8)
        k_device = 8;
    txt_mode();
    if (ovl_load(OVL_SETUP))
        SETUP->load();
    ui_mouse_init();
    serial_open();
}

/* C=-Q on the start screen: the line closed (a modem keeps its call) and the C64 started afresh, to BASIC. */
void sys_quit(void)
{
    ser_close();
    asm("sei");
    asm("lda #$37");
    asm("sta $01");
    asm("jmp $FCE2");
}

/* No network driver yet (eth.h). */
int eth_get(void)
{
    return -1;
}

void __fastcall__ eth_put(unsigned char c)
{
    (void)c;
}

void eth_drop(void)
{
}

void eth_close(void)
{
    eth_on = 0;
}
