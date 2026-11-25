/* ParlorNet for the Apple IIe: Backgammon (docs/types/backgammon-1.md). The server rolls the dice and
   lists every legal step (M lines); a step is chosen by clicking the point to move from, then the point
   to move to (or the tray, to bear off). A turn is staged: each step is a MOV, then Done sends END, and
   Undo takes a step back. The board is turned so that the player's home is at the bottom, on the right
   or, mirrored, on the left (H; kept on the server as the preference bghome). Only the drawing is
   mirrored: the points keep their numbers. */
#include <string.h>
#include "a2.h"
#include "ui.h"
#include "net.h"
#include "game.h"

extern const unsigned char spr_bg_da[], spr_bg_db[], spr_bg_ua[], spr_bg_ub[];
extern const unsigned char spr_bg_m_da[], spr_bg_m_db[], spr_bg_m_ua[], spr_bg_m_ub[];
extern const unsigned char spr_bg_c0[], spr_bg_c1[], spr_bg_cc1[], spr_bg_cc1_o[];
extern const unsigned char spr_bg_die1[], spr_bg_die2[], spr_bg_die3[], spr_bg_die4[], spr_bg_die5[],
    spr_bg_die6[];
static const unsigned char *const die_spr[] = {spr_bg_die1, spr_bg_die2, spr_bg_die3, spr_bg_die4,
                                               spr_bg_die5, spr_bg_die6};
#define CHECKER(s, x) ((s) ? (mono ? spr_bg_c1 : ((x) & 1) ? spr_bg_cc1_o : spr_bg_cc1) : spr_bg_c0)

#define TOP 17                          /* the top points start here, pointing down */
#define BOT 157                         /* the bottom points end here, pointing up */
#define ID_BAR 25
#define ID_OFF 26
#define ID_A 30                         /* the two action buttons */
#define ID_B 31

static signed char pt[25], shown[25];
static unsigned char bar[2], off[2], sbar[2], soff[2], dice[4], nd, dr[2], ndr, cube, ca[2];
static signed char owner;
static char phase[8];
static unsigned char sel, view, drawn_sel;
static char shown_panel[24];
static unsigned char left, home_read;

/* Where something w bytes wide at byte column x of the right-hand layout goes: the board (columns 0 to
   28, the tray at the right) is mirrored for the left-hand one. */
static unsigned char mx(unsigned char x, unsigned char w)
{
    return left ? 29 - x - w : x;
}

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

/* Where point p is drawn: its byte column and whether it is in the top row. */
static unsigned char col_of(unsigned char p, unsigned char *top)
{
    unsigned char d = view ? 25 - p : p, c;
    if (d >= 13) {
        *top = 1;
        c = d - 13;
    } else {
        *top = 0;
        c = 12 - d;
    }
    return mx(c < 6 ? 1 + 2 * c : 15 + 2 * (c - 6), 2);
}

static void label(unsigned char p, unsigned char hi)
{
    unsigned char top, x = col_of(p, &top);
    if (hi)
        ui_ink(F_BLACK, F_WHITE);
    hg_x = x;
    hg_y = top ? GAME_TOP : BOT + 1;
    if (p < 10) {
        hgr_char(' ');
        hgr_char('0' + p);
    } else {
        hgr_char('0' + p / 10);
        hgr_char('0' + p % 10);
    }
    ui_ink(F_WHITE, F_BLACK);
}

/* The checkers a stack has past the ones drawn, as +n in the stack's last place (10 or more without
   the +, which two characters leave no room for). */
static void more_label(unsigned char x, unsigned char y, unsigned char n)
{
    ui_ink(F_BLACK, F_WHITE);
    hg_x = x;
    hg_y = y + 2;
    hgr_char(n < 10 ? '+' : '0' + n / 10);
    hgr_char('0' + n % 10);
    ui_ink(F_WHITE, F_BLACK);
}

