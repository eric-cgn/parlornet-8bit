/* ParlorNet for the Apple IIe: Grid Words (docs/types/gridwords-1.md). The parlor keeps the bag, deals
   the racks and checks every word against its word list; this only lays out a play.

   Click the empty square a word starts on (a second click turns it from across to down), then type the
   letters of your rack, or click its tiles: each goes on the next empty square along the line, past the
   tiles already there, which join the word, as do the tiles just before and after. A letter you do not
   hold uses a blank, if you have one. Delete takes the last tile back, Clear all of them; Return or Play
   sends the word (PLAY x y H|V word, the whole word from its first square). As the tiles are laid, the
   parlor's CHK says what the play would score. Swap marks tiles to exchange and sends them with a second
   Swap; Pass passes.

   The look keeps clear of the commercial game's colors (docs/design/gridwords-colours.md). In color the
   board is black, word squares orange and letter squares violet, a triple a full square and a double a
   diamond, with a legend beside the board. A tile fills its square: in color a green square (violet for
   a blank, orange while it is being laid) with its letter in black; in monochrome a white square with its
   letter in black and its upper left and lower right corners cut away, which shapes the tiles of a word,
   and while it is being laid only those two corners, as marks, around its letter. On the board a color
   tile's letter is drawn a color pixel (two screen pixels) to each of the font's pixels, so it has no
   fringes; the rack's tiles are narrower and take the letter in bold. In monochrome the squares carry
   their names, 3W, 2W, 3L and 2L, the word squares between two lines. A tile shows its letter only, in
   the middle of the tile, a blank's in lower case; the points of the tile under the pointer, on the
   board or the rack, are on the message line, and Open-Apple-V shows every letter's points, as the
   table's TIL lines give them. The rack's tiles are 10 pixels wide, drawn the same way. */
#include <string.h>
#include "a2.h"
#include "ui.h"
#include "net.h"
#include "game.h"

#define N 15
#define X0 0                            /* the board: 2 bytes by 10 lines a square */
#define Y0 10
#define ID_RACK 225                     /* + index */
#define ID_PLAY 232
#define ID_CLEAR 233
#define ID_SWAP 234
#define ID_PASS 235
#define RACK_Y 82                       /* the rack's tiles (10 lines), the swap marks below */
#define RACK_PX 210                     /* the rack's first tile (pixels); each is RACK_W across */
#define RACK_W 10

static char tile[N][N];                 /* 0 empty, a letter, a blank's letter in lower case */
static unsigned char prm[N][N];         /* 0 none, 1 2L, 2 3L, 3 2W, 4 3W */
static unsigned char shown[N][N];
static unsigned char val[27];           /* points of A-Z, then the blank */
static char rack[8];
static unsigned char nrack, bag, seats_k[4];
static char last[12];
static unsigned int lastpts;

/* The play being laid out. */
static signed char sx = -1, sy;         /* its start square */
static unsigned char down;
static unsigned char np;                /* tiles placed */
static unsigned char px[7], py[7], pr[7];   /* where, and which rack tile */
static char pl[7];                      /* the letter (lower case for a blank) */
static unsigned char used;              /* bit per rack tile placed */
static unsigned char swap_mode, swap_marks;
static unsigned char shown_rack[8], shown_used, shown_marks;
static void preview(void);
static unsigned char hovered;

static void gw_begin(void)
{
    memset(tile, 0, sizeof tile);
    rack[0] = 0;
    nrack = 0;
    last[0] = 0;
}

static void gw_line(void)
{
    unsigned char i, x, y, k;
    if (is(w[0], "X")) {
        for (i = 1; i + 2 < wn; i += 3) {
            x = num(w[i]) - 1;
            y = num(w[i + 1]) - 1;
            if (x < N && y < N)
                tile[y][x] = w[i + 2][0];
        }
    } else if (is(w[0], "R") && wn > 1) {
        if (w[1][0] == '-')
            rack[0] = 0;
        else
            strncpy(rack, w[1], 7);
        rack[7] = 0;
        nrack = strlen(rack);
    } else if (is(w[0], "PRM") && wn > 1) {
        k = is(w[1], "DL") ? 1 : is(w[1], "TL") ? 2 : is(w[1], "DW") ? 3 : is(w[1], "TW") ? 4 : 0;
        for (i = 2; i + 1 < wn; i += 2) {
            x = num(w[i]) - 1;
            y = num(w[i + 1]) - 1;
            if (x < N && y < N)
                prm[y][x] = k;
        }
    } else if (is(w[0], "TIL") && wn > 2) {
        k = w[1][0] == '_' ? 26 : (w[1][0] & 0x1F) - 1;
        if (k < 27)
            val[k] = num(w[2]);
    } else if (is(w[0], "BAG") && wn > 1) {
        bag = num(w[1]);
    } else if (is(w[0], "K") && wn > 2) {
        k = num(w[1]);
        if (k < 4)
            seats_k[k] = num(w[2]);
    } else if (is(w[0], "LAST") && wn > 6) {
        strncpy(last, w[5], 11);
        last[11] = 0;
        lastpts = num(w[6]);
    }
}

