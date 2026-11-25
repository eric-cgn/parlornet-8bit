/* ParlorNet on the Commodore 64: the bitmap screen kit (core/machine.h; the shared parts are in src/common).
   Text in the program's font, one character a cell; fills; the pointer, a sprite; and what moves it: the
   keys, a joystick in either port and a 1351 mouse in port 1.

   The screen is white text on blue, as the C64's own colors suggest; F_BLACK and F_WHITE (machine.h) are these
   two, so "inverse" text (the title rows, buttons) is blue on white. In monochrome (a monochrome monitor or
   a black and white TV) every color is a gray of about its brightness and the screen is white on black. */
#include <string.h>
#include "machine.h"
#include "c64.h"
#include "gfx.h"

static unsigned char ptr_shown;
static unsigned char mouse_was;

/* The fills (machine.h) as C64 colors: in color, then in monochrome. */
static const unsigned char fill_col[2][10] = {
    /* black, white, green, violet, orange, blue, grey, light, dark, hatch */
    {C_BLUE, C_WHITE, C_GREEN, C_PURPLE, C_ORANGE, C_LBLUE, C_GREY, C_LGREY, C_DGREY, C_CYAN},
    {C_BLACK, C_WHITE, C_GREY, C_DGREY, C_GREY, C_LGREY, C_GREY, C_LGREY, C_DGREY, C_LGREY}};
/* Any C64 color in monochrome: its gray. */
static const unsigned char grey_of[16] = {
    C_BLACK, C_WHITE, C_DGREY, C_LGREY, C_GREY, C_GREY, C_DGREY, C_LGREY,
    C_GREY, C_DGREY, C_GREY, C_DGREY, C_GREY, C_LGREY, C_GREY, C_LGREY};

unsigned char __fastcall__ ui_col(unsigned char c)
{
    return mono ? grey_of[c] : c;
}

unsigned char __fastcall__ ui_fcol(unsigned char f)
{
    return fill_col[mono][f];
}

void __fastcall__ ui_fill(unsigned char f)
{
    hg_bg = fill_col[mono][f];
}

void __fastcall__ ui_ink(unsigned char fg, unsigned char bg)
{
    hg_fg = fill_col[mono][fg];
    hg_bg = fill_col[mono][bg];
}

/* Colors themselves (C_*), through the monochrome grays when they are on. */
void __fastcall__ ui_inkc(unsigned char fg, unsigned char bg)
{
    hg_fg = ui_col(fg);
    hg_bg = ui_col(bg);
}

/* A fill: whole cells take the color; on lines that are not whole cells, the lines are set in it. */
void ui_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h, unsigned char f)
{
    unsigned char fg = hg_fg, bg = hg_bg;
    hg_x = x;
    hg_w = w;
    if (!(y & 7) && !(h & 7)) {
        hg_fg = fg;
        hg_bg = fill_col[mono][f];
        hg_pat = 0;
    } else {
        hg_fg = fill_col[mono][f];
        hg_pat = 0xFF;
    }
    hg_y = y;
    hg_h = h;
    hgr_rect();
    hg_fg = fg;
    hg_bg = bg;
}

void ui_button(unsigned char x, unsigned char y, const char *s, unsigned char id)
{
    unsigned char n = strlen(s) + 2;
    ui_button_draw(x, y, s);
    ui_target(x * 4 + n * 2, y + 4, n * 2, 4, id);
}

void ui_cls(void)
{
    spr_on &= 3;                        /* (a game's own sprites go with its screen) */
    ui_targets_clear();
    ui_ink(F_WHITE, F_BLACK);
    hg_kbg = hg_bg;
    hg_kfg = mono ? C_WHITE : C_YELLOW;
    hgr_clear();
    VIC_BORDER = hg_bg;
    hg_bold = 0;
}

void ui_status(const char *s)
{
    ui_textn(0, STATUS_Y, s, 40);
}

void ui_msg(const char *s)
{
    ui_textn(0, MSG_Y, s, 40);
}

/* ------------------------------------------------------------------------------------------------ */
/* The pointer: sprites 0 (white) and 1 (its black outline, behind), the arrow's tip at a target's middle
   or where the mouse is. */

static void ptr_place(unsigned int x, unsigned char y)
{
    spr_x[0] = spr_x[1] = x + 24;
    spr_y[0] = spr_y[1] = y + 50;
}

void ui_hide(void)
{
    /* (a sprite: drawing under it is safe) */
}

void ui_show(void)
{
    if (t_cur < t_n || ui_hasmouse) {
        spr_on |= 3;
        ptr_shown = 1;
    }
}

