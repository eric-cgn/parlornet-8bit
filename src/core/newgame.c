/* ParlorNet: the lobby's New game (an overlay, ovl.h): the game, its rules, the number of players and the
   opponent, then the table, made and joined, with the bot or the player invited. The lobby's list of the
   parlor's games (lobby.h) is read when the lobby starts. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "app.h"
#include "games.h"
#include "lobby.h"
#include "ovl.h"

#define B_BACK 204

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
    const struct gam *gp;
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
    gp = gams + g;
    for (i = 0; i < ngam; ++i)
        if (strcmp(gams[i].type, gp->type) == 0) {
            items[n] = gams[i].rules;
            item_gam[n++] = i;
        }
    if (n > 1) {
        k = choose("Which rules?", items, n);
        if (k == 255)
            goto again;
        g = item_gam[k];
    }
    gp = gams + g;
    players = gp->min;
    if (gp->max > players) {
        static const char *const counts[] = {"2 players", "3 players", "4 players", "5 players",
                                             "6 players", "7 players", "8 players"};
        k = choose("How many players?", counts + (players - 2), gp->max - players + 1);
        if (k == 255)
            goto again;
        players += k;
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
    q_add(gp->type);
    q_add(gp->rules);
    if (players != gp->min) {
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

#pragma rodata-name (push, "OVHDR")
const struct newgame newgame_header = {new_game};
#pragma rodata-name (pop)