static const char *const labels[] = {"", "2L", "3L", "2W", "3W"};
extern const unsigned char spr_gw_2l[], spr_gw_3l[], spr_gw_2w[], spr_gw_3w[];
static const unsigned char *const prm_spr[] = {0, spr_gw_2l, spr_gw_3l, spr_gw_2w, spr_gw_3w};

/* What a square shows: 0 empty (or its premium), a letter, a placed letter (| 0x80), 1 or 2 the start
   arrow across or down. */
static unsigned char want(unsigned char x, unsigned char y)
{
    unsigned char i;
    if (tile[y][x])
        return tile[y][x];
    for (i = 0; i < np; ++i)
        if (px[i] == x && py[i] == y)
            return pl[i] | 0x80;
    if (sx == x && sy == y)
        return down ? 2 : 1;
    return 0;
}

extern const unsigned char font[], hgr_ylo[], hgr_yhi[];

/* Tile looks (tile_draw). */
enum { T_INV, T_MARKS, T_COLOUR, T_PLAIN };
/* Colors of a color tile: which screen pixels of a pair are lit (1 the odd one, 2 the even one), and
   the palette bit. */
#define C_GREEN  0x01
#define C_VIOLET 0x02
#define C_ORANGE 0x81

static unsigned char *trow;
static unsigned char tb, ts, tcol;
static unsigned int tcover;

/* Write the line's pixels: the tile covers tcover's bits (from pixel tb * 7 + ts), lit as in on. */
static void tile_line(unsigned char y, unsigned int on)
{
    unsigned char k, mask, bits, old;
    unsigned int c = tcover;
    ((unsigned char *)&trow)[0] = hgr_ylo[y];
    ((unsigned char *)&trow)[1] = hgr_yhi[y];
    trow += tb;
    for (k = 0; k < 3; ++k) {
        if (k == 0) {
            mask = (c << ts) & 0x7F;
            bits = (on << ts) & 0x7F;
        } else {
            mask = (c >> (k * 7 - ts)) & 0x7F;
            bits = (on >> (k * 7 - ts)) & 0x7F;
        }
        if (!mask)
            continue;
#ifdef IIGS
        old = gs_scr_get(trow + k);     /* the screen is in bank $E0 (a2.h) */
#else
        old = trow[k];
#endif
        bits |= old & ~mask & 0x7F;
        bits |= tcol ? tcol & 0x80 : old & 0x80;
#ifdef IIGS
        gs_scr_put(trow + k, bits);
#else
        trow[k] = bits;
#endif
    }
}

/* A tile w pixels across (14 on the board, RACK_W on the rack) and 10 lines down, at pixel px (even) and
   line y, showing ch in the middle. In color (T_COLOUR) the tile is all color col with its letter in
   black. In monochrome (T_INV) it is all white with its letter in black and its upper left and lower
   right corners cut away; a tile being laid shows only those two corners as short marks (T_MARKS).
   T_PLAIN is the letter alone. */
