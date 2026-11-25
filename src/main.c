/* ParlorNet for the Apple IIe: the line console and the main loop (the start screen and the settings are
   the setup overlay, setup.c).

   The console is a plain terminal on the Super Serial Card: what is typed goes out, what comes in is
   shown. It is for setting up the line (dialing a modem, say) before ParlorNet mode, which starts with
   Open-Apple-P. When a line has been typed and not yet sent, Open-Apple-P sends its Return first, so
   that a command that connects to the parlor (nc in a Unix shell) is followed at once by ParlorNet
   mode's opening line: the parlor waits 20 seconds for it. Open-Apple-C comes back to the console
   from ParlorNet mode, and Open-Apple-S opens the settings from either. P on the start screen dials the
   modem first (parlor.c), unless the settings' number is empty. */
#include <string.h>
#include "a2.h"
#include "ui.h"
#include "net.h"
#include "app.h"
#include "ovl.h"
#include "eth.h"
#include "prodos.h"

unsigned char cfg_slot = 2, cfg_baud = 4, cfg_parity, cfg_flow, cfg_echo;
unsigned char cfg_conn;                 /* 0 the Super Serial Card, 1 the Uthernet II */
unsigned char cfg_title = 1;
#ifndef IIGS
unsigned char cfg_reset = 1;
char cfg_init[DIAL_INIT_MAX + 1];
char cfg_number[DIAL_NUM_MAX + 1] = "parlornet.org:6501";
unsigned char dial_now, dialing;
#endif

const char *const baud_name[] = {"300", "1200", "2400", "4800", "9600", "19200"};
static const unsigned char baud_code[] = {6, 8, 10, 12, 14, 15};
const char *const parity_name[] = {"8N1", "7O1", "7E1"};

void say(const char *s)
{
    txt_puts(s);
}

void sayln(const char *s)
{
    txt_puts(s);
    txt_putc(13);
}

void title_bar(const char *left, const char *right)
{
    unsigned char n;
    const char *p;
    txt_goto(0, 0);
    txt_inv = 1;
    txt_puts(left);
    n = 40 - txt_x - strlen(right);
    if (!txt_mt)                        /* each apple is "OA-" there (txt_putc) */
        for (p = right; *p; ++p)
            if ((unsigned char)*p >= 0x7F)
                n -= 2;
    while (n--)
        txt_putc(' ');
    txt_puts(right);
    txt_inv = 0;
}

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

/* The settings screen, in the setup overlay; the line is opened again after. */
void settings(void)
{
    if (ovl_load(OVL_SETUP))
        SETUP->settings();
    serial_open();
}

/* The console. Returns 1 when Open-Apple-P asks for ParlorNet mode, 0 for the start screen. The IIgs
   program, whose connection is Marinetti's, has none. */
#ifdef IIGS
static unsigned char console(void)
{
    return 0;
}
#else
static unsigned char console(void)
{
    static unsigned char k, under, shown, lastcr, typed;
    static unsigned int blink;
    static int c;
again:
    txt_mode();
    txt_top = 0;
    txt_bot = 24;
    txt_clear();
    title_bar("Console", OA "P: ParlorNet  " OA "S: Settings");
    txt_top = 1;
    txt_goto(0, 1);
    say("Line: ");
    sayln(line_desc());
    sayln("Connect to the parlor's native port");
    sayln("(6501, not telnet), then press");
    sayln("Open-Apple-P. A modem: ATDT host:6501");
    sayln("Open-Apple-Q: the start screen.");
    txt_putc(13);
    shown = 0;
    lastcr = 0;
    blink = ticks;
    for (;;) {
        c = ser_get();
        if (c >= 0 || (k = kbd_get()) != 0 || ticks - blink > 20) {
            if (shown) {
                txt_cursor(under);
                shown = 0;
            }
        }
        if (c >= 0) {
            c &= 0x7F;
            if (c == 13) {
                txt_putc(13);
                lastcr = 1;
            } else if (c == 10) {
                if (!lastcr)
                    txt_putc(13);
                lastcr = 0;
            } else {
                lastcr = 0;
                if (c == 8)
                    txt_putc(8);
                else if (c == 9) {
                    do
                        txt_putc(' ');
                    while (txt_x & 7);
                } else if (c >= 32 && c < 127)
                    txt_putc(c);
            }
            continue;
        }
        if (k) {
            if (kbd_mods() & 1) {
                if (k >= 'a')
                    k -= 32;
                if (k == 'P') {
                    /* a command typed and not yet sent (nc to the parlor, say): Return for it first,
                       so that ParlorNet mode's opening follows it at once */
                    if (typed)
                        ser_put(13);
                    typed = 0;
                    return 1;
                }
                if (k == 'Q')
                    return 0;
                if (k == 'S') {
                    settings();
                    goto again;
                }
                k = 0;
                continue;
            }
            if (k == K_LEFT)
                k = 8;
            typed = k != 13;
            ser_put(k);
            if (cfg_echo) {
                if (k == 13)
                    txt_putc(13);
                else if (k == 8 || k == K_DEL) {
                    txt_putc(8);
                    txt_putc(' ');
                    txt_putc(8);
                } else
                    txt_putc(k);
            }
            k = 0;
            continue;
        }
        tick_poll();
        if (!shown && ticks - blink > 20) {
            under = txt_peek();
            txt_cursor(0x20);           /* an inverse space */
            shown = 1;
            blink = ticks;
        } else if (shown && ticks - blink > 20) {
            blink = ticks;
        }
    }
}

