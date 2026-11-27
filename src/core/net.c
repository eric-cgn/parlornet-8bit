/* ParlorNet: PARLORNET/1 over the serial line (net.h).
   The client sends CR LF as its line ending, from its opening line on, so the server ends its lines with CR
   LF too; a line is read up to either byte, and empty lines are skipped. The server sends only in reply to a
   request, and after PAG (net_paged) at most PAG_BYTES of a reply at a time: a page, ended by CNT when more
   is left, which waits until the client asks for it with CNT. So a page is read whole into rbuf while it
   comes, and its lines are handed out (r_each) only after: without the serial card's interrupt only the
   read loop takes the bytes in time.

   A reply that comes but has no final line has lost a byte on the line (noise, a modem's buffer, an interrupt
   held off too long): the line itself is still there, since a line that has gone says NO CARRIER or
   nothing at all. net_req then waits for the line to be quiet and sends a request that changes nothing
   (SIN, LST, VUE, TIM and the like) again, up to twice, with nothing on the screen, and its lines are handed
   out again from the first (r_n 0); a request that changes something (MOV, CHT, NEW and the like) is not
   sent again, since it may have taken: net_unsure says so, and the caller reads the table or the list again
   to see. Such requests always fit one page. */
#include <string.h>
#include "machine.h"
#include "net.h"

/* The reply buffer, at the start of low memory (REPLY, a page's start, for the help pages), and the lines of
   text for pages. */
#pragma bss-name (push, "REPLY")
static char rbuf[RBUF];
#pragma bss-name (pop)
#pragma bss-name (push, "LOWBSS")
char *pg_line[PG_LINES];
#pragma bss-name (pop)
unsigned char pg_n;

char r_text[81];
unsigned char r_n, r_stop, r_more;
void (*r_each)(void);
char *r_final;
unsigned char net_lost;
unsigned char net_unsure;
unsigned char net_again;                /* requests sent again after a broken reply (the labs read it) */
char q[84];
char *w[MAXW];
unsigned char wn;

static char *kept;                      /* where net_keep puts the next line it keeps */
static unsigned char keeping;           /* net_keep kept a line: no more pages into rbuf */
static unsigned char in_page;           /* data lines in the page just handed out */
static const char *sent;                /* the line sent last, which a terminal line may echo */

char *net_rbuf(void)
{
    return rbuf;
}

void net_send_line(const char *s)
{
    sent = s;
    while (*s)
        ser_put(*s++);
    ser_put(13);
    ser_put(10);
}

void net_opening(void)
{
    net_send_line("PARLORNET/1");
}

/* The page in rbuf, n bytes of lines each ended by a 0 (from ser_reply): its data lines handed out. Its last
   line ends it when ser_reply found it a final line or CNT (sr_final); the final line stays in rbuf for
   r_final. Returns sr_final. The line sent last, echoed by a terminal line in between, is skipped once as
   the page's first line, since a reply line can read the same ("MOTD", a blank line) further on. */
static unsigned char __fastcall__ hand_out(unsigned int n)
{
    static char *p, *end;
    static unsigned char first, len;
    p = rbuf;
    end = rbuf + n;
    kept = rbuf;
    in_page = 0;
    first = 1;
    rbuf[n] = 0;
    for (; p < end; p += len + 1, first = 0) {
        len = strlen(p);
        if (sr_final && p + len + 1 >= end) {
            if (sr_final != 'C')
                r_final = p;
            break;
        }
        if ((first && strcmp(p, sent) == 0) || r_stop)
            continue;
        strcpy(r_text, p);
        ++in_page;
        if (r_each)
            r_each();
        ++r_n;
    }
    return sr_final;
}

static unsigned int got;                /* the bytes of the last page */
static unsigned int gap;                /* for the next reply, sr_gap; 0: as long as sr_ticks */

/* Pages read and handed out until the final line, a page that broke, or a stop with pages left (r_more).
   The final line's kind, or R_NONE with net_lost set when none came. */
