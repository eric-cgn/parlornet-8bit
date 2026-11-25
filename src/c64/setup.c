/* ParlorNet on the Commodore 64: the start screen and the settings, an overlay (ovl.h) loaded when they are
   shown. The settings are the modem (Supra 2400 compatible on the user port, or SwiftLink compatible on the
   expansion port), its speed and flow control, the console's echo, the dialing (an init string, ATZ as it
   starts, the number and pulse dialing), the display and the title screen. They are kept in PARLOR.CFG on the program's disk, written only when they
   have changed. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "app.h"
#include "ovl.h"
#include "c64.h"
#include "gfx.h"



/* Edit buf on the text screen at row, after a label; it starts with what buf holds. 1 on Return. */
static unsigned char edit_line(unsigned char row, const char *label, char *buf, unsigned char max)
{
    unsigned char n = strlen(buf), k, x0;
    txt_goto(1, row);
    say(label);
    x0 = txt_x;
    for (;;) {
        txt_goto(x0, row);
        say(buf);
        txt_cursor(0x20);
        ++txt_x;
        txt_clreol();
        do
            k = kbd_get();
        while (!k);
        if (k == K_RET)
            return 1;
        if (k == K_ESC)
            return 0;
        if (k == K_DEL || k == K_LEFT) {
            if (n)
                buf[--n] = 0;
        } else if (k == 24) {
            n = 0;
            buf[0] = 0;
        } else if (k >= 32 && k < 127 && n < max) {
            buf[n++] = k;
            buf[n] = 0;
        }
    }
}

/* The next speed the interface can do. */
static void next_baud(void)
{
    unsigned char i;
    for (i = 0; i < 7; ++i) {
        cfg_baud = cfg_baud == 6 ? 0 : cfg_baud + 1;
        if (baud_ok(cfg_slot, cfg_baud))
            return;
    }
}

static void setup_settings(void)
{
    unsigned char k;
    static char b[DIAL_NUM_MAX + 1];
    txt_mode();
    txt_top = 0;
    txt_bot = 24;
    txt_clear();
    title_bar("ParlorNet settings", "");
    for (;;) {
        setting_line(2, "1", "Modem: ", cfg_slot == SER_USER ? "Supra 2400 compatible" : "SwiftLink compatible");
        txt_clrrow(3);
        if (cfg_slot == SER_USER) {
            txt_goto(3, 3);
            say("on the user port");
        } else
            setting_line(3, "3", "Address: ", acia_name[cfg_acia]);
        setting_line(4, "2", "Speed: ", baud_name[cfg_baud]);
        setting_line(6, "4", "Flow: ", cfg_flow ? "RTS/CTS" : "no flow control");
        setting_line(8, "5", "Echo typing in the console: ", cfg_echo ? "on" : "off");
        setting_line(10, "6", "Init string: ", cfg_init[0] ? cfg_init : "none");
        setting_line(11, "7", "Number: ", !cfg_number[0] ? "none, no dialing" : "");
        txt_goto(3, 12);
        say(cfg_number);
        txt_clreol();
        setting_line(13, "8", cfg_pulse ? "[x] " : "[ ] ", "Pulse dial");
        setting_line(15, "D", "Display: ", mono ? "monochrome" : "color");
        setting_line(16, "T", "Title screen at start: ", cfg_title ? "on" : "off");
        setting_line(17, "M", "1351 mouse in port 1: ", mouse_on ? "on" : "off");
        txt_goto(1, 19);
        say("A key to change a setting, Return when");
        txt_goto(1, 20);
        say("done. The parlor's native port is 6501.");
        txt_goto(1, 21);
        txt_clreol();
        do
            k = kbd_get();
        while (!k);
        if (k >= 'a')
            k -= 32;
        switch (k) {
        case K_RET:
        case K_ESC:
            return;
        case 'D': mono ^= 1; break;
        case 'T': cfg_title ^= 1; break;
        case 'M': mouse_on ^= 1; break;
        case '1':
            modem_set(cfg_slot == SER_USER ? SER_SWIFT : SER_USER);
            break;
        case '2': next_baud(); break;
        case '3':
            if (cfg_slot != SER_USER)
                cfg_acia = cfg_acia == 2 ? 0 : cfg_acia + 1;
            break;
        case '4': cfg_flow ^= 1; break;
        case '5': cfg_echo ^= 1; break;
        case '8': cfg_pulse ^= 1; break;
        case '6':
            strcpy(b, cfg_init);
            if (edit_line(21, "Init string: ", b, DIAL_INIT_MAX))
                strcpy(cfg_init, b);
            break;
        case '7':
            strcpy(b, cfg_number);
            if (edit_line(12, "  ", b, DIAL_NUM_MAX))
                strcpy(cfg_number, b);
            break;
        }
    }
}

