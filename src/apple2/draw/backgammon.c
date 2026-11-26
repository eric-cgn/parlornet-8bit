/* ParlorNet on the Apple IIe and IIgs: Backgammon's board (core/games/backgammon.c includes it).

   On the IIe it is drawn in Double Hi-Res (dhr.h) after the C64's look: lanes of red and green on a wooden
   board, each ending in a tip toward the middle, and solid white and black checkers. A point shows up to
   four checkers and the bar up to two a side; on a taller stack the last one shown stands for itself and
   those not shown, and has that number cut out of it, a digit from 2 to 9 or else +, in its place's color.
   The dice and the cube are in the row across the middle and the trays on the right hold the counts borne
   off. The board is 14 groups of 28 dots (7 color pixels, the text's 28 columns): 12 points, the bar and the
   tray, mirrored when the player's home is on the left. Every place on the board is a group by 12 lines,
   drawn a whole byte at a time with its lane, its checker and its count in one go (d_place).

   In monochrome the board has drawings of its own, in single dots: the wood is black, the points are
   outlined and filled with a fine pattern by turns, a white checker has a black rim and a black one a white
   rim, so that each shows on any place, and the count is drawn in the other of white and black.

   The IIgs keeps the Hi-Res board, draw/backgammon_hgr.c. */
#ifdef IIGS
#include "draw/backgammon_hgr.c"
#else
#include "apple2.h"
#include "dhr.h"

#ifdef DHR_TIMING
#define MARK(v) (*(unsigned char *)0x03CF = (v))   /* for timing in MAME (test/mame/run.lua's watch) */
#else
#define MARK(v)
#endif

extern const unsigned char dhr_c_disc[], dhr_c_tipd[], dhr_face[];
extern const unsigned char dhr_m_rim[], dhr_m_disc[], dhr_m_tipdf[], dhr_m_tipdl[], dhr_m_side[];
extern const unsigned char dhr_m_n2[], dhr_m_n3[], dhr_m_n4[], dhr_m_n5[], dhr_m_n6[], dhr_m_n7[], dhr_m_n8[],
    dhr_m_n9[], dhr_m_nplus[];
static const unsigned char *const m_num[] = {dhr_m_n2, dhr_m_n3, dhr_m_n4, dhr_m_n5, dhr_m_n6, dhr_m_n7,
                                             dhr_m_n8, dhr_m_n9, dhr_m_nplus};

/* The masks, 12 lines each, in the auxiliary memory with the kit (art/apple2/backgammon.dhr), which only the
   kit reads: the disc and its rim, a tip pointing down (up is the same from its last line up, UP), in
   monochrome filled (TIPD) and outlined (TIPDL), a die's face, a lane's sides (one line, for every line),
   and a number or a die's pips, which the kit makes (dhr_knum) */
extern const unsigned char dhr_knum[];
#define B_DISC (mono ? dhr_m_disc : dhr_c_disc)
#define B_RIM dhr_m_rim
#define B_NUM dhr_knum
#define B_TIPD (mono ? dhr_m_tipdf : dhr_c_tipd)
#define B_TIPDL dhr_m_tipdl
#define B_FACE dhr_face
#define B_SIDE dhr_m_side
#define UP 0xFC                         /* (a stride of -4) */

/* The kinds of place of a point (draw/backgammon_kinds.s), for d_column: a checker of seat 1 (white) or 0
   (black) on a red or green lane, KC + 2 * lane + (seat ^ 1); the tips, down and up; and the empty lanes. */
#define KC 0
#define KTD 4
#define KTU 6
#define KE 8

/* Colors (dhr.s), and an ink: a color for even lines and one for odd ones, ce + 16 * co */
#define D_BLACK 0
#define D_RED 1                         /* magenta, the nearest to the C64's red */
#define D_BROWN 8
#define D_ORANGE 9
#define D_GREEN 12
#define D_YELLOW 13
#define D_WHITE 15
#define INK(ce, co) ((ce) | (co) << 4)
#define SOLID(c) INK(c, c)
#define M_CHECK INK(5, 10)              /* monochrome: a checkerboard of dots */
#define M_LINES SOLID(8)                /* one dot in four: fine upright lines */

