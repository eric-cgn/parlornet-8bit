/* ParlorNet on the Apple IIe and IIgs: Grid Words' board and rack (core/games/gridwords.c includes it).

   The look keeps clear of the commercial game's colors (docs/design/gridwords-colours.md). In color the
   board is black, word squares orange and letter squares violet, a triple a full square and a double a
   diamond, with a legend beside the board. A tile fills its square: in color a green square (violet for
   a blank, orange while it is being laid) with its letter in black; in monochrome a white square with its
   letter in black and its upper left and lower right corners cut away, which shapes the tiles of a word,
   and while it is being laid only those two corners, as marks, around its letter. On the board a color
   tile's letter is drawn a color pixel (two screen pixels) to each of the font's pixels, so it has no
   fringes; the rack's tiles are narrower and take the letter in bold. In monochrome the squares carry
   their names, 3W, 2W, 3L and 2L, the word squares between two lines. A tile shows its letter only, in
   the middle of the tile, a blank's in lower case; the points of the tile under the pointer, on the
   board or the rack, are on the message line, and Open-Apple-V shows every letter's points, as the
   table's TIL lines give them. The rack's tiles are 10 pixels wide, drawn the same way. */
#include "apple2.h"
#ifndef IIGS
#include "dhr.h"
#endif

#define X0 0                            /* the board: 2 bytes by 10 lines a square */
#define Y0 10
#define RACK_Y 82                       /* the rack's tiles (10 lines), the swap marks below */
#define RACK_PX 210                     /* the rack's first tile (pixels); each is RACK_W across */
#define RACK_W 10

static unsigned char shown[N][N];
static unsigned char shown_rack[8], shown_used, shown_marks;

static const char *const labels[] = {"", "2L", "3L", "2W", "3W"};
extern const unsigned char spr_gw_2l[], spr_gw_3l[], spr_gw_2w[], spr_gw_3w[];
static const unsigned char *const prm_spr[] = {0, spr_gw_2l, spr_gw_3l, spr_gw_2w, spr_gw_3w};

extern const unsigned char font[], hgr_ylo[], hgr_yhi[];

/* Tile looks (tile_draw). */
enum { T_INV, T_MARKS, T_COLOUR, T_PLAIN };
/* Colors of a color tile: which screen pixels of a pair are lit (1 the odd one, 2 the even one), and
   the palette bit. */
#define C_GREEN  0x01
#define C_VIOLET 0x02
#define C_ORANGE 0x81

static unsigned char *trow;
static unsigned char tb, ts, tcol;
static unsigned int tcover;

/* Write the line's pixels: the tile covers tcover's bits (from pixel tb * 7 + ts), lit as in on. On the
   IIe the kit writes them into Double Hi-Res (DK_HROW), as Hi-Res bytes of which only the tile's pixels
   change. */
static void tile_line(unsigned char y, unsigned int on)
{
    unsigned char k, mask, bits;
    unsigned int c = tcover;
#ifdef IIGS
    unsigned char old;
    ((unsigned char *)&trow)[0] = hgr_ylo[y];
    ((unsigned char *)&trow)[1] = hgr_yhi[y];
    trow += tb;
#endif
    for (k = 0; k < 3; ++k) {
        if (k == 0) {
            mask = (c << ts) & 0x7F;
            bits = (on << ts) & 0x7F;
        } else {
            mask = (c >> (k * 7 - ts)) & 0x7F;
            bits = (on >> (k * 7 - ts)) & 0x7F;
        }
#ifdef IIGS
        if (!mask)
            continue;
        old = gs_scr_get(trow + k);     /* the screen is in bank $E0 (apple2.h) */
        bits |= old & ~mask & 0x7F;
        bits |= tcol ? tcol & 0x80 : old & 0x80;
        gs_scr_put(trow + k, bits);
#else
        hg_m[k] = mask;
        hg_p[k] = bits | (tcol ? tcol & 0x80 : 0x80);
#endif
    }
#ifndef IIGS
    hg_x = tb;
    hg_y = y;
    dhr_call(DK_HROW);
#endif
}

/* A tile w pixels across (14 on the board, RACK_W on the rack) and 10 lines down, at pixel px (even) and
   line y, showing ch in the middle. In color (T_COLOUR) the tile is all color col with its letter in
   black. In monochrome (T_INV) it is all white with its letter in black and its upper left and lower
   right corners cut away; a tile being laid shows only those two corners as short marks (T_MARKS).
   T_PLAIN is the letter alone. */
