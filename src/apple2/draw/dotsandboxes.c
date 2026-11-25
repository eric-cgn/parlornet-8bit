/* ParlorNet on the Apple IIe and IIgs: Dots and Boxes' board (core/games/dotsandboxes.c includes it). A box
   taken is filled with its player's color and letter; in monochrome the letter alone marks it. The chosen
   dot has a frame round it, drawn and taken away by flipping. */
#include "apple2.h"
#define DX 1                            /* the board's top left dot: byte column, line */
#define DY 14
#define PX 29                           /* the panel */

static unsigned char sx, sy;            /* the spacing (bytes, lines) */
static char shl[MH + 1][MW + 1], svl[MH][MW + 2], sbx[MH][MW + 1];
static const unsigned char seat_fill[] = {F_ORANGE, F_BLUE, F_GREEN, F_VIOLET};

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