#define TOP 17                          /* the top lanes: places from line 17 down, the tips at 65 */
#define BOTP 146                        /* the bottom lanes: places from line 146 up, the tips at 98 */
#define BOT 157                         /* the board's last line */
#define MID 77                          /* the row across the middle, 77 to 97 */
#define PH 12                           /* a place: a group by 12 lines */
#define G_BAR 6                         /* the bar's group and the tray's, in the right-hand layout */
#define G_TRAY 13
#define G_PANEL 15                      /* the names' checkers, at text column 30 */

/* A place's kind, for its lane (or bar, tray ...) */
#define K_RED 0                         /* odd points: red, or outlined */
#define K_GREEN 1                       /* even points and the names: green, or the fine pattern */
#define K_BAR 2
#define K_TRAY 3

static signed char shown[25];
static unsigned char sbar[2], soff[2];
static char shown_panel[24];
static unsigned char ax, ay;            /* a place: color pixel, line */
static unsigned char nl;                /* layers set so far */
static unsigned char ready;             /* the kinds and masks are for mono + 1 */

/* n bytes from p set to v (memset would add itself to the resident program). */
static void fill(unsigned char *p, unsigned int n, unsigned char v)
{
    while (n--)
        *p++ = v;
}

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

/* The next layer: mask m (0: every dot), moving on a line a line (ms 4) or not (0), in ink k. */
static void layer(const unsigned char *m, unsigned char ms, unsigned char k)
{
    dl_m[nl] = m;
    dl_ms[nl] = ms;
    dl_ce[nl] = k & 15;
    dl_co[nl] = k >> 4;
    ++nl;
}

/* Draw the layers on w groups from g, h lines from y. */
static void go(unsigned char g, unsigned char y, unsigned char w, unsigned char h)
{
    dl_x = g;
    dl_y = y;
    dl_w = w;
    dl_h = h;
    dl_n = nl;
    dhr_place();
    nl = 0;
}

static unsigned char wood(void)
{
    return mono ? SOLID(D_BLACK) : SOLID(D_ORANGE);
}

static unsigned char checker_ink(unsigned char s)
{
    return s ? SOLID(D_WHITE) : SOLID(D_BLACK);
}

/* The first layer of a place of kind k; in monochrome a checker covers an outlined lane's sides, as a
   checker on a real board covers its point. */
static void ground(unsigned char k, unsigned char checker)
{
    if (!mono)
        layer(0, 0, k == K_RED ? SOLID(D_RED) : k == K_GREEN ? SOLID(D_GREEN) : SOLID(D_BROWN));
    else if (k == K_GREEN)
        layer(0, 0, M_CHECK);
    else if (k == K_BAR)
        layer(0, 0, M_LINES);
    else
        layer(checker ? 0 : B_SIDE, 0, checker ? SOLID(D_BLACK) : SOLID(D_WHITE));
}

/* The kinds of place for the display in use, into the kit (d_types; draw/backgammon_kinds.s). */
extern const unsigned char dhr_bg_kinds[], dhr_bg_mkinds[];
static void prepare(void)
{
    if (ready == mono + 1)
        return;
    ready = mono + 1;
    dl_m[0] = mono ? dhr_bg_mkinds : dhr_bg_kinds;
    dhr_call(DK_TYPES);
}

static void label(unsigned char p, unsigned char hi)
{
    unsigned char top, g = col_of(p, &top);
    if (hi)
        ui_ink(F_BLACK, mono ? F_WHITE : F_DHR(D_YELLOW));
    else
        ui_ink(F_WHITE, F_BLACK);
    hg_x = 2 * g;
    hg_y = top ? GAME_TOP : BOT + 1;
    hgr_char(p < 10 ? ' ' : '0' + p / 10);
    hgr_char('0' + p % 10);
    ui_ink(F_WHITE, F_BLACK);
}

/* The points' numbers above and below the board, each row in one go, and their targets: on the middle of
   each point's 7 pixels, and only its own width. */