static void tile_draw(unsigned int px, unsigned char y, unsigned char w, char ch, unsigned char look,
                      unsigned char col)
{
    const unsigned char *g = font + (ch - 32) * 8;
    unsigned char top, h, l, r, gx, j, row, bold = look == T_COLOUR || (look == T_PLAIN && !mono);
    unsigned char wide = look == T_COLOUR && w == 14;
    unsigned int full = (1 << w) - 1, gl, fill = 0;
    unsigned int left = 7, right = 7u << (w - 4);   /* a corner's bar across: 3 pixels */
    tb = px / 7;
    ts = px % 7;
    tcover = full;
    tcol = look == T_COLOUR ? col : 0;
    /* the letter's lines with ink, centered in the tile's lines */
    for (top = 0; top < 8 && !g[top]; ++top)
        ;
    for (h = 8; h > top && !g[h - 1]; --h)
        ;
    h -= top;
    l = look == T_MARKS ? 1 + (7 - h) / 2 : (11 - h) / 2;
    /* its 5 columns (6 in bold) in the middle */
    gx = wide ? 2 : (look == T_MARKS ? w - 1 - 5 : w - 6) / 2;
    if (look == T_COLOUR)
        fill = (col & 3) == C_GREEN ? 0xAAAA : 0x5555;     /* odd or even pixels, from an even px */
    for (r = 0; r < 10; ++r) {
        gl = 0;
        if (r >= l && r < l + h) {
            row = (g[top + r - l] >> 1) & 0x1F;
            if (wide) {
                /* each pixel of the letter a color pixel (two screen pixels) */
                for (j = 0; j < 5; ++j)
                    if (row & (1 << j))
                        gl |= 3 << (2 * j);
            } else {
                gl = row;
                if (bold)
                    gl |= gl << 1;
            }
            gl <<= gx;
        }
        switch (look) {
        case T_MARKS:
            /* the upper left and lower right corners: a bar across on the tile's first (last) line and
               two lines down (up) from it, in the first (last) of its inner columns */
            if (r == 0)
                gl |= left;
            else if (r < 3)
                gl |= 1;
            else if (r == 6 || r == 7)
                gl |= 1 << (w - 2);
            else if (r == 8)
                gl |= right;
            break;
        case T_INV:
            /* the whole square white with the letter in black, so tiles side by side meet as the color
               ones do; only the upper left and lower right corners are cut away, two pixels of the first
               line and one of the second, and the same at the far end, which gives each tile its shape */
            gl = full & ~gl;
            if (r == 0)
                gl &= ~3u;
            else if (r == 1)
                gl &= ~1u;
            else if (r == 8)
                gl &= ~(1u << (w - 1));
            else if (r == 9)
                gl &= ~(3u << (w - 2));
            break;
        case T_COLOUR:
            gl = fill & full & ~gl;
            break;
        }
        tile_line(y + r, gl);
    }
}

static void draw_square(unsigned char x, unsigned char y, unsigned char c)
{
    unsigned char cx = X0 + 2 * x, cy = Y0 + 10 * y, k;
    shown[y][x] = c;
    ui_rect(cx, cy, 2, 10, F_BLACK);
    if (c >= 'A' || (c & 0x80)) {
        unsigned char letter = c & 0x7F;
        /* the letter alone (its points show when the pointer rests on it); a blank in lower case */
        if (mono)
            tile_draw(cx * 7, cy, 14, letter, c & 0x80 ? T_MARKS : T_INV, 0);
        else
            tile_draw(cx * 7, cy, 14, letter, T_COLOUR,
                      c & 0x80 ? C_ORANGE : letter >= 'a' ? C_VIOLET : C_GREEN);
        return;
    }
    if (c == 1 || c == 2) {
        /* the start of play: > across or v down, alone in the middle of the square, in an outline */
        tile_draw(cx * 7, cy, 14, c == 1 ? '>' : 'v', T_PLAIN, 0);
        ui_box(cx * 7, cy, 14, 10);
        return;
    }
    k = prm[y][x];
    if (!k) {
        hg_px = cx * 7 + 7;             /* a dot in the middle of an empty square */
        hg_y = cy + 5;
        hg_w = 1;
        hgr_hline();
        return;
    }
    if (mono) {
        hg_x = cx;
        hg_y = cy + 1;
        hgr_text(labels[k]);
        if (k >= 3) {                   /* word squares: a line above and below (tiles are solid) */
            hg_px = cx * 7 + 1;
            hg_y = cy;
            hg_w = 12;
            hgr_hline();
            hg_px = cx * 7 + 1;
            hg_y = cy + 9;
            hgr_hline();
        }
        return;
    }
    /* color: word squares orange, letter squares violet; a triple full, a double a diamond */
    hg_x = cx;
    hg_y = cy;
    hg_src = prm_spr[k];
    hgr_blit();
}

