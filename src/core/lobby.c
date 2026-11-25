/* ParlorNet: the lobby. Your games (from LST: the tables where you sit), your
   invitations (IVL), the tables where someone waits for a player, and a new game against the bot, anyone,
   or a player you invite. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "app.h"
#include "games.h"

struct gam {
    char type[16];
    char rules[10];
    unsigned char name;                 /* its name in gnames: one for each game, whatever its rules */
    unsigned char min, max;
};
struct tab {
    unsigned int id;
    char type[16];
    char who[22];                       /* the other players, shortened */
    unsigned char mine, open, full;
};
struct inv {
    unsigned int id;
    char type[16];
    char from[13];
};

#define MAXG 20
#define MAXTAB 16
#define MAXI 6
static struct gam gams[MAXG];
static unsigned char ngam;
#pragma bss-name (push, "LOWBSS")
static char gnames[160];                /* (in low memory with the reply buffer) */
#pragma bss-name (pop)
/* The tables and invitations share the language card's RAM above the overlays with a table's moves and
   chat (LOBBY and TABLE in parlor.cfg), so they are read again after a table. */
#pragma bss-name (push, "LOBBY")
static struct tab tabs[MAXTAB];
static struct inv invs[MAXI];
#pragma bss-name (pop)
static unsigned char ntab;
static unsigned char ninv;

#define ID_TAB 0                        /* + index: open a table of yours, or join one */
#define ID_ACC 40                       /* + index: accept an invitation */
#define ID_DEC 60                       /* + index: decline it */
#define B_NEW 200
#define B_REFRESH 201
#define B_DISPLAY 202
#define B_OUT 203
#define B_BACK 204

static const char *nice_name(const char *type)
{
    unsigned char i, n = strlen(type);
    const char *p = strchr(type, '/');
    if (p)
        n = p - type;
    for (i = 0; i < ngam; ++i)
        if (strncmp(gams[i].type, type, n) == 0 && gams[i].type[n] == '/')
            return gnames + gams[i].name;
    return type;
}

static void load_gam(void)
{
    static unsigned char i, k;
    static struct gam *g, *h;
    static char *p;
    q_set("GAM");
    if (net_req() != R_OK)
        return;
    ngam = 0;
    k = 0;
    for (i = 0; i < r_n && ngam < MAXG; ++i) {
        words(r_line[i]);
        if (wn < 6 || !is(w[0], "GAM"))
            continue;
        g = gams + ngam;
        strncpy(g->type, w[1], 15);
        strncpy(g->rules, w[2], 9);
        for (h = gams; h < g && strcmp(h->type, w[1]); ++h)
            ;
        if (h < g)
            g->name = h->name;          /* the same game with other rules */
        else {
            if (k + 20 > sizeof gnames)
                continue;
            g->name = k;
            p = gnames + k;
            strncpy(p, w[3], 19);
            p[19] = 0;
            for (; *p; ++p, ++k)
                if (*p == '_')
                    *p = ' ';
            ++k;
        }
        g->min = num(w[4]);
        g->max = num(w[5]);
        ++ngam;
    }
}

static unsigned char names_have_me(char *names, char *who)
{
    unsigned char mine = 0;
    char *p = names, *e;
    who[0] = 0;
    for (;;) {
        e = strchr(p, ',');
        if (e)
            *e = 0;
        if (strcmp(p, me) == 0)
            mine = 1;
        else if (strlen(who) + strlen(p) < 20) {
            if (who[0])
                strcat(who, ",");
            strcat(who, p);
        }
        if (!e)
            break;
        p = e + 1;
    }
    return mine;
}

