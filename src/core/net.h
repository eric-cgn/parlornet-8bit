/* ParlorNet: PARLORNET/1 over the serial line (docs/protocol.md). */
#ifndef NET_H
#define NET_H

/* The reply to a request comes a line at a time. A page of it is read whole into the reply buffer, and then
   each data line in turn is copied to r_text (80 characters at most) and handed to r_each, when it is set;
   r_n counts the lines handed out so far, so a handler starts afresh when r_n is 0 (a reply asked for again
   after a broken one starts over). Whatever a handler keeps it copies into its own variables, since r_text
   is the next line's soon after. A reply too long for the buffer comes in pages, each ended by CNT, and
   the next page is asked for (CNT) once this one's lines are handed out. The final line (OK, ERR or ASK)
   ends the reply. After it, r_text holds the last data line, which is enough for a reply of one line.
   net_req sets r_each back to 0. */
extern char r_text[81];
extern unsigned char r_n;               /* data lines handed out */
extern void (*r_each)(void);            /* called for each data line, or 0 */
extern unsigned char r_stop;            /* r_each sets it to take no more lines: the rest of the reply is
                                           dropped, and the next request abandons what the parlor has left */
extern unsigned char r_more;            /* the reply stopped (r_stop, net_keep) with pages left: net_more */
extern char *r_final;                   /* the final line, or 0 when none came */
extern unsigned char net_lost;          /* the connection went away (or the session ended) */
extern unsigned char net_again;         /* requests sent again after a broken reply, counted (for the labs) */
extern unsigned char net_unsure;        /* the last request changes something and its reply came broken: it
                                           may have taken, so read the table or the list again */

enum { R_NONE, R_OK, R_ERR, R_ASK };
unsigned char net_req(void);            /* send q (below), read the reply */
unsigned char net_more(void);           /* after r_more: the reply's next page, handed out as before */
unsigned char net_reply(unsigned int secs);  /* read a reply without sending (a modem's words) */
unsigned char net_line(unsigned int ticks); /* one line, into r_text or r_final; 1 if one came */
char *__fastcall__ net_keep(const char *s);  /* keep s (r_text, or the end of it) in the reply buffer, which
                                           then takes no more of the reply (r_more says if more is left);
                                           it stays until the next request */
void net_paged(void);                   /* PAG: the parlor sends what fits the reply buffer at a time */
char *net_rbuf(void);                   /* the reply buffer, RBUF bytes from a page's start, which the help
                                           pages borrow */
#ifndef RBUF
#define RBUF 1024                       /* (a machine may set its own in machdefs.h) */
#endif
#define PAG_BYTES (RBUF - 100)          /* a page's bytes, with room for an echoed request and the final line */

/* Lines of text for pages (frame.h): the help's, or a reply's that net_text keeps. */
#define PG_LINES 48
extern char *pg_line[PG_LINES];
extern unsigned char pg_n;
void net_text(void);                    /* an r_each that keeps the reply's lines (net_keep) in pg_line and
                                           pg_n, an INF line's text without the INF, until the reply buffer
                                           holds no more (r_more); a page's worth with net_more */

/* The request being built. */
extern char q[84];
void __fastcall__ q_set(const char *s);
void __fastcall__ q_add(const char *s);      /* a space, then s */
void __fastcall__ q_num(unsigned int n);     /* a space, then n */
void __fastcall__ q_cat(const char *s);      /* s, no space */

/* Splitting a line into words (in place). */
#define MAXW 40
extern char *w[MAXW];
extern unsigned char wn;
unsigned char __fastcall__ words(char *line);
unsigned char __fastcall__ is(const char *word, const char *s);   /* word equals s */
unsigned int __fastcall__ num(const char *s);
int __fastcall__ snum(const char *s);
char *__fastcall__ utoa10(unsigned int n);    /* a static buffer */
char *__fastcall__ err_text(char *final);     /* the text of an ERR line, after code and wait= */

void net_send_line(const char *s);
void net_opening(void);                       /* PARLORNET/1, the public port's first line */

#endif
