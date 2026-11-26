/* ParlorNet: ParlorNet mode's start. The opening line (PARLORNET/1, which sets the line
   ending), the greeting, and signing in or up as the server asks (docs/accounts.md): every INF line is shown, every
   ASK is a prompt with the server's label, so the email sign-up, the temporary password and the new
   password after it all work without the client knowing which method it is. On the serial line, P first
   dials the modem: the init string (ATZ as it starts), ATDT (or ATDP) and the number, and on CONNECT the opening at
   once; the
   dialer hangs the line up when the session ends. A machine with a network connects over it instead
   (eth_connect, in the machine's signin_net.c). */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "app.h"
#include "ovl.h"
#include "frame.h"
#if !MACHINE_SERIAL
#define CONSOLE_KEYS ""                 /* no serial line (the IIgs): no console */
#define NAME_KEYS "(" OA "Q: disconnect)"
#define NETWORKED 1                     /* always through the network (the IIgs's Marinetti) */
#define DIALING 0
#else
#define CONSOLE_KEYS OA "C: Console"
#define NAME_KEYS OA "C: the console  " OA "Q: disconnect"
#define NETWORKED cfg_conn
#define DIALING dialing
#endif

static unsigned char auth_none;
static unsigned char keyed;             /* 1: the next password question is answered with the key; 2: it was */

/* Text-mode line input at the cursor, to the end of the row. 1 on Return, 0 on Escape; OA-C (the
   console) gives 2. */
