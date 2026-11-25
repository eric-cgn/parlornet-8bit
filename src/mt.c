/* ParlorNet for the Apple IIgs: the connection to the parlor through Marinetti, the IIgs TCP/IP stack.

   Marinetti is tool set 54, which the player installs; the program loads it through the Tool Locator
   and calls it with call numbers and parameters from the Marinetti 3.0 Programmers' Guide. Nothing of
   Marinetti's is part of this program.

   This file has the Uthernet II driver's interface (eth.h), so the serial calls (sys.s) and everything
   above them work the same: eth_tcp opens the connection, eth_get, eth_put and eth_drop carry it, and
   when the parlor closes it the reader hands back "NO CARRIER" once, as a modem would. The settings that
   belong to the Uthernet II are kept too, so PARLOR.CFG has the same form on both machines.

   Nothing runs Marinetti's own task queue while the program has the machine (there is no Event Manager),
   so the program calls TCPIPPoll: while it waits for data, and once a tick while connected (sys.s). */
#include <string.h>
#include "a2.h"
#include "eth.h"
#include "gs.h"

unsigned char eth_on;
unsigned char cfg_eslot = 1;
unsigned char cfg_ip[4] = {192, 168, 1, 50}, cfg_mask[4] = {255, 255, 255, 0},
    cfg_gw[4] = {192, 168, 1, 1}, cfg_dns[4] = {192, 168, 1, 1};
char cfg_host[32] = "parlornet.org";
unsigned int cfg_port = 6501;

#define MT_START    0x0236
#define MT_SHUTDOWN 0x0336
#define MT_STATUS   0x0936              /* TCPIPGetConnectStatus */
#define MT_CONNECT  0x1236
#define MT_DNR      0x2136              /* TCPIPDNRNameToIP */
#define MT_POLL     0x2236
#define MT_LOGIN    0x2336
#define MT_LOGOUT   0x2436
#define MT_OPEN     0x2C36              /* TCPIPOpenTCP */
#define MT_WRITE    0x2D36
#define MT_READ     0x2E36
#define MT_CLOSE    0x2F36
#define MT_ABORT    0x3036
#define MT_TCPSTAT  0x3136              /* TCPIPStatusTCP */
#define LOADONETOOL 0x0F01

#define S_CLOSED      0
#define S_ESTABLISHED 4
#define S_CLOSEWAIT   7
#define S_TIMEWAIT    10

static unsigned char started;
static unsigned int ipid;
static unsigned char sr[22];            /* TCPIPStatusTCP's answer: state, error, queued out, queued in, ... */
static unsigned char rr[14];            /* TCPIPReadTCP's: the count, ... */
static unsigned char rbuf[128], rlen, rpos, closed_said;
static unsigned char tbuf[96], tlen;

#define SR_STATE (*(unsigned int *)sr)
#define SR_RCVQ  (*(unsigned int *)(sr + 8))
#define SR_RCVQH (*(unsigned int *)(sr + 10))

static unsigned int call0(unsigned int fn)
{
    tl_nin = 0;
    tl_nout = 0;
    return tl_call(fn);
}

static void poll(void)
{
    call0(MT_POLL);
}

/* TCPIPStatusTCP into sr; the state. */
static unsigned int tcp_state(void)
{
    tl_nin = 3;
    tl_nout = 1;
    tl_in[0] = ipid;
    TL_PTR(1, sr);
    tl_call(MT_TCPSTAT);
    return SR_STATE;
}

/* A call on the connection that takes only the ipid and gives a word. */
static unsigned int ipid_call(unsigned int fn)
{
    tl_nin = 1;
    tl_nout = fn == MT_LOGOUT ? 0 : 1;
    tl_in[0] = ipid;
    return tl_call(fn);
}

unsigned int mt_up(void)
{
    unsigned int e;
    if (!started) {
        tl_nin = 2;
        tl_nout = 0;
        tl_in[0] = 54;
        tl_in[1] = 0x0200;
        e = tl_call(LOADONETOOL);
        if (e)
            return e;
        e = call0(MT_START);
        if (e)
            return e;
        started = 1;
    }
    tl_nin = 0;
    tl_nout = 1;
    tl_call(MT_STATUS);
    if (tl_out[0])
        return 0;
    /* not connected to the network: connect, as set in the TCP/IP control panel */
    tl_nin = 2;
    tl_nout = 0;
    tl_in[0] = 0;
    tl_in[1] = 0;
    e = tl_call(MT_CONNECT);
    if (e == 0x3606)                    /* connected already */
        e = 0;
    return e;
}

