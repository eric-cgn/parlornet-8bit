/* ParlorNet: Rummy Tile Classic (docs/types/rummytile-1.md), 2 to 4 players. The table's sets are rows, ROWS
   of them shown at once from top (the arrows buttons scroll when there are more), and the rack is below. A
   tile is its color letter and number (R red, B blue, K black, Y yellow, J a joker).

   Rack tiles are chosen by clicking them (several at once); a click on a set then puts them in it, and
   New makes a new set of them. A tile of the table is chosen with a click; a click on another set moves
   it there, New moves it to a new set, Split splits its set before it, and a click on the rack takes it
   back (a tile played this turn only). Done ends the turn: the parlor keeps it if the table is valid,
   and otherwise puts everything back and deals a tile. Undo takes back a step, Reset the whole turn.

   The machine draws it (draw/rummytile.c, included below: rt_draw and redraw), with the targets: a set's
   places row * SETMAX + place, the rack's ID_RACK up, the buttons ID_DONE to ID_DOWN. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "game.h"

#define MAXS 40
#define SETMAX 13
#define MAXR 39
#define ROWS 8                          /* sets shown at once */
#define ID_RACK 110
#define ID_DONE 200
#define ID_UNDO 201
#define ID_RESET 202
#define ID_NEW 203
#define ID_SPLIT 204
#define ID_UP 205
#define ID_DOWN 206

static unsigned char set_t[MAXS][SETMAX], set_n[MAXS], nsets;
static char set_k[MAXS];                /* r, g or x; | 0x80 when changed this turn */
static unsigned char rack[MAXR], nrack;
static unsigned char pool, cnt[4], meld[4], gone[4];
static unsigned char tp_n, tp_code;     /* TP: tiles played; ok, none, sets, meld, joker */
static unsigned char sorted;            /* OA-O: the rack as dealt (0), by color (1) or by number (2) */
static char tp_pts[4];
static unsigned char top;               /* the first set shown */
static unsigned char rsel[MAXR];        /* rack tiles chosen */
static signed char tsel_k = -1, tsel_p; /* a table tile chosen: set, place */

static const char colours[] = "RBKYJ";

static void rt_begin(void)
{
    nsets = 0;
    nrack = 0;
    tp_n = 0;
    tp_code = 0;
}

static unsigned char tile_of(const char *s)
{
    const char *c = strchr(colours, s[0] & 0xDF);
    if (!c || s[0] == 0)
        return 0xFF;
    return ((c - colours) << 4) | (s[0] == 'J' || s[0] == 'j' ? 0 : num(s + 1));
}

/* The rack in the order OA-O chose: a tile's color is its high four bits, its number the low four. */
static void sort_rack(void)
{
    unsigned char i, j, t;
    if (!sorted)
        return;
    for (i = 1; i < nrack; ++i)
        for (j = i; j; --j) {
            t = rack[j - 1];
            if (sorted == 1 ? t <= rack[j] : ((t & 15) << 4 | t >> 4) <= ((rack[j] & 15) << 4 | rack[j] >> 4))
                break;
            rack[j - 1] = rack[j];
            rack[j] = t;
        }
}

static void rt_line(void)
{
    unsigned char i, k;
    if (is(w[0], "ST") && wn > 2) {
        k = num(w[1]) - 1;
        if (k >= MAXS)
            return;
        set_k[k] = w[2][0] | (w[2][1] == '*' ? 0x80 : 0);
        set_n[k] = 0;
        for (i = 3; i < wn && set_n[k] < SETMAX; ++i)
            set_t[k][set_n[k]++] = tile_of(w[i]);
        if (k >= nsets)
            nsets = k + 1;
    } else if (is(w[0], "H")) {
        for (i = 1; i < wn && nrack < MAXR; ++i)
            rack[nrack++] = tile_of(w[i]);
        sort_rack();
    } else if (is(w[0], "P") && wn > 1) {
        pool = num(w[1]);
    } else if ((is(w[0], "C") || is(w[0], "I") || is(w[0], "G")) && wn > 1) {
        unsigned char *a = w[0][0] == 'C' ? cnt : w[0][0] == 'I' ? meld : gone;
        for (i = 1; i < wn && i <= 4; ++i)
            a[i - 1] = num(w[i]);
    } else if (is(w[0], "TP") && wn > 3) {
        tp_n = num(w[1]);
        strncpy(tp_pts, w[2], 3);
        tp_code = w[3][0];
    }
}