static void load_lists(void)
{
    unsigned char i;
    char *p;
    q_set("LST");
    ntab = 0;
    if (net_req() == R_OK)
        for (i = 0; i < r_n && ntab < MAXTAB; ++i) {
            words(r_line[i]);
            if (wn < 7 || !is(w[0], "TAB"))
                continue;
            tabs[ntab].id = num(w[1]);
            strncpy(tabs[ntab].type, w[2], 15);
            tabs[ntab].open = is(w[5], "open");
            p = strchr(w[4], '/');
            tabs[ntab].full = p != 0 && num(w[4]) >= num(p + 1);
            tabs[ntab].mine = names_have_me(w[6], tabs[ntab].who);
            if (tabs[ntab].mine || (tabs[ntab].open && !tabs[ntab].full))
                ++ntab;
        }
    q_set("IVL");
    ninv = 0;
    if (net_req() == R_OK)
        for (i = 0; i < r_n && ninv < MAXI; ++i) {
            words(r_line[i]);
            if (wn < 5 || !is(w[0], "IVT"))
                continue;
            invs[ninv].id = num(w[1]);
            strncpy(invs[ninv].type, w[2], 15);
            strncpy(invs[ninv].from, w[4], 12);
            ++ninv;
        }
    clk_sync();
}

static unsigned char row;

static void heading(const char *s)
{
    ++row;
    ui_ink(F_BLACK, F_WHITE);
    ui_textn(mono ? 0 : 1, row * 8, s, mono ? 40 : 39);
    ui_ink(F_WHITE, F_BLACK);
    if (!mono)
        ui_rect(0, row * 8, 1, 8, F_ORANGE);
    ++row;
}

/* The lobby's own working buffers below go in LOBBY too: nothing in them outlives the lobby screen. */
#pragma bss-name (push, "LOBBY")
static void entry(const char *a, const char *b, const char *c)
{
    static char s[42];
    strncpy(s, a, 13);
    s[13] = 0;
    while (strlen(s) < 14)
        strcat(s, " ");
    strncat(s, b, 14);
    ui_textn(1, row * 8, s, 28);
    if (c)
        ui_textn(30, row * 8, c, 10);
}

static void draw_lobby(void)
{
    static char b[42];
    unsigned char i, n;
    gr_mode();
    ui_cls();
    ui_ink(F_BLACK, F_WHITE);
    /* "ParlorNet: name at server", in the 32 columns before the connected time (clk_show): without
       "ParlorNet: " when that does not fit, then with the server's name cut short (b holds 41) */
    b[0] = 0;
    if (strlen(me) + strlen(server_name) <= 32 - 15)
        strcpy(b, "ParlorNet: ");
    strcat(b, me);
    strcat(b, " at ");
    strncat(b, server_name, 32 - strlen(b));
    ui_textn(0, 0, b, 40);
    ui_ink(F_WHITE, F_BLACK);
    row = 1;
    heading("Your games");
    n = 0;
    for (i = 0; i < ntab && row < 17; ++i) {
        if (!tabs[i].mine)
            continue;
        strcpy(b, "v ");
        strcat(b, tabs[i].who);
        entry(nice_name(tabs[i].type), b, tabs[i].open ? "waiting" : game_for(tabs[i].type) ? "playing"
                                                                                      : "(web)");
        ui_target(10 + 14 * 7 / 2, row * 8 + 4, 98, 4, ID_TAB + i);
        ++row;
        ++n;
    }
    if (!n) {
        ui_text(1, row * 8, "None yet: start one with New game.");
        ++row;
    }
    if (ninv) {
        heading("Invitations");
        for (i = 0; i < ninv && row < 19; ++i) {
            strcpy(b, "from ");
            strcat(b, invs[i].from);
            entry(nice_name(invs[i].type), b, 0);
            ui_button(26, row * 8, "Join", ID_ACC + i);
            ui_button(33, row * 8, "No", ID_DEC + i);
            ++row;
        }
    }
    n = 0;
    for (i = 0; i < ntab; ++i)
        if (!tabs[i].mine)
            ++n;
    if (n && row < 18) {
        heading("Waiting for a player");
        for (i = 0; i < ntab && row < 20; ++i) {
            if (tabs[i].mine)
                continue;
            entry(nice_name(tabs[i].type), tabs[i].who, 0);
            ui_button(32, row * 8, "Join", ID_TAB + i);
            ++row;
        }
    }
    ui_button(0, BUTTON_Y, "New game", B_NEW);
    ui_button(11, BUTTON_Y, "Refresh", B_REFRESH);
    ui_button(21, BUTTON_Y, "Display", B_DISPLAY);
    ui_button(31, BUTTON_Y, "Sign out", B_OUT);
#if !MACHINE_SERIAL
    ui_status(K6 "Return choose, " OA "Q quit");
#else
    ui_status(K6 "Return choose, " OA "C console, " OA "Q quit");
#endif
    ui_point(0);
}
#pragma bss-name (pop)

