/* ParlorNet: the table screen (table.c) and what each game gives it. */
#ifndef GAME_H
#define GAME_H

/* A game: the view lines it reads and how it draws and plays. */
struct game {
    const char *type;                   /* "reversi" */
    const char *name;                   /* "Reversi" */
    void (*begin)(void);                /* a view is coming: forget the last one's lines */
    void (*line)(void);                 /* one view line, in w[] and wn (net.h) */
    void (*draw)(void);                 /* draw the table; everything when g_full */
    void (*click)(unsigned char id);    /* a target of the game's was clicked */
    unsigned char (*key)(unsigned char k);  /* a key; 1 if the game took it */
    const char *help;                   /* the game's keys, for Keys (a second press), or 0 */
    unsigned char no_m;                 /* 1: the view is fetched without its M lines (too many) */
    void (*event)(void);                /* an EVT line, in w[] (w[3] its kind), before the view is
                                           fetched again; or 0 */
    /* The pointer rests on a target (255: none) on the player's turn: the second sentence of the sentence
       line, what Return does there and the shortcuts that apply, with g_message; or 0. */
    void (*hover)(unsigned char id);
    /* An offer waiting for the player's answer (a double, a draw offer, an offer to resign), or 0. With 0,
       whether one waits: its dialog's words in dlg_words (frame.h) and the dialog's flags (D_OFFER, and
       D_FIRST to start on Accept), or 0 for none. With 1 or 2, the answer: Accept or Decline. */
    unsigned char (*offer)(unsigned char answer);
    /* The first sentence of the sentence line when the game has its own ("Your shot.", "You must
       jump."), or 0 for the table's ("Your turn.", "Waiting for ann."); or 0 for none. */
    const char *(*say)(void);
};

/* The table, as the last view and the lobby list say. */
extern unsigned int g_table;
extern signed char g_seat;              /* my seat, or -1 when watching */
extern unsigned char g_seats;           /* seats at the table */
extern char g_names[8][13];
extern signed char g_turn;              /* W: whose move, -1 when over */
extern unsigned char g_over;
extern unsigned char g_full;            /* draw everything */
extern unsigned char g_started;
extern unsigned char g_myturn;
extern char g_status[42];
extern char g_rules[13];
extern int g_score[8];                  /* T lines */
extern unsigned char g_bot;             /* bit per seat held by the bot */

/* Moves from the view's M lines, each a string. */
#define MAXM 64
extern unsigned char m_n;
extern char *m_tok[MAXM];
unsigned char __fastcall__ m_has(const char *tok);

/* Play: send MOV with these tokens (and so on); the view is fetched again after. 1 if accepted. */
unsigned char __fastcall__ g_move(const char *tokens);
unsigned char __fastcall__ g_chk(const char *tokens);   /* CHK; the reply's OK text in g_chkinfo */
extern char g_chkinfo[42];
unsigned char g_end(void);
unsigned char g_undo(void);
void __fastcall__ g_cmd(const char *line);  /* a command line run as if typed, such as "/pass" */
extern unsigned char g_reply_more;      /* the last MOV said OK more (staged) */
extern unsigned char g_reply_done;      /* ... OK done */

/* The screen's frame, from the machine (machdefs.h): a title line at the top, then the game's area (from
   line GAME_TOP), a message line (MSG_Y), the status line (STATUS_Y) and the buttons (BUTTON_Y). */
void g_message(const char *s);          /* until the next key; "" for the second sentence (hover) */
/* The other player's move in a sentence, made by the game's event as it comes ("ann placed a disc on d3
   and flipped 2."): g_who(seat) starts it with the player's name and returns it to add to. */
extern char g_last[48];
char *__fastcall__ g_who(unsigned char seat);
void g_redraw(void);                    /* clear the screen and draw it all again */
void g_title(void);
const char *__fastcall__ g_name(unsigned char seat);   /* the seat's player, or "bot" */

/* Ids of the frame's buttons; games use ids below 200. */
#define B_LOBBY 250
#define B_RESIGN 249
#define B_CHAT 248
#define B_HELP 247

#endif
