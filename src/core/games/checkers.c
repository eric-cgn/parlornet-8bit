/* ParlorNet: Checkers (docs/types/checkers-1.md). The view's B line gives the 32 dark squares; the server
   lists every legal step (M lines). A step is the piece to move, then the square to move to; after a jump
   that must go on, the same piece stays chosen and the next square is clicked. A turn is staged: Done sends
   END, Undo takes a step back. The board is turned so that the player's own men start at the bottom (flip).

   The machine draws it (draw/checkers.c, included below: ck_draw and redraw), each dark square as sq_want
   says, and two buttons with the words panel_offer chooses; the dark squares are targets 1 to 32, the
   buttons ID_A and ID_B. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "game.h"
#include "frame.h"

#define ID_A 40                         /* the panel's two buttons */
#define ID_B 41

static char cells[33];                  /* the B line: '.', x, X, o, O per dark square 1-32 */
static unsigned char cnt[4];            /* men and kings: seat 0, then seat 1 */
static char phase[8];
static unsigned char path[16], npath;   /* J: the moving piece's path this turn */
static signed char drawer = -1;         /* D: a draw offer waits, from this seat */
static unsigned char sel, flip;
static const char *offer_a, *offer_b;   /* the panel's buttons ("" for none) */

static void ck_begin(void)
{
    phase[0] = 0;
    npath = 0;
    drawer = -1;
}

static void ck_line(void)
{
    unsigned char i;
    if (is(w[0], "B") && wn > 1) {
        strncpy(cells + 1, w[1], 32);
    } else if (is(w[0], "C") && wn > 4) {
        for (i = 0; i < 4; ++i)
            cnt[i] = num(w[i + 1]);
    } else if (is(w[0], "Q") && wn > 1) {
        strncpy(phase, w[1], 7);
    } else if (is(w[0], "J")) {
        npath = 0;
        for (i = 1; i < wn && npath < 16; ++i)
            path[npath++] = num(w[i]);
    } else if (is(w[0], "D") && wn > 1) {
        drawer = num(w[1]);
    }
}

static unsigned char from_of(const char *m)
{
    return num(m);
}

/* The square a step token ends on ("11-15", "15x22"). */
static unsigned char to_of(const char *m)
{
    while (*m >= '0' && *m <= '9')
        ++m;
    return *m ? num(m + 1) : 0;
}

static unsigned char can_go(unsigned char from, unsigned char to)
{
    unsigned char i;
    for (i = 0; i < m_n; ++i)
        if (from_of(m_tok[i]) == from && (!to || to_of(m_tok[i]) == to))
            return 1;
    return 0;
}

/* The panel's buttons, by the state of the turn: ending or undoing a staged turn, undoing a step, or offering
   a draw. A draw offered is answered in its dialog (ck_offer). */
static void panel_offer(void)
{
    offer_a = offer_b = "";
    if (g_myturn && !(drawer >= 0 && drawer != g_seat)) {
        if (strcmp(phase, "done") == 0) {
            offer_a = "End turn";
            offer_b = "Undo";
        } else if (npath)
            offer_b = "Undo";
        else
            offer_b = "Draw";
    }
}

/* Before a draw: the board's way round, and the chosen piece (mid-jump, the piece that must go on). */
static void board_state(void)
{
    flip = g_seat != 1;                 /* Black (seat 0) starts on 1-12, at the top unturned */
    if (npath && strcmp(phase, "move") == 0 && g_myturn)
        sel = path[npath - 1];
}

/* How dark square s should look: its piece ('.', x, X, o, O), 'h' where the chosen piece can go, with bit 7
   set on the chosen piece. */
static char sq_want(unsigned char s)
{
    char want = cells[s];
    if (want == '.' && sel && g_myturn && can_go(sel, s))
        want = 'h';
    if (s == sel)
        want |= 0x80;
    return want;
}

/* The machine's drawing: ck_draw and redraw. */
#include "draw/checkers.c"

static void ck_click(unsigned char id)
{
    static char tok[8];
    if (!g_myturn) {
        g_message("It's not your turn.");
        return;
    }
    if (id == ID_A || id == ID_B) {
        const char *word = id == ID_A ? offer_a : offer_b;
        if (!word[0])
            return;
        sel = 0;
        if (word[0] == 'E')
            g_end();
        else if (is(word, "Undo"))
            g_undo();
        else if (word[0] == 'D')
            g_move("draw");
        return;
    }
    if (strcmp(phase, "move") != 0)
        return;
    if (!sel || (id != sel && can_go(id, 0) && !npath)) {
        if (!can_go(id, 0)) {
            g_message("No piece of yours can move from there.");
            return;
        }
        sel = id;
        redraw();
        return;
    }
    if (id == sel) {
        if (!npath) {
            sel = 0;
            g_message("");
            redraw();
        }
        return;
    }
    if (!can_go(sel, id)) {
        g_message("That is not a legal move.");
        return;
    }
    strcpy(tok, utoa10(sel));
    strcat(tok, "-");
    strcat(tok, utoa10(id));
    sel = 0;
    g_move(tok);
}

