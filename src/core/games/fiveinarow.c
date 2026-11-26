/* ParlorNet: Five in a Row (docs/types/fiveinarow-1.md). The view lists each seat's stones (P lines) and the
   last one (L); every empty point is a legal move (flag nomoves), so a click on one sends it. Boards up to
   15 by 15 are played here (the pointer has a target for every point, its id row * size + column); a larger
   one can be played on the web or by telnet. The machine draws the board (draw/fiveinarow.c, included below:
   fr_draw) from cell: seat 0 Black, seat 1 White, the last stone marked, the winning line, once there is one,
   marked too. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "game.h"

#define MAXN 15

static unsigned char size = 15;
static char cell[MAXN][MAXN];           /* 0 empty, 1 seat 0, 2 seat 1; | 4 the last stone, | 8 the win */
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

/* The machine's drawing: fr_draw. */
#include "draw/fiveinarow.c"

static char pt[4];

/* The point with this target id, as its name ("h8"), in pt; 1 when it is taken. */
static unsigned char pt_of(unsigned char id)
{
    unsigned char x = id % size, y = id / size;
    pt[0] = 'a' + x;
    strcpy(pt + 1, utoa10(y + 1));
    return cell[y][x] & 3;
}

static void fr_click(unsigned char id)
{
    if (!g_myturn) {
        g_message("It's not your turn.");
        return;
    }
    if (pt_of(id)) {
        g_message("That point is taken.");
        return;
    }
    g_move(pt);
}

/* What Return does on the point under the pointer. */
static void fr_hover(unsigned char id)
{
    static char b[42];
    if (id == 255) {
        g_message("Move to an empty point.");
        return;
    }
    strcpy(b, pt_of(id) ? "That point is taken." : "Press " KEY_RET " to place a stone on ");
    if (b[0] == 'P') {
        strcat(b, pt);
        strcat(b, ".");
    }
    g_message(b);
}

/* The other player's move in words: "ann placed a stone on h8." */
static void fr_event(void)
{
    if (is(w[3], "PUT") && wn > 5 && num(w[4]) != (unsigned char)g_seat) {
        strcat(g_who(num(w[4])), " placed a stone on ");
        strcat(g_last, w[5]);
        strcat(g_last, ".");
    }
}

#pragma rodata-name (push, "OVHDR")
const struct game g_fiveinarow = {"fiveinarow", "Five in a Row", fr_begin, fr_line, fr_draw, fr_click, 0,
                                  "Click an empty point to play it", 0, fr_event, fr_hover};
#pragma rodata-name (pop)
