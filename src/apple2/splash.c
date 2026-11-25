/* ParlorNet for the Apple IIe: the title screen shown at start-up, an overlay (ovl.h). PARLOR over NET in
   the title's block letters (art/splash.txt, tools/apple2_art.py), each block filled solid, 4 color pixels
   across and 6 lines down. On a color monitor the rows of blocks are bands of the Hi-Res colors: PARLOR
   white, orange and violet, NET white, green and blue. Each screen line has one color, so no screen byte
   mixes the two palette groups. In monochrome every block is white. main.c keeps it up until a key is
   pressed. */
#include "machine.h"
#include "apple2.h"
#include "ovl.h"
#include "version.h"

extern const unsigned char hgr_ylo[], hgr_yhi[];
extern const unsigned char sg_P[], sg_A[], sg_R[], sg_L[], sg_O[], sg_N[], sg_E[], sg_T[];

static const unsigned char *const parlor_word[] = {sg_P, sg_A, sg_R, sg_L, sg_O, sg_R, 0};
static const unsigned char *const net_word[] = {sg_N, sg_E, sg_T, 0};

#define GAP 2                           /* color pixels between letters */

static unsigned char *row;
/* For each color pixel c: the byte and the bit of its left screen pixel (2c) and its right one (2c + 1). */
static unsigned char lb[140], lm[140], rb[140], rm[140];

static void tables(void)
{
    unsigned char c, b = 0, m = 1;
    for (c = 0; c < 140; ++c) {
        lb[c] = b;
        lm[c] = m;
        if ((m <<= 1) == 0x80) {
            m = 1;
            ++b;
        }
        rb[c] = b;
        rm[c] = m;
        if ((m <<= 1) == 0x80) {
            m = 1;
            ++b;
        }
    }
}

/* The current line's address, from hgr.s's table of line addresses. */
#define LINE(y) (((unsigned char *)&row)[0] = hgr_ylo[y], ((unsigned char *)&row)[1] = hgr_yhi[y])

#ifdef IIGS
#define PUT(a, v) gs_scr_put(row + (a), gs_scr_get(row + (a)) | (v))
#else
#define PUT(a, v) (row[a] |= (v))
#endif

/* The colors: white lights both screen pixels of a color pixel, violet and blue the left one, green and
   orange the right one; orange and blue are the second palette group (bit 7 of the byte). */
enum { WHITE, VIOLET, GREEN, ORANGE, BLUE };
/* Each row of blocks: the color of its top 3 lines and of its bottom 3, so a row between two bands has
   the upper band's color in its top half and the lower band's in the rest. */
static const unsigned char warm[9][2] = {{WHITE, WHITE}, {WHITE, WHITE}, {WHITE, ORANGE}, {ORANGE, ORANGE},
                                         {ORANGE, ORANGE}, {ORANGE, VIOLET}, {VIOLET, VIOLET},
                                         {VIOLET, VIOLET}, {VIOLET, VIOLET}};
static const unsigned char cool[9][2] = {{WHITE, WHITE}, {WHITE, WHITE}, {WHITE, GREEN}, {GREEN, GREEN},
                                         {GREEN, GREEN}, {GREEN, BLUE}, {BLUE, BLUE}, {BLUE, BLUE},
                                         {BLUE, BLUE}};

/* One word, centered, its top at line y0. A letter's data: its width in blocks, then a byte a row of 9,
   the blocks in bits 7 down. */
static void word(const unsigned char *const *w, unsigned char y0, const unsigned char (*colors)[2])
{
    unsigned char total = 0, c0, r, l, c, k, x, bits, col;
    const unsigned char *const *p;
    const unsigned char *g;
    for (p = w; *p; ++p)
        total += (*p)[0] * 4 + GAP;
    c0 = (140 - (total - GAP)) / 2;
    for (p = w; *p; ++p) {
        g = *p;
        for (r = 0; r < 9; ++r)
            for (l = 0; l < 6; ++l) {
                LINE(y0 + r * 6 + l);
                col = mono ? WHITE : colors[r][l >= 3];
                bits = g[1 + r];
                for (c = 0; c < g[0]; ++c, bits <<= 1) {
                    if (!(bits & 0x80))
                        continue;
                    for (k = 0, x = c0 + c * 4; k < 4; ++k, ++x) {
                        if (col == WHITE || col == VIOLET || col == BLUE)
                            PUT(lb[x], lm[x]);
                        if (col == WHITE || col == GREEN || col == ORANGE)
                            PUT(rb[x], rm[x]);
                        if (col == ORANGE || col == BLUE) {
                            PUT(lb[x], 0x80);
                            PUT(rb[x], 0x80);
                        }
                    }
                }
            }
        c0 += g[0] * 4 + GAP;
    }
}

static void show(void)
{
    tables();
    hgr_clear();
    word(parlor_word, 14, warm);
    word(net_word, 82, cool);
    ui_ink(F_WHITE, F_BLACK);
    hg_bold = !mono;
#ifdef IIGS
    ui_text(4, 150, "ParlorNet for the Apple IIgs " PN_VERSION);
#else
    ui_text(4, 150, "ParlorNet for the Apple IIe " PN_VERSION);
#endif
    ui_text(14, 168, "Press a key");
    gr_mode();
}

#pragma rodata-name (push, "OVHDR")
const struct splash splash_header = {show};
#pragma rodata-name (pop)
