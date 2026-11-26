/* ParlorNet on the Apple IIe and IIgs: Backgammon's board (core/games/backgammon.c includes it). Each step
   glides its checker from where it was to where it goes. A checker is a solid disc, white or black, in a
   place two bytes by 12 lines filled with its point's color (orange or blue; on the bar and beside the
   names, blue; in monochrome a gray of alternate dots), since a black disc needs a color round it to show
   on the black board. A point shows up to five checkers and the bar up to three a side; on a taller stack
   the last one shown has the number not shown cut out of it, a digit up to 9 or else +, in its place's
   color. */
#include "apple2.h"
extern const unsigned char spr_bg_da[], spr_bg_db[], spr_bg_ua[], spr_bg_ub[];
extern const unsigned char spr_bg_m_da[], spr_bg_m_db[], spr_bg_m_ua[], spr_bg_m_ub[];
extern const unsigned char spr_bg_disc[], spr_bg_n1[], spr_bg_n2[], spr_bg_n3[], spr_bg_n4[], spr_bg_n5[],
    spr_bg_n6[], spr_bg_n7[], spr_bg_n8[], spr_bg_n9[], spr_bg_nplus[];
extern const unsigned char spr_bg_die1[], spr_bg_die2[], spr_bg_die3[], spr_bg_die4[], spr_bg_die5[],
    spr_bg_die6[];
static const unsigned char *const die_spr[] = {spr_bg_die1, spr_bg_die2, spr_bg_die3, spr_bg_die4,
                                               spr_bg_die5, spr_bg_die6};
static const unsigned char *const num_spr[] = {spr_bg_n1, spr_bg_n2, spr_bg_n3, spr_bg_n4, spr_bg_n5,
                                               spr_bg_n6, spr_bg_n7, spr_bg_n8, spr_bg_n9, spr_bg_nplus};
#define ORANGE 0                        /* a place's color */
#define BLUE 1

#define TOP 17                          /* the top points start here, pointing down */
#define BOT 157                         /* the bottom points end here, pointing up */
static signed char shown[25];
static unsigned char sbar[2], soff[2], drawn_sel;
static char shown_panel[24];
static unsigned char ax, ay;
static unsigned char kbuf[2 + 24] = {2, 12};

/* Seat s's checker (0 white, 1 black) in kbuf, for the place at byte column x: the disc on color c (ORANGE
   or BLUE; in monochrome alternate dots), with the count n (0 for none) in its middle lines. A color is the
   bits of every other screen column (odd for orange, even for blue) with the high bit set; bit 0 of a byte
   is its leftmost column, 7x on the screen. In color the count is cut out of the disc and shows the
   place's color; in monochrome, where that color is a pattern of dots that would break up so small a digit,
   it shows the other of white and black. */
static void make_checker(unsigned char x, unsigned char s, unsigned char c, unsigned char n)
{
    unsigned char i, k, pat, in, dg, ink;
    const unsigned char *g = 0;
    if (n)
        g = num_spr[n > 9 ? 9 : n - 1] + 2 - 6;      /* its 5 lines are the disc's 3 to 7 */
    for (i = 0; i < 24; ++i) {
        k = i >> 1;                     /* the line */
        pat = ((x + i) & 1) ? 0x55 : 0x2A;      /* the odd columns */
        if (mono ? k & 1 : c)
            pat ^= 0x7F;
        in = spr_bg_disc[2 + i];
        dg = g && k >= 3 && k < 8 ? g[i] : 0;
        if (mono)
            ink = s ? dg : in & ~dg;
        else {
            in &= ~dg;
            ink = s ? 0 : in;
        }
        kbuf[2 + i] = (pat & ~in) | ink | (mono ? 0 : 0x80);
    }
}

/* Seat s's checker in the place at (x, y) on color c, with the count n cut out of it. */
static void checker(unsigned char x, unsigned char y, unsigned char s, unsigned char c, unsigned char n)
{
    make_checker(x, s, c, n);
    hg_x = x;
    hg_y = y;
    hg_src = kbuf;
    hgr_blit();
}

/* Where something w bytes wide at byte column x of the right-hand layout goes: the board (columns 0 to
   28, the tray at the right) is mirrored for the left-hand one. */
