/* ParlorNet's template machine: Grid Words's drawing (core/games/gridwords.c includes it, after its rules and
   their variables, which it reads). It must define, as static functions:
     gw_draw(void)
     redraw(void)
     values_chart(void)
   and draw, in the game's area (GAME_TOP to MSG_Y): each square as want() says, the squares targets row * N +
   column, the rack's tiles ID_RACK up, the buttons ID_PLAY to ID_PASS; values_chart shows every letter's
   points (OA-V). The rules file's first comment is the contract; src/c64/draw/gridwords.c is a finished one.
   */

static void gw_draw(void)
{
}

static void redraw(void)
{
}

static void values_chart(void)
{
}
