/* ParlorNet on the Commodore 64: Checkers' board (core/games/checkers.c includes it). The squares are two
   cells by two (16 by 16 pixels), green and buff as on a tournament board; the men are black and white on
   the green, a king with a crown. The chosen piece has a frame round it, a sprite over the board, and the
   squares it can go to a yellow dot. Beside the board: each side's man, player and counts, and the buttons.
   In monochrome the squares are two grays. */
#include "gfx.h"

extern const unsigned char pic_ck_man[], pic_ck_king[], pic_ck_hint[], pic_ck_empty[], spr_ck_frame[];

#define X0 2                            /* the board's first cell column, and its top line */
#define Y0 16
#define PX 21                           /* the panel's column */
#define SPR_FRAME (SPR_GAME)

static char shown[33];
static const char *shown_a, *shown_b;

/* Where dark square s (1-32) is drawn: cell column and line. */
static unsigned char sq_x, sq_y;
static void place_sq(unsigned char s)
{
    unsigned char r = (s - 1) >> 2, c = ((s - 1) & 3) * 2 + !(r & 1);
    if (flip) {
        r = 7 - r;
        c = 7 - c;
    }
    sq_x = X0 + c * 2;
    sq_y = Y0 + r * 16;
}

/* A piece (or none, or a dot) in a square's two cells by two, on the square's color. */
static void piece_at(char c, unsigned char x, unsigned char y, unsigned char square)
{
    c &= 0x7F;
    hg_x = x;
    hg_y = y;
    hg_w = 2;
    hg_h = 16;
    hg_bg = square;
    hg_fg = c == 'x' || c == 'X' ? C_BLACK : c == 'o' || c == 'O' ? C_WHITE : mono ? C_WHITE : C_YELLOW;
    hgr_color();
    hg_src = c == 'x' || c == 'o' ? pic_ck_man : c == 'X' || c == 'O' ? pic_ck_king
           : c == 'h' ? pic_ck_hint : pic_ck_empty;
    hgr_blit();
}

static unsigned char dark(void)
{
    return mono ? C_GREY : C_GREEN;
}

static void draw_sq(unsigned char s, char c)
{
    shown[s] = c;
    place_sq(s);
    piece_at(c, sq_x, sq_y, dark());
    if (c & 0x80) {                     /* the chosen piece: the frame over it */
        spr_x[2] = sq_x * 8 + 24 - 1;
        spr_y[2] = sq_y + 50 - 1;
        spr_on |= 4;
    }
}

/* A panel button: its words at column x, line y, and target id on their middle, as ui_button puts a
   button's target, since the words change with the turn. With no words the target is out of use. */
static void button(unsigned char x, unsigned char y, const char *s, unsigned char id)
{
    unsigned char i = ui_find(id), hw = 0;
    if (*s) {
        ui_button_draw(x, y, s);
        hw = (unsigned char)(strlen(s) + 2) << 1;
    }
    if (i < t_n) {
        t_x[i] = (unsigned char)(x << 2) + hw;
        t_hw[i] = hw;
    }
}

static void draw_panel(void)
{
    unsigned char s, me = flip ? 0 : 1, y;
    static char t[14];
    for (s = 0; s < 2; ++s) {
        y = s == me ? 120 : GAME_TOP + 8;
        piece_at(s ? 'o' : 'x', PX, y, dark());
        ui_ink(F_WHITE, F_BLACK);       /* (the piece left its colors in the ink) */
        ui_textn(PX + 3, y, g_name(s), 16);
        ui_textn(PX + 3, y + 8, s ? "White" : "Black", 16);
        strcpy(t, utoa10(cnt[s * 2]));
        strcat(t, " men ");
        if (cnt[s * 2 + 1]) {
            strcat(t, utoa10(cnt[s * 2 + 1]));
            strcat(t, " kings");
        }
        ui_textn(PX, y + 24, t, 19);
        ui_textn(PX, y + 32, g_turn == s && !g_over ? "to play" : "", 19);
    }
    panel_offer();
    if (offer_a != shown_a || offer_b != shown_b) {
        shown_a = offer_a;
        shown_b = offer_b;
        ui_textn(PX, 72, "", 14);
        ui_textn(PX, 88, "", 14);
        button(PX, 72, offer_a, ID_A);
        button(PX, 88, offer_b, ID_B);
    }
}

static void ck_draw(void)
{
    unsigned char s, r, c;
    char want;
    board_state();
    if (g_full) {
        sel = 0;
        shown_a = shown_b = 0;
        memset(shown, 0, sizeof shown);
        spr_load(SPR_FRAME, spr_ck_frame);
        spr_ptr[2] = SPR_FRAME;
        spr_col[2] = mono ? C_WHITE : C_YELLOW;
        for (r = 0; r < 8; ++r)
            for (c = 0; c < 8; ++c)
                if (!((r + c) & 1))
                    piece_at('.', X0 + c * 2, Y0 + r * 16, mono ? C_LGREY : C_YELLOW);
        for (s = 1; s <= 32; ++s) {
            place_sq(s);
            ui_target(sq_x * 4 + 4, sq_y + 8, 4, 8, s);
        }
        ui_target(PX * 4, 76, 0, 4, ID_A);     /* (put on their words by button) */
        ui_target(PX * 4, 92, 0, 4, ID_B);
    }
    board_state();
    if (!sel)
        spr_on &= ~4;
    for (s = 1; s <= 32; ++s) {
        want = sq_want(s);
        if (want != shown[s])
            draw_sq(s, want);
    }
    ui_ink(F_WHITE, F_BLACK);
    draw_panel();
}

static void redraw(void)
{
    ck_draw();
}