static void tile_draw(unsigned int px, unsigned char y, unsigned char w, char ch, unsigned char look,
                      unsigned char col)
{
    const unsigned char *g = font + (ch - 32) * 8;
    unsigned char top, h, l, r, gx, j, row, bold = look == T_COLOUR || (look == T_PLAIN && !mono);
    unsigned char wide = look == T_COLOUR && w == 14;
    unsigned int full = (1 << w) - 1, gl, fill = 0;
    unsigned int left = 7, right = 7u << (w - 4);   /* a corner's bar across: 3 pixels */
    tb = px / 7;
    ts = px % 7;
    tcover = full;
    tcol = look == T_COLOUR ? col : 0;
    /* the letter's lines with ink, centered in the tile's lines */
    for (top = 0; top < 8 && !g[top]; ++top)
        ;
    for (h = 8; h > top && !g[h - 1]; --h)
        ;
    h -= top;
    l = look == T_MARKS ? 1 + (7 - h) / 2 : (11 - h) / 2;
    /* its 5 columns (6 in bold) in the middle */
    gx = wide ? 2 : (look == T_MARKS ? w - 1 - 5 : w - 6) / 2;
    if (look == T_COLOUR)
        fill = (col & 3) == C_GREEN ? 0xAAAA : 0x5555;     /* odd or even pixels, from an even px */
    for (r = 0; r < 10; ++r) {
        gl = 0;
        if (r >= l && r < l + h) {
            row = (g[top + r - l] >> 1) & 0x1F;
            if (wide) {
                /* each pixel of the letter a color pixel (two screen pixels) */
                for (j = 0; j < 5; ++j)
                    if (row & (1 << j))
                        gl |= 3 << (2 * j);
            } else {
                gl = row;
                if (bold)
                    gl |= gl << 1;
            }
            gl <<= gx;
        }
        switch (look) {
        case T_MARKS:
            /* the upper left and lower right corners: a bar across on the tile's first (last) line and
               two lines down (up) from it, in the first (last) of its inner columns */
            if (r == 0)
                gl |= left;
            else if (r < 3)
                gl |= 1;
            else if (r == 6 || r == 7)
                gl |= 1 << (w - 2);
            else if (r == 8)
                gl |= right;
            break;
        case T_INV:
            /* the whole square white with the letter in black, so tiles side by side meet as the color
               ones do; only the upper left and lower right corners are cut away, two pixels of the first
               line and one of the second, and the same at the far end, which gives each tile its shape */
            gl = full & ~gl;
            if (r == 0)
                gl &= ~3u;
            else if (r == 1)
                gl &= ~1u;
            else if (r == 8)
                gl &= ~(1u << (w - 1));
            else if (r == 9)
                gl &= ~(3u << (w - 2));
            break;
        case T_COLOUR:
            gl = fill & full & ~gl;
            break;
        }
        tile_line(y + r, gl);
    }
}

static void draw_square(unsigned char x, unsigned char y, unsigned char c)
{
    unsigned char cx = X0 + 2 * x, cy = Y0 + 10 * y, k;
    shown[y][x] = c;
    ui_rect(cx, cy, 2, 10, F_BLACK);
    if (c >= 'A' || (c & 0x80)) {
        unsigned char letter = c & 0x7F;
        /* the letter alone (its points show when the pointer rests on it); a blank in lower case */
        if (mono)
            tile_draw(cx * 7, cy, 14, letter, c & 0x80 ? T_MARKS : T_INV, 0);
        else
            tile_draw(cx * 7, cy, 14, letter, T_COLOUR,
                      c & 0x80 ? C_ORANGE : letter >= 'a' ? C_VIOLET : C_GREEN);
        return;
    }
    if (c == 1 || c == 2) {
        hg_x = cx;
        hg_y = cy + 1;
        hgr_text(c == 1 ? "->" : "|v");
        ui_box(cx * 7, cy, 14, 10);
        return;
    }
    k = prm[y][x];
    if (!k) {
        hg_px = cx * 7 + 7;             /* a dot in the middle of an empty square */
        hg_y = cy + 5;
        hg_w = 1;
        hgr_hline();
        return;
    }
    if (mono) {
        hg_x = cx;
        hg_y = cy + 1;
        hgr_text(labels[k]);
        if (k >= 3) {                   /* word squares: a line above and below (tiles are solid) */
            hg_px = cx * 7 + 1;
            hg_y = cy;
            hg_w = 12;
            hgr_hline();
            hg_px = cx * 7 + 1;
            hg_y = cy + 9;
            hgr_hline();
        }
        return;
    }
    /* color: word squares orange, letter squares violet; a triple full, a double a diamond */
    hg_x = cx;
    hg_y = cy;
    hg_src = prm_spr[k];
    hgr_blit();
}

