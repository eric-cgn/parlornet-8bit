/* ParlorNet on the Apple IIe and IIgs: Mancala's board (core/games/mancala.c includes it). In color the pits
   are black holes in an orange board; in monochrome they are white outlines. Each shows its count of seeds. */
#include "apple2.h"
extern const unsigned char spr_mc_pit[], spr_mc_store[], spr_mc_m_pit[], spr_mc_m_store[];

#define BX 1                            /* the board: from byte column 1, lines 24 to 96 */
#define BY 24
#define PW 4                            /* a pit: 4 bytes by 24 lines */
#define TOPY (BY + 4)
#define BOTY (BY + 44)
#define STY (BY + 3)

static unsigned char shown[14];
static unsigned char shown_last = 255;

/* Where count k (0-13 in B order) is drawn, for a viewer in seat me. */
static unsigned char at_x, at_y, at_store;
static void place(unsigned char k)
{
    unsigned char s = k / 7, p = k % 7;    /* seat, and pit 0-5 or 6 the store */
    at_store = p == 6;
    if (s == me) {
        at_x = at_store ? BX + 1 + 7 * PW : BX + 1 + (p + 1) * PW;
        at_y = at_store ? STY : BOTY;
    } else {
        at_x = at_store ? BX + 1 : BX + 1 + (6 - p) * PW;
        at_y = at_store ? STY : TOPY;
    }
}

static void draw_count(unsigned char k)
{
    unsigned char n = cnt[k];
    place(k);
    hg_x = at_x;
    hg_y = at_y;
    hg_src = mono ? (at_store ? spr_mc_m_store : spr_mc_m_pit) : (at_store ? spr_mc_store : spr_mc_pit);
    hgr_blit();
    hg_x = at_x + 1;
    hg_y = at_y + (at_store ? 29 : 8);
    hgr_char(n >= 10 ? '0' + n / 10 : ' ');
    hgr_char('0' + n % 10);
    shown[k] = n;
}

/* The last pit played: a mark under it (the bottom row) or above it (the top row). */
static void last_mark(unsigned char on)
{
    unsigned char k;
    if (shown_last == 255)
        return;
    k = shown_last;
    place(k);
    ui_rect(at_x + 1, at_y == BOTY ? at_y + 25 : at_y - 3, 2, 2, on ? F_WHITE : (mono ? F_BLACK : F_ORANGE));
}

static void mc_draw(void)
{
    unsigned char k, y;
    static char b[30];
    me = g_seat == 1 ? 1 : 0;
    if (g_full) {
        memset(shown, 255, sizeof shown);
        shown_last = 255;
        if (!mono)
            ui_rect(BX, BY, 2 + 8 * PW, 74, F_ORANGE);
        else
            ui_box(BX * 7, BY, (2 + 8 * PW) * 7, 74);
        for (k = 1; k <= 6; ++k) {
            place(me * 7 + k - 1);
            ui_target((at_x * 7 + 14) / 2, at_y + 12, 13, 12, k);
        }
    }
    last_mark(0);
    for (k = 0; k < 14; ++k)
        if (cnt[k] != shown[k])
            draw_count(k);
    shown_last = last_seat < 2 ? last_seat * 7 + last_pit - 1 : 255;
    last_mark(1);
    /* names: the other player above the board, the viewer below; the store's owner beside it */
    for (k = 0; k < 2; ++k) {
        y = k == me ? BY + 78 : 12;
        strcpy(b, g_turn == k && !g_over ? "> " : "  ");
        strcat(b, g_name(k));
        strcat(b, ": ");
        strcat(b, utoa10(cnt[k * 7 + 6]));
        ui_textn(BX + 1, y, b, 30);
    }
    strcpy(b, rules[0] == 'o' ? "Oware" : "Kalah");
    strcat(b, ", ");
    strcat(b, utoa10(seeds));
    strcat(b, " seeds a pit");
    ui_textn(BX + 1, 130, b, 30);
    ui_textn(BX + 1, 140, g_myturn ? "Choose a pit of your row." : "", 30);
}
