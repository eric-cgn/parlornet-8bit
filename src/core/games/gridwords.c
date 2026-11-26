/* ParlorNet: Grid Words (docs/types/gridwords-1.md). The parlor keeps the bag, deals the racks and checks every
   word against its word list; this only lays out a play.

   Click the empty square a word starts on (a second click turns it from across to down), then type the
   letters of your rack, or click its tiles: each goes on the next empty square along the line, past the
   tiles already there, which join the word, as do the tiles just before and after. A letter you do not
   hold uses a blank, if you have one. Delete takes the last tile back, Cancel (or Esc) all of them; Return or
   Play sends the word (PLAY x y H|V word, the whole word from its first square). As the tiles are laid, the
   parlor's CHK says what the play would score. Exchange (or OA-X) marks tiles to put back and sends them with
   a second Exchange; Pass (or OA-P) passes, after its dialog. The points of the tile under the pointer, on the board or the rack, are on the message
   line, and OA-V shows every letter's points, as the table's TIL lines give them.

   The machine draws it (draw/gridwords.c, included below: gw_draw, redraw and values_chart), each square as
   want() says, the squares targets row * N + column, the rack's tiles ID_RACK up, the buttons ID_PLAY to
   ID_PASS. The look keeps clear of the commercial game's colors (docs/design/gridwords-colours.md). */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "game.h"

#define N 15
#define ID_RACK 225                     /* + index */
#define ID_PLAY 232
#define ID_CLEAR 233
#define ID_SWAP 234
#define ID_PASS 235

static char tile[N][N];                 /* 0 empty, a letter, a blank's letter in lower case */
static unsigned char prm[N][N];         /* 0 none, 1 2L, 2 3L, 3 2W, 4 3W */
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

/* The machine's drawing: gw_draw, redraw and values_chart. */
#include "draw/gridwords.c"

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
            g_message("Pick tiles to put back, then Exchange.");
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
        redraw();
        g_cmd("/pass");                 /* (its dialog first) */
        break;
    }
}

/* The keys: OA-V shows the letters' points, Esc puts the tiles laid back on the rack (or ends an
   exchange), OA-P passes (its dialog first) and OA-X starts an exchange. On your turn letters lay tiles,
   Delete takes the last one back and Return plays the word. */
static unsigned char gw_key(unsigned char k)
{
    if (k == (U_OA | 'V')) {
        values_chart();
        return 1;
    }
    if (k == K_ESC) {
        if (!swap_mode && !np && sx < 0)
            return 0;
        gw_click(ID_CLEAR);
        return 1;
    }
    if (k == (U_OA | 'P') || k == (U_OA | 'X')) {
        gw_click(k == (U_OA | 'P') ? ID_PASS : ID_SWAP);
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
                                 OA "P pass, " OA "X exchange, " OA "V points", 0, 0, gw_hover};
#pragma rodata-name (pop)
