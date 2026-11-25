/* ParlorNet: a setting on the text screen's row, for the settings screens (each machine's setup.c): its key
   in inverse, then what it is and its value. build.sh links it into the setup overlay with setup.c, so it
   takes no room in the resident program. */
#include "machine.h"
#include "app.h"

void setting_line(unsigned char row, const char *key, const char *what, const char *value)
{
    txt_goto(1, row);
    txt_inv = 1;
    say(key);
    txt_inv = 0;
    say(" ");
    say(what);
    say(value);
    txt_clreol();
}
