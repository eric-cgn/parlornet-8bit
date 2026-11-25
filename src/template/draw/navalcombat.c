/* ParlorNet's template machine: Naval Combat's drawing (core/games/navalcombat.c includes it, after its rules
   and their variables, which it reads). It must define, as static functions:
     nc_draw(void)
     redraw(void)
   and draw, in the game's area (GAME_TOP to MSG_Y): the large grid (the player's waters while placing, the
   other's while shooting) and the small one; the large grid's squares targets row * 10 + column, the buttons
   ID_RANDOM to ID_READY. The rules file's first comment is the contract; src/c64/draw/navalcombat.c is a
   finished one. */

static void nc_draw(void)
{
}

static void redraw(void)
{
}