static void draw_rack(void)
{
    unsigned char i, c, on;
    if (!memcmp(shown_rack, rack, 8) && shown_used == used && shown_marks == swap_marks)
        return;
    memcpy(shown_rack, rack, 8);
    shown_used = used;
    shown_marks = swap_marks;
    ui_textn(30, RACK_Y - 10, g_seat >= 0 ? "Your rack" : "", 10);
    for (i = 0; i < 7; ++i) {
        c = i < nrack ? rack[i] : ' ';
        if (c == '_')
            c = '?';                    /* a blank */
        /* a tile still on the rack is a tile; one laid on the board shows as a plain letter */
        on = i < nrack && !(used & (1 << i));
        tile_draw(RACK_PX + RACK_W * i, RACK_Y, RACK_W, c, !on ? T_PLAIN : mono ? T_INV : T_COLOUR,
                  c == '?' ? C_VIOLET : C_GREEN);
        tile_draw(RACK_PX + RACK_W * i, RACK_Y + 10, RACK_W, swap_marks & (1 << i) ? '^' : ' ', T_PLAIN, 0);
    }
}

/* In color, what the squares mean: each color beside its name. */
static void legend(void)
{
    unsigned char k;
    for (k = 1; k <= 4; ++k) {
        hg_x = k & 1 ? 30 : 34;            /* even columns, where the colors come out right */
        hg_y = k <= 2 ? 50 : 61;
        hg_src = prm_spr[k];
        hgr_blit();
        ui_text(hg_x + 2, hg_y + 1, labels[k]);
    }
}

static void draw_panel(void)
{
    unsigned char s;
    static char b[12];
    for (s = 0; s < g_seats && s < 4; ++s) {
        hg_x = 30;
        hg_y = Y0 + s * 9;
        hgr_char(g_turn == s && !g_over ? '>' : ' ');
        ui_textn(31, Y0 + s * 9, g_name(s), 5);
        strcpy(b, utoa10(g_score[s]));
        ui_textn(36, Y0 + s * 9, "    ", 4);
        ui_textn(40 - strlen(b), Y0 + s * 9, b, strlen(b));
    }
    strcpy(b, "Bag ");
    strcat(b, utoa10(bag));
    ui_textn(30, 40, b, 10);
    draw_rack();
    strncpy(b, last, 10);
    b[10] = 0;
    ui_textn(30, 150, b, 10);
    ui_textn(30, 158, last[0] ? utoa10(lastpts) : "", 10);
}

static void gw_draw(void)
{
    unsigned char x, y, c, i;
    if (g_full) {
        memset(shown, 0xFF, sizeof shown);
        memset(shown_rack, 0xFF, sizeof shown_rack);
        for (y = 0; y < N; ++y)
            for (x = 0; x < N; ++x)
                ui_target(((X0 + 2 * x) * 7 + 7) / 2, Y0 + 10 * y + 4, 3, 5, y * N + x);
        for (i = 0; i < 7; ++i)
            ui_target((RACK_PX + RACK_W * i + RACK_W / 2) / 2, RACK_Y + 5, 2, 5, ID_RACK + i);
        ui_button(30, 103, "Play", ID_PLAY);
        ui_button(30, 114, "Clear", ID_CLEAR);
        ui_button(30, 125, "Swap", ID_SWAP);
        ui_button(30, 136, "Pass", ID_PASS);
        if (!mono)
            legend();
    }
    for (y = 0; y < N; ++y)
        for (x = 0; x < N; ++x) {
            c = want(x, y);
            if (c != shown[y][x])
                draw_square(x, y, c);
            t_hw[y * N + x] = 7;
        }
    draw_panel();
}

static void redraw(void)
{
    ui_hide();
    gw_draw();
    ui_show();
}

static void clear_play(void)
{
    np = 0;
    used = 0;
    sx = -1;
    ui_retkey = 0;
}

/* The next empty square after the last tile placed (or the start), or 0 if the line runs off. */
static unsigned char next_square(unsigned char *x, unsigned char *y)
{
    unsigned char cx, cy, i, taken;
    if (!np) {
        *x = sx;
        *y = sy;
        return 1;
    }
    cx = px[np - 1];
    cy = py[np - 1];
    for (;;) {
        if (down)
            ++cy;
        else
            ++cx;
        if (cx >= N || cy >= N)
            return 0;
        taken = tile[cy][cx] != 0;
        for (i = 0; i < np; ++i)
            if (px[i] == cx && py[i] == cy)
                taken = 1;
        if (!taken) {
            *x = cx;
            *y = cy;
            return 1;
        }
    }
}

