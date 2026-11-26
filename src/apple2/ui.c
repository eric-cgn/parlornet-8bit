/* ParlorNet for the Apple IIe: the Hi-Res screen kit (core/machine.h), with the shared parts in
   src/common (ui_targets.c, ui_hgr.c). */
#include <string.h>
#include "machine.h"
#include "apple2.h"

static unsigned char ptr_on;
static unsigned int ptr_x;
static unsigned char ptr_y;
static unsigned char mouse_was;

/* Per fill: even line (even column, odd column), odd line (even, odd). */
static const unsigned char fills[][4] = {
    {0x00, 0x00, 0x00, 0x00},   /* black */
    {0x7F, 0x7F, 0x7F, 0x7F},   /* white */
    {0x2A, 0x55, 0x2A, 0x55},   /* green */
    {0x55, 0x2A, 0x55, 0x2A},   /* violet */
    {0xAA, 0xD5, 0xAA, 0xD5},   /* orange */
    {0xD5, 0xAA, 0xD5, 0xAA},   /* blue */
    {0x55, 0x2A, 0x2A, 0x55},   /* gray: a checkerboard of pixels */
    {0x7F, 0x7F, 0x55, 0x2A},   /* light: mostly white */
    {0x55, 0x2A, 0x00, 0x00},   /* dark: mostly black */
    {0x7F, 0x7F, 0x00, 0x00},   /* hatch: white lines */
};

void __fastcall__ ui_fill(unsigned char f)
{
    memcpy(hg_p, fills[f], 4);
}

void __fastcall__ ui_ink(unsigned char fg, unsigned char bg)
{
    hg_fg[0] = fills[fg][0];
    hg_fg[1] = fills[fg][1];
    hg_bg[0] = fills[bg][0];
    hg_bg[1] = fills[bg][1];
}

void ui_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h, unsigned char f)
{
    ui_fill(f);
    hg_x = x;
    hg_y = y;
    hg_w = w;
    hg_h = h;
    hgr_rect();
}

void ui_button(unsigned char x, unsigned char y, const char *s, unsigned char id)
{
    unsigned char n = strlen(s);
    ui_button_draw(x, y, s);
    ui_target((unsigned char)((x * 7 + (n + 2) * 7 / 2) / 2), y + 4, (n + 2) * 7 / 4, 5, id);
}

void ui_cls(void)
{
    ui_hide();
    hgr_clear();
    clk_shown = 255;                    /* (the time signed in drawn again: clk_show) */
    ui_targets_clear();
    ui_ink(F_WHITE, F_BLACK);
    hg_bold = !mono;
}

void ui_status(const char *s)
{
    ui_hide();
    ui_textn(0, 176, s, 40);
    ui_show();
}

void ui_msg(const char *s)
{
    ui_hide();
    ui_textn(0, 167, s, 40);
    ui_show();
}

/* ------------------------------------------------------------------------------------------------ */
/* The pointer and targets */

void ui_hide(void)
{
    if (ptr_on) {
        hg_px = ptr_x;
        hg_y = ptr_y;
        hgr_ptr();
        ptr_on = 0;
    }
}

void ui_show(void)
{
    if (!ptr_on && (t_cur < t_n || ui_hasmouse)) {
        hg_px = ptr_x;
        hg_y = ptr_y;
        hgr_ptr();
        ptr_on = 1;
    }
}

void ui_targets_clear(void)
{
    ui_hide();
    t_n = 0;
    t_cur = 255;
}

void __fastcall__ ui_point(unsigned char i)
{
    if (i >= t_n)
        return;
    ui_hide();
    t_cur = i;
    ptr_x = (unsigned int)t_x[i] * 2;
    ptr_y = t_y[i];
    ui_show();
}

/* ------------------------------------------------------------------------------------------------ */
/* The mouse (AppleMouse II, firmware calls; passive mode, read each poll) */

#define HOLE(a) (*(unsigned char *)(a))

void ui_mouse_init(void)
{
    ui_hasmouse = 0;
    if (!mouse_find())
        return;
    mouse_call(0x19);                   /* INITMOUSE */
    HOLE(0x478) = 0; HOLE(0x578) = 0;   /* x from 0 */
    HOLE(0x4F8) = 23; HOLE(0x5F8) = 1;  /* to 279 */
    mouse_call(0x17);                   /* CLAMPMOUSE x (A = 0) */
    HOLE(0x478) = 0; HOLE(0x578) = 0;
    HOLE(0x4F8) = 191; HOLE(0x5F8) = 0;
    mouse_call(0x0117);                 /* CLAMPMOUSE y (A = 1) */
    mouse_call(0x0112);                 /* SETMOUSE: on, no interrupts */
    ui_hasmouse = 1;
}

static unsigned char poll_mouse(void)
{
    static unsigned int mx, last_mx = 0xFFFF;
    static unsigned char my, st, i, last_my;
    mouse_call(0x14);                   /* READMOUSE */
    mx = HOLE(0x478 + mouse_slot) | (HOLE(0x578 + mouse_slot) << 8);
    my = HOLE(0x4F8 + mouse_slot);
    st = HOLE(0x778 + mouse_slot);
    if (last_mx == 0xFFFF) {            /* the first reading: the keys keep the pointer */
        last_mx = mx;
        last_my = my;
    }
    if (mx != last_mx || my != last_my) {   /* the mouse moved: the pointer follows it */
        last_mx = mx;
        last_my = my;
        ui_hide();
        ptr_x = mx;
        ptr_y = my;
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
        ui_show();
    }
    if ((st & 0x80) && !mouse_was) {
        mouse_was = 1;
        if (t_cur < t_n) {
            ui_clicked = t_id[t_cur];
            return U_CLICK;
        }
    } else if (!(st & 0x80))
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
    if (ui_hasmouse)
        return poll_mouse();
    return 0;
}

/* ------------------------------------------------------------------------------------------------ */
/* Line editing */

unsigned char ui_input(unsigned char x, unsigned char y, unsigned char w, char *buf, unsigned char max,
                       unsigned char secret)
{
    unsigned char n = strlen(buf), k, i, from;
    ui_hide();
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
