/* ParlorNet: Backgammon (docs/types/backgammon-1.md). The server rolls the dice and lists every legal step (M
   lines); a step is chosen by clicking the point to move from, then the point to move to (or the tray, to
   bear off). A turn is staged: each step is a MOV, then Done sends END, and Undo takes a step back. The board
   is turned so that the player's home is at the bottom (view), on the right or, mirrored, on the left (left;
   kept on the server as the preference bghome). Only the drawing is mirrored: the points keep their numbers.

   The machine draws it (draw/backgammon.c, included below: bg_draw, and mark, which shows the chosen point
   and where it can go, or takes that away), each point a target 1 to 24, the bar ID_BAR, the tray ID_OFF, the
   buttons ID_A and ID_B with the words panel_offer chooses. Steps the other player made since the last view
   come as STEP events (bg_event), queued for the drawing to show one after another. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "game.h"
#include "frame.h"

#define ID_BAR 25
#define ID_OFF 26
#define ID_A 30                         /* the two action buttons */
#define ID_B 31

static signed char pt[25];
static unsigned char bar[2], off[2], dice[4], nd, dr[2], ndr, cube, ca[2];
static signed char owner;
static char phase[8];
static unsigned char sel, view;
static unsigned char rs_pts;            /* RS: an offer to resign waits, for so many points, of this kind */
static char rs_kind;
static unsigned char left, home_read;
static const char *offer_a, *offer_b;   /* the buttons' words ("" for none) */

/* Steps played since the last view (STEP events): seat, from (0 the bar), to (25 the tray), a hit. */
#define MAXQ 8
static unsigned char nq, q_s[MAXQ], q_f[MAXQ], q_t[MAXQ], q_h[MAXQ];

static void bg_begin(void)
{
    nd = ndr = 0;
    phase[0] = 0;
    rs_pts = 0;
}

static void bg_line(void)
{
    unsigned char i, base;
    if (is(w[0], "P") && wn > 1) {
        base = num(w[1]);
        for (i = 2; i < wn && base + i - 2 <= 24; ++i)
            pt[base + i - 2] = snum(w[i]);
    } else if (is(w[0], "B") && wn > 2) {
        bar[0] = num(w[1]);
        bar[1] = num(w[2]);
    } else if (is(w[0], "O") && wn > 2) {
        off[0] = num(w[1]);
        off[1] = num(w[2]);
    } else if (is(w[0], "D")) {
        nd = 0;
        for (i = 1; i < wn && nd < 4; ++i)
            dice[nd++] = num(w[i]);
    } else if (is(w[0], "DR")) {
        ndr = 0;
        for (i = 1; i < wn && ndr < 2; ++i)
            dr[ndr++] = num(w[i]);
    } else if (is(w[0], "C") && wn > 2) {
        cube = num(w[1]);
        owner = w[2][0] == '-' ? -1 : (signed char)num(w[2]);
    } else if (is(w[0], "Q") && wn > 1) {
        strncpy(phase, w[1], 7);
    } else if (is(w[0], "RS") && wn > 3) {
        rs_pts = num(w[2]);
        rs_kind = w[3][0];
    } else if (is(w[0], "CA") && wn > 2) {
        ca[0] = num(w[1]);
        ca[1] = num(w[2]);
    }
}

/* The other player's turn in words, step by step: "ann rolled 6 and 3 and moved 12-18 and 12-15.",
   "ann doubled to 4.", "ann accepted the double." */
static unsigned char moved;

static void said(void)
{
    unsigned char s = num(w[4]);
    if (s == (unsigned char)g_seat)
        return;
    if (is(w[3], "ROLL") && wn > 6) {
        strcat(g_who(s), " rolled ");
        strcat(g_last, w[5]);
        strcat(g_last, " and ");
        strcat(g_last, w[6]);
        moved = 0;
    } else if (is(w[3], "STEP") && wn > 6 && g_last[0] && strlen(g_last) < 36) {
        strcat(g_last, moved ? " and " : " and moved ");
        strcat(g_last, w[5]);
        strcat(g_last, "-");
        strcat(g_last, w[6]);
        moved = 1;
    } else if (is(w[3], "DBL") && wn > 5) {
        strcat(g_who(s), " doubled to ");
        strcat(g_last, w[5]);
    } else if (is(w[3], "TAKE") || is(w[3], "DROP"))
        strcat(g_who(s), w[3][0] == 'T' ? " accepted the double" : " declined the double");
    else if (is(w[3], "PASS"))
        strcat(g_who(s), " could not move");
}

