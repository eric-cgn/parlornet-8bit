/* ParlorNet for the Apple IIe: ParlorNet mode's start. The opening line (PARLORNET/1, which sets the line
   ending), the greeting, and signing in or up as the server asks (docs/accounts.md): every INF line is shown, every
   ASK is a prompt with the server's label, so the email sign-up, the temporary password and the new
   password after it all work without the client knowing which method it is. On the serial line, P first
   dials the modem: ATZ, the init string, ATDT and the number, and on CONNECT the opening at once; the
   dialer hangs the line up when the session ends. */
#include <string.h>
#include "a2.h"
#include "ui.h"
#include "net.h"
#include "app.h"
#include "ovl.h"
#include "eth.h"
#ifdef IIGS
#include "gs.h"
#define CONSOLE_KEYS ""                 /* the IIgs program has no console */
#define NAME_KEYS "(" OA "Q: disconnect)"
#define NETWORKED 1                     /* always through Marinetti */
#define DIALING 0
#else
#define CONSOLE_KEYS OA "C: Console"
#define NAME_KEYS OA "C: the console  " OA "Q: disconnect"
#define NETWORKED cfg_conn
#define DIALING dialing
#endif

static unsigned char auth_none;

/* The connected time on the title row, after "ParlorNet", when the second changed (clk_text). */
static void txt_clock(void)
{
    unsigned char x, y, inv, top;
    const char *p;
    if (!connected || clk_shown == clk_t[0])
        return;
    x = txt_x;
    y = txt_y;
    inv = txt_inv;
    top = txt_top;
    for (p = clk_text(); *p == ' '; ++p)
        ;
    txt_top = 0;
    txt_goto(11, 0);
    txt_inv = 1;
    say(p);
    txt_top = top;
    txt_inv = inv;
    txt_goto(x, y);
}

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
        do {
            txt_clock();
            k = kbd_get();
        } while (!k);
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
#ifndef IIGS
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
#ifdef IIGS
    sayln("Return: try again");
    sayln("Esc: the start screen");
#else
    say(DIALING ? "Return: dial again" : "Return: try again");
    sayln("   Open-Apple-C:");
    sayln("the console   Esc: the start screen");
#endif
    for (;;) {
        k = kbd_get();
        if (k == K_RET)
            return 1;
        if (k == K_ESC)
            return 0;
#ifndef IIGS
        if ((k | 0x20) == 'c')          /* with Open-Apple or without (key_out) */
            return 2;
#endif
    }
}

#ifndef IIGS
/* The dialer. What the modem says is read a burst at a time, until a second of quiet (net_reply), and
   nothing is drawn while it may be coming: without the serial card's interrupt only the read loop takes
   the bytes in time. The modem's echo of a command (the command, or any line that starts with AT) is
   skipped. */
extern unsigned char sr_final;          /* (sys.s) the first letter of a final line: N for NO CARRIER */
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

/* The modem's answer within a second's quiet: its first line that is not an echo, or 0. With keys set,
   Escape or C first stops the wait (cancel). */
static char *modem_read(unsigned char keys)
{
    unsigned char i;
    char *l;
    if (keys && (cancel = key_out()) != 0)
        return 0;
    net_reply(1);
    net_lost = 0;
    for (i = 0; i < r_n; ++i) {
        l = r_line[i];
        if (((l[0] | 0x20) != 'a' || (l[1] | 0x20) != 't') && strcmp(l, sent))
            return l;
    }
    if (sr_final == 'N')
        return "NO CARRIER";            /* (net_reply takes it as the end of the connection) */
    return r_final;
}

/* A command before the dial, shown with the modem's OK, waiting up to three seconds; a line that says
   nothing (pndial, say, until it has started) dials on all the same. 0 on ERROR (the reason in q) or a key
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

/* Reset the modem (ATZ, when the settings say so), send the init string, dial ATDT and the number, and
   wait up to a minute for CONNECT. 1 on CONNECT; 0 with the reason in q; 2 for the console and 3 for
   Escape, after a Return that stops a dial. */
