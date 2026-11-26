/* ParlorNet on the Commodore 64: the tutorial's squares (core/tutorial.c includes it). A square is five cells
   by four, in the two greens by turns as Reversi's are (two grays in monochrome), with your X in white or the
   partner's O, a ring, in black, from art/c64/tutorial.pic. */
#include "gfx.h"
extern const unsigned char pic_tu_x[], pic_tu_o[];

/* Square i as board[i] says. The text's colors are kept. */
static void square(unsigned char i)
{
    unsigned char odd = (i % 3 + i / 3) & 1, fg = hg_fg, bg = hg_bg;
    hg_x = BX + i % 3 * SW;
    hg_y = BY + i / 3 * SH;
    hg_w = SW;
    hg_h = SH;
    hg_bg = mono ? (odd ? C_LGREY : C_GREY) : odd ? C_LGREEN : C_GREEN;
    hg_fg = board[i] == 'X' ? C_WHITE : C_BLACK;
    if (board[i] == ' ') {
        hg_pat = 0;
        hgr_rect();
    } else {
        hgr_color();
        hg_src = board[i] == 'X' ? pic_tu_x : pic_tu_o;
        hgr_blit();
    }
    hg_fg = fg;
    hg_bg = bg;
}
