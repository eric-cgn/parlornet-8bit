/* ParlorNet: the main loop (the start screen and the settings are the machine's setup overlay, setup.c; the
   machine starts up, opens the line and quits in its own code, sys_init and the rest in machine.h).

   The console, a plain terminal on the serial line for setting up the line before ParlorNet mode, is in the
   sign-in overlay (parlor.c). OA-P (Open-Apple on the Apple II, the Commodore key on the C64) goes from it
   into ParlorNet mode, OA-C comes back to it from the lobby, closing the connection to the parlor first, and
   OA-S opens the settings from it. P on the start screen dials the modem first (parlor.c), unless the
   settings' number is empty. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "app.h"
#include "ovl.h"

unsigned char connected;
unsigned char signin_note;
unsigned char cfg_echo;
unsigned char cfg_title = 1;
unsigned char cfg_tutor;
unsigned char cfg_stay;
char cfg_user[13];
char cfg_key[24];
unsigned char cfg_changed;

/* The settings into their file, by the setup overlay. */
static void cfg_save(void)
{
    cfg_changed = 0;
    if (ovl_load(OVL_SETUP))
        SETUP->save();
}

void tutorial(void)
{
    if (ovl_load(OVL_FRAME) && ovl_load(OVL_TUTORIAL))
        TUTORIAL->run();
}
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

unsigned char signed_in;

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

/* The console, in the sign-in overlay (parlor.c), which OA-P's opening needs at once: the parlor waits 20 seconds
   for it after the command that connected, with no disk load in between, and on a C64's 1541 a load takes
   seconds. A connection to the parlor still open is closed first, as Open-Apple-Q closes it: what is typed in
   the console ends its lines with CR alone, which the parlor would refuse. Returns 1 when Open-Apple-P asks
   for ParlorNet mode, 0 for the start screen. A machine with no serial line (the IIgs) has none. */
static unsigned char console(void)
{
#if MACHINE_SERIAL
    unsigned char k;
    if (connected)
        close_parlor();
    while (ovl_load(OVL_SIGNIN)) {
        k = SIGNIN->console();
        if (k != 2)
            return k;
        settings();
    }
#endif
    return 0;
}

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
            /* (the frame first: on the IIe it goes through the overlays' memory into the language card's
               bank 1, ovl.h) */
            if (!ovl_load(OVL_FRAME) || !ovl_load(OVL_SIGNIN))
                return 0;
            k = SIGNIN->run();
            if (k == 3)
                goto quit;
            if (k != 4)
                return k;
            signed_in = 1;
            k = 0;
            if (!cfg_tutor) {
                /* the first sign-in: the tutorial's dialog, and the answer kept, so it comes once */
                cfg_tutor = cfg_changed = 1;
                k = SIGNIN->offer();
            }
            if (cfg_changed)            /* (and a key that came or went with the sign-in) */
                cfg_save();
            if (k)
                tutorial();
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
            if (k != 3) {               /* (3: signed out already, by /quit) */
                q_set("OUT");
                net_req();
            }
            signin_note = 1;
            if (cfg_key[0]) {           /* signing out forgets this computer's key, here as on the parlor */
                cfg_key[0] = 0;
                cfg_save();
            }
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
        else if (k == 'T')
            tutorial();
        while (go) {
            /* parlor() returns 1 when the player asks for the console. */
            if (!parlor())
                break;
            if (!console())
                break;
        }
    }
}