#pragma bss-name (push, "LOBBY")
/* A column of buttons to choose from; returns the index chosen, or 255 for Back or Escape. */
static unsigned char choose(const char *title, const char *const *items, unsigned char n)
{
    unsigned char i, k, wid = 0;
    static char pad[20];
    ui_cls();
    ui_ink(F_BLACK, F_WHITE);
    ui_textn(0, 0, title, 40);
    ui_ink(F_WHITE, F_BLACK);
    /* the buttons all as wide as the widest, so that they line up and the arrows go straight down */
    for (i = 0; i < n; ++i)
        if (strlen(items[i]) > wid)
            wid = strlen(items[i]);
    if (wid > 18)
        wid = 18;
    for (i = 0; i < n; ++i) {
        strncpy(pad, items[i], 18);
        pad[18] = 0;
        while (strlen(pad) < wid)
            strcat(pad, " ");
        ui_button(2 + (i >= 10 ? 20 : 0), 20 + (i % 10) * 14, pad, i);
    }
    ui_button(0, BUTTON_Y, "Back", B_BACK);
    ui_status(K6 "Arrows, " K6 "Return: choose. " K3 "Esc: back.");
    ui_point(0);
    for (;;) {
        k = ui_poll();
        if (k == K_ESC)
            return 255;
        if (k == U_CLICK)
            return ui_clicked == B_BACK ? 255 : ui_clicked;
    }
}

static const char *items[MAXG];
static unsigned char item_gam[MAXG];

/* New game: the game, its rules, the number of players, the opponent. Returns the table, or 0. */
static unsigned int new_game(void)
{
    static char invitee[14];
    static char b[20];
    unsigned char i, j, n, g, players, k, bots;
    unsigned int id;
    const struct gameinfo *gm;
again:
    n = 0;
    for (i = 0; i < ngam; ++i) {
        gm = game_for(gams[i].type);
        if (!gm)
            continue;
        for (j = 0; j < n; ++j)
            if (strcmp(gams[item_gam[j]].type, gams[i].type) == 0)
                break;
        if (j < n)
            continue;
        items[n] = gnames + gams[i].name;
        item_gam[n++] = i;
    }
    if (!n) {
        ui_status("This parlor has no game for this program.");
        return 0;
    }
    k = choose("New game: which game?", items, n);
    if (k == 255)
        return 0;
    g = item_gam[k];
    /* Its rules, when there are several. */
    n = 0;
    for (i = 0; i < ngam; ++i)
        if (strcmp(gams[i].type, gams[g].type) == 0) {
            items[n] = gams[i].rules;
            item_gam[n++] = i;
        }
    if (n > 1) {
        k = choose("Which rules?", items, n);
        if (k == 255)
            goto again;
        g = item_gam[k];
    }
    players = gams[g].min;
    if (gams[g].max > gams[g].min) {
        static const char *const counts[] = {"2 players", "3 players", "4 players", "5 players",
                                             "6 players", "7 players", "8 players"};
        k = choose("How many players?", counts + (gams[g].min - 2), gams[g].max - gams[g].min + 1);
        if (k == 255)
            goto again;
        players = gams[g].min + k;
    }
    {
        static const char *const who[] = {"Play the bot", "Wait for anyone", "Invite a player"};
        k = choose("Against whom?", who, 3);
    }
    if (k == 255)
        goto again;
    invitee[0] = 0;
    if (k == 2) {
        ui_cls();
        ui_text(1, 40, "The player's name:");
        if (!ui_input(1, 52, 14, invitee, 12, 0) || !invitee[0])
            goto again;
    }
    ui_cls();
    ui_status("Making the table...");
    q_set("NEW");
    q_add(gams[g].type);
    q_add(gams[g].rules);
    if (players != gams[g].min) {
        q_add("players=");
        q_cat(utoa10(players));
    }
    if (net_req() != R_OK) {
        if (net_unsure)
            return 0;                   /* (the list, read again, shows the table if it was made) */
        ui_status(r_final ? err_text(r_final) : "No answer.");
        ui_wait_key();
        return 0;
    }
    id = num(r_final + 3);
    q_set("JON");
    q_num(id);
    net_req();
    if (k == 0) {
        for (bots = 1; bots < players; ++bots) {
            q_set("INV");
            q_num(id);
            strcpy(b, "bot");
            if (bots > 1)
                strcat(b, utoa10(bots - 1));
            q_add(b);
            if (net_req() != R_OK && !net_unsure) {
                ui_status(r_final ? err_text(r_final) : "No answer.");
                ui_wait_key();
                break;
            }
        }
    } else if (k == 2) {
        q_set("INV");
        q_num(id);
        q_add(invitee);
        if (net_req() != R_OK && !net_unsure) {
            ui_status(r_final ? err_text(r_final) : "No answer.");
            ui_wait_key();
        }
    }
    return id;
}
#pragma bss-name (pop)