/* Point p with v checkers on it (as pt[]: seat 0's more than 0). */
static void draw_point(unsigned char p, signed char v)
{
    unsigned char top, x = col_of(p, &top), n, i, y, c = mx(x, 2);
    const unsigned char *spr;
    c = (c < 14 ? c - 1 : c - 15) / 2;
    c = (c + top) & 1;
    if (mono)
        spr = top ? (c ? spr_bg_m_db : spr_bg_m_da) : (c ? spr_bg_m_ub : spr_bg_m_ua);
    else
        spr = top ? (c ? spr_bg_db : spr_bg_da) : (c ? spr_bg_ub : spr_bg_ua);
    hg_x = x;
    hg_y = top ? TOP : BOT - 59;
    hg_src = spr;
    hgr_blit();
    n = v < 0 ? -v : v;
    hg_src = CHECKER(v < 0, x);
    /* five places: up to five checkers, or four and +n at the peak for the rest */
    for (i = 0; i < n && i < (n > 5 ? 4 : 5); ++i) {
        y = top ? TOP + 12 * i : BOT - 11 - 12 * i;
        hg_x = x;
        hg_y = y;
        hgr_blit();
    }
    if (n > 5)
        more_label(x, top ? TOP + 48 : BOT - 59, n - 4);
    shown[p] = v;
}

static void draw_cube(unsigned char bx, unsigned char tx);

/* The bar (in the middle) and the trays (at the right of the board). */
static void draw_bar_off(void)
{
    unsigned char s, i, me = view, n, y;
    unsigned char bx = mx(13, 2), tx = mx(27, 2);
    ui_rect(bx, TOP, 2, BOT - TOP + 1, F_BLACK);
    for (s = 0; s < 2; ++s) {
        n = sbar[s];
        hg_src = CHECKER(s, bx);
        /* three places: up to three checkers, or two and +n for the rest */
        for (i = 0; i < n && i < (n > 3 ? 2 : 3); ++i) {
            /* my checkers on the bar wait below the middle, the other's above */
            y = (s == me) ? 98 + 12 * i : 65 - 12 * i;
            hg_x = bx;
            hg_y = y;
            hgr_blit();
        }
        if (n > 3)
            more_label(bx, s == me ? 122 : 41, n - 2);
    }
    ui_rect(tx, TOP, 2, BOT - TOP + 1, F_BLACK);
    ui_box(tx * 7, TOP, 14, BOT - TOP + 1);
    for (s = 0; s < 2; ++s) {
        y = s == me ? BOT - 10 : TOP + 2;
        hg_x = tx;
        hg_y = y;
        hgr_char(soff[s] >= 10 ? '0' + soff[s] / 10 : ' ');
        hgr_char('0' + soff[s] % 10);
    }
    draw_cube(bx, tx);
}

/* The doubling cube: a white face with its value (64 while nobody owns it, as on a real board), on the
   bar while it is in the middle, else in the tray on its owner's side. */
static void draw_cube(unsigned char bx, unsigned char tx)
{
    unsigned char v = owner < 0 && cube == 1 ? 64 : cube, x, y;
    if (owner < 0) {
        x = bx;
        y = 80;
    } else {
        x = tx;
        y = owner == view ? BOT - 32 : TOP + 20;
    }
    ui_rect(x, y, 2, 12, F_WHITE);
    ui_ink(F_BLACK, F_WHITE);
    hg_x = x;
    hg_y = y + 2;
    hgr_char(v >= 10 ? '0' + v / 10 : ' ');
    hgr_char('0' + v % 10);
    ui_ink(F_WHITE, F_BLACK);
}

/* Steps played since the last view (STEP events), drawn as each checker gliding from where it was to
   where it goes, one after another, before the board is brought up to the new view. */
#define MAXQ 8
static unsigned char nq, q_s[MAXQ], q_f[MAXQ], q_t[MAXQ], q_h[MAXQ];
static unsigned char ax, ay;

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

