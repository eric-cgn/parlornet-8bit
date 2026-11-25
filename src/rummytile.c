/* ParlorNet for the Apple IIe: Rummy Tile Classic (docs/types/rummytile-1.md), 2 to 4 players. The
   table's sets are rows across the top, one set a row (the arrows buttons scroll when there are more),
   and the rack is at the bottom. A tile is its color letter and number (R red, B blue, K black, Y
   yellow, J a joker), with a bar in its color underneath on a color monitor.

   Rack tiles are chosen by clicking them (several at once); a click on a set then puts them in it, and
   New makes a new set of them. A tile of the table is chosen with a click; a click on another set moves
   it there, New moves it to a new set, Split splits its set before it, and a click on the rack takes it
   back (a tile played this turn only). Done ends the turn: the parlor keeps it if the table is valid,
   and otherwise puts everything back and deals a tile. Undo takes back a step, Reset the whole turn. */
#include <string.h>
#include "a2.h"
#include "ui.h"
#include "net.h"
#include "game.h"

#define MAXS 40
#define SETMAX 13
#define MAXR 39
#define ROWS 8                          /* sets shown at once */
#define TY 10                           /* the table's first row */
#define RH 11                           /* a row: 10 lines of tile, 1 apart */
#define BTY 100                         /* the buttons */
#define RY 111                          /* the rack */
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
static char tp_pts[4];
static unsigned char top;               /* the first set shown */
static unsigned char rsel[MAXR];        /* rack tiles chosen */
static signed char tsel_k = -1, tsel_p; /* a table tile chosen: set, place */

static const char colours[] = "RBKYJ";
static const unsigned char tile_fill[] = {F_ORANGE, F_BLUE, F_WHITE, F_GREEN, F_VIOLET};

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

static void draw_tile(unsigned char x, unsigned char y, unsigned char t, unsigned char chosen)
{
    static char b[4];
    ui_rect(x, y, 3, RH, F_BLACK);
    if (t == 0xFF)
        return;
    tile_name(b, t);
    if (chosen)
        ui_ink(F_BLACK, F_WHITE);
    hg_x = x;
    hg_y = y;
    hgr_char(b[0]);
    hgr_char(b[1] ? b[1] : ' ');
    hgr_char(b[1] && b[2] ? b[2] : ' ');
    ui_ink(F_WHITE, F_BLACK);
    if (!mono)
        ui_rect(x, y + 8, 3, 2, tile_fill[t >> 4]);
    else
        ui_rect(x, y + 9, 3, 1, F_GREY);
    /* a line at the tile's left edge, so that tiles side by side stay apart */
    hg_px = x * 7;
    hg_y = y;
    hg_h = 10;
    hgr_vline();
}

static void draw_table(void)
{
    unsigned char r, i, k;
    for (r = 0; r < ROWS; ++r) {
        k = top + r;
        ui_rect(0, TY + r * RH, 40, RH, F_BLACK);
        if (k >= nsets)
            continue;
        hg_x = 0;
        hg_y = TY + r * RH;
        /* a set that is not valid: !; one made or changed this turn: * */
        hgr_char((set_k[k] & 0x7F) == 'x' ? '!' : set_k[k] & 0x80 ? '*' : ' ');
        for (i = 0; i < set_n[k]; ++i)
            draw_tile(1 + 3 * i, TY + r * RH, set_t[k][i], tsel_k == k && tsel_p == i);
    }
    if (nsets > ROWS) {
        static char b[12];
        strcpy(b, utoa10(top + 1));
        strcat(b, "-");
        strcat(b, utoa10(top + ROWS < nsets ? top + ROWS : nsets));
        strcat(b, "/");
        strcat(b, utoa10(nsets));
        ui_textn(31, BTY, b, 9);
    }
}

static void draw_rack(void)
{
    unsigned char i;
    ui_rect(0, RY, 40, 3 * RH, F_BLACK);
    for (i = 0; i < nrack; ++i)
        draw_tile((i % 13) * 3, RY + (i / 13) * RH, rack[i], rsel[i]);
}

