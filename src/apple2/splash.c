/* ParlorNet for the Apple IIe: the title screen shown at start-up, an overlay (ovl.h). PARLOR over NET in
   the title's block letters (art/splash.txt, tools/apple2_art.py), as on the C64: each block 4 color
   pixels across and 6 lines down, lit but for a black gap on its right and bottom edge, and each row of
   blocks a color of its own, warm for PARLOR and cool for NET. A row of blocks has one color, so no screen
   byte mixes the two palette groups, and every block of a row has the same pixels lit. The screen is the
   same with the display set to monochrome, and on a monochrome monitor the colored blocks show as fine
   stripes. main.c keeps it up until a key is pressed. */
#include "machine.h"
#include "apple2.h"
#include "ovl.h"
#include "version.h"

extern const unsigned char hgr_ylo[], hgr_yhi[];
extern const unsigned char sg_P[], sg_A[], sg_R[], sg_L[], sg_O[], sg_N[], sg_E[], sg_T[];

static const unsigned char *const parlor_word[] = {sg_P, sg_A, sg_R, sg_L, sg_O, sg_R, 0};
static const unsigned char *const net_word[] = {sg_N, sg_E, sg_T, 0};

#define GAP 4                           /* screen pixels between letters, besides a block's own gap */

/* The colors, as the screen pixels lit in a block's 8 (bit 0 the leftmost), with the palette bit. A block
   starts on an even screen pixel, and its 8 here start a pixel before it. Violet and blue light 3 even pixels,
   from the block's first; green and orange 3 odd ones, green's from the block's second and orange's from
   the pixel before the block, since the palette bit moves orange (and blue) half a pixel to the right.
   Each color then fills the same 5 pixels to within a pixel, as white does; the rest is the gap. Orange
   and blue are the second palette group. */
#define WHITE 0x3E
#define VIOLET 0x2A
#define GREEN 0x54
#define ORANGE (0x15 | 0x100)
#define BLUE (0x2A | 0x100)
/* Each row of blocks, top first, as the C64 has them as near as these colors come: PARLOR white, orange
   and violet for its yellow, its pinks and oranges, and its reds and brown; NET white, green, blue and
   violet for its white, its greens, cyans and blues, and its purples and blue. */
static const unsigned int warm[9] = {WHITE, WHITE, ORANGE, ORANGE, ORANGE, ORANGE, VIOLET, VIOLET, VIOLET};
static const unsigned int cool[9] = {WHITE, GREEN, GREEN, BLUE, BLUE, BLUE, VIOLET, VIOLET, VIOLET};

static unsigned char *row;
/* A screen pixel on the row being made: its byte and its bit. */
static unsigned char b, m;

/* The next screen pixel. */
static void step(void)
{
    if ((m <<= 1) == 0x80) {
        m = 1;
        ++b;
    }
}

/* The current line's address, from hgr.s's table of line addresses. */
#define LINE(y) (((unsigned char *)&row)[0] = hgr_ylo[y], ((unsigned char *)&row)[1] = hgr_yhi[y])

/* One row of blocks across the screen, made here and then copied to its 5 lines. */
static unsigned char line[40];

/* One word, centered, its top at line y0. A letter's data: its width in blocks, then a byte a row of 9,
   the blocks in bits 7 down. A block is 8 screen pixels across, 5 lines of them lit and a black line. */
static void word(const unsigned char *const *w, unsigned char y0, const unsigned int *colors)
{
    unsigned int total = 0;
    unsigned char x0, r, l, c, k, bits, lit, hi;
    const unsigned char *const *p;
    const unsigned char *g;
    for (p = w; *p; ++p)
        total += (*p)[0] * 8 + GAP;
    /* the first block's screen pixel, even (the last block's gap of 3 pixels and the last letter's GAP
       are not counted in the width) */
    x0 = ((280 - (total - GAP - 3)) / 2) & 0xFE;
    for (r = 0; r < 9; ++r, y0 += 6) {
        for (b = 0; b < 40; ++b)
            line[b] = 0;
        b = 0;
        m = 1;
        for (k = 1; k < x0; ++k)        /* to the pixel before the first block */
            step();
        for (p = w; *p; ++p) {
            g = *p;
            bits = g[1 + r];
            for (c = 0; c < g[0]; ++c, bits <<= 1)
                for (k = 0, lit = colors[r]; k < 8; ++k, lit >>= 1) {
                    if ((bits & 0x80) && (lit & 1))
                        line[b] |= m;
                    step();
                }
            for (k = 0; k < GAP; ++k)
                step();
        }
        hi = colors[r] >> 8 ? 0x80 : 0;
        for (l = 0; l < 5; ++l) {
            LINE(y0 + l);
            for (b = 0; b < 40; ++b)
#ifdef IIGS
                gs_scr_put(row + b, line[b] | hi);
#else
                row[b] = line[b] | hi;
#endif
        }
    }
}

static void show(void)
{
    hgr_clear();
    word(parlor_word, 20, warm);
    word(net_word, 80, cool);
    ui_ink(F_WHITE, F_BLACK);
    hg_bold = 1;
#ifdef IIGS
    ui_text(4, 146, "ParlorNet for the Apple IIgs " PN_VERSION);
#else
    ui_text(4, 146, "ParlorNet for the Apple IIe " PN_VERSION);
#endif
    ui_text(14, 164, "Press a key");
    gr_mode();
}

#pragma rodata-name (push, "OVHDR")
const struct splash splash_header = {show};
#pragma rodata-name (pop)
