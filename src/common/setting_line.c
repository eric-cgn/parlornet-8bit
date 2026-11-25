/* ParlorNet: a setting on the text screen's row, for the settings screens (each machine's setup.c): its key
   in inverse, then what it is and its value, all on the one row. A value too long for the row is cut, with
   '>' as its last character to say so; the last column is left free, since a character there would take the
   text on to the next row. build.sh links it into the setup overlay with setup.c, so it takes no room in the
   resident program. */
#include <string.h>
#include "machine.h"
#include "app.h"

void setting_line(unsigned char row, const char *key, const char *what, const char *value)
{
    unsigned char room;
    txt_goto(1, row);
    txt_inv = 1;
    say(key);
    txt_inv = 0;
    say(" ");
    say(what);
    room = 39 - txt_x;
    if (strlen(value) > room) {
        while (--room)
            txt_putc(*value++);
        txt_putc('>');
    } else
        say(value);
    txt_clreol();
}
