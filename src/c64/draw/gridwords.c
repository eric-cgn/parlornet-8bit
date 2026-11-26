/* ParlorNet on the Commodore 64: Grid Words' board and rack (core/games/gridwords.c includes it). A square is
   one cell, so a tile is its letter in the cell's two colors: in black on light green, a blank's (in lower
   case) on cyan, one being laid on yellow. An empty square is black with a gray dot; the premium squares
   keep clear of the commercial game's colors: word squares orange, letter squares purple, a triple the
   whole cell and a double a diamond, with a legend beside the board. The start of a play shows an arrow,
   across or down. Beside the board: the scores, the bag, the rack (seven tiles a cell apart, swap marks
   under them), the buttons and the last word. In monochrome a tile is white with its letter in black (one
   being laid the other way round) and the premium squares grays. */
#include "gfx.h"

#define X0 2                            /* the board's first cell column, and its top line */
#define Y0 16
#define PX 19                           /* the panel */
#define RACK_X 20                       /* the rack's first tile; each two cells on */
#define RACK_Y 96

static unsigned char shown[N][N];
static unsigned char shown_rack[8], shown_used, shown_marks;
static const char *const labels[] = {"", "2L", "3L", "2W", "3W"};
static unsigned char pic[10] = {1, 8};

static const unsigned char diamond[8] = {0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0x7E, 0x3C, 0x18};
static const unsigned char dot[8] = {0, 0, 0, 0x18, 0x18, 0, 0, 0};

/* A cell of the board in its two colors, with the 8 lines given. */
static void cell8(unsigned char x, unsigned char y, unsigned char fg, unsigned char bg, const unsigned char *g)
{
    hg_x = x;
    hg_y = y;
    hg_w = 1;
    hg_h = 8;
    hg_fg = fg;
    hg_bg = bg;
    hgr_color();
    memcpy(pic + 2, g, 8);
    hg_src = pic;
    hgr_blit();
}

/* A tile at (hg_x, hg_y), in hg_fg on hg_bg: its letter, c. The font draws a capital in the top 7 lines
   and keeps the last one for descenders, so a letter that reaches the top line and leaves the last one
   empty goes down a line. It then has a clear line above it and does not seem to run into the tile above.
   A blank's letter with a descender (g, j, p, q or y) needs all 8 lines and stays where it is, and the
   other small letters sit clear of the top already. */
static void tile8(char c)
{
    const unsigned char *g = (const unsigned char *)0xC000 + c * 8;     /* the font (c64.h) */
    unsigned char s = g[0] && !g[7];
    hg_w = 1;
    hg_h = 8;
    hgr_color();
    pic[2] = 0;
    memcpy(pic + 2 + s, g, 8 - s);
    hg_src = pic;
    hgr_blit();
}

/* A premium square's colors, by k (1 2L, 2 3L, 3 2W, 4 3W). */
static unsigned char prm_col(unsigned char k)
{
    if (mono)
        return k >= 3 ? C_LGREY : C_GREY;
    return k >= 3 ? C_ORANGE : C_PURPLE;
}

static void draw_square(unsigned char x, unsigned char y, unsigned char c)
{
    unsigned char cx = X0 + x, cy = Y0 + 8 * y, k;
    shown[y][x] = c;
    if (c >= 'A' || (c & 0x80)) {
        unsigned char letter = c & 0x7F, bg;
        if (mono) {
            hg_fg = c & 0x80 ? C_WHITE : C_BLACK;
            bg = c & 0x80 ? C_BLACK : C_WHITE;
        } else {
            hg_fg = C_BLACK;
            bg = c & 0x80 ? C_YELLOW : letter >= 'a' ? C_CYAN : C_LGREEN;
        }
        hg_bg = bg;
        hg_x = cx;
        hg_y = cy;
        tile8(letter);
        return;
    }
    if (c == 1 || c == 2) {
        hg_x = cx;
        hg_y = cy;
        hg_fg = mono ? C_WHITE : C_YELLOW;
        hg_bg = C_BLACK;
        hgr_char(c == 1 ? '>' : 'v');
        return;
    }
    k = prm[y][x];
    if (!k)
        cell8(cx, cy, C_DGREY, C_BLACK, dot);
    else if (k == 2 || k == 4)
        cell8(cx, cy, prm_col(k), prm_col(k), dot);    /* a triple: the whole square, in both colors */
    else
        cell8(cx, cy, prm_col(k), C_BLACK, diamond);
}