static void info(void)
{
    static char b[42];
    unsigned char s;
    strcpy(b, "Pool ");
    strcat(b, utoa10(pool));
    for (s = 0; s < g_seats && s < 4; ++s) {
        strcat(b, s == g_turn && !g_over ? " >" : "  ");
        strncat(b, g_name(s), 6);
        strcat(b, " ");
        strcat(b, gone[s] ? "-" : utoa10(cnt[s]));
    }
    ui_textn(0, RY + 3 * RH + 2, b, 40);
    b[0] = 0;
    if (g_myturn) {
        if (tp_code == 'n')
            strcpy(b, "Nothing played: Done draws a tile");
        else {
            strcpy(b, utoa10(tp_n));
            strcat(b, tp_n == 1 ? " tile" : " tiles");
            if (tp_pts[0] != '-') {
                strcat(b, ", meld ");
                strcat(b, tp_pts);
            }
            strcat(b, tp_code == 'o' ? ": Done keeps it" : tp_code == 's' ? ": a set is not valid"
                          : tp_code == 'm' ? ": 30 needed" : tp_code == 'j' ? ": joker rule" : "");
        }
    }
    ui_textn(0, MSG_Y - 9, b, 40);
}

static void rt_draw(void)
{
    unsigned char r, i;
    if (g_full) {
        for (r = 0; r < ROWS; ++r)
            for (i = 0; i < SETMAX; ++i)
                ui_target((1 + 3 * i) * 7 / 2 + 5, TY + r * RH + 4, 5, 5, r * SETMAX + i);
        for (i = 0; i < MAXR; ++i)
            ui_target((i % 13) * 21 / 2 + 5, RY + (i / 13) * RH + 4, 5, 5, ID_RACK + i);
        ui_button(0, BTY, "Done", ID_DONE);
        ui_button(6, BTY, "Undo", ID_UNDO);
        ui_button(12, BTY, "Reset", ID_RESET);
        ui_button(19, BTY, "New", ID_NEW);
        ui_button(24, BTY, "Split", ID_SPLIT);
        ui_target(36 * 7 / 2 + 3, BTY + 4, 4, 4, ID_UP);
        ui_target(38 * 7 / 2 + 3, BTY + 4, 4, 4, ID_DOWN);
    }
    if (top && top >= nsets)
        top = nsets > ROWS ? nsets - ROWS : 0;
    /* the targets of empty places are out of use */
    for (r = 0; r < ROWS; ++r)
        for (i = 0; i < SETMAX; ++i)
            t_hw[r * SETMAX + i] = top + r < nsets && (i < set_n[top + r] || i == 0) ? 5 : 0;
    for (i = 0; i < MAXR; ++i)
        t_hw[ROWS * SETMAX + i] = i < nrack ? 5 : 0;
    draw_table();
    draw_rack();
    hg_x = 35;
    hg_y = BTY;
    hgr_text(nsets > ROWS ? " ^ v" : "    ");
    info();
}

static void redraw(void)
{
    ui_hide();
    rt_draw();
    ui_show();
}

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
        g_message("Not your turn.");
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
            g_message("Now a set, New, Split, or the rack.");
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
        if (rsel[p])
            g_message("Now a set, or New for a new one.");
        redraw();
        return;
    }
    switch (id) {
    case ID_DONE:
        unchoose();
        g_end();
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

static unsigned char rt_key(unsigned char k)
{
    switch (k | 0x20) {
    case 'd': rt_click(ID_DONE); return 1;
    case 'u': rt_click(ID_UNDO); return 1;
    case 'n': rt_click(ID_NEW); return 1;
    case 's': rt_click(ID_SPLIT); return 1;
    }
    return 0;
}

#pragma rodata-name (push, "OVHDR")
const struct game g_rummytile = {"rummytile", "Rummy Tile", rt_begin, rt_line, rt_draw, rt_click, rt_key,
                                 K1 "D done " K1 "U undo " K1 "N new set " K1 "S split", 1};
#pragma rodata-name (pop)
