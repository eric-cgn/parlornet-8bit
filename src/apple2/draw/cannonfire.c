/* ParlorNet on the Apple IIe and IIgs: Cannon Fire's arena (core/games/cannonfire.c includes it). In color
   the ground is green; in monochrome it is white.

   Each tank has its letter and its health ("87%") over it, a barrel at its aim (the last shot's, from the
   A lines; the player's own as it is set), and on the player's turn a lead of square dots beyond the
   barrel's tip, in the direction the shell will leave in: one dot, and one more for each 40 of power.
   A shot's events are shown as they come: the shell
   flies along the path the TRJ lines give, leaving a dot at each point, bursts at the HIT, and the ground
   the crater takes (ZCH) dissolves in eight steps; ground that a view shows lower than the screen does
   (a shot whose events were not shown) dissolves the same way. The barrels, the lead, the shell, the
   burst and the path's dots are flipped (exclusive-or) onto the screen, so drawing one again takes it
   away. */
#include "apple2.h"

#define BASE 140                        /* the line of height 0 */
#define TOP 9                           /* the first line of the arena */
#define BY 155
#define BARREL 7                        /* the barrel's length, in pixels */
#define LEAD 4                          /* pixels between the lead's dots */
extern const unsigned char hgr_ylo[], hgr_yhi[];

static unsigned char shown_ht[AMAX];
static unsigned char scale, ox;
static unsigned char tr_on;
static unsigned char shown_tx[8], shown_hp[8], shown_tg[8];
static unsigned char dirty[40];
static unsigned char flying;            /* a shot is being shown: no lead */

/* 64 times the sine of 0 to 90 degrees */
static const unsigned char sn64[91] = {
    0, 1, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 39, 40, 41, 42, 43, 44, 44, 45, 46, 47, 48, 48, 49, 50, 50, 51,
    52, 52, 53, 54, 54, 55, 55, 56, 57, 57, 58, 58, 58, 59, 59, 60, 60, 61, 61, 61, 62, 62, 62, 62, 63, 63,
    63, 63, 63, 64, 64, 64, 64, 64, 64, 64, 64};

/* Screen pixel of arena column x (its middle), and line of height h. */
static unsigned int cx(unsigned char x)
{
    return ox + x * scale + scale / 2;
}

static unsigned char ly(int h)
{
    h = BASE - 2 * h;
    return h < TOP ? TOP : (unsigned char)h;
}

/* The line of height h, above the arena too. */
static int line_of(unsigned char h)
{
    return BASE - 2 * (int)h;
}

/* Wait n sixtieths of a second. */
static void nap(unsigned char n)
{
    unsigned int t = ticks;
    while (ticks - t < n)
        tick_poll();
}