static unsigned char dial(void)
{
    unsigned int start;
    char *l;
    sayln("Dialing  (Esc: cancel)");
    txt_putc(13);
    cancel = 0;
    if ((cfg_reset && !modem_ok("ATZ")) || (cfg_init[0] && !modem_ok(cfg_init))) {
        if (cancel)
            goto stop;
        return 0;
    }
    /* ATDT and the number; a number that starts with AT is the whole command (ATD"host:port" for a
       modem whose ATDT means telnet) */
    q_set((cfg_number[0] | 0x20) == 'a' && (cfg_number[1] | 0x20) == 't' ? "" : "ATDT");
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
    q_set("No CONNECT from the modem in a minute.");
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
    clk_shown = 255;
    txt_clock();
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

#ifndef IIGS
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

/* A name's address from the DNS server in the settings, over UDP on the card's socket 1 (a dotted
   address needs no lookup). 1 when found. */
static unsigned char dns(const char *name, unsigned char *ip)
{
    static unsigned char m[300];
    unsigned char n, tries, *p, *e, i;
    unsigned int len, t0, k, an;
    const char *s;
    if (ip_parse(name, ip))
        return 1;
    /* the question: id, recursion wanted, one question; the name as labels; type A, class IN */
    memset(m, 0, 12);
    m[0] = 0x50;
    m[1] = 0x4E;
    m[2] = 0x01;
    m[5] = 1;
    n = 12;
    for (s = name; *s;) {
        i = n++;
        while (*s && *s != '.' && n < 250)
            m[n++] = *s++;
        m[i] = n - i - 1;
        if (*s == '.')
            ++s;
    }
    m[n++] = 0;
    m[n++] = 0;
    m[n++] = 1;
    m[n++] = 0;
    m[n++] = 1;
    for (tries = 0; tries < 3; ++tries) {
        w_cmd(W_S1, 0x10);
        w_setreg(W_S1, 0x02);           /* UDP */
        w_set16(W_S1 + 0x04, 0xC100 | (ticks & 0xFF));
        w_cmd(W_S1, 0x01);
        if (w_reg(W_S1 + 0x03) != 0x22)
            return 0;
        w_at(W_S1 + 0x0C);
        for (i = 0; i < 4; ++i)
            w_put(cfg_dns[i]);
        w_set16(W_S1 + 0x10, 53);
        w_write(W_S1, m, n);
        t0 = ticks;
        while (!w_rsr(W_S1) && ticks - t0 < 180)
            tick_poll();
        if (!w_rsr(W_S1))
            continue;
        /* a UDP packet: the sender (4 bytes), its port (2), the length (2), then the data */
        w_read(W_S1, m, 8);
        len = (m[6] << 8) | m[7];
        k = len > sizeof m ? sizeof m : len;
        w_read(W_S1, m, k);
        if (len > k) {
            /* the rest is skipped */
            w_set16(W_S1 + 0x28, w_get16(W_S1 + 0x28) + len - k);
        }
        w_cmd(W_S1, 0x40);
        w_cmd(W_S1, 0x10);
        if (m[0] != 0x50 || m[1] != 0x4E || (m[3] & 0x0F))
            return 0;
        an = (m[6] << 8) | m[7];
        p = m + 12;
        e = m + k;
        /* past the question */
        while (p < e && *p)
            p += (*p & 0xC0) ? 1 : *p + 1;
        p += (*p & 0xC0) ? 2 : 1;
        p += 4;
        while (an-- && p + 12 <= e) {
            if ((*p & 0xC0) == 0xC0)
                p += 2;
            else {
                while (p < e && *p)
                    p += *p + 1;
                ++p;
            }
            len = (p[8] << 8) | p[9];
            if (p[1] == 1 && p[3] == 1 && len == 4) {
                memcpy(ip, p + 10, 4);
                return 1;
            }
            p += 10 + len;
        }
        return 0;
    }
    w_cmd(W_S1, 0x10);
    return 0;
}

/* Over the Uthernet II: the card, the parlor's address, the connection. 1 when connected. */
static unsigned char eth_connect(void)
{
    static unsigned char ip[4];
    if (!eth_init()) {
        q_set("No Uthernet II answers in slot ");
        q_cat(utoa10(cfg_eslot));
        q_cat(" (settings: S).");
        return 0;
    }
    say("Looking up ");
    sayln(cfg_host);
    if (!dns(cfg_host, ip)) {
        q_set("No address for ");
        q_cat(cfg_host);
        q_cat(" from the DNS server (settings: S).");
        return 0;
    }
    say("Connecting to ");
    say(ip_text(ip));
    say(" port ");
    sayln(utoa10(cfg_port));
    if (!eth_tcp(ip, cfg_port)) {
        q_set("No connection to ");
        q_cat(cfg_host);
        q_cat(". Check the addresses and port (S).");
        return 0;
    }
    return 1;
}

#else
/* A number in hex, after a dollar sign: "$3604". */
static const char *hex4(unsigned int v)
{
    static char b[6];
    unsigned char i, d;
    b[0] = '$';
    for (i = 0; i < 4; ++i) {
        d = (v >> (12 - 4 * i)) & 15;
        b[1 + i] = d < 10 ? '0' + d : 'A' - 10 + d;
    }
    b[5] = 0;
    return b;
}

/* Through Marinetti: the network as set up in the TCP/IP control panel, the parlor's address, the
   connection. 1 when connected. */
static unsigned char eth_connect(void)
{
    static unsigned char ip[4];
    unsigned int e;
    sayln("Starting Marinetti...");
    e = mt_up();
    if (e) {
        /* (q holds 83 characters) */
        q_set(e < 0x3600 ? "Marinetti did not load: error " : "Marinetti could not connect: error ");
        q_cat(hex4(e));
        q_cat(e < 0x3600 ? ". Is it installed?" : ". See the TCP/IP control panel.");
        return 0;
    }
    say("Looking up ");
    sayln(cfg_host);
    if (!mt_dns(cfg_host, ip)) {
        q_set("No address for ");
        q_cat(cfg_host);
        q_cat(". Check the name (S) and the DNS server.");
        return 0;
    }
    say("Connecting to ");
    say(ip_text(ip));
    say(" port ");
    sayln(utoa10(cfg_port));
    if (!eth_tcp(ip, cfg_port)) {
        q_set("No connection to ");
        q_cat(cfg_host);
        q_cat(". Check the name and port (S).");
        return 0;
    }
    return 1;
}
#endif

/* The sign-in overlay's entry: connect if the line has not been greeted yet, then sign in.
   Returns 0 for the start screen, 1 for the console, 3 to close the connection, 4 when signed in. */
static unsigned char run(void)
{
    static char name[14];
    unsigned char k, n, signed_in;
    const char *p;

#ifndef IIGS
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
#ifndef IIGS
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
        k = ask_again(k == 2 ? err_text(q) : "No answer from the parlor. Is the line connected to its "
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
    memset(clk_t, 0, 3);                /* the connected time from 0:00, or the parlor's */
    clk_tim_off = 0;
    clk_tim_at = 0xFF;                  /* (now) */
    clk_sync();
    clk_shown = 255;
    txt_clock();
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
        sayln("Your name, or new for a new account.");
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
        q_add(name);
        signed_in = dialogue();
        if (signed_in >= 2)
            return signed_in == 2 ? 1 : 3;
        if (net_lost)
            goto lost;
        if (!signed_in) {
            if (r_final && strncmp(r_final, "ERR STATE", 9) == 0)
                signed_in = 1;          /* signed in already, on this connection */
            else
                continue;
        }
        strcpy(me, name);
        prefs_load();
        return 4;
    }
lost:
    connected = 0;
#ifndef IIGS
    online = 1;                         /* a modem that has not noticed is hung up before the dial */
#endif
    k = ask_again("The connection to the parlor was lost.");
    if (k == 1)
        goto connect;
    return k == 2;
}

#pragma rodata-name (push, "OVHDR")
#ifdef IIGS
const struct signin signin_header = {run};
#else
const struct signin signin_header = {run, hangup};
#endif
#pragma rodata-name (pop)
