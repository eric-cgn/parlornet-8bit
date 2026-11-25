/* ParlorNet on the Commodore 64: Dots and Boxes' board (core/games/dotsandboxes.c includes it). Each box is
   whole cells (three by three, or fewer on a large board), its top and left lines along the top and left
   edges of its cells, so that a box taken is its cells' color, its player's, under white lines, dots and
   letter. The chosen dot has a frame round it, a sprite. Beside the board, each player's color and letter,
   name and boxes. In monochrome a box taken shows its letter alone. */
#include "gfx.h"

extern const unsigned char spr_db_frame[];

#define DX 1                            /* the board's top left dot: cell column, line */
#define DY 16
#define PX 28                           /* the panel */
#define SPR_FRAME (SPR_GAME)

static unsigned char sx, sy;            /* a box: cells across and down */
static char shl[MH + 1][MW + 1], svl[MH][MW + 2], sbx[MH][MW + 1];
static const unsigned char seat_col[] = {C_RED, C_GREEN, C_PURPLE, C_ORANGE};

static unsigned int dpx(unsigned char i)
{
    return (DX + i * sx) * 8;
}

static unsigned char dpy(unsigned char j)
{
    return DY + j * sy * 8;
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
    hg_w = sx * 8 + 2;
    hgr_hline();
    hg_px = dpx(i);
    ++hg_y;
    hgr_hline();
}

static void vline(unsigned char i, unsigned char j)
{
    hg_px = dpx(i);
    hg_y = dpy(j);
    hg_h = sy * 8 + 2;
    hgr_vline();
    hg_px = dpx(i) + 1;
    hgr_vline();
}

static void box(unsigned char i, unsigned char j, char c)
{
    unsigned char s = c - '0';
    if (s > 3)
        return;
    hg_x = DX + i * sx;
    hg_y = dpy(j);
    hg_w = sx;
    hg_h = sy * 8;
    hg_fg = C_WHITE;
    hg_bg = mono ? ui_fcol(F_BLACK) : seat_col[s];
    hgr_color();
    hg_x = DX + i * sx + sx / 2;
    hg_y = dpy(j) + (sy > 1 ? (sy / 2) * 8 : 0);
    if (sy == 1)                        /* (a row of one cell: the letter under its top line) */
        hg_y = dpy(j) + 1;
    hgr_char('A' + s);
    if (shl[j][i] == '-')               /* (in a box one cell high the letter's cell holds its top line) */
        hline(i, j);
    dot(i, j);
}

/* The chosen dot: the frame (a sprite) round it, or none. */
static void sel_frame(void)
{
    if (seld < 0) {
        spr_on &= ~4;
        return;
    }
    spr_x[2] = dpx(seld % (bw + 1)) + 24 - 3;
    spr_y[2] = dpy(seld / (bw + 1)) + 50 - 3;
    spr_on |= 4;
}

static void panel(void)
{
    unsigned char s, y;
    static char b[16];                  /* "255 boxes, left" */
    for (s = 0; s < g_seats && s < 4; ++s) {
        y = DY + s * 32;
        hg_x = PX;
        hg_y = y;
        hg_fg = C_WHITE;
        hg_bg = mono ? ui_fcol(F_BLACK) : seat_col[s];
        hgr_char(g_turn == s && !g_over ? '>' : ' ');
        hgr_char('A' + s);
        ui_ink(F_WHITE, F_BLACK);
        ui_textn(PX + 3, y, g_name(s), 9);
        strcpy(b, utoa10(cnt[s]));
        strcat(b, cnt[s] == 1 ? " box" : " boxes");
        if (gone & (1 << s))
            strcat(b, ", left");
        ui_textn(PX + 1, y + 8, b, 11);
    }
    strcpy(b, "Last ");
    strcat(b, last);
    ui_textn(PX, 152, last[0] ? b : "", 12);
}

static void db_draw(void)
{
    unsigned char i, j;
    sy = 20 / bh;
    if (sy > 3)
        sy = 3;
    if (!sy)
        sy = 1;
    sx = 26 / bw;
    if (sx > sy)
        sx = sy > 1 ? sy : 2;
    if (!sx)
        sx = 1;
    if (g_full) {
        seld = -1;
        memset(shl, '.', sizeof shl);
        memset(svl, '.', sizeof svl);
        memset(sbx, '.', sizeof sbx);
        spr_load(SPR_FRAME, spr_db_frame);
        spr_ptr[2] = SPR_FRAME;
        spr_col[2] = mono ? C_WHITE : C_YELLOW;
        ui_ink(F_WHITE, F_BLACK);
        for (j = 0; j <= bh; ++j)
            for (i = 0; i <= bw; ++i) {
                dot(i, j);
                ui_target((dpx(i) + 1) / 2, dpy(j) + 1, sx * 4 - 1, sy * 4, j * (bw + 1) + i);
            }
    }
    for (j = 0; j < bh; ++j)
        for (i = 0; i < bw; ++i)
            if (bx[j][i] != sbx[j][i]) {
                sbx[j][i] = bx[j][i];
                box(i, j, bx[j][i]);
            }
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
    sel_frame();
    panel();
}
