/* ParlorNet on the Commodore 64: Backgammon's board (core/games/backgammon.c includes it). The points are
   lanes of whole cells, two wide, red and green by turns on a wooden board, each ending in a tip toward the
   middle, so that a checker (black or white, two cells by two) needs only its lane's color and its own. A
   point shows up to four checkers, or three and +n; the bar up to two a side, or one and +n. Each step glides
   its checker from where it was to where it goes: a sprite moves over the board. The dice and the cube are in
   the row across the middle, the trays on the right hold the counts borne off, and the names and buttons are
   beside the board. The columns are the Apple II's (mx, col_of): the board is 29 cells wide, mirrored when
   the player's home is on the left. In monochrome the lanes are two grays. */
#include "gfx.h"

extern const unsigned char pic_bg_checker[], pic_bg_tip_down[], pic_bg_tip_up[], pic_bg_lane[];
extern const unsigned char pic_bg_die1[], pic_bg_die2[], pic_bg_die3[], pic_bg_die4[], pic_bg_die5[],
    pic_bg_die6[], spr_bg_glide[];
static const unsigned char *const die_pic[] = {pic_bg_die1, pic_bg_die2, pic_bg_die3, pic_bg_die4, pic_bg_die5,
                                               pic_bg_die6};

#define TOP 16                          /* the top lanes: checkers from line 16 down, the tip at 80 */
#define MID 88                          /* the row across the middle: dice and cube */
#define BOT 152                         /* the bottom lanes: checkers from line 152 up, the tip at 96 */
#define SPR_GLIDE (SPR_GAME)

static signed char shown[25];
static unsigned char sbar[2], soff[2];
static const char *shown_a, *shown_b;
static unsigned char ax, ay;            /* a place on the board: cell column, line */

static unsigned char mx(unsigned char x, unsigned char w)
{
    return left ? 29 - x - w : x;
}

/* Where point p is drawn: its cell column and whether it is in the top row. */
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

static unsigned char wood(void)
{
    return mono ? C_DGREY : C_ORANGE;
}

static unsigned char lane_col(unsigned char p)
{
    return (p & 1) ? (mono ? C_GREY : C_RED) : (mono ? C_LGREY : C_GREEN);
}

static unsigned char checker_col(unsigned char s)
{
    return s ? C_WHITE : C_BLACK;
}

static void label(unsigned char p, unsigned char hi)
{
    unsigned char top, x = col_of(p, &top);
    if (hi)
        ui_inkc(mono ? C_BLACK : C_BLACK, mono ? C_WHITE : C_YELLOW);
    else
        ui_ink(F_WHITE, F_BLACK);
    hg_x = x;
    hg_y = top ? GAME_TOP : BOT + 16;
    hgr_char(p < 10 ? ' ' : '0' + p / 10);
    hgr_char('0' + p % 10);
    ui_ink(F_WHITE, F_BLACK);
}

/* A place of two cells by two: a checker of seat s (or none, s 2) on color bg. */
static void place(unsigned char x, unsigned char y, unsigned char s, unsigned char bg)
{
    hg_x = x;
    hg_y = y;
    hg_w = 2;
    hg_h = 16;
    hg_bg = bg;
    hg_fg = s < 2 ? checker_col(s) : C_WHITE;
    hgr_color();
    hg_src = s < 2 ? pic_bg_checker : pic_bg_lane;
    hgr_blit();
    if (s >= 2) {
        hg_y = y + 8;
        hgr_blit();
    }
}

/* "+n" in a place, white on its color. */
static void more_label(unsigned char x, unsigned char y, unsigned char n, unsigned char bg)
{
    place(x, y, 2, bg);
    hg_fg = C_WHITE;
    hg_bg = bg;
    hg_x = x;
    hg_y = y + 4;
    hgr_char(n < 10 ? '+' : '0' + n / 10);
    hgr_char('0' + n % 10);
}

