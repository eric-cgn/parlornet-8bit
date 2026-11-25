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

#define ID_BAR 25
#define ID_OFF 26
#define ID_A 30                         /* the two action buttons */
#define ID_B 31

static signed char pt[25];
static unsigned char bar[2], off[2], dice[4], nd, dr[2], ndr, cube, ca[2];
static signed char owner;
static char phase[8];
static unsigned char sel, view;
static unsigned char left, home_read;
static const char *offer_a, *offer_b;   /* the buttons' words ("" for none) */

/* Steps played since the last view (STEP events): seat, from (0 the bar), to (25 the tray), a hit. */
#define MAXQ 8
static unsigned char nq, q_s[MAXQ], q_f[MAXQ], q_t[MAXQ], q_h[MAXQ];

static void bg_begin(void)
{
    nd = ndr = 0;
    phase[0] = 0;
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
    } else if (is(w[0], "CA") && wn > 2) {
        ca[0] = num(w[1]);
        ca[1] = num(w[2]);
    }
}

static void bg_event(void)
{
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

/* The buttons, by the phase of the turn: Roll (and Double, when the cube may be turned), Done (once every
   die is played) and Undo, Take and Drop a double, Accept and Reject a resignation. */
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
        offer_a = nd && m_n ? "" : "Done";     /* (a die no step can use leaves the turn complete) */
        offer_b = "Undo";
    } else if (strcmp(phase, "take") == 0) {
        offer_a = "Take";
        offer_b = "Drop";
    } else if (strcmp(phase, "resign") == 0) {
        offer_a = "Accept";
        offer_b = "Reject";
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
        g_message("Not your turn.");
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
        else if (is(word, "Done"))
            g_end();
        else if (is(word, "Undo"))
            g_undo();
        else if (is(word, "Take"))
            g_move("take");
        else if (is(word, "Drop"))
            g_move("drop");
        else if (is(word, "Accept"))
            g_move("accept");
        else if (is(word, "Reject"))
            g_move("reject");
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
            g_message("Now the point to move to.");
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
    if (g_reply_done)
        g_message("All dice played: Done, or Undo.");
}

static unsigned char bg_key(unsigned char k)
{
    k |= 0x20;
    if (k == 'h') {
        left ^= 1;
        q_set("PRF SET bghome");
        q_add(left ? "left" : "right");
        net_req();
        g_redraw();
        g_message(left ? "Home board on the left (" K1 "H: right)." : "Home board on the right (" K1 "H: left).");
        return 1;
    }
    if (k == 'u') {
        bg_click(ID_B);
        return 1;
    }
    if (k == 'd' || k == 'r') {
        bg_click(ID_A);
        return 1;
    }
    return 0;
}

#pragma rodata-name (push, "OVHDR")
const struct game g_backgammon = {"backgammon", "Backgammon", bg_begin, bg_line, bg_draw, bg_click, bg_key,
                                  K1 "R roll, " K1 "D done, " K1 "U undo, " K1 "H home side", 0,
                                  bg_event};
#pragma rodata-name (pop)
