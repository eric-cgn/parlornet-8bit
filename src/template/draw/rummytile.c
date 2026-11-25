/* ParlorNet's template machine: Rummy Tile's drawing (core/games/rummytile.c includes it, after its rules and
   their variables, which it reads). It must define, as static functions:
     rt_draw(void)
     redraw(void)
   and draw, in the game's area (GAME_TOP to MSG_Y): the table's sets as rows and the rack below; a set's
   places targets row * SETMAX + place, the rack's ID_RACK up, the buttons ID_DONE to ID_DOWN. The rules
   file's first comment is the contract; src/c64/draw/rummytile.c is a finished one. */

static void rt_draw(void)
{
}

static void redraw(void)
{
}
