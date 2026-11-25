/* ParlorNet for the Apple IIe: the title screen shown at start-up, an overlay (ovl.h). PARLOR over NET in
   hollow letters with a double outline, drawn on Hi-Res page 1 from the runs that tools/apple2_art.py
   traces from art/splash.txt: green on a color monitor, white in monochrome. main.c keeps it up until a
   key is pressed. */
#include "machine.h"
#include "apple2.h"
#include "ovl.h"
#include "version.h"

extern const unsigned char hgr_ylo[], hgr_yhi[];
extern const unsigned char sg_P[], sg_A[], sg_R[], sg_L[], sg_O[], sg_N[], sg_E[], sg_T[];

static const unsigned char *const parlor_word[] = {sg_P, sg_A, sg_R, sg_L, sg_O, sg_R, 0};
static const unsigned char *const net_word[] = {sg_N, sg_E, sg_T, 0};

#define GAP 2                           /* color pixels between letters */

/* Each color pixel is lit in its right screen pixel only in color, which is green on an odd column,
   and in both in monochrome. */
static unsigned char *row;
static unsigned char green;
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

static unsigned char c0, x, y, n, b, m, lb2, lm2;
static const unsigned char *g;

/* One letter at color pixel c0, its top at line y0: each run across or down. */
static void letter(unsigned char y0)
{
    for (++g; *g != 0xFF; g += 3) {
        x = g[0];
        y = y0 + g[1];
        n = g[2];
        if (x & 0x80) {
            x = c0 + (x & 0x7F);
            b = rb[x];
            m = rm[x];
            lb2 = lb[x];
            lm2 = lm[x];
            do {
                LINE(y);
                ++y;
#ifdef IIGS
                gs_scr_put(row + b, gs_scr_get(row + b) | m);
                if (!green)
                    gs_scr_put(row + lb2, gs_scr_get(row + lb2) | lm2);
#else
                row[b] |= m;
                if (!green)
                    row[lb2] |= lm2;
#endif
            } while (--n);
        } else {
            LINE(y);
            x += c0;
            do {
#ifdef IIGS
                gs_scr_put(row + rb[x], gs_scr_get(row + rb[x]) | rm[x]);
                if (!green)
                    gs_scr_put(row + lb[x], gs_scr_get(row + lb[x]) | lm[x]);
#else
                row[rb[x]] |= rm[x];
                if (!green)
                    row[lb[x]] |= lm[x];
#endif
                ++x;
            } while (--n);
        }
    }
}

static void word(const unsigned char *const *w, unsigned char y0)
{
    unsigned char total = 0;
    const unsigned char *const *p;
    for (p = w; *p; ++p)
        total += (*p)[0] + GAP;
    c0 = (140 - (total - GAP)) / 2;
    for (p = w; *p; ++p) {
        g = *p;
        letter(y0);
        c0 += (*p)[0] + GAP;
    }
}

static void show(void)
{
    green = !mono;
    tables();
    hgr_clear();
    word(parlor_word, 14);
    word(net_word, 82);
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
