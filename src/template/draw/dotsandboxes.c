/* ParlorNet's template machine: Dots and Boxes's drawing (core/games/dotsandboxes.c includes it, after its
   rules and their variables, which it reads). It must define, as static functions:
     db_draw(void)
     sel_frame(void)
   and draw, in the game's area (GAME_TOP to MSG_Y): the dots, the lines (HL, VL) and the boxes taken in their
   player's letter; sel_frame shows the chosen dot (seld) or takes it away; each dot a target, its id row *
   (bw + 1) + column. The rules file's first comment is the contract; src/c64/draw/dotsandboxes.c is a
   finished one. */

static void db_draw(void)
{
}

static void sel_frame(void)
{
}