static void label_rows(void)
{
    static char rows[2][29];
    unsigned char c, g, t, p, tens;
    fill((unsigned char *)rows, sizeof rows, ' ');
    rows[0][28] = rows[1][28] = 0;
    for (c = 0; c < 12; ++c) {
        g = c < 6 ? c : c + 1;
        if (left)
            g = 13 - g;
        for (t = 0; t < 2; ++t) {
            p = t ? 13 + c : 12 - c;    /* (the top row from 13, the bottom one from 12, as col_of has them) */
            if (view)
                p = 25 - p;
            tens = p >= 20 ? 2 : p >= 10;
            rows[t][2 * g] = tens ? '0' + tens : ' ';
            rows[t][2 * g + 1] = '0' + p - (tens == 2 ? 20 : tens ? 10 : 0);
            ui_target(7 * g + 3, t ? TOP + 30 : BOT - 30, 3, 30, p);
        }
    }
    ui_textn(0, GAME_TOP, rows[1], 28);
    ui_textn(0, BOT + 1, rows[0], 28);
}

#define PLUS 100

/* In color: v (up to 99, or PLUS) in dhr_knum in whole color pixels, so that it has no colored fringe: two
   digits at pixels 0 and 4, or one (or +) from pixel 4 - sh (the kit's d_num). */
static void number(unsigned char v, unsigned char sh)
{
    dl_v = v;
    dl_sh = sh;
    dhr_call(DK_NUM);
}

/* A place at group g, line y, of kind k, with a checker of seat s (white for seat 1) or none (s 2); n,
   when not 0, the number it stands for (2 or more), cut out of it: the digit, or + for 10 or more, in the
   place's color. In monochrome the place's pattern would break up so small a digit, so it is drawn in the
   other of white and black. */
static void place(unsigned char g, unsigned char y, unsigned char s, unsigned char k, unsigned char n)
{
    ground(k, s < 2);
    if (s < 2) {
        if (mono)
            layer(B_RIM, 4, checker_ink(s ^ 1));
        layer(B_DISC, 4, checker_ink(s));
        if (n) {
            if (mono)
                layer(m_num[n > 9 ? 8 : n - 2], 4, checker_ink(s ^ 1));
            else {
                number(n > 9 ? PLUS : n, 3);    /* (pixels 1 to 3 of the disc's 0 to 5) */
                layer(B_NUM, 4, k == K_RED ? SOLID(D_RED) : k == K_GREEN ? SOLID(D_GREEN) : SOLID(D_BROWN));
            }
        }
    }
    go(g, y, 1, PH);
}

/* A number up to 99 (the cube's value, a tray's count) on a place at group g, line y, in color fg on bg:
   in color in whole color pixels (pxnum), one digit in the middle; in monochrome the font's characters, in
   white or black. */
static void place_number(unsigned char g, unsigned char y, unsigned char v, unsigned char fg, unsigned char bg)
{
    if (mono) {
        ui_ink(fg == D_WHITE ? F_WHITE : F_BLACK, bg == D_WHITE ? F_WHITE : F_BLACK);
        hg_x = 2 * g;
        hg_y = y + 2;
        hgr_char(v >= 10 ? '0' + v / 10 : ' ');
        hgr_char('0' + v % 10);
        ui_ink(F_WHITE, F_BLACK);
        return;
    }
    number(v, 2);
    layer(0, 0, SOLID(bg));
    layer(B_NUM, 4, SOLID(fg));
    go(g, y, 1, PH);
}

static unsigned char count(signed char v)
{
    return v < 0 ? -v : v;
}

/* Point p with v checkers on it (as pt[]: seat 0's more than 0): four places, up to four checkers, and its
   tip, as one column (d_column), top first; then the last checker again with the count it stands for when
   there are more. */
static void draw_point(unsigned char p, signed char v)
{
    unsigned char top, g = col_of(p, &top), n = count(v), i, s = v < 0, lane = (p & 1) ^ 1, k;
    for (i = 0; i < 4; ++i) {
        k = i < n ? KC + 2 * lane + (s ^ 1) : KE + lane;
        dl_col[top ? i : 4 - i] = k;
    }
    dl_col[top ? 4 : 0] = (top ? KTD : KTU) + lane;
    dl_x = g;
    dl_y = top ? TOP : BOTP - 4 * PH;
    dl_cn = 5;
    dhr_call(DK_COLUMN);
    if (n > 4)
        place(g, top ? TOP + 3 * PH : BOTP - 3 * PH, s, lane ? K_GREEN : K_RED, n - 3);
    shown[p] = v;
}

static void draw_cube(unsigned char gb, unsigned char gt);

