/* ParlorNet's template machine: the title screen overlay (ovl.h's struct splash): PARLOR over NET, the
   version (version.h's PN_VERSION) and "Press a key"; the core waits for the key. */
#include "machine.h"
#include "ovl.h"
#include "version.h"

static void show(void)
{
    gr_mode();
    ui_cls();
    ui_text(14, 80, "ParlorNet " PN_VERSION);
    ui_text(15, 120, "Press a key");
}

#pragma rodata-name (push, "OVHDR")
const struct splash splash_header = {show};
#pragma rodata-name (pop)
