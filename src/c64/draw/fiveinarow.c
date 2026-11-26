/* ParlorNet on the Commodore 64: Five in a Row's board (core/games/fiveinarow.c includes it). A point is one
   cell, 8 by 8 pixels, on a wooden board: the grid's lines cross in its middle, and a stone, black or white,
   covers them, so each cell needs only the wood's color and the line's or the stone's. The last stone has a
   hole in its middle, and the stones of the winning line are rings. Beside the board, each player's stone,
   name and count, and the rules. In monochrome the wood is gray. */
#include "gfx.h"

#define X0 3                            /* the board's first cell column, and its top line */
#define Y0 16
#define PX 20                           /* the panel's column */

static char shown[MAXN][MAXN];
static unsigned char pic[2 + 8] = {1, 8};

static const unsigned char stone[3][8] = {
    {0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C},       /* a stone */
    {0x3C, 0x7E, 0xFF, 0xE7, 0xE7, 0xFF, 0x7E, 0x3C},       /* the last one */
    {0x3C, 0x7E, 0xC3, 0xC3, 0xC3, 0xC3, 0x7E, 0x3C}};      /* on the winning line */

static void draw_point(unsigned char x, unsigned char y, char c)
{
    unsigned char r, b, last = size - 1;
    shown[y][x] = c;
    hg_x = X0 + x;
    hg_y = Y0 + y * 8;
    hg_w = 1;
    hg_h = 8;
    hg_bg = mono ? C_LGREY : C_YELLOW;
    if (c & 3) {
        hg_fg = (c & 3) == 1 ? C_BLACK : C_WHITE;
        memcpy(pic + 2, stone[c & 8 ? 2 : c & 4 ? 1 : 0], 8);
    } else {
        /* the grid: across row 3, down column 3 (bit 4), stopping at the board's edges */
        hg_fg = mono ? C_DGREY : C_BROWN;
        for (r = 0; r < 8; ++r) {
            b = 0;
            if (r == 3)
                b = (x ? 0xF0 : 0x1F) & (x < last ? 0xFF : 0xF0) | 0x10;
            else if ((r < 3 && y) || (r > 3 && y < last))
                b = 0x10;
            pic[2 + r] = b;
        }
    }
    hgr_color();
    hg_src = pic;
    hgr_blit();
}

static void panel(void)
{
    unsigned char s, y;
    static char b[14];
    for (s = 0; s < 2; ++s) {
        y = Y0 + s * 40;
        hg_x = PX;
        hg_y = y;
        hg_w = 1;
        hg_h = 8;
        hg_bg = mono ? C_LGREY : C_YELLOW;
        hg_fg = s ? C_WHITE : C_BLACK;
        memcpy(pic + 2, stone[0], 8);
        hgr_color();
        hg_src = pic;
        hgr_blit();
        ui_ink(F_WHITE, F_BLACK);
        ui_textn(PX + 2, y, g_turn == s && !g_over ? "to play" : "", 8);
        ui_textn(PX, y + 8, g_name(s), 19);
        strcpy(b, s ? "White, " : "Black, ");
        strcat(b, utoa10(stones[s]));
        ui_textn(PX, y + 16, b, 19);
    }
    ui_textn(PX, 112, rul[0] == 's' ? "Exactly 5 in a row" : "5 or more in a row", 19);
    ui_textn(PX, 120, "wins.", 19);
}

static void fr_draw(void)
{
    unsigned char x, y;
    if (too_big) {
        if (g_full) {
            ui_text(1, 40, "This board is larger than 15 by 15.");
            ui_text(1, 56, "Play it on the web or by telnet.");
        }
        return;
    }
    if (g_full) {
        memset(shown, 0xFF, sizeof shown);
        ui_ink(F_WHITE, F_BLACK);
        for (x = 0; x < size; ++x) {
            hg_x = X0 + x;
            hg_y = GAME_TOP;
            hgr_char('a' + x);
        }
        for (y = 0; y < size; ++y) {
            ui_textn(X0 - 3, Y0 + y * 8, utoa10(y + 1), 2);
            for (x = 0; x < size; ++x)
                ui_target((X0 + x) * 4 + 2, Y0 + y * 8 + 4, 2, 4, y * size + x);
        }
    }
    for (y = 0; y < size; ++y)
        for (x = 0; x < size; ++x)
            if (cell[y][x] != shown[y][x])
                draw_point(x, y, cell[y][x]);
    panel();
}
