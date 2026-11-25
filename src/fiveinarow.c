/* ParlorNet for the Apple IIe: Five in a Row (docs/types/fiveinarow-1.md). The view lists each seat's
   stones (P lines) and the last one (L); every empty point is a legal move (flag nomoves), so a click on
   one sends it. Seat 0 is Black, a white rim round black, and seat 1 White, a solid stone, so the two
   differ in shape on any monitor. The last stone has a mark in its middle, and the winning line, once
   there is one, is framed. Boards up to 15 by 15 are played here (the pointer has a target for every
   point); a larger one is shown and can be played on the web or by telnet. */
#include <string.h>
#include "a2.h"
#include "ui.h"
#include "net.h"
#include "game.h"

extern const unsigned char spr_fr_w[], spr_fr_b[], spr_fr_wl[], spr_fr_bl[], spr_fr_e[];

#define MAXN 15
#define X0 0                            /* the board: 2 bytes by 10 lines a point */
#define Y0 11
#define PX 31                           /* the panel */

static unsigned char size = 15;
static char cell[MAXN][MAXN];           /* 0 empty, 1 seat 0, 2 seat 1; | 4 the last stone, | 8 the win */
static char shown[MAXN][MAXN];
static unsigned char stones[2], too_big;
static char rul[10];

static void fr_begin(void)
{
    memset(cell, 0, sizeof cell);
}

/* A point ("h8") into x and y; 0 if it is not on the board. */
static unsigned char px_, py_;
static unsigned char point(const char *p)
{
    px_ = (p[0] | 0x20) - 'a';
    py_ = num(p + 1) - 1;
    return px_ < size && py_ < size && size <= MAXN;
}

static void fr_line(void)
{
    unsigned char i, s;
    if (is(w[0], "BRD") && wn > 1) {
        size = num(w[1]);
        too_big = size > MAXN;
    } else if (is(w[0], "RUL") && wn > 1) {
        strncpy(rul, w[1], 9);
    } else if (is(w[0], "P") && wn > 1) {
        s = num(w[1]) + 1;
        for (i = 2; i < wn; ++i)
            if (point(w[i]))
                cell[py_][px_] = (cell[py_][px_] & ~3) | s;
    } else if (is(w[0], "L") && wn > 2) {
        if (point(w[2]))
            cell[py_][px_] |= 4;
    } else if (is(w[0], "F")) {
        for (i = 1; i < wn; ++i)
            if (point(w[i]))
                cell[py_][px_] |= 8;
    } else if (is(w[0], "C") && wn > 2) {
        stones[0] = num(w[1]);
        stones[1] = num(w[2]);
    }
}

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

static void fr_click(unsigned char id)
{
    static char p[4];
    unsigned char x = id % size, y = id / size;
    if (!g_myturn) {
        g_message("Not your turn.");
        return;
    }
    if (cell[y][x] & 3) {
        g_message("That point is taken.");
        return;
    }
    p[0] = 'a' + x;
    strcpy(p + 1, utoa10(y + 1));
    g_move(p);
}

#pragma rodata-name (push, "OVHDR")
const struct game g_fiveinarow = {"fiveinarow", "Five in a Row", fr_begin, fr_line, fr_draw, fr_click, 0,
                                  "Click an empty point to play it"};
#pragma rodata-name (pop)
