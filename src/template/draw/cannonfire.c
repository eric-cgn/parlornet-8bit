/* ParlorNet's template machine: Cannon Fire's drawing (core/games/cannonfire.c includes it, after its rules
   and their variables, which it reads). It must define, as static functions:
     cf_draw(void)
     cf_event(void)
     aim_shown(void)
   and draw, in the game's area (GAME_TOP to MSG_Y): the terrain (Z lines), the tanks (K), the wind (N);
   cf_event shows a shot's events as they come (the shell along its path, the burst, the crater); aim_shown
   shows a changed aim; the buttons ID_AL to ID_MR. The rules file's first comment is the contract;
   src/c64/draw/cannonfire.c is a finished one. */

static void cf_draw(void)
{
}

static void cf_event(void)
{
}

static void aim_shown(void)
{
}