/* What Return does on the square under the pointer. */
static void ck_hover(unsigned char id)
{
    static char b[42];
    const char *s;
    unsigned char n = id;
    if (strcmp(phase, "done") == 0)
        s = id == ID_A ? "Press " KEY_RET " to end your turn." : id == ID_B ? "Press " KEY_RET " to undo a step." :
            "Press " OA "E to end your turn or " OA "U to undo.";
    else if (id == ID_B && offer_b[0])
        s = offer_b[0] == 'D' ? "Press " KEY_RET " to offer a draw." : "Press " KEY_RET " to undo a step.";
    else if (sel && id != sel)
        s = id < ID_A && can_go(sel, id) ? "Press " KEY_RET " to move it to " : "Press " KEY_RET " on a marked square.";
    else if (sel)
        s = npath ? "You must jump on with this piece." : "Press " KEY_RET " on a marked square.";
    else if (id < ID_A && can_go(id, 0))
        s = cells[id] == 'X' || cells[id] == 'O' ? "Press " KEY_RET " to pick up the king on " :
            "Press " KEY_RET " to pick up the man on ";
    else
        s = "Pick up a piece that can move.";
    strcpy(b, s);
    if (b[strlen(b) - 1] == ' ') {
        strcat(b, utoa10(n));
        strcat(b, ".");
    }
    g_message(b);
}

/* The other player's move in words: "ann moved 24-19." or "ann jumped 15x22x31." */
static void ck_event(void)
{
    if (num(w[4]) == (unsigned char)g_seat || wn < 7)
        return;
    if (is(w[3], "STEP")) {
        strcat(g_who(num(w[4])), " moved ");
        strcat(g_last, w[5]);
        strcat(g_last, "-");
    } else if (is(w[3], "JUMP")) {
        if (!g_last[0]) {
            strcat(g_who(num(w[4])), " jumped ");
            strcat(g_last, w[5]);
        }
        strcat(g_last, "x");
    } else
        return;
    strcat(g_last, w[6]);
}

/* The first sentence: a jump that must go on, or a draw the player offered. */
static const char *ck_say(void)
{
    static char b[40];
    if (g_myturn && npath && strcmp(phase, "move") == 0)
        return "You must jump.";
    if (g_myturn || drawer != g_seat)
        return 0;
    strcpy(b, "You offered a draw. Waiting for ");
    strcat(b, g_name(1 - g_seat));
    strcat(b, ".");
    return b;
}

/* The keys: Esc puts a picked piece back (not in the middle of a jump), OA-D offers a draw, OA-E ends the
   turn and OA-U takes back a step. */
static unsigned char ck_key(unsigned char k)
{
    if (k == K_ESC) {
        if (!sel || npath)
            return 0;
        sel = 0;
        g_message("");
        redraw();
        return 1;
    }
    if (k != (U_OA | 'D') && k != (U_OA | 'E') && k != (U_OA | 'U'))
        return 0;
    sel = 0;
    g_cmd(k == (U_OA | 'D') ? "/draw" : k == (U_OA | 'E') ? "/end" : "/undo");
    return 1;
}

/* A draw offered by the other player, waiting for the player's answer: its dialog's words, or the answer. */
static unsigned char ck_offer(unsigned char answer)
{
    if (!g_myturn || drawer < 0 || drawer == g_seat)
        return 0;
    if (answer) {
        g_move(answer == 1 ? "accept" : "reject");
        return 0;
    }
    strcpy(dlg_words, g_name(drawer));
    strcat(dlg_words, " offers a draw. If you accept, the game ends as a draw. If you decline, the game goes on.");
    return D_OFFER;
}

/* The empty board, while the table waits (game.h). */
static void ck_empty(void)
{
    memset(cells, '.', 33);
    memset(cnt, 0, 4);
}

#pragma rodata-name (push, "OVHDR")
const struct game g_checkers = {"checkers", "Checkers", ck_begin, ck_line, ck_draw, ck_click, ck_key, 0, ck_event,
                                ck_hover, ck_offer, ck_say, ck_empty};
#pragma rodata-name (pop)