static void draw_rack(void)
{
    unsigned char i, c, on;
    if (!memcmp(shown_rack, rack, 8) && shown_used == used && shown_marks == swap_marks)
        return;
    memcpy(shown_rack, rack, 8);
    shown_used = used;
    shown_marks = swap_marks;
    ui_textn(30, RACK_Y - 10, g_seat >= 0 ? "Your rack" : "", 10);
    for (i = 0; i < 7; ++i) {
        c = i < nrack ? rack[i] : ' ';
        if (c == '_')
            c = '?';                    /* a blank */
        /* a tile still on the rack is a tile; one laid on the board shows as a plain letter */
        on = i < nrack && !(used & (1 << i));
        tile_draw(RACK_PX + RACK_W * i, RACK_Y, RACK_W, c, !on ? T_PLAIN : mono ? T_INV : T_COLOUR,
                  c == '?' ? C_VIOLET : C_GREEN);
        tile_draw(RACK_PX + RACK_W * i, RACK_Y + 10, RACK_W, swap_marks & (1 << i) ? '^' : ' ', T_PLAIN, 0);
    }
}

/* In color, what the squares mean: each color beside its name. */
static void legend(void)
{
    unsigned char k;
    for (k = 1; k <= 4; ++k) {
        hg_x = k & 1 ? 30 : 34;            /* even columns, where the colors come out right */
        hg_y = k <= 2 ? 50 : 61;
        hg_src = prm_spr[k];
        hgr_blit();
        ui_text(hg_x + 2, hg_y + 1, labels[k]);
    }
}

static void draw_panel(void)
{
    unsigned char s;
    static char b[12];
    for (s = 0; s < g_seats && s < 4; ++s) {
        hg_x = 30;
        hg_y = Y0 + s * 9;
        hgr_char(g_turn == s && !g_over ? '>' : ' ');
        ui_textn(31, Y0 + s * 9, g_name(s), 5);
        b[0] = '-';                     /* (a score can end below 0: the tiles left on the rack) */
        strcpy(b + (g_score[s] < 0), utoa10(g_score[s] < 0 ? -g_score[s] : g_score[s]));
        ui_textn(36, Y0 + s * 9, "    ", 4);
        ui_textn(40 - strlen(b), Y0 + s * 9, b, strlen(b));
    }
    strcpy(b, "Bag ");
    strcat(b, utoa10(bag));
    ui_textn(30, 40, b, 10);
    draw_rack();
    strncpy(b, last, 10);
    b[10] = 0;
    ui_textn(30, 150, b, 10);
    ui_textn(30, 158, last[0] ? utoa10(lastpts) : "", 10);
}

static void gw_draw(void)
{
    unsigned char x, y, c, i;
    if (g_full) {
        memset(shown, 0xFF, sizeof shown);
        memset(shown_rack, 0xFF, sizeof shown_rack);
        for (y = 0; y < N; ++y)
            for (x = 0; x < N; ++x)
                ui_target(((X0 + 2 * x) * 7 + 7) / 2, Y0 + 10 * y + 4, 3, 5, y * N + x);
        for (i = 0; i < 7; ++i)
            ui_target((RACK_PX + RACK_W * i + RACK_W / 2) / 2, RACK_Y + 5, 2, 5, ID_RACK + i);
        ui_button(30, 103, "Play", ID_PLAY);
        ui_button(30, 114, "Pass", ID_PASS);
        ui_button(30, 125, "Exchange", ID_SWAP);
        ui_button(30, 136, "Cancel", ID_CLEAR);
        if (!mono)
            legend();
    }
    for (y = 0; y < N; ++y)
        for (x = 0; x < N; ++x) {
            c = want(x, y);
            if (c != shown[y][x])
                draw_square(x, y, c);
            t_hw[y * N + x] = 3;
        }
    draw_panel();
}

static void redraw(void)
{
    ui_hide();
    gw_draw();
    ui_show();
}

/* OA-V: every letter's points, as the table's rules give them (its TIL lines), over the board until a
   key. */
static void values_chart(void)
{
    static char b[8];
    unsigned char i, v;
    ui_hide();
    ui_rect(X0, Y0, 30, 150, F_BLACK);
    ui_ink(F_WHITE, F_BLACK);
    ui_text(2, Y0 + 6, "Points for each letter");
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
        ui_text(3 + (i / 9) * 9, Y0 + 24 + (i % 9) * 12, b);
    }
    ui_text(2, Y0 + 138, "A key: back to the board");
    while (!kbd_get())
        ;
    g_redraw();
}
