/* ParlorNet for the Apple IIe: the Uthernet II card (a WIZnet W5100), our own driver (eth.h).

   The card is driven in the W5100's indirect bus mode: in slot n its four registers are at $C084+n*16:
   the mode register, the address (high, then low byte) and the data, with the address moving on by
   one after each data byte (w5100.s reaches them). Socket 0 carries the TCP connection to the parlor; socket 1 asks a DNS
   server for a name's address over UDP. Each socket has 2K of buffer each way.

   While the connection is up, the serial calls (ser_get, ser_put, ser_drop) come here instead (sys.s),
   so everything above them works the same over either. When the parlor closes the connection, the
   reader hands back "NO CARRIER" once, as a modem would, and the reply reader takes the line as lost. */
#include <string.h>
#include "a2.h"
#include "eth.h"

unsigned char eth_on;                   /* the serial calls come here (sys.s reads it) */
unsigned char cfg_eslot = 1;
unsigned char cfg_ip[4] = {192, 168, 1, 50}, cfg_mask[4] = {255, 255, 255, 0},
    cfg_gw[4] = {192, 168, 1, 1}, cfg_dns[4] = {192, 168, 1, 1};
char cfg_host[32] = "parlornet.org";
unsigned int cfg_port = 6501;

static unsigned char have_card;
static unsigned char rbuf[64], rlen, rpos, closed_said;
static unsigned char tbuf[96], tlen;

#define S0 0x0400
#define S1 0x0500
#define CR 0x01
#define IR 0x02
#define SR 0x03
#define TX_FSR 0x20
#define TX_WR 0x24
#define RX_RSR 0x26
#define RX_RD 0x28

unsigned int w_get16(unsigned int a)
{
    unsigned int v;
    w_at(a);
    v = w_get() << 8;
    return v | w_get();
}

void __fastcall__ w_set16(unsigned int a, unsigned int v)
{
    w_at(a);
    w_put(v >> 8);
    w_put(v);
}

unsigned char __fastcall__ w_reg(unsigned int a)
{
    w_at(a);
    return w_get();
}

void w_setreg(unsigned int a, unsigned char v)
{
    w_at(a);
    w_put(v);
}

/* A socket command, waited for until the chip has taken it. */
void w_cmd(unsigned int s, unsigned char c)
{
    unsigned int n = 0;
    w_setreg(s + CR, c);
    while (w_reg(s + CR) && ++n)
        ;
}

/* The card: reset, indirect mode with the address moving on, our addresses. 0 if there is none. */
unsigned char eth_init(void)
{
    unsigned char i;
    have_card = 0;
    w_slot(cfg_eslot);
    w_mode(0x80);
    w_mode(0x03);
    if (w_mode_get() != 0x03)
        return 0;
    w_at(0x0001);
    for (i = 0; i < 4; ++i)
        w_put(cfg_gw[i]);
    for (i = 0; i < 4; ++i)
        w_put(cfg_mask[i]);
    /* a locally administered address, the last bytes from our IP address */
    w_put(0x02);
    w_put(0xA2);
    for (i = 0; i < 4; ++i)
        w_put(cfg_ip[i]);
    for (i = 0; i < 4; ++i)
        w_put(cfg_ip[i]);
    w_setreg(0x001A, 0x55);             /* 2K each way for each socket */
    w_setreg(0x001B, 0x55);
    have_card = w_reg(0x000F) == cfg_ip[0];
    return have_card;
}

/* Bytes out of a socket's receive buffer (the caller sends RECV after). */
void w_read(unsigned int s, unsigned char *p, unsigned int n)
{
    unsigned int rd = w_get16(s + RX_RD), base = s == S0 ? 0x6000 : 0x6800, off, i;
    off = rd & 0x07FF;
    w_at(base + off);
    for (i = 0; i < n; ++i) {
        p[i] = w_get();
        if (++off == 0x0800) {
            off = 0;
            w_at(base);
        }
    }
    w_set16(s + RX_RD, rd + n);
}

