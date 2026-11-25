/* ParlorNet on the Commodore 64: the sign-in's network connection (machine.h's eth_connect), linked into the
   sign-in overlay with core/parlor.c. There is no network driver yet, so it only says so. */
#include "machine.h"
#include "c64.h"
#include "net.h"

unsigned char eth_connect(void)
{
    q_set("This program has no network card driver yet; it uses the serial line.");
    return 0;
}
