/* ParlorNet for the Apple IIe: the games this program plays, and their overlays (the order of
   OVERLAYS in build.sh). */
#include <string.h>
#include "games.h"

const struct gameinfo catalog[] = {
    {"reversi", 1},
    {"backgammon", 2},
    {"gridwords", 3},
    {"checkers", 4},
    {"mancala", 5},
    {"fiveinarow", 6},
    {"dotsandboxes", 7},
    {"navalcombat", 8},
    {"cannonfire", 9},
    {"rummytile", 10},
    {0, 0}
};

const struct gameinfo *__fastcall__ game_for(const char *type)
{
    unsigned char i, n;
    const char *p = strchr(type, '/');
    n = p ? p - type : strlen(type);
    for (i = 0; catalog[i].type; ++i)
        if (strlen(catalog[i].type) == n && strncmp(catalog[i].type, type, n) == 0)
            return &catalog[i];
    return 0;
}
