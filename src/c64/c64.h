/* ParlorNet on the Commodore 64: the machine's own calls (sys.s, serial.s, gfx.s, disk.s, line.c), beyond the
   machine interface (core/machine.h) and its constants (machdefs.h). A C128 runs it in 64 mode.

   Memory, with BASIC and the KERNAL switched out ($01 = $35) except while the KERNAL reads or writes the
   disk (disk.s):
     $0002-$008F  zero page (cc65's and ours); the KERNAL's above
     $0400-$05FF  the serial line's receive and send rings
     $0600-$07FF  cc65's C stack
     $0800-$14FF  the reply buffer (net.c), and the loader (PARLORNET) before it
     $1500-       the program (PN.MAIN), then its zeroed data, then the lobby and the tables (PN.PARLOR,
                 loaded before the first sign-in: ovl.c)
     OVL_ADDR-$600  a table's moves and chat (TABLE), or the lobby's lists (LOBBY)
     OVL_ADDR-$BFFF one overlay at a time: the sign-in, a game, the start screen or the title screen
     $C000-$C7FF  the character set: the program's font, then the same inverted (PN.CHARS, made at start)
     $C800-$CBFF  the text screen; $CC00-$CFFF the colors of the bitmap's cells; both end in sprite pointers
     $D000-$DFFF  the I/O; the RAM under it holds the sprites' shapes
     $E000-$FF3F  the bitmap (320 by 200), under the KERNAL ROM
     $FFFA-$FFFF  the 6502 vectors (ours) */
#ifndef C64_OWN_H
#define C64_OWN_H

/* The serial line (serial.s), the modem in the settings: Supra 2400 compatible, the user port bit by bit
   (300 to 2400 baud, the KERNAL's wiring), or SwiftLink compatible, a 6551 (300 to 38400): a
   SwiftLink or Turbo232 cartridge, or the modem of a Commodore 64 Ultimate or an Ultimate 64. */
#define SER_USER 0
#define SER_SWIFT 2
void __fastcall__ ser_open(unsigned char iface, unsigned char baud);   /* baud: an index of baud_name */
void ser_close(void);
unsigned char ser_used(void);
void ser_pause(void);                   /* around the KERNAL's disk calls: the line stops (RTS off) */
void ser_resume(void);
extern unsigned char ser_flow;          /* 0 none, 1 RTS/CTS */
extern unsigned char ser_lost;
extern unsigned char ser_iface;
extern unsigned int ser_acia;           /* the 6551's address: $DE00, $DF00 or $DF80 */

/* The line's settings (line.c): the modem (SER_*), the speed (baud_name), flow control, and the SwiftLink
   compatible modem's address (cfg_acia: acia_name, $DE00 as it starts). */
extern unsigned char cfg_slot, cfg_baud, cfg_flow, cfg_acia;
extern const char *const acia_name[];
extern const char *const baud_name[];

/* PAL or NTSC, from the raster at start-up. */
extern unsigned char pal;               /* 1 on a PAL machine (50 frames a second) */

/* The disk (disk.s) */
extern unsigned char k_device;          /* the drive: 8 up */
extern unsigned char k_bar;             /* 1: the next k_load shows a progress bar as it loads */
unsigned char __fastcall__ k_load(const char *name, void *addr);    /* 0 when loaded; 0 addr: its own */
void __fastcall__ k_unpack(void *dst);  /* the packed file just loaded, unpacked (tools/c64_pack.py) */
unsigned char __fastcall__ k_cmd(const char *cmd);
unsigned char __fastcall__ k_readfile(const char *name, unsigned char *buf, unsigned char max);
unsigned char __fastcall__ k_writefile(const char *name, const unsigned char *buf, unsigned char n);

/* The settings as kept in PARLOR.CFG (setup.c), and as last read or written (line.c). */
#define CFG_LEN 72
#define CFG_VER 4                       /* PARLOR.CFG's version (setup.c) */
extern unsigned char cfg_saved[CFG_LEN];
void cfg_load(void);                    /* the settings from PARLOR.CFG, if it has them (line.c) */
unsigned char __fastcall__ baud_ok(unsigned char iface, unsigned char b);
void __fastcall__ modem_set(unsigned char iface);   /* the modem, at the speed it starts with */

#endif
