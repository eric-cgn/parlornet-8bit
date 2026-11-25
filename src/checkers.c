/* ParlorNet for the Apple IIe: Checkers (docs/types/checkers-1.md). The view's B line gives the 32 dark
   squares; the server lists every legal step (M lines). A step is the piece to move, then the square to
   move to; after a jump that must go on, the same piece stays chosen and the next square is clicked.
   A turn is staged: Done sends END, Undo takes a step back. The board is turned so that the player's
   own men start at the bottom. Dark squares are black, light squares orange (dotted in monochrome);
   Black's men are white rims round black and White's are solid white, so both show on the dark squares
   in color and on a monochrome screen alike. */
#include <string.h>
#include "a2.h"
#include "ui.h"
#include "net.h"
#include "game.h"

extern const unsigned char spr_ck_w[], spr_ck_b[], spr_ck_wk[], spr_ck_bk[], spr_ck_hint[];

#define X0 1                            /* the board: 3 bytes by 19 lines a square */
#define Y0 10
#define CW 3
#define CH 19
#define ID_A 40                         /* the panel's two buttons */
#define ID_B 41
#define PX 27                           /* the panel's column */

static char cells[33];                  /* the B line: '.', x, X, o, O per dark square 1-32 */
static char shown[33];
static unsigned char cnt[4];            /* men and kings: seat 0, then seat 1 */
static char phase[8];
static unsigned char path[16], npath;   /* J: the moving piece's path this turn */
static signed char drawer = -1;         /* D: a draw offer waits, from this seat */
static unsigned char sel, flip;
static char shown_panel[24];

static void ck_begin(void)
{
    phase[0] = 0;
    npath = 0;
    drawer = -1;
}

static void ck_line(void)
{
    unsigned char i;
    if (is(w[0], "B") && wn > 1) {
        strncpy(cells + 1, w[1], 32);
    } else if (is(w[0], "C") && wn > 4) {
        for (i = 0; i < 4; ++i)
            cnt[i] = num(w[i + 1]);
    } else if (is(w[0], "Q") && wn > 1) {
        strncpy(phase, w[1], 7);
    } else if (is(w[0], "J")) {
        npath = 0;
        for (i = 1; i < wn && npath < 16; ++i)
            path[npath++] = num(w[i]);
    } else if (is(w[0], "D") && wn > 1) {
        drawer = num(w[1]);
    }
}

/* Where dark square s (1-32) is drawn: byte column and line. */
static unsigned char sq_x, sq_y;
static void place_sq(unsigned char s)
{
    unsigned char r = (s - 1) >> 2, c = ((s - 1) & 3) * 2 + !(r & 1);
    if (flip) {
        r = 7 - r;
        c = 7 - c;
    }
    sq_x = X0 + c * CW;
    sq_y = Y0 + r * CH;
}

static const unsigned char *piece(char c)
{
    switch (c) {
    case 'x': return spr_ck_b;
    case 'X': return spr_ck_bk;
    case 'o': return spr_ck_w;
    case 'O': return spr_ck_wk;
    case 'h': return spr_ck_hint;
    }
    return 0;
}

static void draw_sq(unsigned char s, char c)
{
    const unsigned char *p = piece(c);
    shown[s] = c;
    place_sq(s);
    ui_rect(sq_x, sq_y, CW, CH, F_BLACK);
    if (p) {
        hg_x = sq_x;
        hg_y = sq_y;
        hg_src = p;
        hgr_blit();
    }
    if (c & 0x80) {       /* the chosen piece: its own picture in a frame */
        p = piece(c & 0x7F);
        if (p) {
            hg_x = sq_x;
            hg_y = sq_y;
            hg_src = p;
            hgr_blit();
        }
        ui_box(sq_x * 7, sq_y, CW * 7, CH);
    }
}

static unsigned char from_of(const char *m)
{
    return num(m);
}

/* The square a step token ends on ("11-15", "15x22"). */
static unsigned char to_of(const char *m)
{
    while (*m >= '0' && *m <= '9')
        ++m;
    return *m ? num(m + 1) : 0;
}

static unsigned char can_go(unsigned char from, unsigned char to)
{
    unsigned char i;
    for (i = 0; i < m_n; ++i)
        if (from_of(m_tok[i]) == from && (!to || to_of(m_tok[i]) == to))
            return 1;
    return 0;
}

