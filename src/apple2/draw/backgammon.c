/* ParlorNet on the Apple IIe and IIgs: Backgammon's board (core/games/backgammon.c includes it).

   The dhr-study branch draws it on the IIe in Double Hi-Res (dhr.s), after the C64's look: lanes of red and
   green on a wooden board, each ending in a tip toward the middle, solid white and black checkers, and on a
   stack taller than four the last one shown has the number not shown cut out of it in its lane's color (a
   digit to 9, else +). The dice and the cube are in the row across the middle and the trays on the right hold
   the counts borne off. The board is 14 groups of 7 color pixels (98 pixels, the text's 28 columns): 12
   points, the bar and the tray, mirrored when the player's home is on the left. A group is four whole bytes
   of the screen, so every place on the board is drawn a byte at a time (dhr_cells), each line of it in one
   go: a checker, its count and its lane's color together. Only a gliding checker and the pointer are drawn
   at any pixel. The IIgs (and a IIe build with BG_HGR) keeps the Hi-Res board, draw/backgammon_hgr.c. */
#if defined(IIGS) || defined(BG_HGR)
#include "draw/backgammon_hgr.c"
#else
#include "apple2.h"
#include "dhr.h"

#ifdef DHR_TIMING
#define MARK(v) (*(unsigned char *)0x0300 = (v))   /* for the study's timing in MAME */
#else
#define MARK(v)
#endif

/* Double Hi-Res colors (dhr.s), and in monochrome dithers of two of them, one on even lines and one on odd
   ones, so that a gray is a checkerboard of dots rather than stripes */
#define D_BLACK 0
#define D_RED 1                         /* magenta, the nearest to the C64's red */
#define D_DBLUE 2
#define D_GREY1 5
#define D_LBLUE 7
#define D_BROWN 8
#define D_ORANGE 9
#define D_GREY2 10
#define D_GREEN 12
#define D_YELLOW 13
#define D_WHITE 15
#define M_75 0x80                       /* three dots in four */
#define M_50 0x81                       /* every other dot */
#define M_25 0x82                       /* one in four */
static const unsigned char dith_e[] = {D_LBLUE, D_GREY2, D_DBLUE};
static const unsigned char dith_o[] = {D_YELLOW, D_GREY1, D_BROWN};

#define TOP 17                          /* the top lanes: places from line 17 down, the tips at 65 */
#define BOTP 146                        /* the bottom lanes: places from line 146 up, the tips at 98 */
#define BOT 157                         /* the board's last line */
#define MID 77                          /* the row across the middle, 77 to 97 */
#define PH 12                           /* a place: a group (7 pixels) by 12 lines */
#define G_BAR 6                         /* the bar's group and the tray's, in the right-hand layout */
#define G_TRAY 13
#define G_PANEL 15                      /* the names' checkers, at text column 30 */

/* Rows of 7 pixels, bit 7 the first: a checker (a disc 6 pixels by 11 lines, the place's color in its last
   column and line, between it and the next), the points' tips, a die's face and its pips. */
static const unsigned char disc[PH] = {0x30, 0x78, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0x78, 0x30, 0};
static const unsigned char tip_down[PH] = {0xFE, 0xFE, 0x7C, 0x7C, 0x7C, 0x38, 0x38, 0x38, 0x38, 0x10, 0x10,
                                           0x10};
static const unsigned char tip_up[PH] = {0x10, 0x10, 0x10, 0x38, 0x38, 0x38, 0x38, 0x7C, 0x7C, 0x7C, 0xFE,
                                         0xFE};
static const unsigned char face[PH] = {0x78, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC,
                                       0x78};
