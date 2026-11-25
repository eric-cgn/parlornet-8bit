/* ParlorNet on the Apple IIe and IIgs: Five in a Row's board (core/games/fiveinarow.c includes it). Seat 0 is
   Black, a white rim round black, and seat 1 White, a solid stone, so the two differ in shape on any monitor.
   The last stone has a mark in its middle, and the winning line is framed. A point is 2 bytes by 10 lines. */
#include "apple2.h"
extern const unsigned char spr_fr_w[], spr_fr_b[], spr_fr_wl[], spr_fr_bl[], spr_fr_e[];

#define X0 0                            /* the board: 2 bytes by 10 lines a point */
#define Y0 11
#define PX 31                           /* the panel */

static char shown[MAXN][MAXN];

static void draw_point(unsigned char x, unsigned char y, char c)
{
    static const unsigned char *const spr[] = {spr_fr_e, spr_fr_b, spr_fr_w, spr_fr_e,
                                               spr_fr_e, spr_fr_bl, spr_fr_wl, spr_fr_e};
    unsigned char bx = X0 + 2 * x, by = Y0 + 10 * y;
    shown[y][x] = c;
    hg_x = bx;
    hg_y = by;
    hg_src = spr[c & 7];
    hgr_blit();
    if (c & 8)                          /* on the winning line */
        ui_box(bx * 7, by, 14, 10);
}

static void panel(void)
{
    unsigned char s, y;
    static char b[12];
    for (s = 0; s < 2; ++s) {
        y = Y0 + 4 + s * 40;
        hg_x = PX;
        hg_y = y;
        hg_src = s ? spr_fr_w : spr_fr_b;
        hgr_blit();
        ui_textn(PX + 2, y + 1, g_turn == s && !g_over ? "<" : "", 1);
        ui_textn(PX, y + 12, g_name(s), 9);
        strcpy(b, s ? "White " : "Black ");
        strcat(b, utoa10(stones[s]));
        ui_textn(PX, y + 21, b, 9);
    }
    ui_textn(PX, 100, rul[0] == 's' ? "Exactly 5" : "5 or more", 9);
    ui_textn(PX, 109, "in a row", 9);
    ui_textn(PX, 118, "wins.", 9);
}

static void fr_draw(void)
{
    unsigned char x, y;
    if (too_big) {
        if (g_full) {
            ui_text(1, 40, "This board is larger than 15 by 15.");
            ui_text(1, 52, "Play it on the web or by telnet.");
        }
        return;
    }
    if (g_full) {
        memset(shown, 0xFF, sizeof shown);
        for (y = 0; y < size; ++y)
            for (x = 0; x < size; ++x)
                ui_target(((X0 + 2 * x) * 7 + 7) / 2, Y0 + 10 * y + 4, 3, 5, y * size + x);
    }
    for (y = 0; y < size; ++y)
        for (x = 0; x < size; ++x)
            if (cell[y][x] != shown[y][x])
                draw_point(x, y, cell[y][x]);
    panel();
}
