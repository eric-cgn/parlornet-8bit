/* ParlorNet for the Apple IIe: the start screen and the settings, an overlay (ovl.h) loaded when they are
   shown. The settings are the connection (the Super Serial Card's slot and line, or the Uthernet II's
   slot, this Apple's addresses and the parlor's name and port) and the display. On the serial line there
   is a modem, dialed with P: an init string (ATZ as it starts), the number, and whether to pulse dial. */
#include <string.h>
#include "machine.h"
#include "apple2.h"
#include "net.h"
#include "app.h"
#include "ovl.h"
#include "eth.h"
#include "prodos.h"

extern const char *const baud_name[], *const parity_name[];
static const char *const flow_name[] = {"no flow control", "RTS/CTS", "XON/XOFF"};

/* "192.168.1.50" */
static const char *ip_text(const unsigned char *ip)
{
    static char b[16];
    unsigned char i;
    b[0] = 0;
    for (i = 0; i < 4; ++i) {
        if (i)
            strcat(b, ".");
        strcat(b, utoa10(ip[i]));
    }
    return b;
}

/* "a.b.c.d" into ip; 0 if it is not one. */
static unsigned char ip_parse(const char *s, unsigned char *ip)
{
    unsigned char i;
    unsigned int v;
    for (i = 0; i < 4; ++i) {
        if (*s < '0' || *s > '9')
            return 0;
        v = 0;
        while (*s >= '0' && *s <= '9')
            v = v * 10 + (*s++ - '0');
        if (v > 255 || (i < 3 && *s++ != '.'))
            return 0;
        ip[i] = v;
    }
    return *s == 0;
}

static unsigned char edit_space;        /* 1: a space can be typed (the modem's strings) */

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
        } else if (k >= 33 - edit_space && k < 127 && n < max) {
            buf[n++] = k;
            buf[n] = 0;
        }
    }
}

static void edit_ip(unsigned char row, const char *label, unsigned char *ip)
{
    static char b[16];
    strcpy(b, ip_text(ip));
    if (edit_line(row, label, b, 15))
        ip_parse(b, ip);
}

#ifdef IIGS
/* The IIgs program: the network is Marinetti's, set up in its TCP/IP control panel, so the settings are
   the parlor's name and port, and the display. */