static void draw_panel(void)
{
    unsigned char s, me = flip ? 0 : 1, y;
    static const char *a, *b;
    static char key[24], t[12];
    for (s = 0; s < 2; ++s) {
        y = s == me ? 120 : Y0;
        hg_x = PX;
        hg_y = y;
        hg_src = s ? spr_ck_w : spr_ck_b;
        hgr_blit();
        ui_textn(PX + 3, y + 2, g_name(s), 10);
        ui_textn(PX + 3, y + 11, s ? "White" : "Black", 10);
        strcpy(t, utoa10(cnt[s * 2]));
        strcat(t, " men ");
        if (cnt[s * 2 + 1]) {
            strcat(t, utoa10(cnt[s * 2 + 1]));
            strcat(t, "K");
        }
        ui_textn(PX, y + 21, t, 13);
        ui_textn(PX, y + 30, g_turn == s && !g_over ? "to play" : "", 13);
    }
    a = b = "";
    if (g_myturn) {
        if (drawer >= 0 && drawer != g_seat) {
            a = "Accept";
            b = "Reject";
        } else if (strcmp(phase, "done") == 0) {
            a = "Done";
            b = "Undo";
        } else if (npath)
            b = "Undo";
        else
            b = "Offer draw";
    }
    strcpy(key, a);
    strcat(key, "/");
    strcat(key, b);
    if (strcmp(key, shown_panel) != 0) {
        strcpy(shown_panel, key);
        ui_rect(PX, 66, 13, 8, F_BLACK);
        ui_rect(PX, 86, 13, 8, F_BLACK);
        if (a[0])
            ui_button_draw(PX, 66, a);
        if (b[0])
            ui_button_draw(PX, 86, b);
    }
}

static void ck_draw(void)
{
    unsigned char s, r, c;
    char want;
    flip = g_seat != 1;                 /* Black (seat 0) starts on 1-12, at the top unturned */
    if (g_full) {
        sel = 0;
        shown_panel[0] = 1;
        memset(shown, 0, sizeof shown);
        for (r = 0; r < 8; ++r)
            for (c = 0; c < 8; ++c)
                if (!((r + c) & 1))
                    ui_rect(X0 + c * CW, Y0 + r * CH, CW, CH, mono ? F_GREY : F_ORANGE);
        for (s = 1; s <= 32; ++s) {
            place_sq(s);
            ui_target((sq_x * 7 + CW * 7 / 2) / 2, sq_y + CH / 2, CW * 7 / 4, CH / 2, s);
        }
        ui_target((PX * 7 + 21) / 2, 70, 21, 4, ID_A);
        ui_target((PX * 7 + 35) / 2, 90, 35, 4, ID_B);
    }
    /* mid-jump, the piece that must go on stays chosen */
    if (npath && strcmp(phase, "move") == 0 && g_myturn)
        sel = path[npath - 1];
    for (s = 1; s <= 32; ++s) {
        want = cells[s];
        if (want == '.' && sel && g_myturn && can_go(sel, s))
            want = 'h';
        if (s == sel)
            want |= 0x80;
        if (want != shown[s])
            draw_sq(s, want);
    }
    draw_panel();
}

static void redraw(void)
{
    ui_hide();
    ck_draw();
    ui_show();
}

static void ck_click(unsigned char id)
{
    static char tok[8];
    if (!g_myturn) {
        g_message("Not your turn.");
        return;
    }
    if (id == ID_A || id == ID_B) {
        static char word[12];
        char *slash = strchr(shown_panel, '/');
        if (!slash)
            return;
        if (id == ID_B)
            strncpy(word, slash + 1, 11);
        else {
            *slash = 0;
            strncpy(word, shown_panel, 11);
            *slash = '/';
        }
        sel = 0;
        if (is(word, "Done"))
            g_end();
        else if (is(word, "Undo"))
            g_undo();
        else if (is(word, "Accept"))
            g_move("accept");
        else if (is(word, "Reject"))
            g_move("reject");
        else if (word[0] == 'O' && g_move("draw"))
            g_message("Draw offered.");
        return;
    }
    if (strcmp(phase, "move") != 0)
        return;
    if (!sel || (id != sel && can_go(id, 0) && !npath)) {
        if (!can_go(id, 0)) {
            g_message("No piece of yours can move from there.");
            return;
        }
        sel = id;
        g_message("Now the square to move to.");
        redraw();
        return;
    }
    if (id == sel) {
        if (!npath) {
            sel = 0;
            g_message("");
            redraw();
        }
        return;
    }
    if (!can_go(sel, id)) {
        g_message("That is not a legal move.");
        return;
    }
    strcpy(tok, utoa10(sel));
    strcat(tok, "-");
    strcat(tok, utoa10(id));
    sel = 0;
    g_move(tok);
    if (g_reply_more)
        g_message("Jump again with the same piece.");
    else if (g_reply_done)
        g_message("Done to end the turn, or Undo.");
}

static unsigned char ck_key(unsigned char k)
{
    k |= 0x20;
    if (k == 'u') {
        ck_click(ID_B);
        return 1;
    }
    if (k == 'd') {
        ck_click(ID_A);
        return 1;
    }
    return 0;
}

#pragma rodata-name (push, "OVHDR")
const struct game g_checkers = {"checkers", "Checkers", ck_begin, ck_line, ck_draw, ck_click, ck_key,
                                "Piece, then square; " K1 "D done, " K1 "U undo"};
#pragma rodata-name (pop)
