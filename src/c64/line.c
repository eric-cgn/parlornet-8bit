/* ParlorNet on the Commodore 64: the serial line's settings and opening (app.h), the program's start-up and
   quitting, and the network's place (eth.h) until there is a driver for it. */
#include <string.h>
#include "machine.h"
#include "app.h"
#include "ovl.h"
#include "c64.h"
#include "gfx.h"

/* As a fresh settings file has them: SwiftLink compatible at 19200, no flow control, as the modem of a
   Commodore 64 Ultimate or an Ultimate 64 is at its default mapping ($DE00). */
#define BAUD_SWIFT 5                    /* 19200, when the modem becomes SwiftLink compatible */
#define BAUD_USER 2                     /* 2400, when it becomes Supra 2400 compatible */
unsigned char cfg_slot = SER_SWIFT, cfg_baud = BAUD_SWIFT, cfg_flow, cfg_acia;
unsigned char cfg_conn;
unsigned char eth_on;
unsigned char cfg_saved[CFG_LEN];      /* the settings as PARLOR.CFG has them (setup.c) */

const char *const baud_name[] = {"300", "1200", "2400", "4800", "9600", "19200", "38400"};
/* The SwiftLink compatible modem's three addresses: $DE00 and $DF00 as a SwiftLink's or Turbo232's jumper sets
   them, $DE00 and $DF80 as the Commodore 64 Ultimate's ACIA mapping does. Nothing looks for which one holds a
   6551. */
const char *const acia_name[] = {"$DE00", "$DF00", "$DF80"};
static const unsigned int acia_addr[] = {0xDE00, 0xDF00, 0xDF80};

/* The settings from PARLOR.CFG (setup.c has its layout), read at the start without the setup overlay, into
   cfg_saved; when the file is not there or not of this version, cfg_saved[0] is 0 and the settings stay as
   they start. */
void cfg_load(void)
{
    if (k_readfile("PARLOR.CFG,S,R", cfg_saved, CFG_LEN) != CFG_LEN || cfg_saved[0] != 'P' || cfg_saved[1] != 'C'
        || cfg_saved[2] != CFG_VER) {
        cfg_saved[0] = 0;
        return;
    }
    cfg_slot = cfg_saved[3];
    cfg_baud = cfg_saved[4];            /* (serial_open puts a modem or speed it cannot use right) */
    cfg_flow = cfg_saved[5] & 1;
    cfg_echo = cfg_saved[6] & 1;
    mono = cfg_saved[7] & 1;
    cfg_title = cfg_saved[8] & 1;
    cfg_pulse = cfg_saved[9] & 1;
    memcpy(cfg_init, cfg_saved + 10, DIAL_INIT_MAX);
    memcpy(cfg_number, cfg_saved + 10 + DIAL_INIT_MAX + 1, DIAL_NUM_MAX);
    mouse_on = cfg_saved[70] & 1;
    cfg_acia = cfg_saved[71];           /* (serial_open puts one it cannot use right) */
}

/* The speeds each modem can do: Supra 2400 compatible 300 to 2400, SwiftLink compatible 300 to 38400. */
unsigned char __fastcall__ baud_ok(unsigned char iface, unsigned char b)
{
    return b <= (iface == SER_USER ? 2 : 6);
}

/* The modem chosen: its speed as each starts. */
void __fastcall__ modem_set(unsigned char iface)
{
    cfg_slot = iface;
    cfg_baud = iface == SER_USER ? BAUD_USER : BAUD_SWIFT;
}

const char *line_desc(void)
{
    static char b[24];
    if (cfg_slot == SER_USER)
        strcpy(b, "Supra 2400, ");
    else {
        strcpy(b, "SwiftLink ");
        strcat(b, acia_name[cfg_acia]);
        strcat(b, ", ");
    }
    strcat(b, baud_name[cfg_baud]);
    return b;
}

void serial_open(void)
{
    if (cfg_slot != SER_USER && cfg_slot != SER_SWIFT)
        modem_set(SER_SWIFT);
    if (!baud_ok(cfg_slot, cfg_baud))
        modem_set(cfg_slot);
    if (cfg_acia > 2)
        cfg_acia = 0;
    ser_acia = acia_addr[cfg_acia];
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
    cfg_load();
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