static unsigned char isqrt(unsigned int n)
{
    unsigned int r = 0, b = 0x4000;
    while (b > n)
        b >>= 2;
    while (b) {
        if (n >= r + b) {
            n -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
        b >>= 2;
    }
    return r;
}

/* Flip a mark w pixels across (1 or 2) at (x, y), in the arena. In color a mark is 2 pixels from an even
   column, which a color monitor shows white. */
static int qx;
static void dot(int x, int y, unsigned char w)
{
    if (!mono)
        w = 2;
    if (w == 2)
        x &= ~1;
    qx = x;
    if (x < 0 || x > 279 || y < TOP || y > BASE)
        return;
    hg_px = x;
    hg_y = y;
    hg_w = w;
    hgr_hline();
}

/* ---- the barrels and the lead ---------------------------------------------------------------------- */

/* The way a shell at angle a leaves on the screen, as pixels across and lines up for each pixel of
   length (in 64ths): a column is scale pixels and a height 2 lines, and a line is 7/6 of a pixel high
   on a monitor (280 pixels at 7.16 MHz, 192 lines of a 240-line field). */
static unsigned int ux, uy;
static unsigned char left;
static void dir(unsigned char a)
{
    unsigned int fx, fy, fp;
    unsigned char l;
    left = a > 90;
    fx = sn64[left ? a - 90 : 90 - a] * scale;
    fy = sn64[left ? 180 - a : a] * 2;
    fp = fy * 7 / 6;
    l = isqrt(fx * fx + fp * fp);
    if (!l)
        l = 1;
    ux = fx * 64 / l;
    uy = fy * 64 / l;
}

/* The point d pixels along the direction from (x0, y0). */
static int px_x, px_y;
static void along(unsigned int x0, unsigned char y0, unsigned char d)
{
    unsigned int k = (ux * d + 32) >> 6;
    px_x = left ? (int)x0 - (int)k : (int)(x0 + k);
    px_y = (int)y0 - (int)((uy * d + 32) >> 6);
}

static unsigned int gx[8];              /* each tank's gun as drawn: its turret's middle, */
static unsigned char gy[8], ga[8], gn[8], gd[8];   /* the turret's line, the angle, lead dots, drawn */
static unsigned char g_on;              /* the guns are on the screen */

static void gun(unsigned char s)
{
    unsigned char d, n;
    int lx = -1, ly0 = -1;
    dir(ga[s]);
    for (d = 1; d <= BARREL; ++d) {
        along(gx[s], gy[s], d);
        if (px_x == lx && px_y == ly0)  /* each pixel once */
            continue;
        lx = px_x;
        ly0 = px_y;
        qx = mono ? px_x : px_x & ~1;
        if (px_y == gy[s] && qx <= (int)gx[s] + 1 && qx + (mono ? 0 : 1) >= (int)gx[s] - 1)
            continue;                   /* the turret itself */
        dot(px_x, px_y, 1);
    }
    for (n = 1; n <= gn[s]; ++n) {
        along(gx[s], gy[s], BARREL + n * LEAD);
        dot(px_x, px_y, 2);             /* 2 lines down, unlike the path's dots */
        dot(px_x, px_y + 1, 2);
    }
}

/* Where seat s's gun is to be drawn now. */
static void gun_at(unsigned char s)
{
    unsigned char mine = s == g_seat && g_myturn && !flying;
    gd[s] = s < ntank && hp[s];
    gx[s] = cx(shown_tx[s]);
    gy[s] = ly(shown_tg[s]) - 4;
    ga[s] = mine ? ang : aim_a[s];
    if (ga[s] > 180)
        ga[s] = 180;
    gn[s] = mine ? 1 + pw / 40 : 0;
}

/* Flip every gun: drawn where the tanks are now, taken away again from where they were drawn. */
static void guns(void)
{
    unsigned char s;
    hg_xor = 1;
    for (s = 0; s < 8; ++s) {
        if (!g_on)
            gun_at(s);
        if (gd[s])
            gun(s);
    }
    hg_xor = 0;
    g_on ^= 1;
}

/* ---- the ground, the tanks and the path ------------------------------------------------------------- */

/* One column of ground: green in color (only its odd pixels), white in monochrome. */
static void column(unsigned char x)
{
    unsigned int p = ox + x * scale, e = p + scale;
    unsigned char top = ly(ht[x]);
    shown_ht[x] = ht[x];
    for (; p < e; ++p)
        if (mono || (p & 1)) {
            hg_px = p;
            hg_y = top;
            hg_h = BASE - top + 1;
            hgr_vline();
        }
}

/* The screen bytes a column (or a tank round it) covers, to be cleared and drawn again. */
static void mark(unsigned char x, unsigned char wide)
{
    unsigned int p0 = ox + x * scale, p1 = p0 + scale - 1;
    unsigned char b;
    if (wide) {                         /* the tank, its letter and its health (tanks) */
        p0 = cx(x) > 16 ? cx(x) - 16 : 0;
        p1 = cx(x) + 17;
    }
    for (b = p0 / 7; b <= p1 / 7 && b < 40; ++b)
        dirty[b] = 1;
}

static void redraw_bytes(void)
{
    unsigned char b, x, x1;
    for (b = 0; b < 40; ++b) {
        if (!dirty[b])
            continue;
        dirty[b] = 0;
        ui_rect(b, TOP, 1, BASE - 8, F_BLACK);
        x = b * 7 < ox ? 0 : (b * 7 - ox) / scale;
        x1 = b * 7 + 6 < ox ? 0 : (b * 7 + 6 - ox) / scale;
        for (; x <= x1 && x < aw; ++x)
            column(x);
    }
}

/* A point of the path: a dot 2 pixels across. */
static void trail(unsigned char i)
{
    int y = line_of(tr_y[i]);
    if (tr_x[i] < aw && y >= TOP)
        dot(cx(tr_x[i]), y, 2);
}

static void trajectory(void)
{
    unsigned char i;
    hg_xor = 1;
    for (i = 0; i < ntr; ++i)
        trail(i);
    hg_xor = 0;
    tr_on ^= 1;
}

static unsigned char *row;
#define LINE(y) (((unsigned char *)&row)[0] = hgr_ylo[y], ((unsigned char *)&row)[1] = hgr_yhi[y])

/* Clear the pixels from x0 to x1 on lines y0 to y1 (in the arena). */
static void clear_box(unsigned int x0, unsigned int x1, unsigned char y0, unsigned char y1)
{
    static unsigned char b, m, y;
    if (y0 < TOP)
        y0 = TOP;
    for (; x0 <= x1 && x0 < 280; ++x0) {
        b = x0 / 7;
        m = ~(1 << (x0 % 7));
        for (y = y0; y <= y1; ++y) {
            LINE(y);
#ifdef IIGS
            gs_scr_put(row + b, gs_scr_get(row + b) & m);
#else
            row[b] &= m;
#endif
        }
    }
}

/* A tank's health over its letter (cell b): what is left of its 100, as "87%" (a wreck's, nothing). */
static void health(unsigned char s, unsigned char b, unsigned char y)
{
    static char t[5];
    t[0] = 0;
    if (hp[s]) {
        strcpy(t, utoa10(hp[s]));
        strcat(t, "%");
    }
    ui_textn(b ? b - 1 : 0, y, t, 4);
}

/* The tanks. In monochrome each is cut out of the ground first, a black edge 2 pixels wide round it and 2
   lines under it, and its hull is hollow, so a white tank stands out from the white ground it sits on or
   in. */
static void tanks(void)
{
    unsigned char s, top, y;
    unsigned int l;
    for (s = 0; s < ntank; ++s) {
        top = ly(ht[tx[s]]);
        l = cx(tx[s]) > 3 ? cx(tx[s]) - 3 : 0;
        if (mono)
            clear_box(l > 2 ? l - 2 : 0, l + 8, top - (hp[s] ? 6 : 3), top + 1);
        if (hp[s])
            for (y = 0; y < 4; ++y) {   /* a turret, then the hull */
                hg_px = l + (y ? 0 : 2);
                hg_y = top - 4 + y;
                hg_w = y ? 7 : 3;
                hgr_hline();
            }
        if (hp[s] && mono)
            clear_box(l + 1, l + 5, top - 2, top - 2);  /* the hull hollow */
        else {                          /* a wreck: flat */
            hg_px = l;
            hg_y = top - 1;
            hg_w = 7;
            hgr_hline();
        }
        /* its letter above it, clear of the barrel and the lead, and its health over that */
        y = top < TOP + 36 ? TOP + 8 : top - 28;
        hg_x = l / 7;
        hg_y = y;
        hgr_char('A' + s);
        health(s, l / 7, y - 8);
        shown_tx[s] = tx[s];
        shown_hp[s] = hp[s];
        shown_tg[s] = ht[tx[s]];
    }
}

/* ---- a shot ----------------------------------------------------------------------------------------- */

/* The shell: 2 pixels by 2 lines; above the arena, on its top line, where it will come down. */
static void shell(int x, int y)
{
    if (y < TOP)
        y = TOP;
    hg_xor = 1;
    dot(x, y, 2);
    dot(x, y + 1, 2);
    hg_xor = 0;
}

/* The burst: the crater's shape (r columns across, r heights down) flipped at (x, y). */
static void flash(unsigned int x, int y, unsigned char r)
{
    unsigned char rx = r * scale, ry = r * 2, hw;
    signed char dy;
    int l;
    hg_xor = 1;
    for (dy = -ry; dy <= (signed char)ry; ++dy) {
        l = y + dy;
        if (l < TOP || l > BASE)
            continue;
        hw = rx * isqrt(ry * ry - dy * dy) / ry;
        hg_y = l;
        if (x < hw) {
            hg_px = 0;
            hg_w = x + hw + 1;
        } else {
            hg_px = x - hw;
            hg_w = 2 * hw + 1;
        }
        hgr_hline();
    }
    hg_xor = 0;
}

static void burst(unsigned char hx, unsigned char hy, unsigned char r)
{
    unsigned char k, rk;
    unsigned int x = cx(hx);
    int y = line_of(hy);
    if (!r || r > 10)
        r = 7;
    for (k = 1; k <= 3; ++k) {
        rk = (r * k + 2) / 3;
        flash(x, y, rk);
        nap(4);
        flash(x, y, rk);
    }
}

/* The shell along the path: from the muzzle to each point in three steps, a dot left at each point,
   then the burst (at hx, hy when hit). */
static void fly(unsigned char hit, unsigned char hx, unsigned char hy, unsigned char r)
{
    static int sx, sy, ex, ey, x, y;
    unsigned char i, k;
    if (fs >= ntank)
        return;
    sx = cx(shown_tx[fs]);
    sy = line_of(shown_tg[fs] + 2);
    for (i = 0; i <= ntr; ++i) {
        if (i < ntr) {
            if (tr_x[i] >= aw)
                continue;
            ex = cx(tr_x[i]);
            ey = line_of(tr_y[i]);
        } else if (hit) {
            ex = cx(hx);
            ey = line_of(hy);
        } else
            break;
        for (k = 1; k <= 3; ++k) {
            /* k as an int: cc65 makes an int times an unsigned char unsigned, and then a shell going left
               or up the screen, whose distance is negative, is divided as a large unsigned number and
               lands far from its path */
            x = sx + (ex - sx) * (int)k / 3;
            y = sy + (ey - sy) * (int)k / 3;
            shell(x, y);
            nap(2);
            shell(x, y);
        }
        if (i < ntr) {
            hg_xor = 1;
            trail(i);
            hg_xor = 0;
        }
        sx = ex;
        sy = ey;
    }
    tr_on = 1;
    if (hit)
        burst(hx, hy, r);
}

/* The ground a crater took, dissolving: the pixels between each column's old top and its new one are
   cleared in eight steps, an eighth of them each, in the order of a 4 by 4 ordered dither. */
static const unsigned char dz_rc[16] = {0, 10, 2, 8, 5, 15, 7, 13, 1, 11, 3, 9, 4, 14, 6, 12};
static unsigned char dz_n, dz_x[40], dz_t[40], dz_b[40];

static void dissolve(void)
{
    static unsigned int p, e;
    static unsigned char f, i, v, rc, b, bit, m, y, q;
    for (f = 0; f < 8; ++f) {
        for (i = 0; i < dz_n; ++i) {
            p = ox + dz_x[i] * scale;
            e = p + scale;
            b = p / 7;
            bit = p % 7;
            for (; p < e; ++p) {
                if (mono || (p & 1)) {
                    q = mono ? p : p >> 1;
                    m = ~(1 << bit);
                    for (v = f * 2; v < f * 2 + 2; ++v) {
                        rc = dz_rc[v];
                        if ((rc & 3) != (q & 3))
                            continue;
                        for (y = dz_t[i] + (((rc >> 2) - dz_t[i]) & 3); y < dz_b[i]; y += 4) {
                            LINE(y);
#ifdef IIGS
                            gs_scr_put(row + b, gs_scr_get(row + b) & m);
#else
                            row[b] &= m;
#endif
                        }
                    }
                }
                if (++bit == 7) {
                    bit = 0;
                    ++b;
                }
            }
        }
        nap(4);
    }
}

/* The box round the ground that goes, and the flipped drawings in it (the path's dots, the guns), which
   are taken away while it dissolves and put back after. */
static unsigned int bx0, bx1;
static unsigned char by0, by1;

static unsigned char in_box(int x0, int x1, int y0, int y1)
{
    return x1 >= (int)bx0 && x0 <= (int)bx1 && y1 >= by0 && y0 < by1;
}

static void over_box(void)
{
    unsigned char i;
    int x, y;
    hg_xor = 1;
    if (tr_on)
        for (i = 0; i < ntr; ++i) {
            x = cx(tr_x[i]) & ~1;
            y = line_of(tr_y[i]);
            if (in_box(x, x + 1, y, y))
                trail(i);
        }
    if (g_on)
        for (i = 0; i < 8; ++i)
            if (gd[i] && in_box((int)gx[i] - 24, gx[i] + 24, (int)gy[i] - 22, gy[i]))
                gun(i);
    hg_xor = 0;
}

/* ZCH x chars: the new heights from column x on. */
static void crater(void)
{
    unsigned char x = num(w[4]), i, h;
    const char *c = w[5];
    dz_n = 0;
    by0 = 255;
    by1 = 0;
    for (i = 0; c[i] && x < aw; ++i, ++x) {
        h = c[i] - 0x30;
        if (live && shown_ht[x] == ht[x] && h < ht[x] && dz_n < 40) {
            dz_x[dz_n] = x;
            dz_t[dz_n] = ly(ht[x]);
            dz_b[dz_n] = ly(h);
            if (dz_t[dz_n] < by0)
                by0 = dz_t[dz_n];
            if (dz_b[dz_n] > by1)
                by1 = dz_b[dz_n];
            ++dz_n;
            shown_ht[x] = h;
        }
        ht[x] = h;
    }
    if (dz_n) {
        bx0 = ox + dz_x[0] * scale;
        bx1 = ox + dz_x[dz_n - 1] * scale + scale - 1;
        over_box();
        dissolve();
        over_box();
    }
}

/* ---- the screen ------------------------------------------------------------------------------------- */

static void info(void)
{
    static char b[42];
    strcpy(b, "Angle ");
    strcat(b, utoa10(ang));
    strcat(b, " Power ");
    strcat(b, utoa10(pw));
    strcat(b, " Wind ");
    if (wind < 0) {
        strcat(b, "<");
        strcat(b, utoa10(-wind));
    } else if (wind > 0) {
        strcat(b, utoa10(wind));
        strcat(b, ">");
    } else
        strcat(b, "0");
    ui_textn(0, BASE + 4, b, 27);
    b[0] = 0;
    if (g_seat >= 0) {                  /* which tank is the player's, and their moves left */
        strcpy(b, "You:A Mv ");
        b[4] += g_seat;
        strcat(b, utoa10(moves[g_seat]));
    }
    ui_textn(28, BASE + 4, b, 12);
}

static void cf_draw(void)
{
    unsigned char x, changed = 0;
    scale = 280 / aw;
    if (scale > 3)
        scale = 3;
    ox = (280 - aw * scale) / 2;
    if (g_full) {
        memset(shown_ht, 255, sizeof shown_ht);
        memset(shown_tx, 0, sizeof shown_tx);
        g_on = tr_on = 0;
        ui_button(0, BY, "<", ID_AL);
        ui_button(3, BY, ">", ID_AR);
        ui_button(7, BY, "-", ID_PD);
        ui_button(10, BY, "+", ID_PU);
        ui_button(14, BY, "Fire", ID_FIRE);
        ui_button(21, BY, "Move<", ID_ML);
        ui_button(29, BY, "Move>", ID_MR);
    }
    /* the flipped drawings off, while the ground is drawn */
    if (g_on)
        guns();
    if (tr_on)
        trajectory();
    flying = 0;
    /* ground taken since the last view by a shot whose events were not shown dissolves too */
    dz_n = 0;
    for (x = 0; x < aw; ++x)
        if (shown_ht[x] != 255 && ht[x] < shown_ht[x] && dz_n < 40) {
            dz_x[dz_n] = x;
            dz_t[dz_n] = ly(shown_ht[x]);
            dz_b[dz_n++] = ly(ht[x]);
            shown_ht[x] = ht[x];
        }
    if (dz_n)
        dissolve();
    for (x = 0; x < aw; ++x)
        if (ht[x] != shown_ht[x]) {
            mark(x, 0);
            changed = 1;
        }
    for (x = 0; x < ntank; ++x)
        if (changed || tx[x] != shown_tx[x] || hp[x] != shown_hp[x] || ht[tx[x]] != shown_tg[x]) {
            mark(shown_tx[x], 1);
            mark(tx[x], 1);
            changed = 1;
        }
    if (changed) {
        redraw_bytes();
        tanks();
    }
    if (!g_myturn)
        aim_set = 0;
    else if (!aim_set && g_seat >= 0) {
        aim_set = 1;
        ang = aim_a[g_seat];
        pw = aim_p[g_seat];
    }
    if (ntr)
        trajectory();
    guns();
    info();
    live = 1;
}

/* Events: a shot is shown as its events come (the view is fetched after them all). */
static void cf_event(void)
{
    unsigned char i, s;
    if (is(w[3], "FIRE") && wn > 6) {
        ui_hide();
        if (live && g_on)
            guns();
        if (live && tr_on)
            trajectory();
        ntr = 0;
        fs = num(w[4]);
        if (fs < 8) {
            aim_a[fs] = num(w[5]);
            aim_p[fs] = num(w[6]);
        }
        flying = 1;
        if (live)
            guns();                     /* the barrel turns to the shot's angle */
        ui_show();
    } else if (is(w[3], "TRJ")) {
        for (i = 4; i + 1 < wn && ntr < 18; i += 2) {
            tr_x[ntr] = w[i][0] == '-' ? 255 : num(w[i]);
            tr_y[ntr] = w[i + 1][0] == '-' ? 0 : num(w[i + 1]) > 255 ? 255 : num(w[i + 1]);
            ++ntr;
        }
    } else if ((is(w[3], "HIT") && wn > 6) || (is(w[3], "OUT") && wn > 4)) {
        if (!live)
            return;
        s = w[3][0] == 'H';
        ui_hide();
        if (tr_on)
            trajectory();
        fly(s, s ? num(w[4]) : 0, s ? num(w[5]) : 0, s ? num(w[6]) : 0);
        ui_show();
    } else if (is(w[3], "ZCH") && wn > 5) {
        ui_hide();
        crater();
        ui_show();
    }
}

/* The aim changed (set_aim): this tank's gun again, and the numbers. */
static void aim_shown(void)
{
    unsigned char s = g_seat;
    ui_hide();
    if (g_on && s < 8) {                /* only this tank's gun again */
        hg_xor = 1;
        if (gd[s])
            gun(s);
        gun_at(s);
        if (gd[s])
            gun(s);
        hg_xor = 0;
    }
    info();
    ui_show();
}
