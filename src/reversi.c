/* ParlorNet for the Apple IIe: Reversi (docs/types/reversi-1.md). The view's R lines give the board,
   C the disc counts; a move is a square, a column letter and a row number. In color the board is
   green with black and white discs; in monochrome seat 0 is a ring and seat 1 a solid disc. A square
   is 3 bytes (21 pixels) by 18 lines, square on a monitor (where a pixel is about 7/6 as tall as it is
   wide); on a 10 by 10 board, 15 lines, to fit. */
#include <string.h>
#include "a2.h"
#include "ui.h"
#include "net.h"
#include "game.h"

extern const unsigned char spr_rv_empty[], spr_rv_disc0[], spr_rv_disc1[], spr_rv_hint[];
extern const unsigned char spr_rv_empty_o[], spr_rv_disc0_o[], spr_rv_disc1_o[], spr_rv_hint_o[];
extern const unsigned char spr_rv_m_empty[], spr_rv_m_disc0[], spr_rv_m_disc1[], spr_rv_m_hint[];
extern const unsigned char spr_rv_s_empty[], spr_rv_s_disc0[], spr_rv_s_disc1[], spr_rv_s_hint[];
extern const unsigned char spr_rv_s_empty_o[], spr_rv_s_disc0_o[], spr_rv_s_disc1_o[], spr_rv_s_hint_o[];
extern const unsigned char spr_rv_s_m_empty[], spr_rv_s_m_disc0[], spr_rv_s_m_disc1[], spr_rv_s_m_hint[];

/* The squares: [10 by 10][color at an even byte, at an odd one, monochrome][empty, disc 0, disc 1, mark] */
static const unsigned char *const spr[2][3][4] = {
    {{spr_rv_empty, spr_rv_disc0, spr_rv_disc1, spr_rv_hint},
     {spr_rv_empty_o, spr_rv_disc0_o, spr_rv_disc1_o, spr_rv_hint_o},
     {spr_rv_m_empty, spr_rv_m_disc0, spr_rv_m_disc1, spr_rv_m_hint}},
    {{spr_rv_s_empty, spr_rv_s_disc0, spr_rv_s_disc1, spr_rv_s_hint},
     {spr_rv_s_empty_o, spr_rv_s_disc0_o, spr_rv_s_disc1_o, spr_rv_s_hint_o},
     {spr_rv_s_m_empty, spr_rv_s_m_disc0, spr_rv_s_m_disc1, spr_rv_s_m_hint}}};

#define CW 3                            /* a square: 3 bytes by CH lines */
#define PX 28                           /* the players' panel */
static unsigned char CH;

static unsigned char size;
static char board[10][11];              /* rows of '.', '0', '1' */
static char shown[10][11];              /* what is on the screen ('h' a hint) */
static unsigned char x0, y0;
static unsigned int count[2];

static void rv_begin(void)
{
}

static void rv_line(void)
{
    unsigned char y;
    if (is(w[0], "BRD") && wn > 1) {
        size = num(w[1]);
        if (size > 10)
            size = 10;
    } else if (is(w[0], "R") && wn > 2) {
        y = num(w[1]) - 1;
        if (y < 10)
            strncpy(board[y], w[2], 10);
    } else if (is(w[0], "C") && wn > 2) {
        count[0] = num(w[1]);
        count[1] = num(w[2]);
    }
}

/* Square c ('.', '0', '1', 'h') drawn at byte column x and line y. */
static void cell(char c, unsigned char x, unsigned char y)
{
    unsigned char k = 0;                /* (not a chain of ?: on c, which cc65 2.19 compiles wrongly here) */
    if (c == '0')
        k = 1;
    else if (c == '1')
        k = 2;
    else if (c == 'h')
        k = 3;
    hg_x = x;
    hg_y = y;
    hg_src = spr[size > 8][mono ? 2 : x & 1][k];
    hgr_blit();
}

static void panel(void)
{
    unsigned char s, y;
    if (size > 8)
        return;
    for (s = 0; s < 2; ++s) {
        y = y0 + 8 + s * 48;
        cell('0' + s, PX, y);
        ui_textn(PX + 4, y + 5, g_turn == s && !g_over ? "<" : "", 1);
        ui_textn(PX, y + 20, g_name(s), 12);
        ui_textn(PX, y + 29, utoa10(count[s]), 6);
    }
}

static void rv_draw(void)
{
    unsigned char x, y;
    static char sq[3];
    static char c;
    if (!size)
        size = 8;
    CH = size > 8 ? 15 : 18;
    x0 = size > 8 ? (40 - size * CW) / 2 : (PX - 1 - size * CW) / 2 + 1;
    y0 = size > 8 ? GAME_TOP : GAME_TOP + 9;
    if (g_full) {
        memset(shown, 0, sizeof shown);
        if (size <= 8)
            for (x = 0; x < size; ++x) {
                hg_x = x0 + x * CW + 1;
                hg_y = GAME_TOP;
                hgr_char('a' + x);
            }
        for (y = 0; y < size; ++y) {
            if (size <= 8) {
                hg_x = x0 - 1;
                hg_y = y0 + y * CH + 5;
                hgr_char(y < 9 ? '1' + y : '0');
            }
            for (x = 0; x < size; ++x)
                ui_target((x0 + x * CW) * 7 / 2 + 5, y0 + y * CH + CH / 2 - 1, 5, CH / 2, y * 16 + x);
        }
    }
    sq[2] = 0;
    for (y = 0; y < size; ++y)
        for (x = 0; x < size; ++x) {
            c = board[y][x];
            if (c == '.' && g_myturn) {
                sq[0] = 'a' + x;
                sq[1] = '1' + y;
                if (y < 9 && m_has(sq))
                    c = 'h';
            }
            if (c != shown[y][x]) {
                shown[y][x] = c;
                cell(c, x0 + x * CW, y0 + y * CH);
            }
        }
    panel();
}

static void rv_click(unsigned char id)
{
    static char sq[4];
    unsigned char x = id & 15, y = id >> 4;
    if (!g_myturn) {
        g_message("Not your move.");
        return;
    }
    sq[0] = 'a' + x;
    if (y < 9) {
        sq[1] = '1' + y;
        sq[2] = 0;
    } else {
        sq[1] = '1';
        sq[2] = '0';
        sq[3] = 0;
    }
    g_move(sq);
}

#pragma rodata-name (push, "OVHDR")
const struct game g_reversi = {"reversi", "Reversi", rv_begin, rv_line, rv_draw, rv_click, 0};
#pragma rodata-name (pop)