/* (ax, ay): the place of seat s's checker i on point p, the bar (0) or the tray (25). */
static void spot(unsigned char p, unsigned char s, unsigned char i)
{
    unsigned char top;
    if (!p) {
        ax = mx(13, 2);
        ay = s == view ? 98 + 12 * i : 65 - 12 * i;
    } else if (p == 25) {
        ax = mx(27, 2);
        ay = s == view ? BOT - 24 : TOP + 12;
    } else {
        ax = col_of(p, &top);
        ay = top ? TOP + 12 * i : BOT - 11 - 12 * i;
    }
}

/* A checker of seat s from (x0, y0) to (ax, ay): drawn and taken away (exclusive-or) at seven places
   between, three sixtieths of a second each. */
static void glide(unsigned char s, unsigned char x0, unsigned char y0)
{
    static int dx, dy;
    unsigned char f, t;
    dx = (int)ax - x0;
    dy = (int)ay - y0;
    hg_xor = 1;
    for (f = 1; f < 8; ++f) {
        hg_x = x0 + dx * f / 8;
        hg_y = y0 + dy * f / 8;
        hg_src = CHECKER(s, hg_x);
        hgr_blit();
        for (t = ticks; (unsigned char)(ticks - t) < 3;)
            tick_poll();
        hgr_blit();
    }
    hg_xor = 0;
}

static unsigned char count(signed char v)
{
    return v < 0 ? -v : v;
}

static void play_step(unsigned char k)
{
    unsigned char s = q_s[k], f = q_f[k], t = q_t[k], o = s ^ 1, n, x0, y0;
    signed char d = s ? -1 : 1;
    /* off the place it leaves: the top checker, or the +n place of a tall stack */
    if (!f) {
        n = sbar[s];
        if (!n)
            return;
        spot(0, s, (n > 3 ? 3 : n) - 1);
        --sbar[s];
        draw_bar_off();
    } else {
        n = count(shown[f]);
        if (!n)
            return;
        spot(f, s, (n > 5 ? 5 : n) - 1);
        draw_point(f, shown[f] - d);
    }
    x0 = ax;
    y0 = ay;
    /* a hit: the other's checker goes to the bar first */
    if (q_h[k] && t <= 24) {
        spot(t, o, 0);
        n = ax;
        draw_point(t, 0);
        f = ay;
        spot(0, o, sbar[o] > 2 ? 2 : sbar[o]);
        glide(o, n, f);
        ++sbar[o];
        draw_bar_off();
    }
    if (t > 24) {
        spot(25, s, 0);
        glide(s, x0, y0);
        ++soff[s];
        draw_bar_off();
    } else {
        n = count(shown[t]);
        spot(t, s, n > 4 ? 4 : n);
        glide(s, x0, y0);
        draw_point(t, shown[t] + d);
    }
}

/* The panel: names, dice, cube and the two buttons. */
static void draw_panel(void)
{
    unsigned char s, me = view;
    static const char *a, *b;
    static char key[24];
    /* the dice in the right half of the board: the ones still to play (a die is blanked as it is played,
       and comes back with Undo); four for doubles */
    ui_rect(mx(15, 12), 81, 12, 12, F_BLACK);
    if (ndr == 2) {
        unsigned char left_[4], n = 0, k, used = 0, j;
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
        for (k = 0; k < n; ++k)
            if (left_[k]) {
                /* clear of the cube on the bar */
                j = n == 4 ? 1 + 3 * k : 3 + 4 * k;
                hg_x = mx(15, 12) + (left ? 10 - j : j);
                hg_y = 81;
                hg_src = die_spr[left_[k] - 1];
                hgr_blit();
            }
    }
    /* names */
    for (s = 0; s < 2; ++s) {
        hg_x = 30;
        hg_y = s == me ? BOT - 11 : TOP;
        hg_src = CHECKER(s, 30);
        hgr_blit();
        ui_textn(32, s == me ? BOT - 9 : TOP + 2, g_name(s), 8);
        if (g_turn == s && !g_over)
            ui_textn(30, s == me ? BOT - 20 : TOP + 13, "to play", 9);
        else
            ui_textn(30, s == me ? BOT - 20 : TOP + 13, "", 9);
    }
    /* buttons */
    a = b = "";
    if (g_myturn) {
        if (strcmp(phase, "roll") == 0) {
            a = "Roll";
            if (ca[me])
                b = "Double";
        } else if (strcmp(phase, "move") == 0) {
            a = nd ? "" : "Done";
            b = "Undo";
        } else if (strcmp(phase, "take") == 0) {
            a = "Take";
            b = "Drop";
        } else if (strcmp(phase, "resign") == 0) {
            a = "Accept";
            b = "Reject";
        }
    }
    strcpy(key, a);
    strcat(key, "/");
    strcat(key, b);
    if (strcmp(key, shown_panel) != 0) {
        strcpy(shown_panel, key);
        ui_rect(30, 66, 10, 8, F_BLACK);
        ui_rect(30, 86, 10, 8, F_BLACK);
        /* (their targets, ID_A and ID_B, are made once, at the full draw) */
        if (a[0])
            ui_button_draw(30, 66, a);
        if (b[0])
            ui_button_draw(30, 86, b);
    }
}

