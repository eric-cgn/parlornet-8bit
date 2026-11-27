/* ParlorNet on the Apple IIe and IIgs: the tutorial's squares (core/tutorial.c includes it). A square is drawn
   whole from art/apple2/tutorial.spr through hgr_blit, as the games draw theirs: green with your white X or
   the partner's black ring in color, and an outline with the mark in single dots in monochrome. */
#include "apple2.h"
extern const unsigned char spr_tu_empty[], spr_tu_x[], spr_tu_o[];
extern const unsigned char spr_tu_empty_o[], spr_tu_x_o[], spr_tu_o_o[];
extern const unsigned char spr_tu_m_empty[], spr_tu_m_x[], spr_tu_m_o[];

/* Color at an even byte, at an odd one, monochrome; each empty, X, O. (One list and not two dimensions,
   which would bring a multiplication from the runtime into the resident program.) */
static const unsigned char *const tu_spr[9] = {
    spr_tu_empty, spr_tu_x, spr_tu_o,
    spr_tu_empty_o, spr_tu_x_o, spr_tu_o_o,
    spr_tu_m_empty, spr_tu_m_x, spr_tu_m_o};

/* Square i as board[i] says. */
static void square(unsigned char i)
{
    unsigned char k;
    hg_x = BX + i % 3 * SW;
    hg_y = BY + i / 3 * SH;
    k = board[i] == 'X' ? 1 : board[i] == 'O' ? 2 : 0;
    hg_src = tu_spr[k + (mono ? 6 : hg_x & 1 ? 3 : 0)];
    hgr_blit();
}
