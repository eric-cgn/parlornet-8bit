/* ParlorNet on the Commodore 64: Reversi's board (core/games/reversi.c includes it). A square is two cells
   by two (16 by 16 pixels), the squares in the two greens by turns, so that every cell has just the colors
   its square and its disc need: a disc is black or white on its square's green, and a legal move's mark a
   yellow dot. The 10 by 10 board fits the screen too. Beside the board, each player's disc, name and count.
   In monochrome the squares are two grays. */
#include "gfx.h"

extern const unsigned char pic_rv_disc[], pic_rv_hint[], pic_rv_empty[];

#define PX 25                           /* the players' panel */
static char shown[10][11];              /* what is on the screen ('h' a mark) */
static unsigned char x0, y0;

/* Square (x, y) as c ('.', '0', '1', 'h'). */
static void cell(char c, unsigned char x, unsigned char y)
{
    unsigned char sq = ((x + y) & 1) ? C_LGREEN : C_GREEN;
    if (mono)
        sq = ((x + y) & 1) ? C_LGREY : C_GREY;
    hg_x = x0 + x * 2;
    hg_y = y0 + y * 16;
    hg_w = 2;
    hg_h = 16;
    hg_bg = sq;
    hg_fg = c == '0' ? C_BLACK : c == '1' ? C_WHITE : mono ? C_WHITE : C_YELLOW;
    hgr_color();
    hg_src = c == '0' || c == '1' ? pic_rv_disc : c == 'h' ? pic_rv_hint : pic_rv_empty;
    hgr_blit();
}

/* A seat's disc at cell column x, line y. */
static void disc(unsigned char s, unsigned char x, unsigned char y)
{
    hg_x = x;
    hg_y = y;
    hg_w = 2;
    hg_h = 16;
    hg_bg = ui_fcol(F_BLACK);
    hg_fg = s ? C_WHITE : C_BLACK;
    if (!s)
        hg_bg = mono ? C_GREY : C_GREEN;    /* (black on the screen's own color would hardly show) */
    hgr_color();
    hg_src = pic_rv_disc;
    hgr_blit();
}

static void panel(void)
{
    unsigned char s, y;
    for (s = 0; s < 2; ++s) {
        y = 24 + s * 48;
        disc(s, PX, y);
        ui_ink(F_WHITE, F_BLACK);
        ui_textn(PX + 3, y + 4, g_turn == s && !g_over ? "<" : "", 1);
        ui_textn(PX, y + 24, g_name(s), 13);
        ui_textn(PX, y + 32, utoa10(count[s]), 6);
    }
}

static void rv_draw(void)
{
    unsigned char x, y;
    static char sq[3];
    static char c;
    if (!size)
        size = 8;
    x0 = size > 8 ? 2 : 3;
    y0 = size > 8 ? GAME_TOP : GAME_TOP + 8;
    if (g_full) {
        memset(shown, 0, sizeof shown);
        ui_ink(F_WHITE, F_BLACK);
        if (size <= 8)
            for (x = 0; x < size; ++x) {
                hg_x = x0 + x * 2;
                hg_y = GAME_TOP;
                hgr_char(' ');
                hgr_char('a' + x);
            }
        for (y = 0; y < size; ++y) {
            if (size <= 8) {
                hg_x = x0 - 1;
                hg_y = y0 + y * 16 + 4;
                hgr_char('1' + y);
            }
            for (x = 0; x < size; ++x)
                ui_target((x0 + x * 2) * 4 + 8, y0 + y * 16 + 8, 7, 7, y * 16 + x);
        }
    }
    sq[2] = 0;
    for (y = 0; y < size; ++y)
        for (x = 0; x < size; ++x) {
            c = board[y][x];
            if (c == '.' && g_myturn) {
                sq[0] = 'a' + x;
                sq[1] = '1' + y;
                if (y < 9 && m_has(sq))
                    c = 'h';
            }
            if (c != shown[y][x]) {
                shown[y][x] = c;
                cell(c, x, y);
            }
        }
    ui_ink(F_WHITE, F_BLACK);
    panel();
}
