/* ParlorNet on the Apple IIe and IIgs: Naval Combat's grids (core/games/navalcombat.c includes it): the large
   one 2 bytes by 13 lines a square, the small one 1 byte by 7 lines. */
#include "apple2.h"
extern const unsigned char spr_nc_water[], spr_nc_miss[], spr_nc_hit[], spr_nc_sunk[], spr_nc_ship[],
    spr_nc_mark[];
extern const unsigned char spr_nc_m_water[], spr_nc_m_miss[], spr_nc_m_hit[], spr_nc_m_sunk[],
    spr_nc_m_ship[], spr_nc_m_mark[];
extern const unsigned char spr_nc_m_s_water[], spr_nc_m_s_ship[], spr_nc_m_s_hit[], spr_nc_m_s_sunk[],
    spr_nc_m_s_miss[];

#define X0 2                            /* the large grid: 2 bytes by 13 lines a square */
#define Y0 22
#define CH 13
#define SX 27                           /* the small grid: 1 byte by 7 lines a square */
#define SY 22

static char big[10][10], small[10][10]; /* what is on the screen */
static unsigned char shown_mode = 255;   /* 0 placing, 1 shooting: what the grids show */
static char shown_btn[2];

static const unsigned char *big_sprite(char c)
{
    switch (c) {
    case 'o': return mono ? spr_nc_m_miss : spr_nc_miss;
    case 'x': return mono ? spr_nc_m_hit : spr_nc_hit;
    case '#': return mono ? spr_nc_m_sunk : spr_nc_sunk;
    case 's': return mono ? spr_nc_m_ship : spr_nc_ship;
    case 'm': return mono ? spr_nc_m_mark : spr_nc_mark;
    }
    return mono ? spr_nc_m_water : spr_nc_water;
}

/* A square of the small grid: 6 pixels by 6 lines, white on black in either look (colors a byte
   wide would run together). */
static const unsigned char *small_sprite(char c)
{
    static const unsigned char *const m[] = {spr_nc_m_s_water, spr_nc_m_s_ship, spr_nc_m_s_hit,
                                             spr_nc_m_s_sunk, spr_nc_m_s_miss};
    return m[c == 's' ? 1 : c == 'x' ? 2 : c == '#' ? 3 : c == 'o' ? 4 : 0];
}

static void buttons(void)
{
    char want = placing() && !ready[me()] && g_seat >= 0 ? 'p' : 'n';
    if (shown_btn[0] == want)
        return;
    shown_btn[0] = want;
    ui_rect(SX, 120, 13, 45, F_BLACK);
    if (want == 'p') {
        ui_button_draw(SX, 120, "Random");
        ui_button_draw(SX, 131, "Undo");
        ui_button_draw(SX + 7, 131, "Clear");
        ui_button_draw(SX, 142, "Ready");
    }
}

static void nc_draw(void)
{
    unsigned char x, y, mode = placing() ? 0 : 1, m = me();
    char c;
    static char b[16];
    if (g_full || mode != shown_mode) {
        if (!g_full) {
            ui_rect(0, 10, 40, 157, F_BLACK);
        }
        shown_mode = mode;
        shown_btn[0] = 0;
        sel = -1;
        memset(big, 0, sizeof big);
        memset(small, 0, sizeof small);
        for (x = 0; x < 10; ++x) {
            hg_x = X0 + 2 * x;
            hg_y = Y0 - 9;
            hgr_char('a' + x);
        }
        for (y = 0; y < 10; ++y)
            ui_textn(0, Y0 + y * CH + 2, utoa10(y + 1), 2);
        ui_textn(SX, 12, mode ? "Your waters" : "", 13);
        if (g_full) {
            for (y = 0; y < 10; ++y)
                for (x = 0; x < 10; ++x)
                    ui_target(((X0 + 2 * x) * 7 + 7) / 2, Y0 + y * CH + 6, 3, 6, y * 10 + x);
            ui_target(SX * 7 / 2 + 14, 124, 14, 4, ID_RANDOM);
            ui_target(SX * 7 / 2 + 10, 135, 10, 4, ID_UNDO);
            ui_target((SX + 7) * 7 / 2 + 12, 135, 12, 4, ID_CLEAR);
            ui_target(SX * 7 / 2 + 12, 146, 12, 4, ID_READY);
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
                hg_y = Y0 + y * CH;
                hg_src = big_sprite(c);
                hgr_blit();
            }
        }
    if (mode)
        for (y = 0; y < 10; ++y)
            for (x = 0; x < 10; ++x) {
                c = grid[m][y][x];
                if (c != small[y][x]) {
                    small[y][x] = c;
                    hg_x = SX + x;
                    hg_y = SY + y * 7;
                    hg_src = small_sprite(c);
                    hgr_blit();
                }
            }
    /* the panel */
    if (mode) {
        strcpy(b, "Afloat ");
        strcat(b, utoa10(afloat[m]));
        strcat(b, " v ");
        strcat(b, utoa10(afloat[1 - m]));
        ui_textn(SX, 96, b, 13);
        b[0] = 0;
        if (last_seat >= 0) {
            strcpy(b, last_seat == m ? "You: " : "They: ");
            strcat(b, lastshot);
        }
        ui_textn(SX, 106, b, 13);
        b[0] = 0;
        if (salvo_left && g_myturn) {
            strcpy(b, utoa10(salvo_left));
            strcat(b, " shots left");
        }
        ui_textn(SX, 116, b, 13);
    } else {
        b[0] = 0;
        if (nextlen) {
            strcpy(b, "Place a ");
            strcat(b, utoa10(nextlen));
        } else if (!ready[m])
            strcpy(b, "Fleet placed");
        else
            strcpy(b, "Ready");
        ui_textn(SX, 22, b, 13);
        ui_textn(SX, 32, nextlen ? "One end, then" : "", 13);
        ui_textn(SX, 41, nextlen ? "the other." : "", 13);
        ui_textn(SX, 60, ready[1 - m] ? "Other: ready" : "Other: placing", 13);
        buttons();
    }
}

static void redraw(void)
{
    ui_hide();
    nc_draw();
    ui_show();
}