#endif

char me[13];
char server_name[SERVER_NAME_MAX + 1];

/* ParlorNet mode: the sign-in (an overlay), then the lobby, until the player asks for the console
   (1) or the start screen (0). */
unsigned char parlor(void)
{
    static unsigned char signed_in;
    unsigned char k;
    for (;;) {
        if (!signed_in || net_lost) {
            signed_in = 0;
            if (!ovl_load(OVL_SIGNIN))
                return 0;
            k = SIGNIN->run();
            if (k == 3)
                goto quit;
            if (k != 4)
                return k;
            signed_in = 1;
        }
        k = lobby();
        if (k == 1)
            return 1;
        signed_in = 0;
        if (k == 2) {
quit:
            /* Close the connection: the parlor hangs up, so a modem or a shell's netcat ends too. A
               dialed line is then hung up by the dialer (in the sign-in overlay, loaded first), and the
               start screen comes back. */
            signed_in = 0;
#ifndef IIGS
            k = dialing && ovl_load(OVL_SIGNIN);
#endif
            q_set("BYE");
            net_req();
            connected = 0;
            if (eth_on)
                eth_close();
#ifndef IIGS
            if (k) {
                SIGNIN->hangup();
                return 0;
            }
#endif
            return 1;
        }
        if (net_lost) {
            connected = 0;
            signin_note = 2;
        } else {
            q_set("OUT");
            net_req();
            signin_note = 1;
        }
    }
}

void main(void)
{
    unsigned char k, go, title = 0;
    txt_detect();
    txt_mode();
    if (PRODOS) {
        pd_main_sp = *(unsigned char *)0x081F;
        if (ovl_load(OVL_SETUP))
            SETUP->load();
    }
    ui_mouse_init();
    serial_open();
    /* The title screen says "Press a key" and stays until one is pressed. A key pressed before it
       showed (one left in the keyboard's latch) does not count. */
    if (cfg_title && ovl_load(OVL_SPLASH)) {
        SPLASH->show();
        title = 1;
    }
    if (title) {
        ovl_load(OVL_SETUP);
        while (kbd_get())
            ;
        while (!kbd_get())
            ;
    }
    for (;;) {
        if (ovl_load(OVL_SETUP))
            SETUP->start();
        do
            k = kbd_get();
        while (!k);
        if (k >= 'a')
            k -= 32;
        if ((kbd_mods() & 1) && k == 'Q' && PRODOS)
            pd_quit();
        go = 0;
        if (k == 'P' || k == K_RET) {
            go = 1;
#ifndef IIGS
            dial_now = 1;
#endif
        }
        else if (k == 'C')
            go = console() ? 1 : 0;
        else if (k == 'S')
            settings();
        else if (k == 'D')
            mono ^= 1;
        while (go) {
            /* parlor() returns 1 when the player asks for the console. */
            if (!parlor())
                break;
            if (!console())
                break;
        }
    }
}