/* The lobby. Returns 1 for the console, 0 to sign out. */
unsigned char lobby(void)
{
    unsigned char k, i;
    unsigned int last, id;
    load_gam();
refresh:
    if (net_lost)
        return 0;
    load_lists();
    draw_lobby();
    last = ticks;
    for (;;) {
        k = ui_poll();
        if (net_lost)
            return 0;
        if (k == (U_OA | 'C'))
            return 1;
        if (k == (U_OA | 'Q'))
            return 2;
        if (k == K_ESC)
            continue;
        if (k == (U_OA | 'D')) {
            k = U_CLICK;
            ui_clicked = B_DISPLAY;
        }
        if (k == U_CLICK) {
            k = ui_clicked;
            if (k == B_OUT)
                return 0;
            if (k == B_REFRESH)
                goto refresh;
            if (k == B_DISPLAY) {
                mono ^= 1;
                q_set("PRF SET a2disp");
                q_add(mono ? "mono" : "colour");
                net_req();
                goto refresh;
            }
            if (k == B_NEW) {
                id = new_game();
                if (id && (i = table_play(id)) != 0)
                    return i;
                goto refresh;
            }
            if (k >= ID_DEC && k < ID_DEC + MAXI) {
                i = k - ID_DEC;
                q_set("UNV");
                q_num(invs[i].id);
                q_add(me);
                net_req();
                goto refresh;
            }
            if (k >= ID_ACC && k < ID_ACC + MAXI) {
                i = k - ID_ACC;
                q_set("JON");
                q_num(invs[i].id);
                if (net_req() != R_OK && !net_unsure) {   /* (unsure: the table's view says) */
                    ui_status(r_final ? err_text(r_final) : "No answer.");
                    continue;
                }
                if ((i = table_play(invs[i].id)) != 0)
                    return i;
                goto refresh;
            }
            if (k < MAXTAB) {
                if (!tabs[k].mine) {
                    q_set("JON");
                    q_num(tabs[k].id);
                    if (net_req() != R_OK && !net_unsure) {
                        ui_status(r_final ? err_text(r_final) : "No answer.");
                        continue;
                    }
                }
                if ((i = table_play(tabs[k].id)) != 0)
                    return i;
                goto refresh;
            }
        }
        if (ticks - last > 1800)        /* every 30 seconds */
            goto refresh;
    }
}
