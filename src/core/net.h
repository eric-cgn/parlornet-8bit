/* ParlorNet: PARLORNET/1 over the serial line (docs/protocol.md). */
#ifndef NET_H
#define NET_H

/* The reply to the last request: its data lines, then the final line (OK, ERR or ASK). */
#define MAXL 96
extern unsigned char r_n;               /* data lines */
extern char *r_line[MAXL];
extern char *r_final;                   /* 0 when no final line came */
extern unsigned char net_lost;          /* the connection went away (or the session ended) */
extern unsigned char net_again;         /* requests sent again after a broken reply, counted (for the labs) */
extern unsigned char net_unsure;        /* the last request changes something and its reply came broken: it
                                           may have taken, so read the table or the list again */

enum { R_NONE, R_OK, R_ERR, R_ASK };
unsigned char net_req(void);            /* send q (below), read the reply */
unsigned char net_reply(unsigned int secs);  /* read a reply without sending */
unsigned char net_line(unsigned int ticks); /* one line into the reply; 1 if one came */
void net_reset(void);                   /* empty the reply buffer */

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