static void bg_event(void)
{
    said();
    if (nq < MAXQ && is(w[3], "STEP") && wn > 6) {
        q_s[nq] = num(w[4]) & 1;
        q_f[nq] = w[5][0] == 'b' ? 0 : num(w[5]);        /* 0 the bar */
        q_t[nq] = w[6][0] == 'o' ? 25 : num(w[6]);       /* 25 the tray */
        q_h[nq] = wn > 7;
        ++nq;
    }
}

static unsigned char from_has_moves(unsigned char p)
{
    unsigned char i;
    for (i = 0; i < m_n; ++i)
        if (p == ID_BAR ? strncmp(m_tok[i], "bar-", 4) == 0 : (m_tok[i][0] != 'b' && num(m_tok[i]) == p))
            return 1;
    return 0;
}

/* The buttons, by the phase of the turn: Roll (and Double, when the cube may be turned), End turn (once
   every die is played) and Undo. A double and an offer to resign are answered in their dialogs (bg_offer). */
static void panel_offer(void)
{
    offer_a = offer_b = "";
    if (!g_myturn)
        return;
    if (strcmp(phase, "roll") == 0) {
        offer_a = "Roll";
        if (ca[view])
            offer_b = "Double";
    } else if (strcmp(phase, "move") == 0) {
        offer_a = nd && m_n ? "" : "End turn";  /* (a die no step can use leaves the turn complete) */
        offer_b = "Undo";
    }
}

/* The dice to show: the ones still to play, in left (a die is blanked, 0, as it is played, and comes back
   with Undo); four for doubles. Returns how many places (0 before a roll, 2, or 4). */
static unsigned char dice_left(unsigned char *left_)
{
    unsigned char n = 0, k, used = 0, j;
    if (ndr != 2)
        return 0;
    if (dr[0] == dr[1]) {
        for (k = 0; k < 4; ++k)
            left_[n++] = k < nd ? dr[0] : 0;
    } else
        for (k = 0; k < 2; ++k) {
            left_[n] = 0;
            for (j = 0; j < nd; ++j)
                if (dice[j] == dr[k] && !(used & (1 << j))) {
                    used |= 1 << j;
                    left_[n] = dr[k];
                    break;
                }
            ++n;
        }
    return n;
}

/* Before a draw: the board's way round, and (once) the player's home side from the parlor. */
static void board_view(void)
{
    view = g_seat == 1 ? 1 : 0;
    if (!home_read) {
        home_read = 1;
        q_set("PRF GET bghome");
        if (net_req() == R_OK && r_n && strncmp(r_line[0], "PRF bghome l", 12) == 0)
            left = 1;
    }
}

/* The machine's drawing: bg_draw and mark. */
#include "draw/backgammon.c"

static void bg_click(unsigned char id)
{
    static char tok[10];
    if (!g_myturn) {
        g_message("It's not your turn.");
        return;
    }
    if (id == ID_A || id == ID_B) {
        const char *word = id == ID_A ? offer_a : offer_b;
        if (!word[0])
            return;
        if (is(word, "Roll"))
            g_move("roll");
        else if (is(word, "Double"))
            g_move("double");
        else if (word[0] == 'E')
            g_end();
        else if (is(word, "Undo"))
            g_undo();
        return;
    }
    if (strcmp(phase, "move") != 0)
        return;
    if (!sel) {
        if (from_has_moves(id)) {
            sel = id;
            ui_hide();
            mark(1);
            ui_show();
            g_message("");
        } else
            g_message("No checker of yours can move from there.");
        return;
    }
    if (id == sel) {
        ui_hide();
        mark(0);
        ui_show();
        sel = 0;
        g_message("");
        return;
    }
    if (sel == ID_BAR)
        strcpy(tok, "bar");
    else
        strcpy(tok, utoa10(sel));
    strcat(tok, "-");
    strcat(tok, id == ID_OFF ? "off" : utoa10(id));
    if (!m_has(tok)) {
        if (id <= 24 && from_has_moves(id)) {
            ui_hide();
            mark(0);
            sel = id;
            mark(1);
            ui_show();
        } else
            g_message("That is not a legal move.");
        return;
    }
    ui_hide();
    mark(0);
    ui_show();
    sel = 0;
    g_move(tok);

}

