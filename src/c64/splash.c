/* ParlorNet on the Commodore 64: the title screen shown at start-up, an overlay (ovl.h). PARLOR over NET in
   the title's block letters (art/splash.txt), a block to a cell: each block a raised tile, each row of
   blocks a color of its own, warm for PARLOR and cool for NET, on black. In monochrome the rows are grays.
   main.c keeps it up until a key is pressed. */
#include "machine.h"
#include "c64.h"
#include "ovl.h"
#include "gfx.h"
#include "version.h"

/* (tools/c64_art.py) Each letter: its width in blocks, then a byte a row of 9, the blocks in bits 7 down. */
extern const unsigned char sg_P[], sg_A[], sg_R[], sg_L[], sg_O[], sg_N[], sg_E[], sg_T[];

static const unsigned char *const parlor_word[] = {sg_P, sg_A, sg_R, sg_L, sg_O, sg_R, 0};
static const unsigned char *const net_word[] = {sg_N, sg_E, sg_T, 0};
static const unsigned char warm[9] = {C_YELLOW, C_YELLOW, C_LRED, C_LRED, C_ORANGE, C_ORANGE, C_RED, C_RED,
                                      C_BROWN};
static const unsigned char cool[9] = {C_WHITE, C_LGREEN, C_CYAN, C_CYAN, C_LBLUE, C_LBLUE, C_PURPLE, C_PURPLE,
                                      C_BLUE};
/* A block: lit, with a shadow on its right and bottom edge. */
static const unsigned char tile[] = {1, 8, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0x00};

static void word(const unsigned char *const *w, unsigned char row, const unsigned char *colors)
{
    unsigned char total = 0, x0, r, c, bits;
    const unsigned char *const *p;
    const unsigned char *g;
    for (p = w; *p; ++p)
        total += (*p)[0] + 1;
    x0 = (40 - (total - 1)) / 2;
    hg_src = tile;
    for (p = w; *p; ++p) {
        g = *p;
        for (r = 0; r < 9; ++r) {
            bits = g[1 + r];
            for (c = 0; c < g[0]; ++c, bits <<= 1)
                if (bits & 0x80) {
                    hg_x = x0 + c;
                    hg_y = (row + r) * 8;
                    hg_w = 1;
                    hg_h = 8;
                    hg_fg = ui_col(colors[r]);
                    hg_bg = C_BLACK;
                    hgr_color();
                    hgr_blit();
                }
        }
        x0 += g[0] + 1;
    }
}

static void show(void)
{
    hg_fg = C_WHITE;
    hg_bg = C_BLACK;
    hgr_clear();
    VIC_BORDER = C_BLACK;
    word(parlor_word, 1, warm);
    word(net_word, 11, cool);
    ui_inkc(C_LGREY, C_BLACK);
    hg_kbg = 0xFF;
    ui_text(3, 168, "ParlorNet for the Commodore 64 " PN_VERSION);
    ui_inkc(C_WHITE, C_BLACK);
    ui_text(14, 184, "Press a key");
    gr_mode();
}

#pragma rodata-name (push, "OVHDR")
const struct splash splash_header = {show};
#pragma rodata-name (pop)
