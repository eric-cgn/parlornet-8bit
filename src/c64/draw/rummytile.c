/* ParlorNet on the Commodore 64: Rummy Tile's table and rack (core/games/rummytile.c includes it). A tile is
   two cells in its color (red, light blue, since the screen is blue, black, orange for yellow, purple for a
   joker) with its number in white (in black on light blue), and a cell of the table between tiles; a chosen tile is white with its number in its color. The table's
   sets are on every other row, with ! before a set that is not valid and * before one changed this turn;
   then the turn's count, the buttons, the rack (13 tiles a row) and the pool and players. In monochrome a
   tile is light gray with its color's letter and number in black, three cells, a chosen one inverted. */
#include "gfx.h"

#define TY 8                            /* the table's first row (line), sets 16 lines apart */
#define INFO2 128                       /* the turn's count */
#define BTY 136                         /* the buttons */
#define RY 144                          /* the rack's three rows */
#define INFO 168                        /* the pool and the players */

static const unsigned char tile_col[] = {C_RED, C_LBLUE, C_BLACK, C_ORANGE, C_PURPLE};

static void draw_tile(unsigned char x, unsigned char y, unsigned char t, unsigned char chosen)
{
    static char b[4];
    unsigned char c;
    ui_ink(F_WHITE, F_BLACK);
    ui_textn(x, y, "", 3);
    if (t == 0xFF)
        return;
    tile_name(b, t);
    hg_x = x;
    hg_y = y;
    if (mono) {
        hg_fg = chosen ? C_LGREY : C_BLACK;
        hg_bg = chosen ? C_BLACK : C_LGREY;
        hgr_char(b[0]);
        hgr_char(b[1] ? b[1] : ' ');
        hgr_char(b[1] && b[2] ? b[2] : ' ');
        return;
    }
    c = tile_col[t >> 4];
    hg_fg = chosen ? c : c == C_LBLUE ? C_BLACK : C_WHITE;
    hg_bg = chosen ? C_WHITE : c;
    if ((t >> 4) == 4) {                /* the joker */
        hgr_char(' ');
        hgr_char('J');
    } else {
        hgr_char(b[2] ? b[1] : ' ');
        hgr_char(b[2] ? b[2] : b[1]);
    }
}

static void draw_table(void)
{
    unsigned char r, i, k;
    for (r = 0; r < ROWS; ++r) {
        k = top + r;
        ui_ink(F_WHITE, F_BLACK);
        ui_textn(0, TY + r * 16, "", 40);
        if (k >= nsets)
            continue;
        hg_x = 0;
        hg_y = TY + r * 16;
        /* a set that is not valid: !; one made or changed this turn: * */
        hgr_char((set_k[k] & 0x7F) == 'x' ? '!' : set_k[k] & 0x80 ? '*' : ' ');
        for (i = 0; i < set_n[k]; ++i)
            draw_tile(1 + 3 * i, TY + r * 16, set_t[k][i], tsel_k == k && tsel_p == i);
    }
    ui_ink(F_WHITE, F_BLACK);
    if (nsets > ROWS) {
        static char b[12];
        strcpy(b, utoa10(top + 1));
        strcat(b, "-");
        strcat(b, utoa10(top + ROWS < nsets ? top + ROWS : nsets));
        strcat(b, "/");
        strcat(b, utoa10(nsets));
        ui_textn(29, BTY, b, 7);
    }
}

static void draw_rack(void)
{
    unsigned char i;
    for (i = 0; i < 39; ++i)
        draw_tile(1 + (i % 13) * 3, RY + (i / 13) * 8, i < nrack ? rack[i] : 0xFF, i < nrack && rsel[i]);
}

static void info(void)
{
    static char b[42];
    unsigned char s;
    ui_ink(F_WHITE, F_BLACK);
    strcpy(b, "Pool ");
    strcat(b, utoa10(pool));
    for (s = 0; s < g_seats && s < 4; ++s) {
        strcat(b, s == g_turn && !g_over ? " >" : "  ");
        strncat(b, g_name(s), 6);
        strcat(b, " ");
        strcat(b, gone[s] ? "-" : utoa10(cnt[s]));
    }
    ui_textn(0, INFO, b, 40);
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
    ui_textn(0, INFO2, b, 40);
}

static void rt_draw(void)
{
    unsigned char r, i, mid;
    if (g_full) {
        mid = mono ? 6 : 4;             /* a tile's middle: three cells in monochrome, else two */
        for (r = 0; r < ROWS; ++r)
            for (i = 0; i < SETMAX; ++i)
                ui_target((1 + 3 * i) * 4 + mid, TY + r * 16 + 4, 6, 4, r * SETMAX + i);
        for (i = 0; i < MAXR; ++i)
            ui_target((1 + (i % 13) * 3) * 4 + mid, RY + (i / 13) * 8 + 4, 6, 4, ID_RACK + i);
        ui_button(0, BTY, "End", ID_DONE);
        ui_button(6, BTY, "Undo", ID_UNDO);
        ui_button(12, BTY, "Reset", ID_RESET);
        ui_button(19, BTY, "New", ID_NEW);
        ui_button(24, BTY, "Split", ID_SPLIT);
        ui_target(36 * 4 + 2, BTY + 4, 4, 4, ID_UP);
        ui_target(38 * 4 + 2, BTY + 4, 4, 4, ID_DOWN);
    }
    if (top && top >= nsets)
        top = nsets > ROWS ? nsets - ROWS : 0;
    /* the targets of empty places are out of use */
    for (r = 0; r < ROWS; ++r)
        for (i = 0; i < SETMAX; ++i)
            t_hw[r * SETMAX + i] = top + r < nsets && (i < set_n[top + r] || i == 0) ? 6 : 0;
    for (i = 0; i < MAXR; ++i)
        t_hw[ROWS * SETMAX + i] = i < nrack ? 6 : 0;
    draw_table();
    draw_rack();
    ui_ink(F_WHITE, F_BLACK);
    hg_x = 36;
    hg_y = BTY;
    hgr_text(nsets > ROWS ? "^ v" : "   ");
    /* ^ and v, the last two targets, are out of use while they are not shown */
    t_hw[ROWS * SETMAX + MAXR + 5] = t_hw[ROWS * SETMAX + MAXR + 6] = nsets > ROWS ? 4 : 0;
    info();
}

static void redraw(void)
{
    rt_draw();
}
