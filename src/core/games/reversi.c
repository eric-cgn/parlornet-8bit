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

static char sq[4];

/* The square with this target id, as its name ("d3"), in sq. */
static void sq_of(unsigned char id)
{
    unsigned char y = id >> 4;
    sq[0] = 'a' + (id & 15);
    if (y < 9) {
        sq[1] = '1' + y;
        sq[2] = 0;
    } else {
        sq[1] = '1';
        sq[2] = '0';
        sq[3] = 0;
    }
}

static void rv_click(unsigned char id)
{
    if (!g_myturn) {
        g_message("It's not your turn.");
        return;
    }
    sq_of(id);
    g_move(sq);
}

/* What Return does on the square under the pointer. */
static void rv_hover(unsigned char id)
{
    static char b[42];
    if (id == 255) {
        g_message("Move to a square with a dot.");
        return;
    }
    sq_of(id);
    strcpy(b, m_has(sq) ? "Press " KEY_RET " to place a disc on " : "You can't place a disc on ");
    strcat(b, sq);
    strcat(b, ".");
    g_message(b);
}

/* The other player's move in words: "ann placed a disc on d3 and flipped 2." */
static void rv_event(void)
{
    if (num(w[4]) == (unsigned char)g_seat)
        return;
    if (is(w[3], "PASS"))
        strcat(g_who(num(w[4])), " passed.");
    else if (is(w[3], "PUT") && wn > 5) {
        strcat(g_who(num(w[4])), " placed a disc on ");
        strcat(g_last, w[5]);
        strcat(g_last, " and flipped ");
        strcat(g_last, utoa10(wn > 7 ? wn - 7 : 0));
        strcat(g_last, ".");
    }
}

#pragma rodata-name (push, "OVHDR")
const struct game g_reversi = {"reversi", "Reversi", rv_begin, rv_line, rv_draw, rv_click, 0,
                               0, rv_event, rv_hover};
#pragma rodata-name (pop)
