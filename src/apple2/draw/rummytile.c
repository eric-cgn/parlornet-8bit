/* ParlorNet on the Apple IIe and IIgs: Rummy Tile's table and rack (core/games/rummytile.c includes it). A
   tile is its color letter and number, with a bar in its color underneath on a color monitor. */
#include "apple2.h"
#define TY 10                           /* the table's first row */
#define RH 11                           /* a row: 10 lines of tile, 1 apart */
#define BTY 100                         /* the buttons */
#define RY 111                          /* the rack */
static const unsigned char tile_fill[] = {F_ORANGE, F_BLUE, F_WHITE, F_GREEN, F_VIOLET};

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
        ui_target(36 * 7 / 2 + 2, BTY + 4, 4, 4, ID_UP);
        ui_target(38 * 7 / 2 + 2, BTY + 4, 4, 4, ID_DOWN);
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
    /* ^ and v, the last two targets, are out of use while they are not shown */
    t_hw[ROWS * SETMAX + MAXR + 5] = t_hw[ROWS * SETMAX + MAXR + 6] = nsets > ROWS ? 4 : 0;
    info();
}

static void redraw(void)
{
    ui_hide();
    rt_draw();
    ui_show();
}
