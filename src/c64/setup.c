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
        /* on the one row: a text longer than the room shows its end, with '<' before it */
        txt_goto(x0 - 1, row);
        txt_putc(n > 38 - x0 ? '<' : ' ');
        say(n > 38 - x0 ? buf + n - (38 - x0) : buf);
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

/* A key's digit, for the settings' lines: they are numbered in the order they show. */
static const char *key(unsigned char n)
{
    static char d[2];
    d[0] = '0' + n;
    return d;
}

static void setup_settings(void)
{
    unsigned char k, sw;
    static char b[DIAL_NUM_MAX + 1];
    txt_mode();
    txt_top = 0;
    txt_bot = 24;
    txt_clear();
    title_bar("ParlorNet settings", "");
    for (;;) {
        /* the keys in the order of the lines: the SwiftLink's address (2) only for the SwiftLink, the rest
           after it one further on (sw) */
        sw = cfg_slot != SER_USER;
        setting_line(2, "1", "Modem: ", sw ? "SwiftLink compatible" : "Supra 2400 compatible");
        txt_clrrow(3);
        if (sw)
            setting_line(3, "2", "Address: ", acia_name[cfg_acia]);
        else {
            txt_goto(3, 3);
            say("on the user port");
        }
        setting_line(4, key(2 + sw), "Speed: ", baud_name[cfg_baud]);
        setting_line(6, key(3 + sw), "Flow: ", cfg_flow ? "RTS/CTS" : "no flow control");
        setting_line(8, key(4 + sw), "Echo typing in the console: ", cfg_echo ? "on" : "off");
        setting_line(10, key(5 + sw), "Init string: ", cfg_init[0] ? cfg_init : "none");
        setting_line(11, key(6 + sw), "Number: ", cfg_number[0] ? cfg_number : "none, no dialing");
        setting_line(12, key(7 + sw), cfg_pulse ? "[x] " : "[ ] ", "Pulse dial");
        txt_clrrow(13);
        setting_line(15, "D", "Display: ", mono ? "monochrome" : "color");
        setting_line(16, "T", "Title screen at start: ", cfg_title ? "on" : "off");
        setting_line(17, "M", "1351 mouse in port 1: ", mouse_on ? "on" : "off");
        txt_goto(1, 19);
        say("Press a setting's key to change it and");
        txt_goto(1, 20);
        say("Return when done. Parlors use 6501.");
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
        default:
            if (k < '1' || k > '7' + sw)
                break;
            k -= '0';
            if (k == 1)
                modem_set(sw ? SER_USER : SER_SWIFT);
            else if (sw && k == 2)
                cfg_acia = cfg_acia == 2 ? 0 : cfg_acia + 1;
            else
                switch (k - sw) {
                case 2: next_baud(); break;
                case 3: cfg_flow ^= 1; break;
                case 4: cfg_echo ^= 1; break;
                case 7: cfg_pulse ^= 1; break;
                case 5:
                    strcpy(b, cfg_init);
                    if (edit_line(21, "Init string: ", b, DIAL_INIT_MAX))
                        strcpy(cfg_init, b);
                    break;
                case 6:
                    strcpy(b, cfg_number);
                    if (edit_line(21, "Number: ", b, DIAL_NUM_MAX))
                        strcpy(cfg_number, b);
                    break;
                }
            break;
        }
    }
}

static void pack(unsigned char *b);

static void start_screen(void)
{
    if (!cfg_saved[0])
        pack(cfg_saved);                /* (no PARLOR.CFG read: the settings as they start) */
    txt_mode();
    txt_top = 0;
    txt_bot = 24;
    txt_clear();
    title_bar("ParlorNet", "");
    txt_goto(1, 3);
    sayln("Configure the ParlorNet online");
    txt_goto(1, 4);
    say("game service");
    setting_line(7, "P", "ParlorNet: connect and sign in", "");
    setting_line(9, "C", "Console: set up the line by hand", "");
    setting_line(11, "S", "Settings: ", line_desc());
    setting_line(13, "D", "Display: ", mono ? "monochrome" : "color");
    txt_goto(1, 22);
    say(OA "-Q: quit to BASIC");
    if (cfg_number[0])                  /* (P dials the number: the menu says enough) */
        return;
    txt_goto(1, 16);
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
   left alone: the settings stay as they start until they are saved again. The start-up reads the file
   (line.c, cfg_load) without this overlay; when it had none, cfg_saved is packed here the first time the
   start screen shows, before anything can change the settings. */
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
