/* ParlorNet: the machine-neutral part of the screen kit (core/machine.h), shared by every machine: the
   variables, the targets and the nearest one in a direction, and waiting for a key. Plain C on machine.h
   alone, so a port on any processor can build it as it is. */
#include "machine.h"

unsigned char mono;
unsigned char t_n, t_cur = 255;
unsigned char t_x[MAXT], t_y[MAXT], t_hw[MAXT], t_hh[MAXT], t_id[MAXT];
unsigned char ui_clicked;
unsigned char ui_hasmouse;
unsigned char ui_retkey;

unsigned char ui_target(unsigned char x2, unsigned char y, unsigned char hw, unsigned char hh,
                        unsigned char id)
{
    if (t_n >= MAXT)
        return 255;
    t_x[t_n] = x2;
    t_y[t_n] = y;
    t_hw[t_n] = hw;
    t_hh[t_n] = hh;
    t_id[t_n] = id;
    return t_n++;
}

unsigned char ui_find(unsigned char id)
{
    unsigned char i;
    for (i = 0; i < t_n; ++i)
        if (t_id[i] == id)
            return i;
    return 255;
}

/* The nearest target in a direction: along it, with sideways distance counting three times. */
unsigned char __fastcall__ ui_nearest(unsigned char key)
{
    static int cx, cy, dx, dy, d, best;
    static unsigned char i, bi;
    if (t_cur >= t_n)
        return t_n ? 0 : 255;
    cx = t_x[t_cur] * 2;
    cy = t_y[t_cur];
    bi = 255;
    best = 32767;
    for (i = 0; i < t_n; ++i) {
        if (i == t_cur || !t_hw[i])          /* a target with no width is out of use */
            continue;
        dx = t_x[i] * 2 - cx;
        dy = (int)t_y[i] - cy;
        switch (key) {
        case K_RIGHT: if (dx <= 0) continue; d = dx + 3 * (dy < 0 ? -dy : dy); break;
        case K_LEFT:  if (dx >= 0) continue; d = -dx + 3 * (dy < 0 ? -dy : dy); break;
        case K_DOWN:  if (dy <= 0) continue; d = dy + 3 * (dx < 0 ? -dx : dx); break;
        default:      if (dy >= 0) continue; d = -dy + 3 * (dx < 0 ? -dx : dx); break;
        }
        if (d < best) {
            best = d;
            bi = i;
        }
    }
    return bi;
}

void ui_wait_key(void)
{
    while (!kbd_get())
        ;
}
