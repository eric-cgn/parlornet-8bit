/* ParlorNet's template machine: Backgammon's drawing (core/games/backgammon.c includes it, after its rules
   and their variables, which it reads). It must define, as static functions:
     bg_draw(void)
     mark(unsigned char on)
   and draw, in the game's area (GAME_TOP to MSG_Y): the points, the bar, the dice and the cube from the rules
   file's variables; mark shows or takes away the chosen point and where it can go; the points are targets 1
   to 24, the bar ID_BAR, the tray ID_OFF, the buttons ID_A and ID_B. The other player's steps come queued
   (bg_event) for the drawing to glide one after another; four checkers and +n on a tall point. The rules
   file's first comment is the contract; src/c64/draw/backgammon.c is a finished one. */

static void bg_draw(void)
{
}

static void mark(unsigned char on)
{
    (void)on;
}
