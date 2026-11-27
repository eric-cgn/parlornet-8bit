/* ParlorNet on the Apple IIe and IIgs: Checkers' board (core/games/checkers.c includes it). Dark squares are
   black, light squares orange (dotted in monochrome); Black's men are white rims round black and White's are
   solid white, so both show on the dark squares in color and on a monochrome screen alike. A square is 3
   bytes by 19 lines. */
#include "apple2.h"
extern const unsigned char spr_ck_w[], spr_ck_b[], spr_ck_wk[], spr_ck_bk[], spr_ck_hint[];

#define X0 1                            /* the board: 3 bytes by 19 lines a square */
#define Y0 10
#define CW 3
#define CH 19
#define PX 27                           /* the panel's column */

static char shown[33];
static char shown_panel[24];

/* Where dark square s (1-32) is drawn: byte column and line. */
static unsigned char sq_x, sq_y;
static void place_sq(unsigned char s)
{
    unsigned char r = (s - 1) >> 2, c = ((s - 1) & 3) * 2 + !(r & 1);
    if (flip) {
        r = 7 - r;
        c = 7 - c;
    }
    sq_x = X0 + c * CW;
    sq_y = Y0 + r * CH;
}

static const unsigned char *piece(char c)
{
    switch (c) {
    case 'x': return spr_ck_b;
    case 'X': return spr_ck_bk;
    case 'o': return spr_ck_w;
    case 'O': return spr_ck_wk;
    case 'h': return spr_ck_hint;
    }
    return 0;
}

static void draw_sq(unsigned char s, char c)
{
    const unsigned char *p = piece(c);
    shown[s] = c;
    place_sq(s);
    ui_rect(sq_x, sq_y, CW, CH, F_BLACK);
    if (p) {
        hg_x = sq_x;
        hg_y = sq_y;
        hg_src = p;
        hgr_blit();
    }
    if (c & 0x80) {       /* the chosen piece: its own picture in a frame */
        p = piece(c & 0x7F);
        if (p) {
            hg_x = sq_x;
            hg_y = sq_y;
            hg_src = p;
            hgr_blit();
        }
        ui_box(sq_x * 7, sq_y, CW * 7, CH);
    }
}

/* A panel button: its words at column x, line y, and target id on their middle, as ui_button puts a
   button's target, since the words change with the turn. With no words the target is out of use. */
static void button(unsigned char x, unsigned char y, const char *s, unsigned char id)
{
    unsigned char i = ui_find(id), hw = 0;
    if (*s) {
        ui_button_draw(x, y, s);
        hw = (unsigned char)((strlen(s) + 2) * 7) >> 2;
    }
    if (i < t_n) {
        t_x[i] = (unsigned char)((x * 7) >> 1) + hw;
        t_hw[i] = hw;
    }
}

static void draw_panel(void)
{
    unsigned char s, me = flip ? 0 : 1, y;
    static char key[24], t[12];
    for (s = 0; s < 2; ++s) {
        y = s == me ? 120 : Y0;
        hg_x = PX;
        hg_y = y;
        hg_src = s ? spr_ck_w : spr_ck_b;
        hgr_blit();
        ui_textn(PX + 3, y + 2, g_name(s), 10);
        ui_textn(PX + 3, y + 11, s ? "White" : "Black", 10);
        strcpy(t, utoa10(cnt[s * 2]));
        strcat(t, " men ");
        if (cnt[s * 2 + 1]) {
            strcat(t, utoa10(cnt[s * 2 + 1]));
            strcat(t, "K");
        }
        ui_textn(PX, y + 21, t, 13);
        ui_textn(PX, y + 30, g_turn == s && !g_over ? "to play" : "", 13);
    }
    panel_offer();
    strcpy(key, offer_a);
    strcat(key, "/");
    strcat(key, offer_b);
    if (strcmp(key, shown_panel) != 0) {
        strcpy(shown_panel, key);
        ui_rect(PX, 66, 13, 8, F_BLACK);
        ui_rect(PX, 86, 13, 8, F_BLACK);
        button(PX, 66, offer_a, ID_A);
        button(PX, 86, offer_b, ID_B);
    }
}

static void ck_draw(void)
{
    unsigned char s, r, c;
    char want;
    board_state();
    if (g_full) {
        sel = 0;
        shown_panel[0] = 1;
        memset(shown, 0, sizeof shown);
        for (r = 0; r < 8; ++r)
            for (c = 0; c < 8; ++c)
                if (!((r + c) & 1))
                    ui_rect(X0 + c * CW, Y0 + r * CH, CW, CH, mono ? F_GREY : F_ORANGE);
        for (s = 1; s <= 32; ++s) {
            place_sq(s);
            ui_target((sq_x * 7 + CW * 7 / 2) / 2, sq_y + CH / 2, CW * 7 / 4, CH / 2, s);
        }
        ui_target(PX * 7 / 2, 70, 0, 5, ID_A);     /* (put on their words by button) */
        ui_target(PX * 7 / 2, 90, 0, 5, ID_B);
    }
    board_state();
    for (s = 1; s <= 32; ++s) {
        want = sq_want(s);
        if (want != shown[s])
            draw_sq(s, want);
    }
    draw_panel();
}

static void redraw(void)
{
    ui_hide();
    ck_draw();
    ui_show();
}
