/* ParlorNet for the Apple IIe: Dots and Boxes (docs/types/dotsandboxes-1.md), 2 to 4 players, on any
   board the parlor offers up to 13 by 11 boxes. The view gives the lines across (HL), the lines down
   (VL) and the boxes taken (BX). A line is drawn by clicking a dot, then the dot next to it; the pointer
   moves from dot to dot. A box taken is filled with its player's color and letter (A to D); in
   monochrome the letter alone marks it. */
#include <string.h>
#include "a2.h"
#include "ui.h"
#include "net.h"
#include "game.h"

#define MW 13                           /* the largest board, in boxes */
#define MH 11
#define DX 1                            /* the board's top left dot: byte column, line */
#define DY 14
#define PX 29                           /* the panel */

static unsigned char bw = 5, bh = 5, sx, sy;   /* boxes across and down; the spacing (bytes, lines) */
static char hl[MH + 1][MW + 1], vl[MH][MW + 2], bx[MH][MW + 1];
static char shl[MH + 1][MW + 1], svl[MH][MW + 2], sbx[MH][MW + 1];
static unsigned char cnt[4], gone;
static char last[8];
static signed char seld = -1;            /* the chosen dot, or -1 */

static const unsigned char seat_fill[] = {F_ORANGE, F_BLUE, F_GREEN, F_VIOLET};

static void db_begin(void)
{
    last[0] = 0;
    gone = 0;
}

static void db_line(void)
{
    unsigned char y, i;
    if (is(w[0], "BRD") && wn > 2) {
        bw = num(w[1]);
        bh = num(w[2]);
        if (bw > MW)
            bw = MW;
        if (bh > MH)
            bh = MH;
        return;
    }
    if (wn < 3) {
        if (is(w[0], "LL") && wn > 1)
            strncpy(last, w[1], 7);
        return;
    }
    y = num(w[1]) - 1;
    if (is(w[0], "HL") && y <= MH)
        strncpy(hl[y], w[2], MW);
    else if (is(w[0], "VL") && y < MH)
        strncpy(vl[y], w[2], MW + 1);
    else if (is(w[0], "BX") && y < MH)
        strncpy(bx[y], w[2], MW);
    else if (is(w[0], "C"))
        for (i = 0; i < 4 && i + 1 < wn; ++i)
            cnt[i] = num(w[i + 1]);
    else if (is(w[0], "G"))
        for (i = 0; i < 4 && i + 1 < wn; ++i)
            if (w[i + 1][0] == '1')
                gone |= 1 << i;
}

static unsigned int dpx(unsigned char i)
{
    return (DX + i * sx) * 7;
}

static unsigned char dpy(unsigned char j)
{
    return DY + j * sy;
}

static void dot(unsigned char i, unsigned char j)
{
    hg_px = dpx(i);
    hg_y = dpy(j);
    hg_w = 2;
    hgr_hline();
    ++hg_y;
    hg_px = dpx(i);
    hgr_hline();
}

static void hline(unsigned char i, unsigned char j)
{
    hg_px = dpx(i);
    hg_y = dpy(j);
    hg_w = sx * 7 + 2;
    hgr_hline();
    hg_px = dpx(i);
    ++hg_y;
    hgr_hline();
}

static void vline(unsigned char i, unsigned char j)
{
    hg_px = dpx(i);
    hg_y = dpy(j);
    hg_h = sy + 2;
    hgr_vline();
    hg_px = dpx(i) + 1;
    hgr_vline();
}

static void box(unsigned char i, unsigned char j, char c)
{
    unsigned char s = c - '0';
    if (s > 3)
        return;
    if (!mono)
        ui_rect(DX + i * sx, dpy(j) + 2, sx, sy - 2, seat_fill[s]);
    ui_ink(F_WHITE, mono ? F_BLACK : seat_fill[s]);
    hg_x = DX + i * sx + sx / 2;
    if (sx == 2)
        hg_x = DX + i * sx + 1;
    hg_y = dpy(j) + sy / 2 - 3;
    hgr_char('A' + s);
    ui_ink(F_WHITE, F_BLACK);
    /* the fill covered the box's left line and dots */
    if (vl[j][i] != '.')
        vline(i, j);
    dot(i, j);
    dot(i, j + 1);
}

/* The chosen dot: a frame round it, drawn and taken away by flipping. */
static void sel_frame(void)
{
    unsigned char i, j;
    if (seld < 0)
        return;
    i = seld % (bw + 1);
    j = seld / (bw + 1);
    hg_xor = 1;
    ui_box(dpx(i) - 3, dpy(j) - 3, 8, 8);
    hg_xor = 0;
}