/* digits 3 pixels by 5 lines (bits 7 to 5), 0 to 9 and + */
static const unsigned char digit[11][5] = {
    {0xE0, 0xA0, 0xA0, 0xA0, 0xE0}, {0x40, 0xC0, 0x40, 0x40, 0xE0}, {0xE0, 0x20, 0xE0, 0x80, 0xE0},
    {0xE0, 0x20, 0x60, 0x20, 0xE0}, {0xA0, 0xA0, 0xE0, 0x20, 0x20}, {0xE0, 0x80, 0xE0, 0x20, 0xE0},
    {0xE0, 0x80, 0xE0, 0xA0, 0xE0}, {0xE0, 0x20, 0x40, 0x40, 0x40}, {0xE0, 0xA0, 0xE0, 0xA0, 0xE0},
    {0xE0, 0xA0, 0xE0, 0x20, 0xE0}, {0x00, 0x40, 0xE0, 0x40, 0x00}};
/* a die's pips, a pixel by 2 lines in its columns 1, 3 and 5: lines 2 to 3, 5 to 6 and 8 to 9 */
static const unsigned char pip_rows[6][3] = {
    {0, 0x10, 0}, {0x04, 0, 0x40}, {0x04, 0x10, 0x40}, {0x44, 0, 0x44}, {0x44, 0x10, 0x44}, {0x44, 0x44, 0x44}};
/* the gliding checker, for dhr_mask */
static const unsigned char glider[] = {6, 11, 0x30, 0x78, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0x78, 0x30};

static signed char shown[25];
static unsigned char sbar[2], soff[2], drawn_sel;
static char shown_panel[24];
static unsigned char ax, ay;            /* a place: color pixel, line */
static unsigned char rows[PH];          /* a place's rows, made up */

/* Where group g of the right-hand layout goes: mirrored for the left-hand one. */
static unsigned char gmx(unsigned char g, unsigned char w)
{
    return left ? 14 - g - w : g;
}

/* Where point p is drawn: its group and whether it is in the top row. */
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
    return gmx(c < 6 ? c : c + 1, 1);
}

static unsigned char wood(void)
{
    return mono ? M_75 : D_ORANGE;
}

static unsigned char lane_col(unsigned char p)
{
    return (p & 1) ? (mono ? M_25 : D_RED) : (mono ? M_50 : D_GREEN);
}

static unsigned char bar_col(void)
{
    return mono ? D_GREY1 : D_BROWN;
}

static unsigned char checker_col(unsigned char s)
{
    return s ? D_WHITE : D_BLACK;
}

/* Groups g to g + w - 1, lines y to y + h - 1: the rows r (0: none) in color fg, the rest in bg (0xFF:
   left as it is). A color 0x80 and up is a dither. */
static void cells(unsigned char g, unsigned char y, unsigned char w, unsigned char h, const unsigned char *r,
                  unsigned char fg, unsigned char bg)
{
    dh_x = g;
    dh_y = y;
    dh_w = w;
    dh_h = h;
    dh_src = r;
    dh_c = fg & 0x80 ? dith_e[fg & 0x7F] : fg;
    dh_c2 = fg & 0x80 ? dith_o[fg & 0x7F] : 0x80;
    dh_b = bg != 0xFF && bg & 0x80 ? dith_e[bg & 0x7F] : bg;
    dh_b2 = bg != 0xFF && bg & 0x80 ? dith_o[bg & 0x7F] : 0x80;
    dhr_cells();
}

/* n (up to 99) in rows 3 to 7 of rows[], in digits 3 by 5: two at pixels 0 and 4, one at 2. */
static void number(unsigned char n)
{
    unsigned char i;
    memset(rows, 0, PH);
    for (i = 0; i < 5; ++i)
        rows[3 + i] = n >= 10 ? digit[n / 10][i] | digit[n % 10][i] >> 4 : digit[n][i] >> 2;
}

static void label(unsigned char p, unsigned char hi)
{
    unsigned char top, g = col_of(p, &top);
    ui_ink(F_WHITE, F_BLACK);
    if (hi) {
        dh_tfg = D_BLACK;
        dh_tbg = mono ? D_WHITE : D_YELLOW;
    }
    hg_x = 2 * g;
    hg_y = top ? GAME_TOP : BOT + 1;
    hgr_char(p < 10 ? ' ' : '0' + p / 10);
    hgr_char('0' + p % 10);
    dh_tfg = dh_tbg = 0x80;
}

