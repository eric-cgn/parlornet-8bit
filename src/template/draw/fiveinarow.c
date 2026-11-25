/* ParlorNet's template machine: Five in a Row's drawing (core/games/fiveinarow.c includes it, after its rules
   and their variables, which it reads). It must define, as static functions:
     fr_draw(void)
   and draw, in the game's area (GAME_TOP to MSG_Y): cell[][]: seat 0 Black, seat 1 White, the last stone and
   the winning line marked; each point a target, its id row * size + column. The rules file's first comment is
   the contract; src/c64/draw/fiveinarow.c is a finished one. */

static void fr_draw(void)
{
}
