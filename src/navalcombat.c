/* ParlorNet for the Apple IIe: Naval Combat, the classic, batnav and salvo rules
   (docs/types/navalcombat-1.md). While placing, the large grid on the left is the player's own waters:
   a ship is placed by clicking one end, then the other (the squares where the other end may go are
   marked), or Random places the rest. Ready ends the placing. While shooting, the large grid is the
   other player's waters, where a click fires, and the small grid on the right is the player's own. */
#include <string.h>
#include "a2.h"
#include "ui.h"
#include "net.h"
#include "game.h"

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
#define ID_RANDOM 100
#define ID_UNDO 101
#define ID_CLEAR 102
#define ID_READY 103

static char grid[2][10][11];            /* each seat's waters, as the view's B rows */
static char big[10][10], small[10][10]; /* what is on the screen */
static char phase[8];
static unsigned char afloat[2], ready[2], nextlen, salvo_left;
static char lastshot[6];
static signed char last_seat = -1, sel = -1;
static unsigned char shown_mode = 255;   /* 0 placing, 1 shooting: what the grids show */
static char shown_btn[2];

static void nc_begin(void)
{
    memset(grid, '.', sizeof grid);
    nextlen = 0;
    salvo_left = 0;
    last_seat = -1;
    ready[0] = ready[1] = 0;
}

static void nc_line(void)
{
    unsigned char s, y;
    if (is(w[0], "B") && wn > 3) {
        s = num(w[1]);
        y = num(w[2]) - 1;
        if (s < 2 && y < 10)
            strncpy(grid[s][y], w[3], 10);
    } else if (is(w[0], "Q") && wn > 1) {
        strncpy(phase, w[1], 7);
    } else if (is(w[0], "L") && wn > 2) {
        afloat[0] = num(w[1]);
        afloat[1] = num(w[2]);
    } else if (is(w[0], "RD") && wn > 2) {
        ready[0] = num(w[1]);
        ready[1] = num(w[2]);
    } else if (is(w[0], "N") && wn > 1) {
        nextlen = num(w[1]);
    } else if (is(w[0], "SL") && wn > 1) {
        salvo_left = num(w[1]);
    } else if (is(w[0], "LS") && wn > 2) {
        last_seat = num(w[1]);
        strncpy(lastshot, w[2], 5);
    }
}

static unsigned char me(void)
{
    return g_seat == 1 ? 1 : 0;
}

static unsigned char placing(void)
{
    return strcmp(phase, "place") == 0;
}

static void sq_name(char *b, unsigned char x, unsigned char y)
{
    b[0] = 'a' + x;
    strcpy(b + 1, utoa10(y + 1));
}

/* Could the square be the other end of the next ship, from the chosen end: in line with it, at the
   ship's length, over open water? (The parlor checks the rest.) */
static unsigned char other_end(unsigned char x, unsigned char y)
{
    unsigned char sx, sy, d, i, m = me();
    if (sel < 0 || !nextlen)
        return 0;
    sx = sel % 10;
    sy = sel / 10;
    if (sx == x)
        d = sy > y ? sy - y : y - sy;
    else if (sy == y)
        d = sx > x ? sx - x : x - sx;
    else
        return 0;
    if (d != nextlen - 1 || !d)
        return 0;
    for (i = 0; i <= d; ++i) {
        unsigned char cx = sx == x ? x : (sx < x ? sx + i : sx - i);
        unsigned char cy = sy == y ? y : (sy < y ? sy + i : sy - i);
        if (grid[m][cy][cx] != '.')
            return 0;
    }
    return 1;
}

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

static void nc_click(unsigned char id)
{
    static char t[10];
    unsigned char x = id % 10, y = id / 10;
    if (id >= ID_RANDOM) {
        if (!placing() || ready[me()] || g_seat < 0)
            return;
        sel = -1;
        g_move(id == ID_RANDOM ? "random" : id == ID_UNDO ? "undo" : id == ID_CLEAR ? "clear" : "ready");
        return;
    }
    if (placing()) {
        if (g_seat < 0 || ready[me()] || !nextlen) {
            g_message(nextlen ? "" : "All placed: Ready, or Undo.");
            return;
        }
        if (sel < 0 || sel == id) {
            sel = sel == id ? -1 : id;
            if (sel >= 0 && nextlen == 1) {     /* a ship of one square */
                sq_name(t, x, y);
                strcat(t, "-");
                sq_name(t + strlen(t), x, y);
                sel = -1;
                if (!g_move(t))
                    redraw();
                return;
            }
            g_message(sel < 0 ? "" : "Now the other end.");
            redraw();
            return;
        }
        if (!other_end(x, y)) {
            sel = id;
            g_message("Now the other end.");
            redraw();
            return;
        }
        sq_name(t, sel % 10, sel / 10);
        strcat(t, "-");
        sq_name(t + strlen(t), x, y);
        sel = -1;
        g_move(t);
        return;
    }
    if (!g_myturn) {
        g_message("Not your turn.");
        return;
    }
    if (grid[1 - me()][y][x] != '.') {
        g_message("Fired there already.");
        return;
    }
    sq_name(t, x, y);
    g_move(t);
}

#pragma rodata-name (push, "OVHDR")
const struct game g_navalcombat = {"navalcombat", "Naval Combat", nc_begin, nc_line, nc_draw, nc_click, 0,
                                   "Place: one end, the other. Then fire", 1};
#pragma rodata-name (pop)
