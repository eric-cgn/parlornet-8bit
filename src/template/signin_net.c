/* ParlorNet's template machine: the sign-in overlay's network connection (machine.h's eth_connect), linked
   with core/parlor.c. A machine with a network card or a network device (FujiNet, an Ethernet cartridge)
   looks up the parlor's name and opens a TCP connection to its port here, then sets eth_on so that the
   serial calls go to it. */
#include "machine.h"
#include "net.h"

unsigned char eth_connect(void)
{
    q_set("No network on this machine; it uses the serial line.");
    return 0;
}
