/* ParlorNet on the Commodore 64: Mancala's board (core/games/mancala.c includes it). The pits are black holes
   in a wooden board, four cells by three, the stores four by eight at the ends; each count is written in
   the hole's middle, white, and the last pit played in yellow. Above the board the other player, below it
   the viewer, each with their store's count. In monochrome the wood is gray. */
#include "gfx.h"

extern const unsigned char pic_mc_pit[], pic_mc_store[];

#define BX 3                            /* the board: cell columns 3 to 36, rows 4 to 15 */
#define BY 32
#define TOPY (BY + 8)                   /* the top row's pits, the bottom row's, the stores */
#define BOTY (BY + 56)
#define STY (BY + 8)

static unsigned char shown[14];
static unsigned char shown_last = 255;

/* Where count k (0-13 in B order) is drawn, for a viewer in seat me: its first cell column and line. */
static unsigned char at_x, at_y, at_store;
static void place(unsigned char k)
{
    unsigned char s = k / 7, p = k % 7;    /* seat, and pit 0-5 or 6 the store */
    at_store = p == 6;
    if (s == me) {
        at_x = at_store ? BX + 29 : BX + 1 + (p + 1) * 4;
        at_y = at_store ? STY : BOTY;
    } else {
        at_x = at_store ? BX + 1 : BX + 1 + (6 - p) * 4;
        at_y = at_store ? STY : TOPY;
    }
}

static unsigned char wood(void)
{
    return mono ? C_GREY : C_ORANGE;
}

static void draw_count(unsigned char k)
{
    unsigned char n = cnt[k], last = k == shown_last;
    place(k);
    hg_x = at_x;
    hg_y = at_y;
    hg_w = 4;
    hg_h = at_store ? 64 : 24;
    hg_fg = C_BLACK;
    hg_bg = wood();
    hgr_color();
    hg_src = at_store ? pic_mc_store : pic_mc_pit;
    hgr_blit();
    hg_fg = last && !mono ? C_YELLOW : C_WHITE;
    hg_bg = C_BLACK;
    hg_x = at_x + 1;
    hg_y = at_y + (at_store ? 28 : 8);
    hgr_char(n >= 10 ? '0' + n / 10 : ' ');
    hgr_char('0' + n % 10);
    shown[k] = n;
}

static void mc_draw(void)
{
    unsigned char k, y, was;
    static char b[34];
    me = g_seat == 1 ? 1 : 0;
    if (g_full) {
        memset(shown, 255, sizeof shown);
        shown_last = 255;
        ui_rect(BX, BY, 34, 96, F_BLACK);
        hg_x = BX;
        hg_y = BY;
        hg_w = 34;
        hg_h = 96;
        hg_fg = C_BLACK;
        hg_bg = wood();
        hgr_color();
        for (k = 1; k <= 6; ++k) {
            place(me * 7 + k - 1);
            ui_target(at_x * 4 + 16, at_y + 12, 15, 11, k);
        }
    }
    was = shown_last;
    shown_last = last_seat < 2 ? last_seat * 7 + last_pit - 1 : 255;
    for (k = 0; k < 14; ++k)
        if (cnt[k] != shown[k] || k == was || k == shown_last)
            draw_count(k);
    /* names: the other player above the board, the viewer below; the store's owner beside it */
    ui_ink(F_WHITE, F_BLACK);
    for (k = 0; k < 2; ++k) {
        y = k == me ? BY + 104 : BY - 16;
        strcpy(b, g_turn == k && !g_over ? "> " : "  ");
        strcat(b, g_name(k));
        strcat(b, ": ");
        strcat(b, utoa10(cnt[k * 7 + 6]));
        ui_textn(BX, y, b, 34);
    }
    strcpy(b, rules[0] == 'o' ? "Oware" : "Kalah");
    strcat(b, ", ");
    strcat(b, utoa10(seeds));
    strcat(b, " seeds a pit");
    ui_textn(BX, 152, b, 34);
    ui_textn(BX, 160, g_myturn ? "Choose a pit of your row (1-6)." : "", 34);
}
