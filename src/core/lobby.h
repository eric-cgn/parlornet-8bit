/* ParlorNet: what the lobby (lobby.c) shares with its New game overlay (newgame.c): the parlor's games, as
   GAM lists them, one entry for each game and rules. */
#ifndef LOBBY_H
#define LOBBY_H

struct gam {
    char type[16];
    char rules[10];
    unsigned char name;                 /* its name in gnames: one for each game, whatever its rules */
    unsigned char min, max;
};
#define MAXG 20
extern struct gam gams[MAXG];
extern unsigned char ngam;
extern char gnames[160];                /* the games' names, each ended by a 0 */

#endif