static unsigned char mx(unsigned char x, unsigned char w)
{
    return left ? 29 - x - w : x;
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
    /* five places: up to five checkers, the last with the count of any more, each on the point's color
       (its art is made for an even column, so at an odd one its orange shows as blue, and its blue as
       orange) */
    for (i = 0; i < n && i < 5; ++i) {
        y = top ? TOP + 12 * i : BOT - 11 - 12 * i;
        checker(x, y, v < 0, c ^ (x & 1), i == 4 && n > 5 ? n - 5 : 0);
    }
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
        /* three places: up to three checkers, the last with the count of any more */
        for (i = 0; i < n && i < 3; ++i) {
            /* my checkers on the bar wait below the middle, the other's above */
            y = (s == me) ? 98 + 12 * i : 65 - 12 * i;
            checker(bx, y, s, BLUE, i == 2 && n > 3 ? n - 3 : 0);
        }
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
   between, three sixtieths of a second each, in a blue place as on the bar. Over the black board it shows
   as drawn; over a point the exclusive-or mixes the two colors for that moment. */
static void glide(unsigned char s, unsigned char x0, unsigned char y0)
{
    static int dx, dy;
    unsigned char f, t;
    dx = (int)ax - x0;
    dy = (int)ay - y0;
    hg_xor = 1;
    for (f = 1; f < 8; ++f) {
        make_checker(x0 + dx * f / 8, s, BLUE, 0);
        hg_x = x0 + dx * f / 8;
        hg_y = y0 + dy * f / 8;
        hg_src = kbuf;
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
    /* off the place it leaves: the last checker shown */
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

/* A panel button: its words at column x, line y, and target id on their middle, as ui_button puts a
   button's target, since the words change with the turn. With no words the target is out of use. */
static void button(unsigned char x, unsigned char y, const char *s, unsigned char id)
{
    unsigned char i = ui_find(id), hw = 0;
    if (*s) {
        ui_button_draw(x, y, s);
        hw = (unsigned char)((strlen(s) + 2) * 7) >> 2;
    }
    if (i < t_n) {
        t_x[i] = (unsigned char)((x * 7) >> 1) + hw;
        t_hw[i] = hw;
    }
}

/* The panel: names, dice, cube and the two buttons. */
static void draw_panel(void)
{
    unsigned char s, me = view, left_[4], n, k, j;
    static char key[24];
    /* the dice in the right half of the board: the ones still to play (a die is blanked as it is played,
       and comes back with Undo); four for doubles */
    ui_rect(mx(15, 12), 81, 12, 12, F_BLACK);
    n = dice_left(left_);
    for (k = 0; k < n; ++k)
        if (left_[k]) {
            /* clear of the cube on the bar */
            j = n == 4 ? 1 + 3 * k : 3 + 4 * k;
            hg_x = mx(15, 12) + (left ? 10 - j : j);
            hg_y = 81;
            hg_src = die_spr[left_[k] - 1];
            hgr_blit();
        }
    /* names */
    for (s = 0; s < 2; ++s) {
        checker(30, s == me ? BOT - 11 : TOP, s, BLUE, 0);
        ui_textn(32, s == me ? BOT - 9 : TOP + 2, g_name(s), 8);
        if (g_turn == s && !g_over)
            ui_textn(30, s == me ? BOT - 20 : TOP + 13, "to play", 9);
        else
            ui_textn(30, s == me ? BOT - 20 : TOP + 13, "", 9);
    }
    panel_offer();
    strcpy(key, offer_a);
    strcat(key, "/");
    strcat(key, offer_b);
    if (strcmp(key, shown_panel) != 0) {
        strcpy(shown_panel, key);
        ui_rect(30, 66, 10, 8, F_BLACK);
        ui_rect(30, 86, 10, 8, F_BLACK);
        button(30, 66, offer_a, ID_A);
        button(30, 86, offer_b, ID_B);
    }
}

static void bg_draw(void)
{
    unsigned char p, top;
    board_view();
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
        ui_target(30 * 7 / 2, 70, 0, 5, ID_A);     /* (put on their words by button) */
        ui_target(30 * 7 / 2, 90, 0, 5, ID_B);
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