static void place(unsigned char ri, char letter)
{
    unsigned char x, y;
    if (sx < 0) {
        g_message("First click the square the word starts on.");
        return;
    }
    if (np >= 7 || !next_square(&x, &y)) {
        g_message("The word runs off the board.");
        return;
    }
    px[np] = x;
    py[np] = y;
    pr[np] = ri;
    pl[np] = letter;
    ++np;
    used |= 1 << ri;
    ui_retkey = 1;
    redraw();
    preview();
}

/* A letter typed: a tile of the rack with that letter, else a blank. */
static void type_letter(char c)
{
    unsigned char i;
    c &= 0xDF;
    for (i = 0; i < nrack; ++i)
        if (!(used & (1 << i)) && rack[i] == c) {
            place(i, c);
            return;
        }
    for (i = 0; i < nrack; ++i)
        if (!(used & (1 << i)) && rack[i] == '_') {
            place(i, c | 0x20);
            return;
        }
    g_message("Your rack has no such letter.");
}

static void unplace(void)
{
    if (!np)
        return;
    --np;
    used &= ~(1 << pr[np]);
    if (!np)
        ui_retkey = 0;
    redraw();
    if (np)
        preview();
    else
        g_message("");
}

/* PLAY x y H|V word: the whole word from its first square, the board's letters included. */
static char word[40], tok[56];

static void build(void)
{
    unsigned char x, y, n = 0, i, found;
    char c;
    x = sx;
    y = sy;
    while ((down ? y : x) > 0 && tile[down ? y - 1 : y][down ? x : x - 1]) {
        if (down)
            --y;
        else
            --x;
    }
    strcpy(tok, "PLAY ");
    strcat(tok, utoa10(x + 1));
    strcat(tok, " ");
    strcat(tok, utoa10(y + 1));
    strcat(tok, down ? " V " : " H ");
    while (x < N && y < N && n < 36) {
        c = tile[y][x];
        if (c) {
            word[n++] = c & 0xDF;
        } else {
            found = 0;
            for (i = 0; i < np; ++i)
                if (px[i] == x && py[i] == y) {
                    if (pl[i] >= 'a') {
                        word[n++] = '_';
                        word[n++] = pl[i] - 32;
                    } else
                        word[n++] = pl[i];
                    found = 1;
                }
            if (!found)
                break;
        }
        if (down)
            ++y;
        else
            ++x;
    }
    word[n] = 0;
    strcat(tok, word);
}

static void play(void)
{
    if (!np) {
        g_message("Lay out a word first.");
        return;
    }
    build();
    if (g_move(tok)) {
        clear_play();
        redraw();
    }
}

/* As the tiles are laid, the parlor's CHK says what the play would score, or why it would be refused. */
static void preview(void)
{
    static char b[50];                  /* g_chkinfo (40) and " points: " */
    char *p;
    if (!np)
        return;
    build();
    if (g_chk(tok)) {
        p = strchr(g_chkinfo, ' ');
        if (p)
            *p++ = 0;
        strcpy(b, g_chkinfo);
        strcat(b, " points");
        if (p && strlen(b) < 38) {
            strcat(b, ": ");
            strncat(b, p, 40 - strlen(b));
        }
        g_message(b);
    } else
        g_message(g_chkinfo);
}