/* Point p with v checkers on it (as pt[]: seat 0's more than 0). */
static void draw_point(unsigned char p, signed char v)
{
    unsigned char top, x = col_of(p, &top), n, i, s = v < 0, c = lane_col(p);
    n = v < 0 ? -v : v;
    /* four places: up to four checkers, or three and +n in the last */
    for (i = 0; i < 4; ++i)
        place(x, top ? TOP + 16 * i : BOT - 16 * i, i < n && (i < 3 || n == 4) ? s : 2, c);
    if (n > 4)
        more_label(x, top ? TOP + 48 : BOT - 48, n - 3, c);
    hg_x = x;
    hg_y = top ? TOP + 64 : BOT - 56;
    hg_w = 2;
    hg_h = 8;
    hg_fg = c;
    hg_bg = wood();
    hgr_color();
    hg_src = top ? pic_bg_tip_down : pic_bg_tip_up;
    hgr_blit();
    shown[p] = v;
}

static void draw_cube(unsigned char bx, unsigned char tx);

/* The bar (in the middle) and the trays (at the right of the board). */
static void draw_bar_off(void)
{
    unsigned char s, i, me = view, n, y;
    unsigned char bx = mx(13, 2), tx = mx(27, 2), barc = mono ? C_BLACK : C_BROWN;
    for (s = 0; s < 2; ++s) {
        n = sbar[s];
        /* two places a side: up to two checkers, or one and +n; mine below the middle */
        for (i = 0; i < 2; ++i) {
            y = s == me ? MID + 16 + 16 * i : MID - 32 - 16 * i;
            place(bx, y, i < n && (i < 1 || n == 2) ? s : 2, barc);
        }
        if (n > 2)
            more_label(bx, s == me ? MID + 32 : MID - 48, n - 1, barc);
    }
    for (s = 0; s < 2; ++s) {
        y = s == me ? BOT : TOP;
        place(tx, y, 2, barc);
        hg_x = tx;
        hg_y = y + 4;
        hg_fg = C_WHITE;
        hg_bg = barc;
        hgr_char(soff[s] >= 10 ? '0' + soff[s] / 10 : ' ');
        hgr_char('0' + soff[s] % 10);
    }
    draw_cube(bx, tx);
}

/* The doubling cube: white with its value (64 while nobody owns it, as on a real board), on the bar in the
   middle row while it is in the middle, else in the tray on its owner's side. */
static void draw_cube(unsigned char bx, unsigned char tx)
{
    unsigned char v = owner < 0 && cube == 1 ? 64 : cube, x, y;
    if (owner < 0) {
        x = bx;
        y = MID;
    } else {
        x = tx;
        y = owner == view ? BOT - 16 : TOP + 16;
    }
    hg_x = x;
    hg_y = y;
    hg_fg = C_BLACK;
    hg_bg = C_WHITE;
    hgr_char(v >= 10 ? '0' + v / 10 : ' ');
    hgr_char('0' + v % 10);
}

/* (ax, ay): the place of seat s's checker i on point p, the bar (0) or the tray (25). */
static void spot(unsigned char p, unsigned char s, unsigned char i)
{
    unsigned char top;
    if (!p) {
        ax = mx(13, 2);
        ay = s == view ? MID + 16 + 16 * i : MID - 32 - 16 * i;
    } else if (p == 25) {
        ax = mx(27, 2);
        ay = s == view ? BOT - 16 : TOP + 16;
    } else {
        ax = col_of(p, &top);
        ay = top ? TOP + 16 * i : BOT - 16 * i;
    }
}

/* A checker of seat s gliding from (x0, y0) to (ax, ay): the sprite, at eight places between, three
   sixtieths of a second each, then gone. */