static unsigned char txt_input(char *buf, unsigned char max, unsigned char secret)
{
    unsigned char x0 = txt_x, y0 = txt_y, w = 39 - txt_x, n, k, i, from;
    buf[0] = 0;
    n = 0;




    for (;;) {




        from = n >= w ? n - w + 1 : 0;
        txt_goto(x0, y0);
        for (i = from; i < n; ++i)
            txt_putc(secret ? '*' : buf[i]);
        txt_cursor(0x20);
        ++txt_x;
        txt_clreol();
        while (!(k = kbd_get()))
            ;
        if (kbd_mods() & 1) {
            if ((k | 0x20) == 'c')
                return 2;
            if ((k | 0x20) == 'q')
                return 3;
            continue;
        }
        if (k == K_RET) {
            txt_goto(x0 + (n - from), y0);
            txt_putc(' ');
            txt_putc(13);
            return 1;
        }
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

static void show_wrapped(const char *s)
{
    /* Break at a space where a word would cross the edge. */
    const char *p;
    unsigned char len;
    while (*s) {
        p = s;
        while (*p && *p != ' ')
            ++p;
        len = p - s;
        if (txt_x && txt_x + len > 40)
            txt_putc(13);
        while (s < p)
            txt_putc(*s++);
        if (*s == ' ') {
            if (txt_x)
                txt_putc(' ');
            ++s;
        }
    }
    if (txt_x)
        txt_putc(13);
}

/* While connecting, and on the question after it, nothing is typed: C, with Open-Apple or without, is the
   console, so that Open-Apple-C pressed while the program waits for the line still counts (by the time
   the key is read, Open-Apple may be up). 2 for the console, 3 for Escape, 0 for anything else. */
static unsigned char key_out(void)
{
    unsigned char k = kbd_get();
    if (k == K_ESC)
        return 3;
#if MACHINE_SERIAL
    if ((k | 0x20) == 'c')
        return 2;
#endif
    return 0;
}

/* Wait for the greeting after the opening. 1 with the greeting in q; 2 with the parlor's refusal of the
   opening in q (ERR PROTOCOL, VERSION or BUSY); 0 if none came; 4 for the console (Open-Apple-C), 5 for the
   start screen (Escape). A greeting has the parlor's name after the version: a line that reads just
   "PARLORNET/1" is our own opening echoed by a terminal line, a shell whose nc has ended, say. */
static unsigned char greet(void)
{
    unsigned int start = ticks;
    unsigned char k;
    net_reset();
    while (ticks - start < 6 * 60) {
        k = key_out();
        if (k)
            return k + 2;
        if (net_line(60) && r_n) {
            char *l = r_line[r_n - 1];
            if (strncmp(l, "PARLORNET/", 10) == 0 && strchr(l, ' ')) {
                strcpy(q, l);
                return 1;
            }
            net_reset();
        } else if (r_final) {
            /* another final line: an ERR UNKNOWN from a parlor that greeted this line already, say, or
               a modem's ERROR; parlor_there tells */
            if (strncmp(r_final, "ERR PROTOCOL", 12) == 0 || strncmp(r_final, "ERR VERSION", 11) == 0
                || strncmp(r_final, "ERR BUSY", 8) == 0) {
                strcpy(q, r_final);
                return 2;
            }
            return 0;
        }
        tick_poll();
    }
    return 0;
}

/* No greeting: is a parlor there all the same, on a line it greeted before? Only a parlor answers PRL
   with a PRO line (its own PARLORNET/1 is always one); a shell, a modem or nothing at all does not. */
static unsigned char parlor_there(void)
{
    unsigned char i;
    q_set("PRL");
    ser_drop();
    net_send_line(q);
    if (net_reply(6) == R_OK)
        for (i = 0; i < r_n; ++i)
            if (strncmp(r_line[i], "PRO ", 4) == 0)
                return 1;
    net_lost = 0;
    return 0;
}

static unsigned char ask_again(const char *why)
{
    unsigned char k;
    txt_putc(13);
    show_wrapped(why);
    txt_putc(13);
#if !MACHINE_SERIAL
    sayln("Return: try again");
    sayln("Esc: the start screen");
#else
    say(DIALING ? "Return: dial again" : "Return: try again");
    sayln("   " OA "C: the console");
    sayln("Esc: the start screen");
#endif
    for (;;) {
        k = kbd_get();
        if (k == K_RET)
            return 1;
        if (k == K_ESC)
            return 0;
#if MACHINE_SERIAL
        if ((k | 0x20) == 'c')          /* with Open-Apple or without (key_out) */
            return 2;
#endif
    }
}

#if MACHINE_SERIAL
/* The dialer. What the modem says is read a burst at a time, until a second of quiet (net_reply), and
   nothing is drawn while it may be coming: without the serial card's interrupt only the read loop takes
   the bytes in time. The modem's echo of a command (the command, or any line that starts with AT) is
   skipped. */
static unsigned char cancel;            /* key_out's 2 (the console) or 3 (Escape) while dialing */
static const char *sent = "";           /* the last command */

static void modem_cmd(const char *s)
{
    sent = s;
    ser_drop();
    while (*s)
        ser_put(*s++);
    ser_put(13);
}

/* The modem's answer within a second's quiet, or 0: OK when it came, else the first line that is a modem's
   word (capitals) and not the echo. Anything else is skipped, such as the escape codes a Unix shell sends
   after the echoed line (bash's ESC [?2004l). With keys set, Escape or C first stops the wait (cancel). */
static char *modem_read(unsigned char keys)
{
    unsigned char i;
    char *l;
    if (keys && (cancel = key_out()) != 0)
        return 0;
    net_reply(1);
    net_lost = 0;
    if (r_final)
        return r_final;
    for (i = 0; i < r_n; ++i) {
        l = r_line[i];
        if (l[0] >= 'A' && l[0] <= 'Z' && (l[0] != 'A' || (l[1] | 0x20) != 't') && strcmp(l, sent))
            return l;
    }
    if (sr_final == 'N')
        return "NO CARRIER";            /* (net_reply takes it as the end of the connection) */
    return 0;
}

/* A command before the dial, shown with the modem's OK, waiting up to three seconds; a line that says
   nothing (a shell's ATZ, say, until it has started) dials on all the same. 0 on ERROR (the reason in q) or a key
   (cancel). */
static unsigned char modem_ok(const char *s)
{
    unsigned int start = ticks;
    char *l;
    say(s);
    say(": ");
    modem_cmd(s);
    while (ticks - start < 180) {
        l = modem_read(1);
        if (cancel)
            return 0;
        if (l && strncmp(l, "OK", 2) == 0) {
            sayln(l);
            return 1;
        }
        if (l && strncmp(l, "ERROR", 5) == 0) {
            sayln(l);
            q_set("The modem says ERROR to ");
            q_cat(s);
            q_cat(".");
            return 0;
        }
    }
    sayln("(no answer)");
    return 1;
}

/* Send the init string (ATZ as the settings start; nothing when it is empty), dial ATDT and the number, and
   wait up to a minute for CONNECT. 1 on CONNECT; 0 with the reason in q; 2 for the console and 3 for
   Escape, after a Return that stops a dial. */
static unsigned char dial(void)
{
    unsigned int start;
    char *l;
    sayln("Dialing  (Esc: cancel)");
    txt_putc(13);
    cancel = 0;
    if (cfg_init[0] && !modem_ok(cfg_init)) {
        if (cancel)
            goto stop;
        return 0;
    }
    /* ATDT and the number, or ATDP with the settings' Pulse dial; a number that starts with AT is sent as it
       is, the whole dial command (ATD"host:port" for a Zimodem) */
    q_set((cfg_number[0] | 0x20) == 'a' && (cfg_number[1] | 0x20) == 't' ? "" : cfg_pulse ? "ATDP" : "ATDT");
    q_cat(cfg_number);
    sayln(q);
    modem_cmd(q);
    start = ticks;
    while (ticks - start < 60 * 60) {
        l = modem_read(1);
        if (cancel)
            goto stop;
        if (!l)
            continue;
        sayln(l);
        if (strncmp(l, "CONNECT", 7) == 0)
            return 1;
        /* NO CARRIER, NO DIALTONE, NO ANSWER, BUSY, ERROR */
        if (strncmp(l, "NO ", 3) == 0 || strncmp(l, "BUSY", 4) == 0 || strncmp(l, "ERROR", 5) == 0) {
            q_set("The modem says ");
            q_cat(l);
            q_cat(".");
            return 0;
        }
    }
    ser_put(13);
    q_set("The modem did not say CONNECT in a minute.");
    return 0;
stop:
    ser_put(13);                        /* a key stops a modem's dial */
    modem_read(0);
    return cancel;
}

/* Hang up a dialed line. After BYE the parlor hangs up and the modem says NO CARRIER. A modem that stays
   connected gets the Hayes escape, +++ with a second's quiet before and after, then ATH. (Only here:
   +++ is never sent during a session.) */
static void hangup(void)
{
    unsigned int start = ticks;
    char *l;
    while (ticks - start < 90) {
        l = modem_read(0);
        if (l && strncmp(l, "NO CARRIER", 10) == 0)
            return;
    }
    ser_put('+');
    ser_put('+');
    ser_put('+');
    modem_read(0);                      /* OK, after the second's quiet */
    modem_cmd("ATH");
    modem_read(0);
}
#endif

/* The sign-in dialog after USER, REG or HELO. 1 when signed in, 0 when refused, 2 for the console. */
static unsigned char dialogue(void)
{
    static char ans[66];
    static char field[16];
    unsigned char kind, i, secret, yn, k;
    char *label, *p;
    for (;;) {
        kind = net_req();
        for (i = 0; i < r_n; ++i)
            if (strncmp(r_line[i], "INF", 3) == 0)
                show_wrapped(r_line[i][3] ? r_line[i] + 4 : " ");
        if (kind == R_OK)
            return 1;
        if (kind == R_NONE)
            return 0;
        if (kind == R_ERR) {
            txt_inv = 1;
            show_wrapped(err_text(r_final));
            txt_inv = 0;
            return 0;
        }
        words(r_final);
        strncpy(field, w[1], 15);
        field[15] = 0;
        label = wn > 2 ? w[2] : field;
        for (p = label; *p; ++p)
            if (*p == '_')
                *p = ' ';
        secret = yn = 0;
        for (i = 3; i < wn; ++i) {
            if (is(w[i], "secret"))
                secret = 1;
            if (is(w[i], "yn"))
                yn = 1;
            if (is(w[i], "cred") && keyed == 1)
                keyed = 2;
        }
        if (keyed == 2) {               /* the key for this computer answers it, with no question asked */
            q_set("AUT key ");
            q_cat(cfg_key);
            continue;
        }
        say(label);
        if (yn) {
            say(" ");
            do {
                do
                    k = kbd_get();
                while (!k);
                if (kbd_mods() & 1) {
                    if ((k | 0x20) == 'c')
                        return 2;
                    if ((k | 0x20) == 'q')
                        return 3;
                }
                k |= 0x20;
            } while (k != 'y' && k != 'n');
            txt_putc(k);
            txt_putc(13);
            ans[0] = k;
            ans[1] = 0;
        } else {
            say(": ");
            k = txt_input(ans, 64, secret);
            if (k >= 2)
                return k;
            if (!k) {
                strcpy(ans, "-");       /* Escape: an answer that is refused, ending the dialog */
            }
        }
        q_set("AUT");
        q_add(field);
        q_add(ans);
        memset(ans, 0, sizeof ans);
    }
}

static void prefs_load(void)
{
    unsigned char i;
    q_set("PRF GET a2disp");
    if (net_req() == R_OK)
        for (i = 0; i < r_n; ++i)
            if (strncmp(r_line[i], "PRF a2disp ", 11) == 0)
                mono = r_line[i][11] == 'm';
}

static void screen(void)
{
    txt_mode();
    txt_top = 0;
    txt_bot = 24;
    txt_clear();
    title_bar("ParlorNet", CONSOLE_KEYS);
    txt_top = 2;                        /* the title bar stays when the text scrolls */
    txt_goto(0, 2);
}

/* The screen is full: wait for a key on the bottom row, then go on at the top of a fresh screen. */
static void more(void)
{
    txt_goto(0, 23);
    txt_inv = 1;
    say(" More: press a key ");
    txt_inv = 0;
    ui_wait_key();
    screen();
}

/* The sign-in overlay's entry: connect if the line has not been greeted yet, then sign in.
   Returns 0 for the start screen, 1 for the console, 3 to close the connection, 4 when signed in. */
static unsigned char run(void)
{
    static char name[14];
    unsigned char k, n, in;
    const char *p;

#if MACHINE_SERIAL
    static unsigned char online;
    /* P on the start screen dials, unless the line is connected already or the number is empty; the
       console's Open-Apple-P does not. After a lost connection, a dialed line is dialed again. */
    online = 0;
    if (dial_now && !connected)
        dialing = !cfg_conn && cfg_number[0];
    else if (!connected && signin_note != 2)
        dialing = 0;
    dial_now = 0;
#endif


    screen();


    if (signin_note == 1) {
        q_set("Signed out of ");
        q_cat(server_name);
        show_wrapped(q);
    }
    k = signin_note;
    signin_note = 0;
    if (k == 2)
        goto lost;
    if (connected)
        goto names;
connect:
    screen();
    net_lost = 0;
    if (NETWORKED) {
        if (!eth_connect()) {
            k = ask_again(q);
            if (k == 1)
                goto connect;
            return k == 2;
        }
#if MACHINE_SERIAL
    } else if (dialing) {
        if (online)
            hangup();
        online = 0;
        k = dial();
        if (k == 3)
            return 0;                   /* Escape: the start screen */
        if (k == 0)
            k = ask_again(q);
        else if (k == 1)
            online = 1;
        else
            k = 2;
        if (!online) {
            if (k == 1)
                goto connect;
            return k == 2;
        }
#endif
    } else {
        say("Connecting (");
        say(line_desc());
        sayln(")...");
        sayln("(" OA "C: the console  Esc: start screen)");
    }
    ser_drop();
    net_opening();
    k = greet();
    if (k >= 4)
        return k == 4;
    if (k == 0 && parlor_there()) {
        /* No greeting, but a parlor answers: the line was greeted before. Its name, when this program
           learned it then, and its sign-in method are not asked again. */
        q_set("PARLORNET/1 ");
        q_cat(server_name[0] ? server_name : "parlor");
        k = 1;
    }
    if (k != 1) {
        k = ask_again(k == 2 ? err_text(q) : "The parlor did not answer. Is the line connected to its "
                      "native port (6501, not telnet)?");
        if (k == 1)
            goto connect;
        return k == 2;
    }
    words(q);
    if (!is(w[0], "PARLORNET/1")) {
        k = ask_again("This parlor speaks a later version of the protocol than this program.");
        if (k == 1)
            goto connect;
        return k == 2;
    }
    strncpy(server_name, wn > 1 ? w[1] : "parlor", SERVER_NAME_MAX);
    server_name[SERVER_NAME_MAX] = 0;
    auth_none = 0;
    for (k = 2; k < wn; ++k)
        if (is(w[k], "AUTH=none"))
            auth_none = 1;
    connected = 1;
    q_set("Connected to ");
    q_cat(server_name);
    show_wrapped(q);
    /* The message of the day, all of it, a screen at a time. */
    q_set("MOTD");
    if (net_req() == R_OK) {
        for (k = 0; k < r_n; ++k)
            if (strncmp(r_line[k], "MOTD", 4) == 0) {
                /* A line that fits the row is shown as it is, spaces and all (an ASCII layout, a blank
                   line); a longer one (up to 73 characters) is wrapped and takes at most three rows. */
                p = r_line[k][4] ? r_line[k] + 5 : "";
                n = strlen(p);
                if (txt_y + (n > 40 ? 3 : 1) > 23)
                    more();
                if (n > 40)
                    show_wrapped(p);
                else if (n < 40)
                    sayln(p);
                else
                    say(p);             /* 40: the row's end starts the next */
            }
        if (txt_y > 20)                 /* no room left for the name prompt below it */
            more();
    }
    if (net_lost)
        goto lost;
names:
    for (;;) {
        txt_putc(13);
        keyed = 0;
        if (cfg_key[0] && !auth_none) {
            /* Stay signed in: the key kept for this computer answers the password */
            strcpy(name, cfg_user);
            say("Signing in as ");
            sayln(name);
            keyed = 1;
            q_set("USER");
            goto user;
        }
        sayln("Type your name, or new to sign up.");
        sayln(NAME_KEYS);
        say("Name: ");
        k = txt_input(name, 12, 0);
        if (k >= 2)
            return k == 2 ? 1 : 3;
        if (!k)
            return 0;
        if (net_lost)
            goto lost;
        if (!name[0])
            continue;
        if (is(name, "new") || is(name, "NEW")) {
            say("The name you want: ");
            k = txt_input(name, 12, 0);
            if (k >= 2)
                return k == 2 ? 1 : 3;
            if (!k || !name[0])
                continue;
            q_set(auth_none ? "HELO" : "REG");
        } else
            q_set(auth_none ? "HELO" : "USER");
user:
        q_add(name);
        in = dialogue();
        if (in >= 2)
            return in == 2 ? 1 : 3;
        if (net_lost)
            goto lost;
        if (!in && keyed == 2 && r_final && strncmp(r_final, "ERR NOKEY", 9) == 0) {
            /* the key was refused (signed out elsewhere, or the password changed): forgotten, and the
               password asked for as usual */
            cfg_key[0] = 0;
            cfg_changed = 1;
            keyed = 0;
            q_set("USER");
            goto user;
        }
        if (!in && !(r_final && strncmp(r_final, "ERR STATE", 9) == 0))
            continue;                   /* (ERR STATE: signed in already, on this connection) */
        if (keyed == 2) {
            if (!cfg_stay) {            /* (the setting turned off since: this computer's key forgotten) */
                q_set("KEY DEL");
                net_req();
                cfg_key[0] = 0;
                cfg_changed = 1;
            }
        } else if (cfg_stay && !auth_none) {
            /* the first sign-in with the password gets the key, which is kept instead of the password */
            q_set("KEY NEW " MACHINE_LABEL);
            if (net_req() == R_OK && r_final[2]) {
                strncpy(cfg_key, r_final + 3, sizeof cfg_key - 1);
                strcpy(cfg_user, name);
                cfg_changed = 1;
            }
        }
        strcpy(me, name);
        prefs_load();
        /* The time signed in starts from 0m; the lobby's first poll asks the parlor for it (TIM). */
        memset(clk_t, 0, 3);
        clk_tim_off = 0;
        clk_tim_at = 0xFF;
        return 4;
    }
lost:
    connected = 0;
#if MACHINE_SERIAL
    online = 1;                         /* a modem that has not noticed is hung up before the dial */
#endif
    k = ask_again("The connection to the parlor was lost.");
    if (k == 1)
        goto connect;
    return k == 2;
}

#if MACHINE_SERIAL
/* The console: a plain terminal on the serial line, for setting up the line (dialing a modem, say) before
   ParlorNet mode. What is typed goes out and what comes in is shown. Returns 1 when Open-Apple-P asks for
   ParlorNet mode, 0 for the start screen (Open-Apple-Q) and 2 for the settings (Open-Apple-S), after which
   main.c opens it again. When a line has been typed and not yet sent, OA-P sends its Return first, so that a
   command that connects to the parlor (nc in a Unix shell) is followed at once by ParlorNet mode's opening
   line: the parlor waits 20 seconds for it. */
static unsigned char console(void)
{
    static unsigned char k, under, shown, lastcr, typed;
    static unsigned int blink;
    static int c;
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
                    if (typed)
                        ser_put(13);
                    typed = 0;
                    return 1;
                }
                if (k == 'Q')
                    return 0;
                if (k == 'S')
                    return 2;
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

/* The first sign-in on this computer: the tutorial, offered once in a dialog. 1 to run it now. */
static unsigned char offer(void)
{
    gr_mode();
    ui_cls();
    ui_ink(F_BLACK, F_WHITE);
    ui_textn(0, 0, "ParlorNet", 32);
    ui_ink(F_WHITE, F_BLACK);
    strcpy(dlg_words, "Would you like a short tutorial on how the games work? You can run it again any time "
           "with /tutorial.");
    return dialog(dlg_words, D_FIRST) == D_YES;
}

#pragma rodata-name (push, "OVHDR")
#if !MACHINE_SERIAL
const struct signin signin_header = {run, offer};
#else
const struct signin signin_header = {run, offer, hangup, console};
#endif
#pragma rodata-name (pop)