static void draw_rack(void)
{
    unsigned char i, c, on;
    if (!memcmp(shown_rack, rack, 8) && shown_used == used && shown_marks == swap_marks)
        return;
    memcpy(shown_rack, rack, 8);
    shown_used = used;
    shown_marks = swap_marks;
    ui_ink(F_WHITE, F_BLACK);
    ui_textn(PX, RACK_Y - 8, g_seat >= 0 ? "Your rack" : "", 21);
    for (i = 0; i < 7; ++i) {
        c = i < nrack ? rack[i] : ' ';
        if (c == '_')
            c = '?';                    /* a blank */
        /* a tile still on the rack is a tile; one laid on the board shows as a plain letter */
        on = i < nrack && !(used & (1 << i));
        hg_x = RACK_X + 2 * i;
        hg_y = RACK_Y;
        if (on) {
            hg_fg = C_BLACK;
            hg_bg = mono ? C_WHITE : c == '?' ? C_CYAN : C_LGREEN;
            tile8(c);
        } else {
            ui_ink(F_WHITE, F_BLACK);
            hgr_char(c);
        }
        ui_ink(F_WHITE, F_BLACK);
        hg_x = RACK_X + 2 * i;
        hg_y = RACK_Y + 8;
        hgr_char(swap_marks & (1 << i) ? '^' : ' ');
    }
}

/* In color, what the squares mean: each beside its name. */
static void legend(void)
{
    unsigned char k, x, y;
    for (k = 1; k <= 4; ++k) {
        x = k & 1 ? PX : PX + 5;
        y = k <= 2 ? 64 : 72;
        if (k == 2 || k == 4)
            cell8(x, y, prm_col(k), prm_col(k), dot);
        else
            cell8(x, y, prm_col(k), C_BLACK, diamond);
        ui_ink(F_WHITE, F_BLACK);
        ui_text(x + 1, y, labels[k]);
    }
}

static void draw_panel(void)
{
    unsigned char s;
    static char b[12];
    ui_ink(F_WHITE, F_BLACK);
    for (s = 0; s < g_seats && s < 4; ++s) {
        hg_x = PX;
        hg_y = Y0 + s * 8;
        hgr_char(g_turn == s && !g_over ? '>' : ' ');
        ui_textn(PX + 1, Y0 + s * 8, g_name(s), 13);
        b[0] = '-';                     /* (a score can end below 0: the tiles left on the rack) */
        strcpy(b + (g_score[s] < 0), utoa10(g_score[s] < 0 ? -g_score[s] : g_score[s]));
        ui_textn(PX + 14, Y0 + s * 8, b, 6);
    }
    strcpy(b, "Bag ");
    strcat(b, utoa10(bag));
    ui_textn(PX, 48, b, 10);
    draw_rack();
    ui_ink(F_WHITE, F_BLACK);
    strncpy(b, last, 11);
    b[11] = 0;
    ui_textn(PX, 160, b, 12);
    ui_textn(PX + 13, 160, last[0] ? utoa10(lastpts) : "", 5);
}

static void gw_draw(void)
{
    unsigned char x, y, c, i;
    if (g_full) {
        memset(shown, 0xFF, sizeof shown);
        memset(shown_rack, 0xFF, sizeof shown_rack);
        for (y = 0; y < N; ++y)
            for (x = 0; x < N; ++x)
                ui_target((X0 + x) * 4 + 2, Y0 + 8 * y + 4, 2, 4, y * N + x);
        for (i = 0; i < 7; ++i)
            ui_target((RACK_X + 2 * i) * 4 + 2, RACK_Y + 4, 4, 4, ID_RACK + i);
        ui_button(PX, 120, "Play", ID_PLAY);
        ui_button(PX + 7, 120, "Clear", ID_CLEAR);
        ui_button(PX, 136, "Swap", ID_SWAP);
        ui_button(PX + 7, 136, "Pass", ID_PASS);
        legend();
    }
    for (y = 0; y < N; ++y)
        for (x = 0; x < N; ++x) {
            c = want(x, y);
            if (c != shown[y][x])
                draw_square(x, y, c);
            t_hw[y * N + x] = 2;
        }
    draw_panel();
}

static void redraw(void)
{
    gw_draw();
}

/* OA-V: every letter's points, as the table's rules give them (its TIL lines), over the board until a
   key. */
static void values_chart(void)
{
    static char b[8];
    unsigned char i, v;
    ui_rect(0, GAME_TOP, 40, 168, F_BLACK);
    ui_ink(F_WHITE, F_BLACK);
    ui_text(2, Y0, "Points for each letter");
    for (i = 0; i < 27; ++i) {
        v = val[i];
        if (i < 26) {
            b[0] = 'A' + i;
            b[1] = ' ';
            b[2] = v >= 10 ? '0' + v / 10 : ' ';
            b[3] = '0' + v % 10;
            b[4] = 0;
        } else {
            strcpy(b, "Blank ");
            b[6] = '0' + v % 10;
            b[7] = 0;
        }
        ui_text(3 + (i / 9) * 9, Y0 + 16 + (i % 9) * 12, b);
    }
    ui_text(2, Y0 + 136, "A key: back to the board");
    while (!kbd_get())
        ;
    g_redraw();
}