static void gw_click(unsigned char id)
{
    static char b[12];
    unsigned char i;
    if (!g_myturn) {
        g_message("Not your turn.");
        return;
    }
    if (id < N * N) {
        unsigned char x = id % N, y = id / N;
        if (swap_mode)
            return;
        if (tile[y][x]) {
            g_message("Start on an empty square.");
            return;
        }
        if (np) {
            /* a click on a placed tile takes the tiles back to it */
            for (i = 0; i < np; ++i)
                if (px[i] == x && py[i] == y) {
                    while (np > i)
                        unplace();
                    return;
                }
            return;
        }
        if (sx == x && sy == y)
            down ^= 1;
        else {
            sx = x;
            sy = y;
            down = 0;
        }
        g_message(down ? "Down: type the letters." : "Across: type the letters.");
        redraw();
        return;
    }
    if (id >= ID_RACK && id < ID_RACK + 7) {
        i = id - ID_RACK;
        if (i >= nrack)
            return;
        if (swap_mode) {
            swap_marks ^= 1 << i;
            redraw();
            return;
        }
        if (used & (1 << i))
            return;
        if (rack[i] == '_') {
            g_message("Type the letter the blank stands for.");
            return;
        }
        place(i, rack[i]);
        return;
    }
    switch (id) {
    case ID_PLAY:
        play();
        break;
    case ID_CLEAR:
        clear_play();
        swap_mode = 0;
        swap_marks = 0;
        g_message("");
        redraw();
        break;
    case ID_SWAP:
        if (!swap_mode) {
            clear_play();
            swap_mode = 1;
            swap_marks = 0;
            g_message("Click the tiles to swap, then Swap.");
            redraw();
            break;
        }
        swap_mode = 0;
        if (!swap_marks) {
            redraw();
            break;
        }
        strcpy(b, "EXCH ");
        for (i = 0; i < nrack; ++i)
            if (swap_marks & (1 << i)) {
                unsigned char n = strlen(b);
                b[n] = rack[i];
                b[n + 1] = 0;
            }
        swap_marks = 0;
        g_move(b);
        redraw();
        break;
    case ID_PASS:
        clear_play();
        g_message("Pass? Press " K1 "Y to pass.");
        {
            unsigned char k;
            do
                k = kbd_get();
            while (!k);
            g_message("");
            if ((k | 0x20) == 'y')
                g_move("PASS");
        }
        break;
    }
}

/* OA-V: every letter's points, as the table's rules give them (its TIL lines), over the board until a
   key. */
static void values_chart(void)
{
    static char b[8];
    unsigned char i, v;
    ui_hide();
    ui_rect(X0, Y0, 30, 150, F_BLACK);
    ui_ink(F_WHITE, F_BLACK);
    ui_text(2, Y0 + 6, "Points for each letter");
    for (i = 0; i < 27; ++i) {
        v = val[i];
        if (i < 26) {
            b[0] = 'A' + i;
            b[1] = ' ';
            b[2] = v >= 10 ? '0' + v / 10 : ' ';
            b[3] = '0' + v % 10;
            b[4] = 0;
        } else {
            strcpy(b, "Blank ");
            b[6] = '0' + v % 10;
            b[7] = 0;
        }
        ui_text(3 + (i / 9) * 9, Y0 + 24 + (i % 9) * 12, b);
    }
    ui_text(2, Y0 + 138, "A key: back to the board");
    while (!kbd_get())
        ;
    g_redraw();
}

static unsigned char gw_key(unsigned char k)
{
    if (k == (U_OA | 'V')) {
        values_chart();
        return 1;
    }
    if (!g_myturn)
        return 0;
    if ((k >= 'A' && k <= 'Z') || (k >= 'a' && k <= 'z')) {
        type_letter(k);
        return 1;
    }
    if (k == K_DEL) {
        unplace();
        return 1;
    }
    if (k == K_RET) {
        play();
        return 1;
    }
    return 0;
}

/* The pointer rests on a target: a tile's points on the message line. */
static void gw_hover(unsigned char id)
{
    static char b[24];
    char c = 0;
    if (id < N * N)
        c = tile[id / N][id % N];
    else if (id >= ID_RACK && id < ID_RACK + 7 && id - ID_RACK < nrack)
        c = rack[id - ID_RACK];
    if (!c) {
        if (hovered)
            g_message("");
        hovered = 0;
        return;
    }
    hovered = 1;
    if (c == '_' || c >= 'a') {
        strcpy(b, "A blank");
        if (c >= 'a') {
            strcat(b, " for ");
            b[strlen(b) + 1] = 0;
            b[strlen(b)] = c - 32;
        }
        strcat(b, ": 0 points");
    } else {
        b[0] = c;
        b[1] = 0;
        strcat(b, ": ");
        strcat(b, utoa10(val[c - 'A']));
        strcat(b, val[c - 'A'] == 1 ? " point" : " points");
    }
    g_message(b);
}

#pragma rodata-name (push, "OVHDR")
const struct game g_gridwords = {"gridwords", "Grid Words", gw_begin, gw_line, gw_draw, gw_click, gw_key,
                                 "Point at a tile: its points. " OA "V: all", 0, 0, gw_hover};
#pragma rodata-name (pop)
