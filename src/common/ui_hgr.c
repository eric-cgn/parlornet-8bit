/* ParlorNet: the screen kit's text, outlines and buttons (core/machine.h), for a machine whose drawing code
   gives the Apple II's Hi-Res primitives by the same names (hgr.h in the machine's folder): hg_x (a text
   column), hg_y (a line), hg_w, hg_h, hg_px (a pixel column), hg_key, and hgr_char, hgr_text, hgr_hline and
   hgr_vline. The Apple II's hgr.s and the C64's gfx.s both do. */
#include "machine.h"
#include "hgr.h"

/* Text up to column w; a key's mark (K1 ...) takes no cell, and is passed over while the keys are unlit. */
static void __fastcall__ put(const char *s, unsigned char w)
{
    hg_key = 0;
    for (; *s && hg_x < w; ++s)
        if (!ui_unlit || (unsigned char)*s >= ' ')
            hgr_char(*s);
}

void ui_text(unsigned char x, unsigned char y, const char *s)
{
    hg_x = x;
    hg_y = y;
    if (ui_unlit)
        put(s, 40);
    else
        hgr_text(s);
}

void ui_textn(unsigned char x, unsigned char y, const char *s, unsigned char w)
{
    hg_x = x;
    hg_y = y;
    w += x;
    if (w > 40)
        w = 40;
    put(s, w);
    while (hg_x < w)
        hgr_char(' ');
}

/* A line across, w pixels long, in pieces of at most 255 (hgr_hline's longest). */
static void __fastcall__ across(unsigned int px, unsigned int w)
{
    while (w) {
        hg_px = px;
        hg_w = w > 255 ? 255 : w;
        hgr_hline();
        px += hg_w;
        w -= hg_w;
    }
}

void ui_box(unsigned int px, unsigned char y, unsigned int w, unsigned char h)
{
    hg_y = y;
    across(px, w);
    hg_y = y + h - 1;
    across(px, w);
    hg_px = px;
    hg_y = y;
    hg_h = h;
    hgr_vline();
    hg_px = px + w - 1;
    hgr_vline();
}

/* A button's label in inverse at text column x, line y (ui_button adds its target); unlit, as plain text. */
void ui_button_draw(unsigned char x, unsigned char y, const char *s)
{
    if (!ui_unlit)
        ui_ink(F_BLACK, F_WHITE);
    hg_x = x;
    hg_y = y;
    hgr_char(' ');
    hgr_text(s);
    hgr_char(' ');
    ui_ink(F_WHITE, F_BLACK);
}
