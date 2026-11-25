/* ParlorNet: Cannon Fire (docs/types/cannonfire-1.md), 2 to 8 tanks. The parlor runs all the physics; this
   shows the terrain (Z lines), the tanks (K), the wind (N) and the shots, and sends the player's aim. The aim
   is an angle (0 towards the right, 90 straight up, 180 towards the left) and a power (0 to 100): the
   buttons change them by 5, the keys , and . (angle) and - and = (power) by 1. Fire shoots; Move drives the
   tank left or right as far as one move goes (four a game).

   The machine draws it (draw/cannonfire.c, included below: cf_draw, cf_event, which shows a shot's events
   as they come, and aim_shown, which shows the aim changed), with the buttons ID_AL to ID_MR. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "game.h"

#define AMAX 200
#define ID_AL 1                         /* the buttons */
#define ID_AR 2
#define ID_PD 3
#define ID_PU 4
#define ID_FIRE 5
#define ID_ML 6
#define ID_MR 7

static unsigned char aw = 72, ht[AMAX];
static unsigned char tx[8], hp[8], moves[8], ntank;
static unsigned char aim_a[8], aim_p[8];
static signed char wind;
static unsigned char moved;
static unsigned char ang = 45, pw = 50, aim_set;
static unsigned char tr_x[18], tr_y[18], ntr;
static unsigned char hit_x, hit_y, have_hit;
static unsigned char live;              /* the table is on the screen as the last view said */
static unsigned char fs;                /* the seat that fired */

static void cf_begin(void)
{
    ntank = 0;
    have_hit = 0;
    live = 0;
}

static void cf_line(void)
{
    unsigned char i, x, s;
    if (is(w[0], "ARN") && wn > 1) {
        aw = num(w[1]);
        if (aw > AMAX)
            aw = AMAX;
    } else if (is(w[0], "Z") && wn > 2) {
        x = num(w[1]);
        for (i = 0; w[2][i] && x < aw; ++i, ++x)
            ht[x] = w[2][i] - 0x30;
    } else if (is(w[0], "K") && wn > 3) {
        s = num(w[1]);
        if (s < 8) {
            tx[s] = num(w[2]);
            hp[s] = num(w[3]);
            if (s >= ntank)
                ntank = s + 1;
        }
    } else if (is(w[0], "A") && wn > 3) {
        s = num(w[1]);
        if (s < 8) {
            aim_a[s] = num(w[2]);
            aim_p[s] = num(w[3]);
        }
    } else if (is(w[0], "L") && wn > 2) {
        s = num(w[1]);
        if (s < 8)
            moves[s] = num(w[2]);
    } else if (is(w[0], "N") && wn > 1) {
        wind = snum(w[1]);
    } else if (is(w[0], "B") && wn > 2) {
        moved = num(w[2]);
    } else if (is(w[0], "H") && wn > 2) {
        hit_x = num(w[1]);
        hit_y = num(w[2]);
        have_hit = 1;
    }
}

/* The machine's drawing: cf_draw, cf_event and aim_shown. */
#include "draw/cannonfire.c"

static void set_aim(signed char da, signed char dp)
{
    int a = ang + da, p = pw + dp;
    if (a < 0)
        a = 0;
    if (a > 180)
        a = 180;
    if (p < 0)
        p = 0;
    if (p > 100)
        p = 100;
    ang = a;
    pw = p;
    aim_shown();
}

static void cf_click(unsigned char id)
{
    static char t[16];
    if (!g_myturn) {
        g_message("Not your turn.");
        return;
    }
    switch (id) {
    case ID_AL: set_aim(5, 0); break;
    case ID_AR: set_aim(-5, 0); break;
    case ID_PD: set_aim(0, -5); break;
    case ID_PU: set_aim(0, 5); break;
    case ID_FIRE:
        strcpy(t, "FIRE ");
        strcat(t, utoa10(ang));
        strcat(t, " ");
        strcat(t, utoa10(pw));
        aim_set = 0;
        g_move(t);
        break;
    case ID_ML:
    case ID_MR:
        if (moved) {
            g_message("One move a turn: now fire.");
            break;
        }
        g_move(id == ID_ML ? "MOVE L 999" : "MOVE R 999");
        break;
    }
}

static unsigned char cf_key(unsigned char k)
{
    switch (k) {
    case ',': set_aim(1, 0); return 1;
    case '.': set_aim(-1, 0); return 1;
    case '-': set_aim(0, -1); return 1;
    case '=': case '+': set_aim(0, 1); return 1;
    case 'f': case 'F': cf_click(ID_FIRE); return 1;
    }
    return 0;
}

#pragma rodata-name (push, "OVHDR")
const struct game g_cannonfire = {"cannonfire", "Cannon Fire", cf_begin, cf_line, cf_draw, cf_click, cf_key,
                                  K3 ", . angle  " K3 "- = power  " K1 "F fire", 1, cf_event};
#pragma rodata-name (pop)
