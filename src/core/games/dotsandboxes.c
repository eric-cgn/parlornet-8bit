/* ParlorNet: Dots and Boxes (docs/types/dotsandboxes-1.md), 2 to 4 players, on any board the parlor offers up
   to 13 by 11 boxes. The view gives the lines across (HL), the lines down (VL) and the boxes taken (BX). A
   line is drawn by clicking a dot, then the dot next to it; the pointer moves from dot to dot, each a
   target, its id row * (bw + 1) + column. The machine draws the board (draw/dotsandboxes.c, included
   below: db_draw, and sel_frame, which shows the chosen dot, seld, or takes it away), a box taken in its
   player's letter (A to D). */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "game.h"

#define MW 13                           /* the largest board, in boxes */
#define MH 11

static unsigned char bw = 5, bh = 5;    /* boxes across and down */
static char hl[MH + 1][MW + 1], vl[MH][MW + 2], bx[MH][MW + 1];
static unsigned char cnt[4], gone;
static char last[8];
static signed char seld = -1;            /* the chosen dot, or -1 */

static void db_begin(void)
{
    last[0] = 0;
    gone = 0;
}

static void db_line(void)
{
    unsigned char y, i;
    if (is(w[0], "BRD") && wn > 2) {
        bw = num(w[1]);
        bh = num(w[2]);
        if (bw > MW)
            bw = MW;
        if (bh > MH)
            bh = MH;
        return;
    }
    if (wn < 3) {
        if (is(w[0], "LL") && wn > 1)
            strncpy(last, w[1], 7);
        return;
    }
    y = num(w[1]) - 1;
    if (is(w[0], "HL") && y <= MH)
        strncpy(hl[y], w[2], MW);
    else if (is(w[0], "VL") && y < MH)
        strncpy(vl[y], w[2], MW + 1);
    else if (is(w[0], "BX") && y < MH)
        strncpy(bx[y], w[2], MW);
    else if (is(w[0], "C"))
        for (i = 0; i < 4 && i + 1 < wn; ++i)
            cnt[i] = num(w[i + 1]);
    else if (is(w[0], "G"))
        for (i = 0; i < 4 && i + 1 < wn; ++i)
            if (w[i + 1][0] == '1')
                gone |= 1 << i;
}

/* The machine's drawing: db_draw and sel_frame. */
#include "draw/dotsandboxes.c"

static void db_click(unsigned char id)
{
    static char tok[8];
    unsigned char i, j, i2, j2, open;
    if (!g_myturn) {
        g_message("It's not your turn.");
        return;
    }
    if (seld < 0 || seld == id) {
        ui_hide();
        sel_frame();
        seld = seld == id ? -1 : id;
        sel_frame();
        ui_show();
        g_message("");
        return;
    }
    i = seld % (bw + 1);
    j = seld / (bw + 1);
    i2 = id % (bw + 1);
    j2 = id / (bw + 1);
    if (i2 < i || j2 < j) {             /* the left or upper dot first */
        unsigned char t = i;
        i = i2;
        i2 = t;
        t = j;
        j = j2;
        j2 = t;
    }
    if (j == j2 && i2 == i + 1)
        open = hl[j][i] == '.';
    else if (i == i2 && j2 == j + 1)
        open = vl[j][i] == '.';
    else {
        /* not a neighbor: that dot is chosen instead */
        ui_hide();
        sel_frame();
        seld = id;
        sel_frame();
        ui_show();
        return;
    }
    if (!open) {
        g_message("That line is drawn already.");
        return;
    }
    tok[0] = 'a' + i;
    strcpy(tok + 1, utoa10(j + 1));
    strcat(tok, "-");
    j = strlen(tok);
    tok[j] = 'a' + i2;
    strcpy(tok + j + 1, utoa10(j2 + 1));
    ui_hide();
    sel_frame();
    seld = -1;
    ui_show();
    g_move(tok);
}

/* A dot's name ("c3") into b. */
static void dot_name(char *b, unsigned char id)
{
    b[0] = 'a' + id % (bw + 1);
    strcpy(b + 1, utoa10(id / (bw + 1) + 1));
}

/* What Return does on the dot under the pointer: pick it, or draw the line to it from the dot picked. */
static void db_hover(unsigned char id)
{
    static char b[42], d[4];
    unsigned char a, c;
    if (id == 255) {
        g_message("Move to a dot.");
        return;
    }
    dot_name(d, id);
    if (seld < 0 || seld == id) {
        strcpy(b, seld < 0 ? "Press " KEY_RET " to pick dot " : "Pick a dot next to ");
        strcat(b, d);
        strcat(b, seld < 0 ? "." : ", or press " KEY_ESC ".");
    } else {
        a = seld < id ? seld : id;
        c = seld < id ? id : seld;
        if (c == a + 1 && a % (bw + 1) != bw || c == a + bw + 1) {
            strcpy(b, "Press " KEY_RET " to draw the line ");
            dot_name(b + strlen(b), a);
            strcat(b, "-");
            dot_name(b + strlen(b), c);
            strcat(b, ".");
        } else
            strcpy(b, "Pick a dot next to the one picked.");
    }
    g_message(b);
}

/* The other player's line in words: "ann drew a1-b1 and closed a box." */
static void db_event(void)
{
    if (is(w[3], "LINE") && wn > 5 && num(w[4]) != (unsigned char)g_seat) {
        strcat(g_who(num(w[4])), " drew ");
        strcat(g_last, w[5]);
        if (wn > 7)
            strcat(g_last, wn > 8 ? " and closed two boxes" : " and closed a box");
        strcat(g_last, ".");
    }
}

/* Your turn again after closing a box. */
static const char *db_say(void)
{
    return g_myturn && strstr(g_status, "again") ? "You closed a box, so it's your turn again." : 0;
}

/* Esc lets go of a picked dot. */
static unsigned char db_key(unsigned char k)
{
    if (k != K_ESC || seld < 0)
        return 0;
    ui_hide();
    sel_frame();
    seld = -1;
    ui_show();
    g_message("");
    return 1;
}

/* The empty board, while the table waits (game.h). */
static void db_empty(void)
{
    bw = bh = 5;
    memset(hl, '.', sizeof hl);
    memset(vl, '.', sizeof vl);
    memset(bx, '.', sizeof bx);
    memset(cnt, 0, 4);
}

#pragma rodata-name (push, "OVHDR")
const struct game g_dotsandboxes = {"dotsandboxes", "Dots and Boxes", db_begin, db_line, db_draw, db_click, db_key,
                                    1, db_event, db_hover, 0, db_say, db_empty};
#pragma rodata-name (pop)