static void setup_settings(void)
{
    unsigned char k;
    static char b[34];
    txt_mode();
    txt_top = 0;
    txt_bot = 24;
    txt_clear();
    title_bar("ParlorNet settings", "");
    for (;;) {
        txt_goto(1, 2);
        say("Marinetti makes the connection. Set it");
        txt_goto(1, 3);
        say("up in the TCP/IP control panel.");
        setting_line(5, "1", "Parlor: ", cfg_host);
        setting_line(6, "2", "Port: ", utoa10(cfg_port));
        setting_line(8, "D", "Display: ", mono ? "monochrome" : "color");
        setting_line(9, "T", "Title screen at start: ", cfg_title ? "on" : "off");
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
        if (k == 'D')
            mono ^= 1;
        else if (k == 'T')
            cfg_title ^= 1;
        else if (k == K_RET || k == K_ESC)
            return;
        else if (k == '1') {
            strcpy(b, cfg_host);
            if (edit_line(21, "Parlor: ", b, 31) && b[0])
                strcpy(cfg_host, b);
        } else if (k == '2') {
            strcpy(b, utoa10(cfg_port));
            if (edit_line(21, "Port: ", b, 5) && b[0])
                cfg_port = num(b);
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
    sayln("Configure the ParlorNet online");
    txt_goto(1, 4);
    say("game service");
    setting_line(7, "P", "ParlorNet: connect and sign in", "");
    setting_line(9, "S", "Settings: ", "");
    txt_goto(13, 9);
    say(cfg_host);
    say(" ");
    say(utoa10(cfg_port));
    setting_line(11, "D", "Display: ", mono ? "monochrome" : "color");
    txt_goto(1, 22);
    say("Open-Apple-Q: quit");
}
#else
static void setup_settings(void)
{
    unsigned char k;
    static char b[34];
    txt_mode();
    txt_top = 0;
    txt_bot = 24;
    txt_clear();
    title_bar("ParlorNet settings", "");
    for (;;) {
        setting_line(2, "C", "Connection: ", cfg_conn ? "Uthernet II" : "Super Serial Card");
        if (!cfg_conn) {
            b[0] = '0' + cfg_slot;
            b[1] = 0;
            setting_line(4, "1", "Super Serial Card in slot ", b);
            setting_line(6, "2", "Speed: ", baud_name[cfg_baud]);
            setting_line(8, "3", "Bits and parity: ", parity_name[cfg_parity]);
            setting_line(10, "4", "Flow: ", flow_name[cfg_flow]);
            setting_line(12, "5", "Echo typing in the console: ", cfg_echo ? "on" : "off");
            setting_line(13, "6", "Init string: ", cfg_init[0] ? cfg_init : "none");
            setting_line(14, "7", "Number: ", cfg_number[0] ? cfg_number : "none, no dialing");
            setting_line(15, "8", cfg_pulse ? "[x] " : "[ ] ", "Pulse dial");
            txt_clrrow(16);
        } else {
            txt_clrrow(13);
            txt_clrrow(16);
            b[0] = '0' + cfg_eslot;
            b[1] = 0;
            setting_line(4, "1", "Uthernet II in slot ", b);
            setting_line(6, "2", "This Apple's address: ", ip_text(cfg_ip));
            setting_line(8, "3", "Netmask: ", ip_text(cfg_mask));
            setting_line(10, "4", "Gateway: ", ip_text(cfg_gw));
            setting_line(12, "5", "DNS server: ", ip_text(cfg_dns));
            setting_line(14, "6", "Parlor: ", cfg_host);
            setting_line(15, "7", "Port: ", utoa10(cfg_port));
        }
        setting_line(17, "D", "Display: ", mono ? "monochrome" : "color");
        if (PRODOS)
            setting_line(18, "T", "Title screen at start: ", cfg_title ? "on" : "off");
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
        if (k == 'C')
            cfg_conn ^= 1;
        else if (k == 'D')
            mono ^= 1;
        else if (k == 'T' && PRODOS)
            cfg_title ^= 1;
        else if (k == K_RET || k == K_ESC)
            return; else if (!cfg_conn)
            switch (k) {
            case '1': cfg_slot = cfg_slot == 7 ? 1 : cfg_slot + 1; break;
            case '2': cfg_baud = cfg_baud == 5 ? 0 : cfg_baud + 1; break;
            case '3': cfg_parity = cfg_parity == 2 ? 0 : cfg_parity + 1; break;
            case '4': cfg_flow = cfg_flow == 2 ? 0 : cfg_flow + 1; break;
            case '5': cfg_echo ^= 1; break;
            case '8': cfg_pulse ^= 1; break;
            case '6':
            case '7':
                edit_space = 1;
                if (k == '6') {
                    strcpy(b, cfg_init);
                    if (edit_line(21, "Init string: ", b, DIAL_INIT_MAX))
                        strcpy(cfg_init, b);
                } else {
                    strcpy(b, cfg_number);
                    if (edit_line(21, "Number: ", b, DIAL_NUM_MAX))
                        strcpy(cfg_number, b);
                }
                edit_space = 0;
                break;
            }
        else
            switch (k) {
            case '1': cfg_eslot = cfg_eslot == 7 ? 1 : cfg_eslot + 1; break;
            case '2': edit_ip(21, "Address: ", cfg_ip); break;
            case '3': edit_ip(21, "Netmask: ", cfg_mask); break;
            case '4': edit_ip(21, "Gateway: ", cfg_gw); break;
            case '5': edit_ip(21, "DNS server: ", cfg_dns); break;
            case '6':
                strcpy(b, cfg_host);
                if (edit_line(21, "Parlor: ", b, 31) && b[0])
                    strcpy(cfg_host, b);
                break;
            case '7':
                strcpy(b, utoa10(cfg_port));
                if (edit_line(21, "Port: ", b, 5) && b[0])
                    cfg_port = num(b);
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
    sayln("Configure the ParlorNet online");
    txt_goto(1, 4);
    say("game service");
    setting_line(7, "P", "ParlorNet: connect and sign in", "");
    setting_line(9, "C", "Console: set up the line by hand", "");
    setting_line(11, "S", "Settings: ", "");
    txt_goto(13, 11);
    if (cfg_conn) {
        say(cfg_host);
        say(" ");
        say(utoa10(cfg_port));
    } else {
        say("slot ");
        txt_putc('0' + cfg_slot);
        say(", ");
        say(line_desc());
    }
    setting_line(13, "D", "Display: ", mono ? "monochrome" : "color");
    if (PRODOS) {
        txt_goto(1, 22);
        say("Open-Apple-Q: back to ProDOS");
    }
    if (cfg_conn || cfg_number[0])      /* (P connects or dials: the menu says enough) */
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
    say("then press Open-Apple-P.");
}

#endif

/* Under ProDOS the settings are kept in PARLOR.CFG, beside the program. */
static const char cfg_name[] = "\012PARLOR.CFG";

/* The settings, in this order, after a version byte (CFG_VER, one more whenever the layout changes); then
   the addresses, the port and the parlor's name, then whether the title screen is off; then, on the IIe,
   the modem's init string, number and pulse dial. A file of another version or size is left alone: the
   settings stay as they start until they are saved again. */
#define CFG_VER 3
#ifdef IIGS
#define CFG_LEN 60
#else
#define CFG_LEN (60 + DIAL_INIT_MAX + 1 + DIAL_NUM_MAX + 1 + 1)
#endif
static unsigned char *const fields[] = {&cfg_slot, &cfg_baud, &cfg_parity, &cfg_flow, &cfg_echo, &cfg_conn,
                                        &cfg_eslot, &mono};

static void pack(unsigned char *b)
{
    unsigned char i;
    b[0] = CFG_VER;
    for (i = 0; i < 8; ++i)
        b[1 + i] = *fields[i];
    memcpy(b + 9, cfg_ip, 4);
    memcpy(b + 13, cfg_mask, 4);
    memcpy(b + 17, cfg_gw, 4);
    memcpy(b + 21, cfg_dns, 4);
    b[25] = cfg_port;
    b[26] = cfg_port >> 8;
    memcpy(b + 27, cfg_host, 32);
    b[59] = !cfg_title;
#ifndef IIGS
    memcpy(b + 60, cfg_init, DIAL_INIT_MAX + 1);
    memcpy(b + 60 + DIAL_INIT_MAX + 1, cfg_number, DIAL_NUM_MAX + 1);
    b[CFG_LEN - 1] = cfg_pulse;
#endif
}

static void pd_load(void)
{
    unsigned char *b = PD_CHUNK, i, n;
    if (!pd_open(cfg_name))
        return;
    n = pd_rw(0xCA, b, CFG_LEN);
    if (n == CFG_LEN && b[0] == CFG_VER) {
        for (i = 0; i < 8; ++i)
            *fields[i] = b[1 + i];
        memcpy(cfg_ip, b + 9, 4);
        memcpy(cfg_mask, b + 13, 4);
        memcpy(cfg_gw, b + 17, 4);
        memcpy(cfg_dns, b + 21, 4);
        cfg_port = b[25] | (b[26] << 8);
        memcpy(cfg_host, b + 27, 32);
        cfg_host[31] = 0;
        cfg_title = !b[59];
#ifndef IIGS
        memcpy(cfg_init, b + 60, DIAL_INIT_MAX);
        memcpy(cfg_number, b + 60 + DIAL_INIT_MAX + 1, DIAL_NUM_MAX);
        cfg_pulse = b[CFG_LEN - 1] & 1;
#endif
    }
    pd_close();
}

static void pd_save(void)
{
    /* CREATE (a binary file; it may be there already), then write it whole */
    pd_p[0] = 7;
    *(const char **)(pd_p + 1) = pd_path(cfg_name);
    pd_p[3] = 0xC3;
    pd_p[4] = 0x06;
    pd_p[5] = pd_p[6] = 0;
    pd_p[7] = 1;
    pd_p[8] = pd_p[9] = pd_p[10] = pd_p[11] = 0;
    mli(0xC0, pd_p);
    if (!pd_open(cfg_name))
        return;
    pack(PD_CHUNK);
    pd_rw(0xCB, PD_CHUNK, CFG_LEN);
    pd_p[0] = 2;
    pd_p[1] = pd_ref;
    pd_p[2] = CFG_LEN;
    pd_p[3] = pd_p[4] = 0;
    mli(0xD0, pd_p);                       /* SET_EOF */
    pd_close();
}

static void after_settings(void)
{
    setup_settings();
    if (PRODOS)
        pd_save();
}

#pragma rodata-name (push, "OVHDR")
const struct setup setup_header = {after_settings, start_screen, pd_load};
#pragma rodata-name (pop)