static char *tile_name(char *b, unsigned char t)
{
    b[0] = colours[t >> 4];
    if ((t >> 4) == 4)
        b[1] = 0;
    else
        strcpy(b + 1, utoa10(t & 15));
    return b;
}

/* The machine's drawing: rt_draw and redraw. */
#include "draw/rummytile.c"

static void unchoose(void)
{
    memset(rsel, 0, sizeof rsel);
    tsel_k = -1;
}

/* The rack tiles chosen, as one token's tile names. */
static unsigned char rack_tiles(char *b)
{
    unsigned char i, n = 0;
    static char t[4];
    b[0] = 0;
    for (i = 0; i < nrack; ++i)
        if (rsel[i]) {
            strcat(b, tile_name(t, rack[i]));
            ++n;
        }
    return n;
}

static void send(const char *t)
{
    unchoose();
    if (!g_move(t))
        redraw();
}

static void rt_click(unsigned char id)
{
    static char tok[48];
    unsigned char k, p;
    if (!g_myturn) {
        g_message("It's not your turn.");
        return;
    }
    if (id < ROWS * SETMAX) {             /* a tile (or the first place) of a set on the table */
        k = top + id / SETMAX;
        p = id % SETMAX;
        if (k >= nsets)
            return;
        if (rack_tiles(tok)) {
            strcat(tok, "-");
            strcat(tok, utoa10(k + 1));
            send(tok);
            return;
        }
        if (tsel_k >= 0) {
            if (tsel_k == k && tsel_p == p)
                tsel_k = -1;
            else if (tsel_k != k) {
                strcpy(tok, utoa10(tsel_k + 1));
                strcat(tok, ".");
                strcat(tok, utoa10(tsel_p + 1));
                strcat(tok, "-");
                strcat(tok, utoa10(k + 1));
                send(tok);
                return;
            } else
                tsel_p = p;
        } else if (p < set_n[k]) {
            tsel_k = k;
            tsel_p = p;
            g_message("");
        }
        redraw();
        return;
    }
    if (id >= ID_RACK && id < ID_RACK + MAXR) {
        p = id - ID_RACK;
        if (tsel_k >= 0) {              /* a table tile back to the rack */
            strcpy(tok, utoa10(tsel_k + 1));
            strcat(tok, ".");
            strcat(tok, utoa10(tsel_p + 1));
            strcat(tok, "-r");
            send(tok);
            return;
        }
        rsel[p] ^= 1;
        redraw();
        g_message("");
        return;
    }
    switch (id) {
    case ID_DONE:
        unchoose();
        g_cmd("/end");                  /* (a dialog first when the turn would pass or not count) */
        break;
    case ID_UNDO:
        unchoose();
        g_undo();
        break;
    case ID_RESET:
        send("reset");
        break;
    case ID_NEW:
        if (rack_tiles(tok)) {
            strcat(tok, "-n");
            send(tok);
        } else if (tsel_k >= 0) {
            strcpy(tok, utoa10(tsel_k + 1));
            strcat(tok, ".");
            strcat(tok, utoa10(tsel_p + 1));
            strcat(tok, "-n");
            send(tok);
        } else
            g_message("First choose tiles of your rack.");
        break;
    case ID_SPLIT:
        if (tsel_k < 0 || !tsel_p) {
            g_message("Choose the tile to split the set before.");
            break;
        }
        strcpy(tok, utoa10(tsel_k + 1));
        strcat(tok, "/");
        strcat(tok, utoa10(tsel_p + 1));
        send(tok);
        break;
    case ID_UP:
        if (top) {
            top = top > ROWS ? top - ROWS : 0;
            redraw();
        }
        break;
    case ID_DOWN:
        if (top + ROWS < nsets) {
            top += ROWS;
            redraw();
        }
        break;
    }
}

