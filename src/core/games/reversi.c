/* ParlorNet: Reversi (docs/types/reversi-1.md). The view's R lines give the board, C the disc counts; a move
   is a square, a column letter and a row number. The machine draws the board (draw/reversi.c, included
   below: rv_draw) from size, board and count, and makes each square a target, its id row * 16 + column. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "game.h"

static unsigned char size;
static char board[10][11];              /* rows of '.', '0', '1' */
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

/* The machine's drawing: rv_draw. */
#include "draw/reversi.c"

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