void mt_end(void)
{
    if (eth_on)
        eth_close();
    if (started) {
        call0(MT_SHUTDOWN);
        started = 0;
    }
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

/* Wait n ticks, letting Marinetti work. */
static void pause(unsigned int n)
{
    unsigned int t0 = ticks;
    while (ticks - t0 < n) {
        poll();
        tick_poll();
    }
}

/* The name's address through Marinetti's resolver (DNS servers from the TCP/IP control panel), or the
   dotted address itself. Asked up to three times, a second apart, as a reply can be lost on a slow
   serial line. 1 when found. */
unsigned char mt_dns(const char *name, unsigned char *ip)
{
    static char pname[33];
    static unsigned char dnr[6];        /* the status, then the address */
    unsigned int t0;
    unsigned char tries;
    if (ip_parse(name, ip))
        return 1;
    pname[0] = strlen(name);
    memcpy(pname + 1, name, pname[0]);
    for (tries = 0; tries < 3; ++tries) {
        if (tries)
            pause(60);
        memset(dnr, 0, sizeof dnr);
        tl_nin = 4;
        tl_nout = 0;
        TL_PTR(0, pname);
        TL_PTR(2, dnr);
        if (tl_call(MT_DNR))
            continue;
        t0 = ticks;
        while (dnr[0] == 0 && ticks - t0 < 20 * 60) {
            poll();
            tick_poll();
        }
        if (dnr[0] == 1) {
            memcpy(ip, dnr + 2, 4);
            return 1;
        }
    }
    return 0;
}

static void flush(void)
{
    if (tlen) {
        tl_nin = 7;
        tl_nout = 1;
        tl_in[0] = ipid;
        TL_PTR(1, tbuf);
        tl_in[3] = 0;
        tl_in[4] = tlen;
        tl_in[5] = 1;                   /* push */
        tl_in[6] = 0;                   /* not urgent */
        tl_call(MT_WRITE);
        tlen = 0;
    }
}

/* Read what has come in (up to the buffer) into rbuf; the bytes read. */
static unsigned char take(void)
{
    unsigned int n;
    poll();
    tcp_state();
    if (!SR_RCVQ && !SR_RCVQH)
        return 0;
    n = SR_RCVQH || SR_RCVQ > sizeof rbuf ? sizeof rbuf : SR_RCVQ;
    tl_nin = 8;
    tl_nout = 1;
    tl_in[0] = ipid;
    tl_in[1] = 0;                       /* into our buffer */
    TL_PTR(2, rbuf);
    tl_in[4] = 0;
    tl_in[5] = n;
    TL_PTR(6, rr);
    memset(rr, 0, sizeof rr);
    tl_call(MT_READ);
    rpos = 0;
    rlen = rr[0];
    return rlen;
}

int eth_get(void)
{
    if (!eth_on)
        return -1;
    if (rpos < rlen)
        return rbuf[rpos++];
    flush();
    if (take())
        return rbuf[rpos++];
    if (SR_STATE != S_ESTABLISHED && !closed_said) {
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
    if (!eth_on)
        return;
    tbuf[tlen++] = c;
    if (c == 10 || tlen == sizeof tbuf)
        flush();
}

void eth_drop(void)
{
    rlen = rpos = 0;
    if (!eth_on)
        return;
    while (take())
        ;
    rlen = rpos = 0;
}

/* Open a TCP connection to ip:port. 1 when connected; 0 if refused or no answer within twenty
   seconds. */
unsigned char eth_tcp(const unsigned char *ip, unsigned int port)
{
    unsigned int t0, st;
    if (eth_on)
        eth_close();
    tl_nin = 6;
    tl_nout = 1;
    tl_in[0] = gs_uid;
    tl_in[1] = ip[2] | (ip[3] << 8);    /* the address as it is in memory, a.b.c.d */
    tl_in[2] = ip[0] | (ip[1] << 8);
    tl_in[3] = port;
    tl_in[4] = 0;                       /* type of service */
    tl_in[5] = 64;                      /* time to live */
    if (tl_call(MT_LOGIN))
        return 0;
    ipid = tl_out[0];
    if (ipid_call(MT_OPEN) || tl_out[0]) {
        ipid_call(MT_LOGOUT);
        return 0;
    }
    t0 = ticks;
    for (;;) {
        poll();
        tick_poll();
        st = tcp_state();
        if (st == S_ESTABLISHED)
            break;
        if (st == S_CLOSED || st >= S_CLOSEWAIT || ticks - t0 > 20 * 60) {
            ipid_call(MT_ABORT);
            ipid_call(MT_LOGOUT);
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
    unsigned int t0, st;
    if (!eth_on)
        return;
    flush();
    eth_on = 0;
    ipid_call(MT_CLOSE);
    t0 = ticks;
    do {
        poll();
        tick_poll();
        st = tcp_state();
    } while (st != S_CLOSED && st != S_TIMEWAIT && ticks - t0 < 3 * 60);
    if (st != S_CLOSED && st != S_TIMEWAIT)
        ipid_call(MT_ABORT);
    ipid_call(MT_LOGOUT);
}

/* The Uthernet II's other calls, which the IIgs program does not make. */
unsigned char eth_init(void)
{
    return 1;
}
