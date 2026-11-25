/* ParlorNet on the Commodore 64: Naval Combat's grids (core/games/navalcombat.c includes it). The large grid
   is 10 by 10 squares of two cells by two, the small one (the player's own waters while shooting) squares of
   one cell; the sea is light blue under its waves, a miss a white splash, a ship gray with its deck, a hit a
   red burst on the gray, a sunk ship's squares dark, and the squares where a ship's other end may go a yellow
   mark. Each square needs just its own two colors. Beside the grid, the counts, the last shot and, while
   placing, the buttons. In monochrome the colors are grays. */
#include "gfx.h"

extern const unsigned char pic_nc_wave[], pic_nc_splash[], pic_nc_burst[], pic_nc_ship[], pic_nc_mark[],
    pic_nc_none[], pic_ncs_wave[], pic_ncs_splash[], pic_ncs_burst[], pic_ncs_ship[];

#define X0 2                            /* the large grid: cell column, top line */
#define Y0 16
#define SX 26                           /* the small grid */
#define SY 24
#define PY 112                          /* the panel's text and buttons, under the small grid */

static char big[10][10], small[10][10]; /* what is on the screen */
static unsigned char shown_mode = 255;  /* 0 placing, 1 shooting: what the grids show */
static char shown_btn[2];

/* A square's look: its picture (large, small), its color and its ground's. */
static void square(char c, unsigned char large)
{
    unsigned char sea = mono ? C_GREY : C_LBLUE;
    switch (c) {
    case 'o':
        hg_src = large ? pic_nc_splash : pic_ncs_splash;
        hg_fg = C_WHITE;
        hg_bg = sea;
        break;
    case 'x':
        hg_src = large ? pic_nc_burst : pic_ncs_burst;
        hg_fg = mono ? C_WHITE : C_RED;
        hg_bg = mono ? C_DGREY : C_GREY;
        break;
    case '#':
        hg_src = large ? pic_nc_burst : pic_ncs_burst;
        hg_fg = mono ? C_LGREY : C_RED;
        hg_bg = C_BLACK;
        break;
    case 's':
        hg_src = large ? pic_nc_ship : pic_ncs_ship;
        hg_fg = mono ? C_BLACK : C_LGREY;
        hg_bg = mono ? C_LGREY : C_GREY;
        break;
    case 'm':
        hg_src = large ? pic_nc_mark : pic_ncs_wave;
        hg_fg = mono ? C_WHITE : C_YELLOW;
        hg_bg = sea;
        break;
    default:
        hg_src = large ? pic_nc_wave : pic_ncs_wave;
        hg_fg = mono ? C_LGREY : C_BLUE;
        hg_bg = sea;
    }
    hg_w = large ? 2 : 1;
    hg_h = large ? 16 : 8;
    hgr_color();
    hgr_blit();
}

static void buttons(void)
{
    char want = placing() && !ready[me()] && g_seat >= 0 ? 'p' : 'n';
    if (shown_btn[0] == want)
        return;
    shown_btn[0] = want;
    ui_rect(SX, PY + 32, 14, 32, F_BLACK);
    if (want == 'p') {
        ui_button_draw(SX, PY + 32, "Random");
        ui_button_draw(SX, PY + 48, "Undo");
        ui_button_draw(SX + 7, PY + 48, "Clear");
        ui_button_draw(SX, PY + 64 - 8, "Ready");
    }
}

static void nc_draw(void)
{
    unsigned char x, y, mode = placing() ? 0 : 1, m = me();
    char c;
    static char b[16];
    ui_ink(F_WHITE, F_BLACK);
    if (g_full || mode != shown_mode) {
        if (!g_full)
            ui_rect(0, GAME_TOP, 40, 168, F_BLACK);
        shown_mode = mode;
        shown_btn[0] = 0;
        sel = -1;
        memset(big, 0, sizeof big);
        memset(small, 0, sizeof small);
        for (x = 0; x < 10; ++x) {
            hg_x = X0 + 2 * x;
            hg_y = GAME_TOP;
            hgr_char(' ');
            hgr_char('a' + x);
        }
        for (y = 0; y < 10; ++y)
            ui_textn(0, Y0 + y * 16 + 4, utoa10(y + 1), 2);
        ui_textn(SX, SY - 16, mode ? "Your waters" : "", 14);
        if (g_full) {
            for (y = 0; y < 10; ++y)
                for (x = 0; x < 10; ++x)
                    ui_target((X0 + 2 * x) * 4 + 8, Y0 + y * 16 + 8, 7, 7, y * 10 + x);
            ui_target(SX * 4 + 16, PY + 36, 16, 4, ID_RANDOM);
            ui_target(SX * 4 + 12, PY + 52, 12, 4, ID_UNDO);
            ui_target((SX + 7) * 4 + 14, PY + 52, 14, 4, ID_CLEAR);
            ui_target(SX * 4 + 14, PY + 60, 14, 4, ID_READY);
        }
    }
    /* the large grid: my waters while placing, the other's while shooting */
    for (y = 0; y < 10; ++y)
        for (x = 0; x < 10; ++x) {
            c = grid[mode ? 1 - m : m][y][x];
            if (!mode && c == '.' && other_end(x, y))
                c = 'm';
            if (!mode && sel == y * 10 + x)
                c = 'm';
            if (c != big[y][x]) {
                big[y][x] = c;
                hg_x = X0 + 2 * x;
                hg_y = Y0 + y * 16;
                square(c, 1);
            }
        }
    if (mode)
        for (y = 0; y < 10; ++y)
            for (x = 0; x < 10; ++x) {
                c = grid[m][y][x];
                if (c != small[y][x]) {
                    small[y][x] = c;
                    hg_x = SX + x;
                    hg_y = SY + y * 8;
                    square(c, 0);
                }
            }
    /* the panel */
    ui_ink(F_WHITE, F_BLACK);
    if (mode) {
        strcpy(b, "Afloat ");
        strcat(b, utoa10(afloat[m]));
        strcat(b, " v ");
        strcat(b, utoa10(afloat[1 - m]));
        ui_textn(SX, PY, b, 14);
        b[0] = 0;
        if (last_seat >= 0) {
            strcpy(b, last_seat == m ? "You: " : "They: ");
            strcat(b, lastshot);
        }
        ui_textn(SX, PY + 8, b, 14);
        b[0] = 0;
        if (salvo_left && g_myturn) {
            strcpy(b, utoa10(salvo_left));
            strcat(b, " shots left");
        }
        ui_textn(SX, PY + 16, b, 14);
    } else {
        b[0] = 0;
        if (nextlen) {
            strcpy(b, "Place a ");
            strcat(b, utoa10(nextlen));
        } else if (!ready[m])
            strcpy(b, "Fleet placed");
        else
            strcpy(b, "Ready");
        ui_textn(SX, SY, b, 14);
        ui_textn(SX, SY + 8, nextlen ? "One end, then" : "", 14);
        ui_textn(SX, SY + 16, nextlen ? "the other." : "", 14);
        ui_textn(SX, SY + 32, ready[1 - m] ? "Other: ready" : "Other: placing", 14);
        buttons();
    }
}

static void redraw(void)
{
    nc_draw();
}