static void panel(void)
{
    unsigned char s, y;
    static char b[16];                  /* "255 boxes, left" */
    for (s = 0; s < g_seats && s < 4; ++s) {
        y = DY - 4 + s * 26;
        ui_rect(PX, y, 2, 9, mono ? F_BLACK : seat_fill[s]);
        ui_ink(F_WHITE, mono ? F_BLACK : seat_fill[s]);
        hg_x = PX;
        hg_y = y + 1;
        hgr_char(g_turn == s && !g_over ? '>' : ' ');
        hgr_char('A' + s);
        ui_ink(F_WHITE, F_BLACK);
        ui_textn(PX + 3, y + 1, g_name(s), 8);
        strcpy(b, utoa10(cnt[s]));
        strcat(b, cnt[s] == 1 ? " box" : " boxes");
        if (gone & (1 << s))
            strcat(b, ", left");
        ui_textn(PX + 1, y + 11, b, 11);
    }
    strcpy(b, "Last ");
    strcat(b, last);
    ui_textn(PX, 140, last[0] ? b : "", 11);
}

static void db_draw(void)
{
    unsigned char i, j;
    sx = bw <= 6 ? 4 : bw <= 8 ? 3 : 2;
    sy = 150 / bh;
    if (sy > 28)
        sy = 28;
    if (g_full) {
        seld = -1;
        memset(shl, '.', sizeof shl);
        memset(svl, '.', sizeof svl);
        memset(sbx, '.', sizeof sbx);
        for (j = 0; j <= bh; ++j)
            for (i = 0; i <= bw; ++i) {
                dot(i, j);
                ui_target((dpx(i) + 1) / 2, dpy(j) + 1, sx * 7 / 4, sy / 2, j * (bw + 1) + i);
            }
    }
    sel_frame();
    for (j = 0; j <= bh; ++j)
        for (i = 0; i < bw; ++i)
            if (hl[j][i] == '-' && shl[j][i] != '-') {
                shl[j][i] = '-';
                hline(i, j);
            }
    for (j = 0; j < bh; ++j)
        for (i = 0; i <= bw; ++i)
            if (vl[j][i] == '|' && svl[j][i] != '|') {
                svl[j][i] = '|';
                vline(i, j);
            }
    for (j = 0; j < bh; ++j)
        for (i = 0; i < bw; ++i)
            if (bx[j][i] != sbx[j][i]) {
                sbx[j][i] = bx[j][i];
                box(i, j, bx[j][i]);
            }
    sel_frame();
    panel();
}

static void db_click(unsigned char id)
{
    static char tok[8];
    unsigned char i, j, i2, j2, open;
    if (!g_myturn) {
        g_message("Not your turn.");
        return;
    }
    if (seld < 0 || seld == id) {
        ui_hide();
        sel_frame();
        seld = seld == id ? -1 : id;
        sel_frame();
        ui_show();
        g_message(seld < 0 ? "" : "Now a dot next to it.");
        return;
    }
    i = seld % (bw + 1);
    j = seld / (bw + 1);
    i2 = id % (bw + 1);
    j2 = id / (bw + 1);
    if (i2 < i || j2 < j) {             /* the left or upper dot first */
        unsigned char t = i;
        i = i2;
        i2 = t;
        t = j;
        j = j2;
        j2 = t;
    }
    if (j == j2 && i2 == i + 1)
        open = hl[j][i] == '.';
    else if (i == i2 && j2 == j + 1)
        open = vl[j][i] == '.';
    else {
        /* not a neighbor: that dot is chosen instead */
        ui_hide();
        sel_frame();
        seld = id;
        sel_frame();
        ui_show();
        return;
    }
    if (!open) {
        g_message("That line is drawn already.");
        return;
    }
    tok[0] = 'a' + i;
    strcpy(tok + 1, utoa10(j + 1));
    strcat(tok, "-");
    j = strlen(tok);
    tok[j] = 'a' + i2;
    strcpy(tok + j + 1, utoa10(j2 + 1));
    ui_hide();
    sel_frame();
    seld = -1;
    ui_show();
    g_move(tok);
}

#pragma rodata-name (push, "OVHDR")
const struct game g_dotsandboxes = {"dotsandboxes", "Dots and Boxes", db_begin, db_line, db_draw, db_click, 0,
                                    "A dot, then the dot next to it", 1};
#pragma rodata-name (pop)