void ui_targets_clear(void)
{
    t_n = 0;
    t_cur = 255;
    if (!ui_hasmouse) {
        spr_on &= ~3;
        ptr_shown = 0;
    }
}

void __fastcall__ ui_point(unsigned char i)
{
    if (i >= t_n)
        return;
    t_cur = i;
    ptr_place((unsigned int)t_x[i] * 2, t_y[i]);
    ui_show();
}

/* ------------------------------------------------------------------------------------------------ */
/* The 1351 mouse (read by the interrupt, sys.s): the pointer follows it, onto the target under it. */

void ui_mouse_init(void)
{
    static const unsigned char arrow[63] = {
        0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x60, 0x00, 0x00, 0x70, 0x00, 0x00, 0x78, 0x00, 0x00,
        0x7C, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x6C, 0x00, 0x00,
        0x46, 0x00, 0x00, 0x06, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00};
    static const unsigned char outline[63] = {
        0xC0, 0x00, 0x00, 0xE0, 0x00, 0x00, 0xF0, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xFC, 0x00, 0x00,
        0xFE, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x80, 0x00, 0xFF, 0x80, 0x00, 0xFE, 0x00, 0x00,
        0xEF, 0x00, 0x00, 0xCF, 0x00, 0x00, 0x07, 0x80, 0x00, 0x07, 0x80, 0x00, 0x03, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00};
    spr_load(SPR_ARROW, arrow);
    spr_load(SPR_ARROW + 1, outline);
    spr_ptr[0] = SPR_ARROW;
    spr_ptr[1] = SPR_ARROW + 1;
    spr_col[0] = C_WHITE;
    spr_col[1] = C_BLACK;
    ui_hasmouse = 0;
}

static unsigned char poll_mouse(void)
{
    static unsigned int mx;
    static unsigned char my, i;
    if (!ui_hasmouse && has_mouse)
        ui_hasmouse = 1;
    if (mouse_moved) {
        mouse_moved = 0;
        mx = mouse_x;
        my = mouse_y;
        ptr_place(mx, my);
        spr_on |= 3;
        t_cur = 255;
        for (i = 0; i < t_n; ++i) {
            int dx = (int)(mx >> 1) - t_x[i];
            int dy = (int)my - t_y[i];
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if (t_hw[i] && dx <= t_hw[i] && dy <= t_hh[i]) {
                t_cur = i;
                break;
            }
        }
    }
    if (mouse_btn && !mouse_was) {
        mouse_was = 1;
        if (t_cur < t_n) {
            ui_clicked = t_id[t_cur];
            return U_CLICK;
        }
    } else if (!mouse_btn)
        mouse_was = 0;
    return 0;
}

unsigned char ui_poll(void)
{
    unsigned char k, n;
    clk_show();
    k = kbd_get();
    if (k) {
        if (kbd_mods() & 1) {
            if (k >= 'a' && k <= 'z')
                k -= 32;
            return U_OA | k;
        }
        if (k == K_LEFT || k == K_RIGHT || k == K_UP || k == K_DOWN) {
            n = ui_nearest(k);
            if (n < t_n)
                ui_point(n);
            return 0;
        }
        if ((k == K_RET && !ui_retkey || k == 0x20) && t_cur < t_n) {
            ui_clicked = t_id[t_cur];
            return U_CLICK;
        }
        return k;
    }
    if (has_mouse)
        return poll_mouse();
    return 0;
}

/* ------------------------------------------------------------------------------------------------ */
/* Line editing */

unsigned char ui_input(unsigned char x, unsigned char y, unsigned char w, char *buf, unsigned char max,
                       unsigned char secret)
{
    unsigned char n = strlen(buf), k, i, from;
    for (;;) {
        from = n >= w ? n - w + 1 : 0;
        hg_x = x;
        hg_y = y;
        for (i = from; i < n; ++i)
            hgr_char(secret ? '*' : buf[i]);
        ui_ink(F_BLACK, F_WHITE);
        hgr_char(' ');
        ui_ink(F_WHITE, F_BLACK);
        while (hg_x < x + w)
            hgr_char(' ');
        do
            k = kbd_get();
        while (!k);
        if (k == K_RET) {
            ui_textn(x + (n - from), y, "", 1);
            return 1;
        }
        if (k == K_ESC)
            return 0;
        if (k == K_DEL || k == K_LEFT) {
            if (n)
                buf[--n] = 0;
        } else if (k == 24) {           /* Ctrl-X: clear */
            n = 0;
            buf[0] = 0;
        } else if (k >= 32 && k < 127 && n < max) {
            buf[n++] = k;
            buf[n] = 0;
        }
    }
}
