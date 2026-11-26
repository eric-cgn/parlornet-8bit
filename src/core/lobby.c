/* ParlorNet: the lobby. Your games (from LST: the tables where you sit), your
   invitations (IVL), the tables where someone waits for a player, and a new game against the bot, anyone,
   or a player you invite. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "app.h"
#include "games.h"
#include "frame.h"
#include "lobby.h"
#include "ovl.h"
#include "game.h"

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

#define MAXTAB 24
#define MAXI 6
struct gam gams[MAXG];
unsigned char ngam;
#pragma bss-name (push, "LOWBSS")
char gnames[160];                       /* (in low memory with the reply buffer) */
#pragma bss-name (pop)
/* The tables and invitations share the language card's RAM above the overlays with a table's moves and
   chat (LOBBY and TABLE in parlor.cfg), so they are read again after a table. */
#pragma bss-name (push, "LOBBY")
static struct tab tabs[MAXTAB];
static struct inv invs[MAXI];
#pragma bss-name (pop)
static unsigned char ntab;
static unsigned char ninv;
static unsigned char first;             /* the list's first line on the screen (draw_lobby; More pages on) */

#define ID_TAB 0                        /* + index: open a table of yours, or join one */
#define ID_ACC 40                       /* + index: an invitation, answered in its dialog */
#define B_NEW 200
#define B_REFRESH 201
#define B_DISPLAY 202
#define B_OUT 203
#define B_MORE 205

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