static void bg_draw(void)
{
    unsigned char p, top;
    view = g_seat == 1 ? 1 : 0;
    if (!home_read) {
        home_read = 1;
        q_set("PRF GET bghome");
        if (net_req() == R_OK && r_n && strncmp(r_line[0], "PRF bghome l", 12) == 0)
            left = 1;
    }
    if (g_full) {
        sel = 0;
        drawn_sel = 0;
        shown_panel[0] = 1;
        memset(shown, 99, sizeof shown);
        for (p = 1; p <= 24; ++p) {
            unsigned char x = col_of(p, &top);
            label(p, 0);
            /* the middle of the point's 14 pixels, and only its own width */
            ui_target((x * 7 + 7) / 2, top ? TOP + 30 : BOT - 30, 3, 30, p);
        }
        /* the bar: on my checkers there, below the cube in the middle */
        ui_target((mx(13, 2) * 7 + 7) / 2, 104, 3, 12, ID_BAR);
        ui_target((mx(27, 2) + 1) * 7 / 2, 90, 7, 60, ID_OFF);
        ui_target(30 * 7 / 2 + 21, 70, 21, 4, ID_A);
        ui_target(30 * 7 / 2 + 21, 90, 21, 4, ID_B);
    }
    if (g_full)
        nq = 0;
    for (p = 0; p < nq; ++p)
        play_step(p);
    nq = 0;
    for (p = 1; p <= 24; ++p)
        if (pt[p] != shown[p])
            draw_point(p, pt[p]);
    memcpy(sbar, bar, 2);
    memcpy(soff, off, 2);
    draw_bar_off();
    draw_panel();
}

/* Highlight the selected point's label and the labels of where it can go. */
static void mark(unsigned char on)
{
    unsigned char i, p;
    char *m;
    if (sel && sel <= 24)
        label(sel, on);
    for (i = 0; i < m_n; ++i) {
        m = m_tok[i];
        if ((sel == ID_BAR && strncmp(m, "bar-", 4) == 0) ||
            (sel <= 24 && num(m) == sel && m[0] != 'b')) {
            m = strchr(m, '-');
            if (m && m[1] != 'o') {
                p = num(m + 1);
                if (p >= 1 && p <= 24)
                    label(p, on);
            }
        }
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

static void bg_click(unsigned char id)
{
    static char tok[10];
    if (!g_myturn) {
        g_message("Not your turn.");
        return;
    }
    if (id == ID_A || id == ID_B) {
        static char word[10];
        char *slash = strchr(shown_panel, '/');
        if (!slash)
            return;
        if (id == ID_B)
            strncpy(word, slash + 1, 9);
        else {
            *slash = 0;
            strncpy(word, shown_panel, 9);
            *slash = '/';
        }
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