static void glide(unsigned char s, unsigned char x0, unsigned char y0)
{
    static int dx, dy;
    unsigned char f, t;
    dx = ((int)ax - x0) * 8;
    dy = (int)ay - y0;
    spr_col[2] = checker_col(s);
    spr_on |= 4;
    for (f = 0; f <= 8; ++f) {
        spr_x[2] = x0 * 8 + dx * f / 8 + 24;
        spr_y[2] = y0 + dy * f / 8 + 50;
        for (t = ticks; (unsigned char)(ticks - t) < 3;)
            tick_poll();
    }
    spr_on &= ~4;
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
        spot(0, s, (n > 2 ? 2 : n) - 1);
        --sbar[s];
        draw_bar_off();
    } else {
        n = count(shown[f]);
        if (!n)
            return;
        spot(f, s, (n > 4 ? 4 : n) - 1);
        draw_point(f, shown[f] - d);
    }
    x0 = ax;
    y0 = ay;
    /* a hit: the other's checker goes to the bar first */
    if (q_h[k] && t <= 24) {
        unsigned char hx, hy;
        spot(t, o, 0);
        hx = ax;
        hy = ay;
        draw_point(t, 0);
        spot(0, o, sbar[o] > 1 ? 1 : sbar[o]);
        glide(o, hx, hy);
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
        spot(t, s, n > 3 ? 3 : n);
        glide(s, x0, y0);
        draw_point(t, shown[t] + d);
    }
}

/* The panel: names, dice and the two buttons. */
static void draw_panel(void)
{
    unsigned char s, me = view, left_[4], n, k, x;
    /* the dice, in the right half's middle row: the ones still to play */
    ui_rect(mx(15, 12), MID, 12, 8, F_BLACK);
    hg_x = mx(15, 12);
    hg_y = MID;
    hg_w = 12;
    hg_h = 8;
    hg_fg = C_WHITE;
    hg_bg = wood();
    hgr_color();
    n = dice_left(left_);
    for (k = 0; k < n; ++k)
        if (left_[k]) {
            x = n == 4 ? 1 + 3 * k : 3 + 4 * k;
            hg_x = mx(15, 12) + (left ? 11 - x : x);
            hg_y = MID;
            hg_w = 1;
            hg_h = 8;
            hg_fg = C_BLACK;
            hg_bg = C_WHITE;
            hgr_color();
            hg_src = die_pic[left_[k] - 1];
            hgr_blit();
        }
    /* names */
    for (s = 0; s < 2; ++s) {
        place(30, s == me ? BOT : TOP, s, mono ? C_GREY : C_GREEN);
        ui_ink(F_WHITE, F_BLACK);
        ui_textn(33, s == me ? BOT + 4 : TOP + 4, g_name(s), 7);
        ui_textn(30, s == me ? BOT - 16 : TOP + 24, g_turn == s && !g_over ? "to play" : "", 10);
    }
    /* buttons */
    panel_offer();
    if (offer_a != shown_a || offer_b != shown_b) {
        shown_a = offer_a;
        shown_b = offer_b;
        ui_textn(30, 64, "", 10);
        ui_textn(30, 104, "", 10);
        /* (their targets, ID_A and ID_B, are made once, at the full draw) */
        if (offer_a[0])
            ui_button_draw(30, 64, offer_a);
        if (offer_b[0])
            ui_button_draw(30, 104, offer_b);
    }
}

static void bg_draw(void)
{
    unsigned char p, top;
    board_view();
    if (g_full) {
        sel = 0;
        shown_a = shown_b = 0;
        memset(shown, 99, sizeof shown);
        spr_load(SPR_GLIDE, spr_bg_glide);
        spr_ptr[2] = SPR_GLIDE;
        /* the wood: the board's 29 columns, lines 16 to 167 */
        hg_x = 0;
        hg_y = TOP;
        hg_w = 29;
        hg_h = 152;
        hg_fg = C_WHITE;
        hg_bg = wood();
        hgr_color();
        for (p = 1; p <= 24; ++p) {
            unsigned char x = col_of(p, &top);
            label(p, 0);
            ui_target(x * 4 + 8, top ? TOP + 36 : BOT - 20, 7, 36, p);
        }
        /* the bar: on my checkers there, below the cube in the middle */
        ui_target(mx(13, 2) * 4 + 8, MID + 24, 7, 8, ID_BAR);
        ui_target(mx(27, 2) * 4 + 8, MID, 7, 72, ID_OFF);
        ui_target(30 * 4 + 20, 68, 20, 4, ID_A);
        ui_target(30 * 4 + 20, 108, 20, 4, ID_B);
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

/* Highlight the chosen point's label and the labels of where it can go. */
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