static void start_screen(void)
{
    txt_mode();
    txt_top = 0;
    txt_bot = 24;
    txt_clear();
    title_bar("ParlorNet", "");
    txt_goto(1, 3);
    sayln("The games of a ParlorNet parlor, over");
    txt_goto(1, 4);
    say("a modem or a serial line.");
    setting_line(7, "P", "ParlorNet: connect and sign in", "");
    setting_line(9, "C", "Console, to set up the line first", "");
    setting_line(11, "S", "Settings: ", line_desc());
    setting_line(13, "D", "Display: ", mono ? "monochrome" : "color");
    txt_goto(1, 22);
    say(OA "-Q: quit to BASIC");
    txt_goto(1, 16);
    if (cfg_number[0]) {
        sayln("P dials the modem with the number in");
        txt_goto(1, 17);
        sayln("the settings (S) and signs in. C is a");
        txt_goto(1, 18);
        sayln("plain console, to set up the line by");
        txt_goto(1, 19);
        say("hand.");
        return;
    }
    sayln("If the line already reaches the");
    txt_goto(1, 17);
    sayln("parlor's native port (6501), press P.");
    txt_goto(1, 18);
    sayln("To dial a modem first, press C, type");
    txt_goto(1, 19);
    sayln("ATDT and the parlor's name and port,");
    txt_goto(1, 20);
    say("then press " OA_NAME "-P.");
}

/* PARLOR.CFG: 'P', 'C', a version (CFG_VER, one more whenever the layout or the meaning of a byte changes), the
   modem (SER_USER or SER_SWIFT), its speed and flow control, the echo, the display, the title screen, pulse dialing, then the init string and the
   number, each with
   its 0 and room to spare, whether a mouse is in port 1 (byte 70) and the SwiftLink's address (byte 71). A file of another version or size is
   left alone: the settings stay as they start until they are saved again. */
#define CFG_VER 4
static void pack(unsigned char *b)
{
    memset(b, 0, CFG_LEN);
    b[0] = 'P';
    b[1] = 'C';
    b[2] = CFG_VER;
    b[3] = cfg_slot;
    b[4] = cfg_baud;
    b[5] = cfg_flow;
    b[6] = cfg_echo;
    b[7] = mono;
    b[8] = cfg_title;
    b[9] = cfg_pulse;
    memcpy(b + 10, cfg_init, DIAL_INIT_MAX + 1);
    memcpy(b + 10 + DIAL_INIT_MAX + 1, cfg_number, DIAL_NUM_MAX + 1);
    b[70] = mouse_on;
    b[71] = cfg_acia;
}

static void cfg_load(void)
{
    static unsigned char b[CFG_LEN];
    if (k_readfile("PARLOR.CFG,S,R", b, CFG_LEN) != CFG_LEN || b[0] != 'P' || b[1] != 'C' || b[2] != CFG_VER) {
        pack(cfg_saved);                /* (none yet: the settings as they start) */
        return;
    }
    cfg_slot = b[3];
    cfg_baud = b[4];                    /* (serial_open puts a modem or speed it cannot use right) */
    cfg_flow = b[5] & 1;
    cfg_echo = b[6] & 1;
    mono = b[7] & 1;
    cfg_title = b[8] & 1;
    cfg_pulse = b[9] & 1;
    memcpy(cfg_init, b + 10, DIAL_INIT_MAX);
    memcpy(cfg_number, b + 10 + DIAL_INIT_MAX + 1, DIAL_NUM_MAX);
    mouse_on = b[70] & 1;
    cfg_acia = b[71];                   /* (serial_open puts one it cannot use right) */
    memcpy(cfg_saved, b, CFG_LEN);
}

/* After the settings: PARLOR.CFG written again when they changed. */
static void after_settings(void)
{
    static unsigned char b[CFG_LEN];
    setup_settings();
    pack(b);
    if (memcmp(b, cfg_saved, CFG_LEN) == 0)
        return;
    txt_goto(1, 21);
    say("Saving the settings...");
    k_cmd("S0:PARLOR.CFG");
    if (k_writefile("PARLOR.CFG,S,W", b, CFG_LEN) == 0)
        memcpy(cfg_saved, b, CFG_LEN);
}

#pragma rodata-name (push, "OVHDR")
const struct setup setup_header = {after_settings, start_screen, cfg_load};
#pragma rodata-name (pop)
