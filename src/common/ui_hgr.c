/* ParlorNet: the screen kit's text, outlines and buttons (core/machine.h), for a machine whose drawing code
   gives the Apple II's Hi-Res primitives by the same names (hgr.h in the machine's folder): hg_x (a text
   column), hg_y (a line), hg_w, hg_h, hg_px (a pixel column), hg_key, and hgr_char, hgr_text, hgr_hline and
   hgr_vline. The Apple II's hgr.s and the C64's gfx.s both do. */
#include "machine.h"
#include "hgr.h"

void ui_text(unsigned char x, unsigned char y, const char *s)
{
    hg_x = x;
    hg_y = y;
    hgr_text(s);
}

void ui_textn(unsigned char x, unsigned char y, const char *s, unsigned char w)
{
    hg_x = x;
    hg_y = y;
    hg_key = 0;
    w += x;
    if (w > 40)
        w = 40;
    while (*s && hg_x < w)              /* (a key's mark, K1 ..., takes no cell) */
        hgr_char(*s++);
    while (hg_x < w)
        hgr_char(' ');
}

void ui_box(unsigned int px, unsigned char y, unsigned int w, unsigned char h)
{
    hg_px = px;
    hg_y = y;
    hg_w = w;
    hgr_hline();
    hg_px = px;
    hg_y = y + h - 1;
    hgr_hline();
    hg_px = px;
    hg_y = y;
    hg_h = h;
    hgr_vline();
    hg_px = px + w - 1;
    hgr_vline();
}

/* A button's label in inverse at text column x, line y (ui_button adds its target). */
void ui_button_draw(unsigned char x, unsigned char y, const char *s)
{
    ui_ink(F_BLACK, F_WHITE);
    hg_x = x;
    hg_y = y;
    hgr_char(' ');
    hgr_text(s);
    hgr_char(' ');
    ui_ink(F_WHITE, F_BLACK);
}