/* A place at group g, line y, in color bg, with a checker of seat s (white for seat 1) or none (s 2); n,
   when not 0, the checkers past the ones shown on a stack, cut out of this one: the digit, or + for 10 or
   more, in the place's color. In monochrome a place is a pattern of dots, which would break up so small a
   digit, so it shows in the other of white and black. */
static void place(unsigned char g, unsigned char y, unsigned char s, unsigned char bg, unsigned char n)
{
    unsigned char i;
    const unsigned char *d = digit[n > 9 ? 10 : n];
    if (s > 1) {
        cells(g, y, 1, PH, 0, 0, bg);
        return;
    }
    memcpy(rows, disc, PH);
    if (n && !mono)
        for (i = 0; i < 5; ++i)
            rows[3 + i] &= ~(d[i] >> 1);
    cells(g, y, 1, PH, rows, checker_col(s), bg);
    if (n && mono) {
        memset(rows, 0, PH);
        for (i = 0; i < 5; ++i)
            rows[3 + i] = d[i] >> 1;
        cells(g, y, 1, PH, rows, checker_col(s ^ 1), 0xFF);
    }
}

static unsigned char count(signed char v)
{
    return v < 0 ? -v : v;
}

/* Point p with v checkers on it (as pt[]: seat 0's more than 0). */
static void draw_point(unsigned char p, signed char v)
{
    unsigned char top, g = col_of(p, &top), n = count(v), i, s = v < 0, c = lane_col(p);
    /* four places: up to four checkers, the last with the count of any more */
    for (i = 0; i < 4; ++i)
        place(g, top ? TOP + PH * i : BOTP - PH * i, i < n ? s : 2, c, i == 3 && n > 4 ? n - 4 : 0);
    cells(g, top ? TOP + 4 * PH : BOTP - 4 * PH, 1, PH, top ? tip_down : tip_up, c, wood());
    shown[p] = v;
}

static void draw_cube(unsigned char gb, unsigned char gt);

/* The bar (in the middle) and the trays (at the right of the board). */
static void draw_bar_off(void)
{
    unsigned char s, i, me = view, n, y;
    unsigned char gb = gmx(G_BAR, 1), gt = gmx(G_TRAY, 1), bc = bar_col();
    cells(gb, TOP, 1, BOT - TOP + 1, 0, 0, bc);
    for (s = 0; s < 2; ++s) {
        n = sbar[s];
        /* two places a side, mine below the middle: up to two checkers, the second with the count of
           any more */
        for (i = 0; i < 2 && i < n; ++i)
            place(gb, s == me ? 98 + PH * i : 65 - PH * i, s, bc, i == 1 && n > 2 ? n - 2 : 0);
    }
    cells(gt, TOP, 1, BOT - TOP + 1, 0, 0, wood());
    for (s = 0; s < 2; ++s) {
        number(soff[s]);
        cells(gt, s == me ? BOTP : TOP, 1, PH, rows, D_WHITE, bc);
    }
    draw_cube(gb, gt);
}

/* The doubling cube: white with its value (64 while nobody owns it, as on a real board), on the bar in the
   middle row while it is in the middle, else in the tray on its owner's side. */
static void draw_cube(unsigned char gb, unsigned char gt)
{
    unsigned char v = owner < 0 && cube == 1 ? 64 : cube;
    number(v);
    if (owner < 0)
        cells(gb, 81, 1, PH, rows, D_BLACK, D_WHITE);
    else
        cells(gt, owner == view ? BOTP - PH : TOP + PH, 1, PH, rows, D_BLACK, D_WHITE);
}

/* (ax, ay): the place of seat s's checker i on point p, the bar (0) or the tray (25). */
static void spot(unsigned char p, unsigned char s, unsigned char i)
{
    unsigned char top;
    if (!p) {
        ax = 7 * gmx(G_BAR, 1);
        ay = s == view ? 98 + PH * i : 65 - PH * i;
    } else if (p == 25) {
        ax = 7 * gmx(G_TRAY, 1);
        ay = s == view ? BOTP : TOP;
    } else {
        ax = 7 * col_of(p, &top);
        ay = top ? TOP + PH * i : BOTP - PH * i;
    }
}