static unsigned char __fastcall__ read_pages(unsigned int secs)
{
    unsigned char end, k;
    for (;;) {
        sr_buf = rbuf;
        sr_max = RBUF;
        sr_ticks = secs * 60;
        sr_gap = gap ? gap : sr_ticks;
        sr_any = 0;
        end = hand_out(got = ser_reply());
        if (end != 'C')
            break;
        if (!in_page && strcmp(sent, "CNT") == 0)
            continue;                   /* (our CNT, echoed: the page is still to come) */
        if (r_stop || keeping) {
            r_more = 1;
            gap = 0;
            return R_OK;
        }
        net_send_line("CNT");
    }
    gap = 0;
    if (!end || end == 'N') {           /* (no final line, or NO CARRIER) */
        net_lost = 1;
        r_final = 0;
        return R_NONE;
    }
    k = end == 'O' ? R_OK : end == 'E' ? R_ERR : R_ASK;
    if (k == R_ERR && (strncmp(r_final, "ERR SIGNEDOUT", 13) == 0 || strncmp(r_final, "ERR IDLE", 8) == 0
                       || strncmp(r_final, "ERR TOOLONG", 11) == 0))
        net_lost = 1;
    return k;
}

static void start(void)
{
    r_n = r_stop = r_more = keeping = 0;
    r_final = 0;
}

unsigned char net_reply(unsigned int secs)
{
    start();
    return read_pages(secs);
}

/* Read one line, waiting up to t ticks. 1 if a line came. */
unsigned char net_line(unsigned int t)
{
    start();
    sr_buf = rbuf;
    sr_max = RBUF;
    sr_ticks = sr_gap = t;
    sr_any = 1;
    hand_out(got = ser_reply());
    return got != 0;
}

char *net_keep(const char *s)
{
    char *k = kept;
    strcpy(kept, s);
    kept += strlen(s) + 1;
    keeping = 1;
    return k;
}

void net_text(void)
{
    const char *s = r_text;
    if (!r_n)
        pg_n = 0;
    if (strncmp(s, "INF", 3) == 0)
        s += s[3] ? 4 : 3;              /* "INF text" -> "text"; "INF" alone, an empty line */
    if (pg_n < PG_LINES)
        pg_line[pg_n++] = net_keep(s);
    else
        r_stop = 1;
}

unsigned char net_more(void)
{
    unsigned char k;
    if (!r_more)
        return R_OK;
    r_stop = r_more = keeping = 0;
    net_send_line("CNT");
    gap = 4 * 60;
    k = read_pages(30);
    if (k == R_NONE && got && sr_final != 'N')
        net_lost = 0;                   /* (the rest of the reply came broken, not the line) */
    return k;
}

void net_paged(void)
{
    q_set("PAG");
    q_num(PAG_BYTES);
    net_req();
}

#if MACHINE_SERIAL
/* The verbs, four characters each: the first AGAIN change nothing and are sent again after a broken reply;
   after the others, which change something, the caller looks again. The rest (signing in and out) count a
   broken reply as the line lost. */
static const char verbs[] = "PAG SIN LST VUE TIM MOTDPRL GAM CHK IVL WHO MOV END UND RES CHT NEW JON INV UNV PRF CMD ";
#define AGAIN 11

static unsigned char verb(void)
{
    static unsigned char i;
    for (i = 0; verbs[i]; i += 4)
        if (strncmp(verbs + i, q, 3) == 0 && (verbs[i + 3] == q[3] || verbs[i + 3] == ' '))
            return i / 4 + 1;
    return 0;
}
#endif

unsigned char net_req(void)
{
    static unsigned char k;
#if MACHINE_SERIAL
    static unsigned char n;
    net_unsure = 0;
    n = 3;
again:
#endif
    k = R_NONE;
    if (net_lost)
        goto done;
    start();
    ser_drop();
    net_send_line(q);
#if MACHINE_SERIAL
    gap = 4 * 60;                       /* 30 seconds for the parlor; 4 without a byte once it answers */
#endif
    k = read_pages(30);
#if MACHINE_SERIAL
    if (k != R_NONE || !got || sr_final == 'N')
        goto done;
    net_lost = 0;                       /* the reply came broken, not the line */
    sr_buf = rbuf;                      /* on until half a second of quiet */
    sr_max = RBUF;
    sr_any = 1;
    do
        sr_ticks = sr_gap = 30;
    while (ser_reply());
    k = verb();
    if (k && k <= AGAIN && --n) {
        ++net_again;
        goto again;
    }
    if (k > AGAIN)
        net_unsure = 1;
    else
        net_lost = 1;
    k = R_NONE;
#endif
done:
    r_each = 0;
    return k;
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
