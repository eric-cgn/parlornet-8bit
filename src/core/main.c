/* ParlorNet: the line console and the main loop (the start screen and the settings are the machine's setup
   overlay, setup.c; the machine starts up, opens the line and quits in its own code, sys_init and the rest
   in machine.h).

   The console is a plain terminal on the serial line: what is typed goes out, what comes in is
   shown. It is for setting up the line (dialing a modem, say) before ParlorNet mode, which starts with
   OA-P (Open-Apple on the Apple II, the Commodore key on the C64). When a line has been typed
   and not yet sent, OA-P sends its Return first, so that a command that connects to the parlor (nc in a
   Unix shell) is followed at once by ParlorNet mode's opening line: the parlor waits 20 seconds for it.
   Where the sign-in loads slowly (MACHINE_LATE_SIGNIN, the C64), OA-P sends the opening before the sign-in
   is loaded.
   OA-C comes back to the console from ParlorNet mode, closing the connection to the parlor first, and
   OA-S opens the settings from either. P on the start screen dials the
   modem first (parlor.c), unless the settings' number is empty. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "app.h"
#include "ovl.h"

unsigned char connected;
unsigned char signin_note;
unsigned char cfg_echo;
unsigned char cfg_title = 1;
#if MACHINE_SERIAL
char cfg_init[DIAL_INIT_MAX + 1] = "ATZ";
unsigned char cfg_pulse;
char cfg_number[DIAL_NUM_MAX + 1] = "parlornet.org:6501";
unsigned char dial_now, dialing;
#endif

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

/* The settings screen, in the setup overlay; the line is opened again after. */
void settings(void)
{
    if (ovl_load(OVL_SETUP))
        SETUP->settings();
    serial_open();
}

static unsigned char signed_in;

/* Close the connection to the parlor: BYE, after which the parlor hangs up, so a modem or a shell's
   netcat ends too. A dialed line is then hung up by the dialer (in the sign-in overlay, loaded first).
   1 when it was a dialed line. */
static unsigned char close_parlor(void)
{
    unsigned char k = 0;
    signed_in = 0;
#if MACHINE_SERIAL
    k = dialing && ovl_load(OVL_SIGNIN);
#endif
    q_set("BYE");
    net_req();
    connected = 0;
    if (eth_on)
        eth_close();
#if MACHINE_SERIAL
    if (k)
        SIGNIN->hangup();
#endif
    return k;
}

#if MACHINE_LATE_SIGNIN
/* OA-P with the sign-in not loaded: the opening now, since the parlor waits only 20 seconds for it after the
   line connects (and 15 minutes after it). Its answer is left in the ring until it has all come (a second
   with no more of it, or four seconds with none), and the sign-in reads it there after its load
   (signin_note 3). */
static void send_opening(void)
{
    static unsigned char n, quiet, at;
    ser_drop();
    net_opening();
    n = quiet = 0;
    do {
        if (ser_used() != n) {
            n = ser_used();
            quiet = 0;
        }
        tick_poll();
        if ((unsigned char)ticks != at) {
            at = ticks;
            ++quiet;
        }
    } while (quiet < (n ? 60 : 240));
    signin_note = 3;
}
#endif

/* The console. Returns 1 when Open-Apple-P asks for ParlorNet mode, 0 for the start screen. A machine with no
   serial line (MACHINE_SERIAL 0: the IIgs, whose connection is Marinetti's) has none. */
#if !MACHINE_SERIAL
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
    /* A connection to the parlor still open (from the name prompt, the lobby or a table, or left at the
       start screen) is closed first, as Open-Apple-Q closes it: what is typed in the console ends its
       lines with CR alone, which the parlor would refuse, so the console starts on the line as it is
       without the parlor, at the modem or the shell. */
    if (connected)
        close_parlor();
again:
#if !MACHINE_LATE_SIGNIN
    /* The sign-in overlay first: OA-P's opening must follow the command that connected at once (the parlor
       waits 20 seconds for it), with no disk load in between. */
    ovl_load(OVL_SIGNIN);
#endif
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
    sayln("(6501, not telnet), then press " OA "P.");
    sayln("To dial a modem, type ATDT host:6501.");
    sayln("Press " OA "Q for the start screen.");
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
#if MACHINE_LATE_SIGNIN
                    send_opening();
#endif
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
    unsigned char k;
    for (;;) {
        if (!signed_in || net_lost) {
            signed_in = 0;
            if (!ovl_load(OVL_SIGNIN)) {
                signin_note = 0;
                return 0;
            }
            k = SIGNIN->run();
            if (k == 3)
                goto quit;
            if (k != 4)
                return k;
            signed_in = 1;
        }
        k = lobby();
        if (k == 1)
            return 1;                   /* (the console closes the connection) */
        signed_in = 0;
        if (k == 2) {
quit:
            /* A dialed line comes back to the start screen, one connected by hand to the console. */
            return !close_parlor();
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
    sys_init();
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
        if ((kbd_mods() & 1) && k == 'Q')
            sys_quit();
        go = 0;
        if (k == 'P' || k == K_RET) {
            go = 1;
#if MACHINE_SERIAL
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
