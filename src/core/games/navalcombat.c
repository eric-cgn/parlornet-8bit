/* ParlorNet: Naval Combat, the classic, batnav and salvo rules (docs/types/navalcombat-1.md). While placing,
   the large grid is the player's own waters: a ship is placed by clicking one end, then the other (the
   squares where the other end may go are marked, other_end), or Random places the rest. Ready ends the
   placing. While shooting, the large grid is the other player's waters, where a click fires, and the small
   grid is the player's own. The machine draws them (draw/navalcombat.c, included below: nc_draw and
   redraw), the large grid's squares targets row * 10 + column, the buttons ID_RANDOM to ID_READY. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "game.h"

#define ID_RANDOM 100
#define ID_UNDO 101
#define ID_CLEAR 102
#define ID_READY 103

static char grid[2][10][11];            /* each seat's waters, as the view's B rows */
static char phase[8];
static unsigned char afloat[2], ready[2], nextlen, salvo_left;
static char lastshot[6];
static signed char last_seat = -1, sel = -1;

static void nc_begin(void)
{
    memset(grid, '.', sizeof grid);
    nextlen = 0;
    salvo_left = 0;
    last_seat = -1;
    ready[0] = ready[1] = 0;
}

static void nc_line(void)
{
    unsigned char s, y;
    if (is(w[0], "B") && wn > 3) {
        s = num(w[1]);
        y = num(w[2]) - 1;
        if (s < 2 && y < 10)
            strncpy(grid[s][y], w[3], 10);
    } else if (is(w[0], "Q") && wn > 1) {
        strncpy(phase, w[1], 7);
    } else if (is(w[0], "L") && wn > 2) {
        afloat[0] = num(w[1]);
        afloat[1] = num(w[2]);
    } else if (is(w[0], "RD") && wn > 2) {
        ready[0] = num(w[1]);
        ready[1] = num(w[2]);
    } else if (is(w[0], "N") && wn > 1) {
        nextlen = num(w[1]);
    } else if (is(w[0], "SL") && wn > 1) {
        salvo_left = num(w[1]);
    } else if (is(w[0], "LS") && wn > 2) {
        last_seat = num(w[1]);
        strncpy(lastshot, w[2], 5);
    }
}

static unsigned char me(void)
{
    return g_seat == 1 ? 1 : 0;
}

static unsigned char placing(void)
{
    return strcmp(phase, "place") == 0;
}

static void sq_name(char *b, unsigned char x, unsigned char y)
{
    b[0] = 'a' + x;
    strcpy(b + 1, utoa10(y + 1));
}

/* Could the square be the other end of the next ship, from the chosen end: in line with it, at the
   ship's length, over open water? (The parlor checks the rest.) */
static unsigned char other_end(unsigned char x, unsigned char y)
{
    unsigned char sx, sy, d, i, m = me();
    if (sel < 0 || !nextlen)
        return 0;
    sx = sel % 10;
    sy = sel / 10;
    if (sx == x)
        d = sy > y ? sy - y : y - sy;
    else if (sy == y)
        d = sx > x ? sx - x : x - sx;
    else
        return 0;
    if (d != nextlen - 1 || !d)
        return 0;
    for (i = 0; i <= d; ++i) {
        unsigned char cx = sx == x ? x : (sx < x ? sx + i : sx - i);
        unsigned char cy = sy == y ? y : (sy < y ? sy + i : sy - i);
        if (grid[m][cy][cx] != '.')
            return 0;
    }
    return 1;
}

/* The machine's drawing: nc_draw and redraw. */
#include "draw/navalcombat.c"

static void nc_click(unsigned char id)
{
    static char t[10];
    unsigned char x = id % 10, y = id / 10;
    if (id >= ID_RANDOM) {
        if (!placing() || ready[me()] || g_seat < 0)
            return;
        sel = -1;
        g_move(id == ID_RANDOM ? "random" : id == ID_UNDO ? "undo" : id == ID_CLEAR ? "clear" : "ready");
        return;
    }
    if (placing()) {
        if (g_seat < 0 || ready[me()] || !nextlen) {
            g_message(nextlen ? "" : "All placed: Ready, or Undo.");
            return;
        }
        if (sel < 0 || sel == id) {
            sel = sel == id ? -1 : id;
            if (sel >= 0 && nextlen == 1) {     /* a ship of one square */
                sq_name(t, x, y);
                strcat(t, "-");
                sq_name(t + strlen(t), x, y);
                sel = -1;
                if (!g_move(t))
                    redraw();
                return;
            }
            g_message(sel < 0 ? "" : "Now the other end.");
            redraw();
            return;
        }
        if (!other_end(x, y)) {
            sel = id;
            g_message("Now the other end.");
            redraw();
            return;
        }
        sq_name(t, sel % 10, sel / 10);
        strcat(t, "-");
        sq_name(t + strlen(t), x, y);
        sel = -1;
        g_move(t);
        return;
    }
    if (!g_myturn) {
        g_message("Not your turn.");
        return;
    }
    if (grid[1 - me()][y][x] != '.') {
        g_message("Fired there already.");
        return;
    }
    sq_name(t, x, y);
    g_move(t);
}

/* Esc lets go of a picked end, OA-G places the rest of the fleet at random, OA-U takes back the last ship
   and OA-Z every ship. */
static unsigned char nc_key(unsigned char k)
{
    if (k == K_ESC) {
        if (sel < 0)
            return 0;
        sel = -1;
        g_message("");
        redraw();
        return 1;
    }
    k = k == (U_OA | 'G') ? ID_RANDOM : k == (U_OA | 'U') ? ID_UNDO : k == (U_OA | 'Z') ? ID_CLEAR : 0;
    if (!k)
        return 0;
    nc_click(k);
    return 1;
}

#pragma rodata-name (push, "OVHDR")
const struct game g_navalcombat = {"navalcombat", "Naval Combat", nc_begin, nc_line, nc_draw, nc_click, nc_key,
                                   "Place: one end, the other. Then fire", 1};
#pragma rodata-name (pop)