/* What Return does under the pointer: pick a tile, put the tiles picked on a set, or a button's action. */
static void rt_hover(unsigned char id)
{
    static char b[42], t[4];
    const char *s = 0;
    unsigned char k, p, n = 0;
    for (k = 0; k < nrack; ++k)
        n += rsel[k];
    if (id >= ID_RACK && id < ID_RACK + nrack) {
        p = id - ID_RACK;
        strcpy(b, tsel_k >= 0 ? "Press " KEY_RET " to take it back." : rsel[p] ? "Press " KEY_RET " to let go of "
               : "Press " KEY_RET " to pick ");
        if (tsel_k < 0) {
            strcat(b, tile_name(t, rack[p]));
            strcat(b, ".");
        }
    } else if (id < ROWS * SETMAX) {
        k = top + id / SETMAX;
        p = id % SETMAX;
        strcpy(b, n ? "Press " KEY_RET " to put them on set " : tsel_k >= 0 ? "Press " KEY_RET " to move it to set "
               : p < set_n[k] ? "Press " KEY_RET " to pick a tile of set " : "Pick tiles from your rack.");
        if (b[strlen(b) - 1] == ' ') {
            strcat(b, utoa10(k + 1));
            strcat(b, ".");
        }
    } else
        s = id == ID_DONE ? "Press " KEY_RET " to end your turn." : id == ID_UNDO ? "Press " KEY_RET " to undo a step." :
            id == ID_RESET ? "Press " KEY_RET " to start your turn over." : id == ID_NEW ? "Press " KEY_RET " to make a new set." :
            id == ID_SPLIT ? "Press " KEY_RET " to split the set." : "Pick tiles, or " OA "E to end your turn.";
    g_message(s ? s : (const char *)b);
}

/* The other player's turn in words: "ann played 3 tiles.", "ann ended the turn and took a tile." */
static void rt_event(void)
{
    if (num(w[4]) == (unsigned char)g_seat)
        return;
    if (is(w[3], "DONE") && wn > 5) {
        strcat(g_who(num(w[4])), " played ");
        strcat(g_last, w[5]);
        strcat(g_last, w[5][0] == '1' && !w[5][1] ? " tile." : " tiles.");
    } else if (is(w[3], "DRAW"))
        strcat(g_who(num(w[4])), " ended the turn and took a tile.");
    else if (is(w[3], "PASS"))
        strcat(g_who(num(w[4])), " passed.");
}

/* The keys: Esc lets go of the tiles picked; OA-E ends the turn, OA-U takes back a step, OA-Z the whole turn,
   OA-M makes a new set of the tiles picked, OA-B splits a set before the tile picked, OA-C says whether the
   turn would count, OA-P passes and OA-O sorts the rack. */
static unsigned char rt_key(unsigned char k)
{
    static const char keys[] = "EUZMB";
    static const unsigned char ids[] = {ID_DONE, ID_UNDO, ID_RESET, ID_NEW, ID_SPLIT};
    const char *p;
    if (k == K_ESC) {
        if (tsel_k < 0 && !memchr(rsel, 1, nrack))
            return 0;
        unchoose();
        g_message("");
        redraw();
        return 1;
    }
    if (!(k & U_OA))
        return 0;
    k &= 0x7F;
    if (k == 'C' || k == 'P') {
        g_cmd(k == 'C' ? "/check" : "/pass");
        return 1;
    }
    if (k == 'O') {
        sorted = sorted == 1 ? 2 : 1;
        unchoose();
        sort_rack();
        g_message(sorted == 1 ? "Your rack is sorted by color." : "Your rack is sorted by number.");
        redraw();
        return 1;
    }
    if (!k || !(p = strchr(keys, k)))
        return 0;
    rt_click(ids[p - keys]);
    return 1;
}

#pragma rodata-name (push, "OVHDR")
const struct game g_rummytile = {"rummytile", "Rummy Tile", rt_begin, rt_line, rt_draw, rt_click, rt_key,
                                 1, rt_event, rt_hover};
#pragma rodata-name (pop)