/* Bytes into a socket's send buffer, then SEND. */
void w_write(unsigned int s, const unsigned char *p, unsigned int n)
{
    unsigned int wr = w_get16(s + TX_WR), base = s == S0 ? 0x4000 : 0x4800, off, i;
    while (w_get16(s + TX_FSR) < n)
        ;
    off = wr & 0x07FF;
    w_at(base + off);
    for (i = 0; i < n; ++i) {
        w_put(p[i]);
        if (++off == 0x0800) {
            off = 0;
            w_at(base);
        }
    }
    w_set16(s + TX_WR, wr + n);
    w_cmd(s, 0x20);
}

unsigned int w_rsr(unsigned int s)
{
    unsigned int a, b;
    /* read until two readings agree (the chip may change it between the bytes) */
    do {
        a = w_get16(s + RX_RSR);
        b = w_get16(s + RX_RSR);
    } while (a != b);
    return a;
}

static void flush(void)
{
    if (tlen) {
        w_write(S0, tbuf, tlen);
        tlen = 0;
    }
}

/* The serial calls, while eth_on. */
int eth_get(void)
{
    unsigned int n;
    unsigned char st;
    if (rpos < rlen)
        return rbuf[rpos++];
    flush();
    n = w_rsr(S0);
    if (n) {
        if (n > sizeof rbuf)
            n = sizeof rbuf;
        w_read(S0, rbuf, n);
        w_cmd(S0, 0x40);
        rlen = n;
        rpos = 1;
        return rbuf[0];
    }
    st = w_reg(S0 + SR);
    if (st != 0x17 && !closed_said) {
        /* the parlor closed the connection: say so as a modem would */
        static const char nc[] = "\r\nNO CARRIER\r\n";
        closed_said = 1;
        memcpy(rbuf, nc, sizeof nc - 1);
        rlen = sizeof nc - 1;
        rpos = 1;
        return rbuf[0];
    }
    return -1;
}

void __fastcall__ eth_put(unsigned char c)
{
    tbuf[tlen++] = c;
    if (c == 10 || tlen == sizeof tbuf)
        flush();
}

void eth_drop(void)
{
    unsigned int n;
    rlen = rpos = 0;
    while ((n = w_rsr(S0)) != 0) {
        if (n > sizeof rbuf)
            n = sizeof rbuf;
        w_read(S0, rbuf, n);
        w_cmd(S0, 0x40);
    }
    rlen = 0;
}

/* Open socket 0 to ip:port. 1 when connected; 0 if refused or no answer within about ten seconds. */
unsigned char eth_tcp(const unsigned char *ip, unsigned int port)
{
    unsigned int t0;
    unsigned char st, i;
    eth_on = 0;
    w_cmd(S0, 0x10);                    /* CLOSE, whatever it was */
    w_setreg(S0, 0x01);                 /* TCP */
    w_set16(S0 + 0x04, 0xC000 | (ticks & 0x0FFF));  /* our port */
    w_cmd(S0, 0x01);                    /* OPEN */
    if (w_reg(S0 + SR) != 0x13)
        return 0;
    w_at(S0 + 0x0C);
    for (i = 0; i < 4; ++i)
        w_put(ip[i]);
    w_set16(S0 + 0x10, port);
    w_cmd(S0, 0x04);                    /* CONNECT */
    t0 = ticks;
    for (;;) {
        tick_poll();
        st = w_reg(S0 + SR);
        if (st == 0x17)
            break;
        if (st == 0 || ticks - t0 > 600) {
            w_cmd(S0, 0x10);
            return 0;
        }
    }
    rlen = rpos = tlen = 0;
    closed_said = 0;
    eth_on = 1;
    return 1;
}

void eth_close(void)
{
    if (!have_card)
        return;
    flush();
    w_cmd(S0, 0x08);                    /* DISCON */
    w_cmd(S0, 0x10);                    /* CLOSE */
    eth_on = 0;
}
