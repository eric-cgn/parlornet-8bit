/* ParlorNet: Mancala, Kalah and Oware (docs/types/mancala-1.md). The view's B line gives the 14 counts in
   sowing order; a move is a pit number, 1 to 6 along the mover's own row. The machine draws the board
   (draw/mancala.c, included below: mc_draw), the player's row along the bottom, pit 1 on the left and their
   store on the right, the other row along the top, read right to left, with its store on the left; the
   player's pits are targets 1 to 6. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "game.h"

static unsigned char cnt[14];
static unsigned char last_seat = 255, last_pit;
static unsigned char me;                /* the viewer's seat: 1 for seat 1, else 0 (mc_draw sets it) */
static char rules[8];
static unsigned char seeds;

static void mc_begin(void)
{
    last_seat = 255;
}

static void mc_line(void)
{
    unsigned char i;
    if (is(w[0], "B")) {
        for (i = 1; i < wn && i <= 14; ++i)
            cnt[i - 1] = num(w[i]);
    } else if (is(w[0], "LM") && wn > 2) {
        last_seat = num(w[1]);
        last_pit = num(w[2]);
    } else if (is(w[0], "RU") && wn > 2) {
        strncpy(rules, w[1], 7);
        seeds = num(w[2]);
    }
}

/* The machine's drawing: mc_draw. */
#include "draw/mancala.c"

static void mc_click(unsigned char id)
{
    static char t[2];
    if (!g_myturn) {
        g_message("Not your turn.");
        return;
    }
    t[0] = '0' + id;
    t[1] = 0;
    if (!m_has(t)) {
        g_message(cnt[me * 7 + id - 1] ? "That pit may not be played now." : "That pit is empty.");
        return;
    }
    g_move(t);
}

static unsigned char mc_key(unsigned char k)
{
    if (k >= '1' && k <= '6') {
        mc_click(k - '0');
        return 1;
    }
    return 0;
}

#pragma rodata-name (push, "OVHDR")
const struct game g_mancala = {"mancala", "Mancala", mc_begin, mc_line, mc_draw, mc_click, mc_key,
                               K3 "1-6 or click: play that pit"};
#pragma rodata-name (pop)