/* The bar (in the middle) and the trays (at the right of the board). */
static void draw_bar_off(void)
{
    unsigned char s, i, me = view, n, y;
    unsigned char gb = gmx(G_BAR, 1), gt = gmx(G_TRAY, 1);
    ground(K_BAR, 0);
    go(gb, TOP, 1, BOT - TOP + 1);
    for (s = 0; s < 2; ++s) {
        n = sbar[s];
        /* two places a side, mine below the middle: up to two checkers, the second with the count it
           stands for when there are more */
        for (i = 0; i < 2 && i < n; ++i)
            place(gb, s == me ? 98 + PH * i : 65 - PH * i, s, K_BAR, i == 1 && n > 2 ? n - 1 : 0);
    }
    /* the tray: wood, or in monochrome outlined, with a place for each side's count */
    if (mono)
        ground(K_TRAY, 0);
    else
        layer(0, 0, wood());
    go(gt, TOP, 1, BOT - TOP + 1);
    for (s = 0; s < 2; ++s) {
        y = s == me ? BOTP : TOP;
        place_number(gt, y, soff[s], D_WHITE, mono ? D_BLACK : D_BROWN);
    }
    draw_cube(gb, gt);
}

/* The doubling cube: white with its value (64 while nobody owns it, as on a real board), on the bar in the
   middle row while it is in the middle, else in the tray on its owner's side. */
static void draw_cube(unsigned char gb, unsigned char gt)
{
    unsigned char g = gb, y = 81;
    if (owner >= 0) {
        g = gt;
        y = owner == view ? BOTP - PH : TOP + PH;
    }
    if (mono) {
        layer(0, 0, SOLID(D_WHITE));
        go(g, y, 1, PH);
    }
    place_number(g, y, owner < 0 && cube == 1 ? 64 : cube, D_BLACK, D_WHITE);
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
   three sixtieths of a second each from the first, with what was under it kept and put back. */
static void glide(unsigned char s, unsigned char x0, unsigned char y0)
{
    static int dx, dy;
    unsigned char f, t0;
    dx = (int)ax - x0;
    dy = (int)ay - y0;
    t0 = ticks;
    for (f = 1; f < 8; ++f) {
        dl_sx = 4 * (x0 + dx * f / 8);
        dl_y = y0 + dy * f / 8;
        dl_h = PH;
        dl_n = 0;
        if (mono) {
            dl_m[0] = B_RIM;
            dl_ce[0] = checker_ink(s ^ 1) & 15;
            dl_n = 1;
        }
        dl_m[dl_n] = B_DISC;
        dl_ce[dl_n] = checker_ink(s) & 15;
        ++dl_n;
        dhr_call(DK_GLIDE);
        while ((unsigned char)(ticks - t0) < 3 * f)
            tick_poll();
        dhr_call(DK_UNGLIDE);
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
    unsigned char s, me = view, left_[4], n, k, g;
    static char key[24];
    /* the dice in the right half's middle row: the ones still to play (a die is blanked as it is played,
       and comes back with Undo); four for doubles, from the group after next to the cube's, so that black
       shows between the cube and the first die (a die's last column is black too, between the dice) */
    layer(0, 0, wood());
    go(gmx(7, 6), MID, 6, 97 - MID + 1);
    n = dice_left(left_);
    for (k = 0; k < n; ++k)
        if (left_[k]) {
            g = gmx(n == 4 ? 8 + k : 8 + 2 * k, 1);
            dl_v = left_[k];
            dhr_call(DK_PIPS);
            layer(0, 0, wood());
            layer(B_FACE, 4, SOLID(D_WHITE));
            layer(B_NUM, 4, SOLID(D_BLACK));
            go(g, 81, 1, PH);
        }
    /* names */
    for (s = 0; s < 2; ++s) {
        place(G_PANEL, s == me ? BOTP : TOP, s, K_GREEN, 0);
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
    unsigned char p;
    board_view();
    MARK(g_full ? 3 : 1);
    if (g_full) {
        sel = 0;
        shown_panel[0] = 1;
        fill((unsigned char *)shown, sizeof shown, 99);
        prepare();
        /* the wood of the middle row (the lanes, the bar and the trays cover the rest) */
        layer(0, 0, wood());
        go(0, MID, 14, 97 - MID + 1);
        label_rows();
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
