/* ParlorNet for the Apple IIe: PARLORNET/1 over the serial line (net.h).
   The client sends CR LF as its line ending, from its opening line on, so the server ends its lines with CR LF too; a line is read
   up to either byte, and empty lines are skipped. The server sends only in reply to a request, so a
   reply is read whole into rbuf before anything is drawn. */
#include <string.h>
#include "a2.h"
#include "net.h"

#define RBUF 3328
#pragma bss-name (push, "LOWBSS")
static char rbuf[RBUF];
#pragma bss-name (pop)

unsigned char r_n;
char *r_line[MAXL];
char *r_final;
unsigned char net_lost;
char q[84];
char *w[MAXW];
unsigned char wn;

extern char *sr_buf;
extern unsigned int sr_max, sr_ticks;
extern unsigned char sr_any, sr_final;
unsigned int ser_reply(void);

void net_reset(void)
{
    r_n = 0;
    r_final = 0;
}

void net_send_line(const char *s)
{
    while (*s)
        ser_put(*s++);
    ser_put(13);
    ser_put(10);
}

void net_opening(void)
{
    net_send_line("PARLORNET/1");
}

static unsigned char final_kind(const char *s)
{
    if (s[0] == 'O' && s[1] == 'K' && (s[2] == 0 || s[2] == ' '))
        return R_OK;
    if (s[0] == 'E' && s[1] == 'R' && s[2] == 'R' && (s[3] == 0 || s[3] == ' '))
        return R_ERR;
    if (s[0] == 'A' && s[1] == 'S' && s[2] == 'K' && s[3] == ' ')
        return R_ASK;
    return R_NONE;
}

/* The reply in rbuf (lines ended by 0, from ser_reply) -> r_line and r_final. */
static void split(unsigned int n)
{
    char *p = rbuf, *end = rbuf + n;
    unsigned char echo = 1;
    rbuf[n] = 0;
    net_reset();
    while (p < end) {
        if (final_kind(p) != R_NONE || strncmp(p, "NO CARRIER", 10) == 0)
            r_final = p;
        else if (echo && !r_n && strcmp(p, q) == 0)
            echo = 0;                   /* our own request, echoed by a terminal line in between; once, before
                                           the reply, since a reply line can read the same ("MOTD", a blank line) */
        else if (r_n < MAXL)
            r_line[r_n++] = p;
        p += strlen(p) + 1;
    }
    /* A reply longer than the buffer (LST from a parlor with many tables) lost its end, the final line
       among it. When that line was OK, the reply stands without what did not fit: its last line, cut
       short, is dropped, and r_final is OK. Otherwise the request would count as a lost connection. */
    if (!r_final && sr_final == 'O' && n >= RBUF - 1) {
        if (r_n)
            --r_n;
        r_final = (char *)"OK";
    }
}

/* Read one line, waiting up to t ticks. 1 if a line came. */
unsigned char net_line(unsigned int t)
{
    unsigned int n;
    sr_buf = rbuf;
    sr_max = RBUF;
    sr_ticks = t;
    sr_any = 1;
    n = ser_reply();
    split(n);
    return n != 0;
}

unsigned char net_reply(unsigned int secs)
{
    unsigned char k;
    sr_buf = rbuf;
    sr_max = RBUF;
    sr_ticks = secs * 60;
    sr_any = 0;
    split(ser_reply());
    if (!sr_final || !r_final || strncmp(r_final, "NO CARRIER", 10) == 0) {
        net_lost = 1;
        r_final = 0;
        return R_NONE;
    }
    k = final_kind(r_final);
    if (k == R_ERR && (strncmp(r_final, "ERR SIGNEDOUT", 13) == 0 || strncmp(r_final, "ERR IDLE", 8) == 0
                       || strncmp(r_final, "ERR TOOLONG", 11) == 0))
        net_lost = 1;
    return k;
}

unsigned char net_req(void)
{
    if (net_lost)
        return R_NONE;
    ser_drop();
    net_send_line(q);
    return net_reply(30);
}

void __fastcall__ q_set(const char *s)
{
    strcpy(q, s);
}

void __fastcall__ q_cat(const char *s)
{
    unsigned char n = strlen(q);
    while (*s && n < 78)
        q[n++] = *s++;
    q[n] = 0;
}

void __fastcall__ q_add(const char *s)
{
    q_cat(" ");
    q_cat(s);
}

void __fastcall__ q_num(unsigned int n)
{
    q_add(utoa10(n));
}

char *__fastcall__ utoa10(unsigned int n)
{
    static char b[6];
    unsigned char i = 5;
    b[5] = 0;
    do {
        b[--i] = '0' + n % 10;
        n /= 10;
    } while (n);
    return b + i;
}

unsigned char __fastcall__ words(char *s)
{
    wn = 0;
    while (*s && wn < MAXW) {
        while (*s == ' ')
            ++s;
        if (!*s)
            break;
        w[wn++] = s;
        while (*s && *s != ' ')
            ++s;
        if (*s)
            *s++ = 0;
    }
    return wn;
}

unsigned char __fastcall__ is(const char *word, const char *s)
{
    return strcmp(word, s) == 0;
}

unsigned int __fastcall__ num(const char *s)
{
    unsigned int n = 0;
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (*s++ - '0');
    return n;
}

int __fastcall__ snum(const char *s)
{
    if (*s == '-')
        return -(int)num(s + 1);
    if (*s == '+')
        ++s;
    return num(s);
}

/* "ERR CODE [wait=N] text" -> "text" (or the code when there is no text). */
char *__fastcall__ err_text(char *f)
{
    char *s = f + 4, *code = s;
    while (*s && *s != ' ')
        ++s;
    if (!*s)
        return code;
    ++s;
    if (strncmp(s, "wait=", 5) == 0) {
        while (*s && *s != ' ')
            ++s;
        if (*s)
            ++s;
    }
    return *s ? s : code;
}
