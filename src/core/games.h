/* ParlorNet: the games this program plays. Each is an overlay (ovl.h) whose first
   bytes are its struct game. */
#ifndef GAMES_H
#define GAMES_H
#include "game.h"

struct gameinfo {
    const char *type;                   /* "reversi" */
    unsigned char ovl;                  /* its overlay */
};
extern const struct gameinfo catalog[];
const struct gameinfo *__fastcall__ game_for(const char *type);  /* "reversi/1" or "reversi"; 0 if none */
#define GAME ((const struct game *)OVL_ADDR)

#endif
