/* ParlorNet on the Apple IIe and IIgs: the sign-in's network connection (eth_connect, eth.h), linked into
   the sign-in overlay with core/parlor.c. On the IIe: the Uthernet II, with a DNS lookup over its socket 1; on
   the IIgs: Marinetti. */
#include <string.h>
#include "machine.h"
#include "apple2.h"
#include "net.h"
#include "app.h"
#include "eth.h"
#ifdef IIGS
#include "gs.h"
#endif

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
unsigned char eth_connect(void)
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
unsigned char eth_connect(void)
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