/* LOBBY's memory is a table's too and is not zeroed, so the strings copied into it are ended here. */
static void load_lists(void)
{
    unsigned char i;
    char *p;
    struct tab *t;
    struct inv *v;
    q_set("LST");
    ntab = 0;
    if (net_req() == R_OK)
        for (i = 0; i < r_n && ntab < MAXTAB; ++i) {
            words(r_line[i]);
            if (wn < 7 || !is(w[0], "TAB"))
                continue;
            t = tabs + ntab;
            t->id = num(w[1]);
            strncpy(t->type, w[2], 15);
            t->type[15] = 0;
            t->open = is(w[5], "open");
            p = strchr(w[4], '/');
            t->full = p != 0 && num(w[4]) >= num(p + 1);
            t->mine = names_have_me(w[6], t->who);
            if (t->mine || (t->open && !t->full))
                ++ntab;
        }
    q_set("IVL");
    ninv = 0;
    if (net_req() == R_OK)
        for (i = 0; i < r_n && ninv < MAXI; ++i) {
            words(r_line[i]);
            if (wn < 5 || !is(w[0], "IVT"))
                continue;
            v = invs + ninv;
            v->id = num(w[1]);
            strncpy(v->type, w[2], 15);
            v->type[15] = 0;
            strncpy(v->from, w[4], 12);
            v->from[12] = 0;
            ++ninv;
        }
    if (first >= ntab + ninv)
        first = 0;
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

/* The lobby's buttons and its status line, which show its keys: lit, or unlit while the command line is open
   (cmd_line), when they are drawn again without their targets. */
static unsigned char frame_new;         /* 1: the screen is new, and the buttons' targets are made */
static void lobby_frame(void)
{
    unsigned char n = t_n;
    ui_rect(0, BUTTON_Y, 40, 8, F_BLACK);
    ui_button(0, BUTTON_Y, "New game", B_NEW);
    ui_button(11, BUTTON_Y, "Refresh", B_REFRESH);
    ui_button(21, BUTTON_Y, "Display", B_DISPLAY);
    ui_button(31, BUTTON_Y, "Sign out", B_OUT);
    ui_status("Press " KEY_RET " to open it, or ? for help.");
    if (!frame_new)
        t_n = n;
    frame_new = 0;
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

/* The lobby's list is the player's own tables, then the invitations, then the tables waiting for a player.
   It is shown a screenful at a time, from its line number first, in the rows above LIST_END, where the
   More button is. More shows the next screenful, and on the last one goes back to the start. */
#define LIST_END 20
static unsigned char seen;              /* the lines of the list counted so far */
static unsigned char next;              /* the first line that did not fit, 0 when the rest all fit */
static const char *part;                /* the heading on the screen last */

/* Whether the list's next line, under heading h, is on the screen; the heading comes first when it is
   not there yet. */
static unsigned char __fastcall__ list_line(const char *h)
{
    if (seen++ < first)
        return 0;
    if (row + (part == h ? 1 : 3) > LIST_END) {
        if (!next)
            next = seen - 1;
        return 0;
    }
    if (part != h)
        heading(part = h);
    return 1;
}

static const char h_mine[] = "Your games";

static void draw_lobby(void)
{
    static char b[42];
    unsigned char i, n;
    struct tab *t;
    struct inv *v;
    gr_mode();
    ui_cls();
    ui_ink(F_BLACK, F_WHITE);
    /* "ParlorNet: name at server", in the 32 columns before the time signed in (clk_show): without
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
    seen = next = 0;
    part = 0;
    n = 0;
    for (i = 0, t = tabs; i < ntab; ++i, ++t) {
        if (!t->mine)
            continue;
        ++n;
        if (!list_line(h_mine))
            continue;
        strcpy(b, "v ");
        strcat(b, t->who);
        entry(nice_name(t->type), b, t->open ? "waiting" : game_for(t->type) ? "playing" : "(web)");
        ui_target(MID_X2, row * 8 + 4, MID_X2, 4, ID_TAB + i);
        ++row;
    }
    if (!n && !first) {
        heading(h_mine);
        ui_text(1, row * 8, "None yet: start one with New game.");
        ++row;
    }
    for (i = 0, v = invs; i < ninv; ++i, ++v) {
        if (!list_line("Invitations"))
            continue;
        strcpy(b, "from ");
        strcat(b, v->from);
        entry(nice_name(v->type), b, 0);
        ui_target(MID_X2, row * 8 + 4, MID_X2, 4, ID_ACC + i);
        ++row;
    }
    for (i = 0, t = tabs; i < ntab; ++i, ++t) {
        if (t->mine || !list_line("Waiting for a player"))
            continue;
        entry(nice_name(t->type), t->who, 0);
        ui_button(32, row * 8, "Join", ID_TAB + i);
        ++row;
    }
    if (first || next)
        ui_button(1, LIST_END * 8, next ? "More" : "Back to the start", B_MORE);
    frame_new = 1;
    lobby_frame();
    ui_point(0);
}
#pragma bss-name (pop)


/* The lobby. Returns 1 for the console, 0 to sign out, 2 to close the connection, 3 when signed out
   already (/quit). */
unsigned char lobby(void)
{
    unsigned char k, i, said = 0;
    unsigned int last, id;
    load_gam();
    first = 0;
refresh:
    if (net_lost)
        return 0;
    load_lists();
    draw_lobby();
    if (said)
        cmd_show();
    said = 0;
    last = ticks;
    for (;;) {
        k = ui_poll();
        if (net_lost)
            return 0;
        if (k == '?') {
            k = C_HELP;
            goto help;
        }
        if (k == '/') {
            k = cmd_line(0, lobby_frame);
help:
            if (k & C_HELP) {
                g_help(0);
                k = C_REDRAW;
            }
            if (k & C_OUT)
                return 3;
            said = k & C_SAID;
            if (k & (C_SENT | C_REDRAW))
                goto refresh;
            if (said)
                cmd_show();
            continue;
        }
        if (k == (U_OA | 'C'))
            return 1;
        if (k == (U_OA | 'Q')) {
            k = U_CLICK;
            ui_clicked = B_OUT;
        }
        if (k == K_ESC)
            continue;
        if (k == (U_OA | 'N')) {        /* (no game uses N) */
            k = U_CLICK;
            ui_clicked = B_NEW;
        }
        if (k == U_CLICK) {
            k = ui_clicked;
            if (k == B_OUT) {           /* the sign-out dialog first */
                k = cmd_send(0, "/quit");
                if (k & C_OUT)
                    return 3;
                draw_lobby();
                continue;
            }
            if (k == B_REFRESH)
                goto refresh;
            if (k == B_MORE) {
                first = next;
                draw_lobby();
                continue;
            }
            if (k == B_DISPLAY) {
                mono ^= 1;
                q_set("PRF SET a2disp");
                q_add(mono ? "mono" : "colour");
                net_req();
                goto refresh;
            }
            if (k == B_NEW) {
                id = ovl_load(OVL_NEWGAME) ? NEWGAME->run() : 0;
                if (id && (i = table_play(id)) != 0)
                    return i;
                goto refresh;
            }
            if (k >= ID_ACC && k < ID_ACC + MAXI) {
                /* the invitation's dialog: Accept sits down there, Decline declines it */
                i = k - ID_ACC;
                strcpy(dlg_words, invs[i].from);
                strcat(dlg_words, " invites you to play ");
                strcat(dlg_words, nice_name(invs[i].type));
                strcat(dlg_words, " at table ");
                strcat(dlg_words, utoa10(invs[i].id));
                strcat(dlg_words, ". If you accept, you sit down there.");
                k = dialog(dlg_words, D_OFFER | D_FIRST);
                if (k == D_AWAY) {
                    draw_lobby();
                    continue;
                }
                if (k == D_NO) {
                    q_set("UNV");
                    q_num(invs[i].id);
                    q_add(me);
                    net_req();
                    goto refresh;
                }
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