/* A checker of seat s gliding from (x0, y0) to (ax, ay): drawn over the board at seven places between,
   three sixtieths of a second each, with what was under it kept and put back. */
static void glide(unsigned char s, unsigned char x0, unsigned char y0)
{
    static int dx, dy;
    unsigned char f, t, x, y;
    dx = (int)ax - x0;
    dy = (int)ay - y0;
    for (f = 1; f < 8; ++f) {
        x = x0 + dx * f / 8;
        y = y0 + dy * f / 8;
        dh_x = x;
        dh_y = y;
        dh_w = 6;
        dh_h = 11;
        dhr_save();
        dh_src = glider;
        dh_c = checker_col(s);
        dh_c2 = 0x80;
        dhr_mask();
        for (t = ticks; (unsigned char)(ticks - t) < 3;)
            tick_poll();
        dh_x = x;
        dh_y = y;
        dh_w = 6;
        dh_h = 11;
        dhr_restore();
    }
}

static void play_step(unsigned char k)
{
    unsigned char s = q_s[k], f = q_f[k], t = q_t[k], o = s ^ 1, n, x0, y0;
    signed char d = s ? -1 : 1;
    MARK(2);
    /* off the place it leaves: the last checker shown */
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
    MARK(1);
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

/* The panel: names, dice and the two buttons. */
static void draw_panel(void)
{
    unsigned char s, me = view, left_[4], n, k, g, i;
    static char key[24];
    /* the dice in the right half's middle row: the ones still to play (a die is blanked as it is played,
       and comes back with Undo); four for doubles */
    cells(gmx(7, 6), MID, 6, 97 - MID + 1, 0, 0, wood());
    n = dice_left(left_);
    for (k = 0; k < n; ++k)
        if (left_[k]) {
            g = gmx(n == 4 ? 7 + k : 8 + 2 * k, 1);
            cells(g, 81, 1, PH, face, D_WHITE, wood());
            memset(rows, 0, PH);
            for (i = 0; i < 3; ++i)
                rows[2 + 3 * i] = rows[3 + 3 * i] = pip_rows[left_[k] - 1][i];
            cells(g, 81, 1, PH, rows, D_BLACK, 0xFF);
        }
    /* names */
    for (s = 0; s < 2; ++s) {
        place(G_PANEL, s == me ? BOTP : TOP, s, mono ? M_50 : D_GREEN, 0);
        ui_textn(32, s == me ? BOT - 9 : TOP + 2, g_name(s), 8);
        ui_textn(30, s == me ? BOT - 20 : TOP + 13, g_turn == s && !g_over ? "to play" : "", 9);
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
    MARK(g_full ? 3 : 1);
    if (g_full) {
        if (!dhr_on) {
            /* (the table's frame was drawn in Hi-Res before the board's first view) */
            dhr_enter();
            g_title();
        }
        sel = 0;
        drawn_sel = 0;
        shown_panel[0] = 1;
        memset(shown, 99, sizeof shown);
        /* the wood of the middle row (the lanes, the bar and the trays cover the rest) */
        cells(0, MID, 14, 97 - MID + 1, 0, 0, wood());
        for (p = 1; p <= 24; ++p) {
            unsigned char g = col_of(p, &top);
            label(p, 0);
            /* the middle of the point's 7 pixels, and only its own width */
            ui_target(7 * g + 3, top ? TOP + 30 : BOT - 30, 3, 30, p);
        }
        /* the bar: on my checkers there, below the cube in the middle */
        ui_target(7 * gmx(G_BAR, 1) + 3, 104, 3, 12, ID_BAR);
        ui_target(7 * gmx(G_TRAY, 1) + 3, 90, 3, 60, ID_OFF);
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
    MARK(0);
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
#endif