/* What Return does on the place under the pointer, by the phase of the turn. */
static void bg_hover(unsigned char id)
{
    static char b[48], tok[10];
    const char *s = 0;
    unsigned char n = 0;
    if (strcmp(phase, "roll") == 0) {
        if (id == ID_B && offer_b[0]) {
            s = "Press " KEY_RET " to double to ";
            n = cube * 2;
        } else
            s = ca[g_seat == 1] ? "Press " KEY_RET " to roll, or " OA "D to double." : "Press " KEY_RET " to roll the dice.";
    } else if (id == ID_A && offer_a[0])
        s = "Press " KEY_RET " to end your turn.";
    else if (id == ID_B && offer_b[0])
        s = "Press " KEY_RET " to undo a step.";
    else if (offer_a[0])
        s = "Press " OA "E to end your turn or " OA "U to undo.";
    else if (sel) {
        s = "Press " KEY_RET " on a marked point.";
        if (id <= ID_OFF && id != ID_BAR) {
            strcpy(tok, sel == ID_BAR ? "bar" : utoa10(sel));
            strcat(tok, "-");
            strcat(tok, id == ID_OFF ? "off" : utoa10(id));
            if (m_has(tok)) {
                s = id == ID_OFF ? "Press " KEY_RET " to bear it off." : "Press " KEY_RET " to move it to ";
                n = id;
            }
        }
    } else if (id <= ID_BAR && from_has_moves(id)) {
        s = id == ID_BAR ? "Press " KEY_RET " to pick up a checker." : "Press " KEY_RET " to pick up a checker on ";
        n = id;
    } else
        s = "Pick up a checker that can move.";
    strcpy(b, s);
    if (b[strlen(b) - 1] == ' ') {
        strcat(b, utoa10(n));
        strcat(b, ".");
    }
    g_message(b);
}

/* The first sentence while a double the player made waits for the other's answer. */
static const char *bg_say(void)
{
    static char b[40];
    if (g_myturn || strcmp(phase, "take") != 0)
        return 0;
    strcpy(b, "You doubled to ");
    strcat(b, utoa10(cube * 2));
    strcat(b, ". Waiting for ");
    strcat(b, g_name(g_seat == 1 ? 0 : 1));
    strcat(b, ".");
    return b;
}

/* The keys: Esc puts a picked checker back, OA-D doubles, OA-E ends the turn, OA-U takes back a step and OA-H
   moves the home board to the other side (kept on the parlor as bghome). */
static unsigned char bg_key(unsigned char k)
{
    if (k == K_ESC) {
        if (!sel)
            return 0;
        ui_hide();
        mark(0);
        ui_show();
        sel = 0;
        g_message("");
        return 1;
    }
    if (k == (U_OA | 'H')) {
        left ^= 1;
        q_set("PRF SET bghome");
        q_add(left ? "left" : "right");
        net_req();
        g_redraw();
        g_message(left ? "Your home board is on the left now." : "Your home board is on the right now.");
        return 1;
    }
    if (k != (U_OA | 'D') && k != (U_OA | 'E') && k != (U_OA | 'U'))
        return 0;
    if (sel) {
        ui_hide();
        mark(0);
        ui_show();
        sel = 0;
    }
    g_cmd(k == (U_OA | 'D') ? "/double" : k == (U_OA | 'E') ? "/end" : "/undo");
    return 1;
}

/* "ann", then the rest of a sentence, into dlg_words. */
static void words_of(const char *s)
{
    strcpy(dlg_words, g_name(1 - view));
    strcat(dlg_words, s);
}

static void points(unsigned int n, const char *s)
{
    strcat(dlg_words, utoa10(n));
    strcat(dlg_words, n == 1 ? " point" : " points");
    strcat(dlg_words, s);
}

/* A double or an offer to resign, waiting for the player's answer: its dialog's words, or the answer. */
static unsigned char bg_offer(unsigned char answer)
{
    unsigned char take = strcmp(phase, "take") == 0;
    if (!g_myturn || (!take && strcmp(phase, "resign") != 0))
        return 0;
    if (answer) {
        g_move(take ? (answer == 1 ? "take" : "drop") : (answer == 1 ? "accept" : "reject"));
        return 0;
    }
    view = g_seat == 1 ? 1 : 0;
    if (take) {
        words_of(" doubles to ");
        strcat(dlg_words, utoa10(cube * 2));
        strcat(dlg_words, ". If you accept, the game goes on for ");
        points(cube * 2, ". If you decline, you lose ");
        points(cube, " and the game.");
        return D_OFFER | D_FIRST;
    }
    words_of(rs_kind == 'g' ? " offers to resign a gammon." : rs_kind == 'b' ? " offers to resign a backgammon."
             : " offers to resign.");
    strcat(dlg_words, " If you accept, you win ");
    points(rs_pts, " and the game. If you decline, the game goes on.");
    return D_OFFER;
}

#pragma rodata-name (push, "OVHDR")
const struct game g_backgammon = {"backgammon", "Backgammon", bg_begin, bg_line, bg_draw, bg_click, bg_key,
                                  0, bg_event, bg_hover, bg_offer, bg_say};
#pragma rodata-name (pop)
