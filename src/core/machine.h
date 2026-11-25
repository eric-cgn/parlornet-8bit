/* ParlorNet: the machine interface. Everything the core (src/core) asks of a machine is declared here, and the
   core includes nothing else of a machine's. A machine's folder (src/apple2, src/c64, src/template) gives
   machdefs.h, with the constants below that differ from machine to machine, and implements the functions and
   variables in its own files, in C or assembly. PORTING.md describes the whole; src/template is a machine with
   every one of these stubbed, to start a port from.

   Timing: the core is single-threaded and polls. It calls kbd_get, tick_poll and ser_get in its loops and
   expects them to return at once; anything that must keep time (receiving bytes, the 60 Hz tick, a progress
   bar) is the machine's interrupt work. Memory: the core's code and data are linked by the machine's linker
   configuration (see Overlays and memory), and it needs about 32K for itself and 10K for the largest overlay.

   Notes on each group say what the Apple II (IIe and IIgs) and the C64 do. */
#ifndef MACHINE_H
#define MACHINE_H

/* machdefs.h, from the machine's folder, defines:

   Capabilities
     MACHINE_SERIAL   1: the machine has a serial line, with the console (C on the start screen) and the
                      dialer; 0: it connects only through its network (the IIgs, through Marinetti).
     MACHINE_LOWBSS   defined: a table's move tokens and chat parts go in the LOWBSS segment (the IIgs, whose
                      program space is full; its low memory has no serial ring).
   Names and key legends
     MACHINE_NAME     "the Apple II", "the C64": in "This game is not on ... yet."
     OA_NAME          the command key as the text names it: "Open-Apple", "Commodore".
     OA, CA           the font's characters for the command key ("\x7F") and a second one ("\x80").
     K1, K3, K6       in a key legend on the graphics screen, the next 1, 3 or 6 characters are a key, drawn
                      in the keys' color or in bold.
   Key codes (kbd_get)
     K_LEFT 8, K_RIGHT 21, K_UP 11, K_DOWN 10, K_RET 13, K_ESC 27, K_TAB 9, K_DEL 127, on every machine so far.
   The table screen's frame, in lines of the graphics screen
     GAME_TOP         the game's area starts here, below the title row.
     MSG_Y            the message line (the bot's greeting, chat, a game's messages).
     STATUS_Y         the status line.
     BUTTON_Y         the row of buttons (Lobby, Resign, Chat, Keys).
   Overlays
     OVL_ADDR         where an overlay is loaded and runs ($D000 in the Apple's language card, $9900 on the
                      C64). */
#include "machdefs.h"

/* ---------------------------------------------------------------------------------------------------
   The link: the serial line, or a network connection behind the same calls. */

/* The next byte from the line, or -1 when none has come. Called in tight loops (the reply reader, the
   console), so it only looks at a ring the machine fills by interrupt: the Super Serial Card's 6551 on the
   Apple II (or polled when its interrupt is off), CIA 2's NMI on the C64's user port, the SwiftLink's 6551. */
int ser_get(void);
/* Send a byte. May wait while the machine's send buffer is full; returns as soon as the byte is queued. */
void __fastcall__ ser_put(unsigned char c);
/* Forget what has come in and not been read (before a request, after a modem's echo). */
void ser_drop(void);
/* Open the line as the settings say (after start-up and after the settings screen). */
void serial_open(void);
/* The line as the start screen and console describe it: "9600 8N1", "2400 user port". */
const char *line_desc(void);

/* The reply reader (net.c's net_reply): read lines from ser_get into sr_buf, at most sr_max bytes, each ended
   by a 0, until a final line (OK, ERR, ASK, or a modem's NO CARRIER) has ended, or with sr_any any line, or
   until sr_ticks ticks pass with no byte (sr_gap ticks once part of the reply has come). Returns the bytes
   stored; sr_final is the final line's first letter (O, E, A, N) or 0. It must keep up with the line at its full speed, so on the 6502 machines it is
   assembly shared by both (src/common/6502/reply.inc, which each sys.s includes); a machine on another
   processor writes its own. */
unsigned int ser_reply(void);
extern char *sr_buf;
extern unsigned int sr_max, sr_ticks;
extern unsigned int sr_gap;             /* ticks without a byte that end a reply once part of it has come */
extern unsigned char sr_any, sr_final;

/* A network connection (the Apple's Uthernet II, the IIgs's Marinetti; none on the C64 yet). While eth_on is
   1 the machine's ser_get, ser_put and ser_drop go to it, so nothing above them changes. cfg_conn is the
   setting that chooses it (0 the serial line, 1 the network) on a machine with both; with MACHINE_SERIAL 0
   the network is always used. eth_connect (the machine's signin_net.c, in the sign-in overlay) looks up the
   parlor and connects: 1 when connected, 0 with the reason in q (net.h). eth_close ends the connection. */
extern unsigned char eth_on;
extern unsigned char cfg_conn;
unsigned char eth_connect(void);
void eth_close(void);

/* ---------------------------------------------------------------------------------------------------
   Input */

/* A key, or 0 when none is waiting: ASCII, with the K_ codes above for the arrows, Return, Esc, Tab and
   Delete. The C64 reads its keyboard 60 times a second into a queue; the Apple II reads its keyboard latch. */
unsigned char kbd_get(void);
/* For the last key from kbd_get: bit 0 set when the command key (Open-Apple, the Commodore key) was held,
   bit 1 for a second one (Closed-Apple). */
unsigned char kbd_mods(void);

/* ---------------------------------------------------------------------------------------------------
   The text screen: 40 columns by 24 rows, for the console, the sign-in and the dialer. */

void txt_mode(void);                    /* show the text screen */
void gr_mode(void);                     /* show the graphics screen */
void txt_clear(void);                   /* clear rows txt_top to txt_bot - 1, the cursor to the top */
void __fastcall__ txt_goto(unsigned char x, unsigned char y);
/* A character at the cursor: printable ASCII, OA and CA, 13 (a new line, scrolling the window at its foot),
   8 (back a column). Inverse while txt_inv is 1. */
void __fastcall__ txt_putc(char c);
void __fastcall__ txt_puts(const char *s);
void txt_clreol(void);                  /* clear from the cursor to the end of its row */
void __fastcall__ txt_clrrow(unsigned char row);
void __fastcall__ txt_cursor(unsigned char code);  /* show code at the cursor as the cursor (0: what was there) */
unsigned char txt_peek(void);           /* the character at the cursor */
extern unsigned char txt_x, txt_y, txt_inv, txt_top, txt_bot;
/* 1 when OA prints as one character on the text screen; 0 on an Apple IIe without MouseText, where it is
   "OA-" (three columns), which title_bar counts. */
extern unsigned char txt_mt;

/* ---------------------------------------------------------------------------------------------------
   Drawing: the graphics screen kit, for the lobby, the tables and the games' frame. Positions are text
   columns (0 to 39) and lines (0 to 191 or 199); a pixel position (ui_box) is in the machine's pixels.
   Text is in the program's 7 or 8 by 8 font. src/common/ui_targets.c has the variables, the targets and
   ui_wait_key for every machine, and src/common/ui_hgr.c ui_text, ui_textn, ui_box and ui_button_draw for a
   machine with the Apple's Hi-Res primitives (the Apple II and the C64); the machine writes the rest. */

/* Fills: a color on a color screen; F_GREY, F_LIGHT, F_DARK and F_HATCH are grays or dithers, which
   monochrome uses. */
enum { F_BLACK, F_WHITE, F_GREEN, F_VIOLET, F_ORANGE, F_BLUE, F_GREY, F_LIGHT, F_DARK, F_HATCH };

extern unsigned char mono;              /* 1: the monochrome look (D on the start screen, OA-D at a table) */

void __fastcall__ ui_fill(unsigned char f);                     /* the fill for ui_rect */
void __fastcall__ ui_ink(unsigned char fg, unsigned char bg);   /* text colors (fills) */
void ui_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h, unsigned char f);
void ui_text(unsigned char x, unsigned char y, const char *s);
void ui_textn(unsigned char x, unsigned char y, const char *s, unsigned char w); /* padded or cut to w */
void ui_box(unsigned int px, unsigned char y, unsigned int w, unsigned char h);  /* an outline, in pixels */
void ui_button(unsigned char x, unsigned char y, const char *s, unsigned char id);   /* and its target */
void ui_button_draw(unsigned char x, unsigned char y, const char *s);  /* without a target */
void ui_cls(void);                     /* clear the screen and the targets */
void ui_status(const char *s);         /* the status line (STATUS_Y) */
void ui_msg(const char *s);            /* the message line (MSG_Y) */

/* Targets: what the pointer can rest on and click. A target has its center (pixel x / 2, line y), half its
   width and height, and an id the screen chose (a game's square, a button). The arrows (and a joystick) move
   the pointer to the nearest target that way; Return (or a button) clicks it. */
#define MAXT 250
extern unsigned char t_n, t_cur;
extern unsigned char t_x[MAXT], t_y[MAXT], t_hw[MAXT], t_hh[MAXT], t_id[MAXT];
void ui_targets_clear(void);
unsigned char ui_target(unsigned char x2, unsigned char y, unsigned char hw, unsigned char hh,
                        unsigned char id);
unsigned char ui_find(unsigned char id);   /* the index of the target with this id, or 255 */
/* (src/common/ui_targets.c) the target nearest the current one in the direction of an arrow key, or 255, for
   the machine's ui_poll. */
unsigned char __fastcall__ ui_nearest(unsigned char key);
void __fastcall__ ui_point(unsigned char i);    /* move the pointer to target i */

/* The pointer: a drawn arrow on the Apple II, a sprite on the C64. Hide it before drawing under it, show it
   after (on the C64 these do little, since a sprite is not in the bitmap). */
void ui_hide(void);
void ui_show(void);

/* Input on the graphics screen: a key (the arrows move the pointer and are not returned), U_CLICK when a
   target is clicked (ui_clicked is its id), or 0 when nothing happened. A key with the command key comes
   with U_OA set (U_OA | key). ui_poll also keeps the pointer and the connected time (clk_show) up to date. */
#define U_CLICK 0xFF
#define U_OA    0x80
extern unsigned char ui_clicked;
extern unsigned char ui_retkey;          /* 1: Return is a key for the screen, not a click */
unsigned char ui_poll(void);
/* The mouse (the AppleMouse II card, the C64's 1351 when the settings say so): found at start-up by the
   machine itself; ui_hasmouse is 1 when there is one. */
void ui_mouse_init(void);
extern unsigned char ui_hasmouse;

/* Line editing at (x, y), w characters wide, into buf (at most max characters; shown as * when secret).
   Returns 1 on Return, 0 on Esc. */
unsigned char ui_input(unsigned char x, unsigned char y, unsigned char w, char *buf, unsigned char max,
                       unsigned char secret);

void ui_wait_key(void);                 /* until a key */

/* ---------------------------------------------------------------------------------------------------
   The games' drawing. Each game's rules are in src/core/games/NAME.c, which reads the view's lines into its
   own static variables and #includes "draw/NAME.c" from the machine's folder: the drawing is compiled into the
   same file, sees those variables, and goes into the game's overlay. A draw file provides the static
   functions the rules file names in its first comment (NAME_draw always, which draws the table, everything
   when g_full is 1 and otherwise only what changed; and for some games a few more, such as Backgammon's
   mark or Cannon Fire's cf_event), and makes the targets with the ids that comment gives, so that the
   lab's plans and the orchestrator click the same ids on every machine. It may use everything declared here,
   game.h's g_ variables and functions (the seats, whose turn, the legal moves m_tok and m_has), and its
   machine's own drawing code and pictures. It draws only in the game's area (GAME_TOP to MSG_Y) and the
   players' panels there. */

/* ---------------------------------------------------------------------------------------------------
   Storage and settings. The core keeps no files and has no calls here: the machine's setup overlay (setup.c)
   shows the settings, and reads and writes them (PARLOR.CFG under ProDOS, GS/OS and on the C64's disk) with
   a version byte first; a file of another version or size is left alone and the settings start as they
   always do. The settings the core itself keeps (cfg_echo, cfg_title, and the dialer's cfg_init, cfg_number
   and cfg_pulse) are in app.h, and the setup overlay reads and writes them with the machine's own. */

/* ---------------------------------------------------------------------------------------------------
   Overlays and memory. The sign-in (0), each game (1 to 10, in the catalog's order, games.c), the settings
   and start screen (OVL_SETUP) and the title screen (OVL_SPLASH) are overlays, loaded one at a time at
   OVL_ADDR (ovl.h has their headers). ovl_load returns 1 when the overlay is in memory (at once when it is
   already); 0 when it is not on the disk (a game not made for this machine). It may show a progress bar while
   it loads ("Loading Reversi..." and a bar on the C64's bottom row, from its IRQ), asks for the disk if it is
   not in the drive, and keeps the serial line's bytes safe meanwhile (the C64 holds the line with RTS while
   the KERNAL reads). The Apple II reads the self-booting disk's sectors itself or PARLOR.DAT under ProDOS;
   the C64 loads PN.<NAME> files through the KERNAL and unpacks them. */
unsigned char __fastcall__ ovl_load(unsigned char k);

/* Whether the line has been greeted (so the sign-in skips the opening and greeting), and what the sign-in
   should say on its way in: 1 that the player signed out, 2 that the line was lost. */
extern unsigned char connected;
extern unsigned char signin_note;

/* ---------------------------------------------------------------------------------------------------
   The progress bar. No call: a machine whose disk is slow shows the progress of a load itself, inside
   ovl_load and its start-up, as the C64 does from its interrupt (disk.s) while the KERNAL loads. The core
   says "Loading the game..." on the status line before a game's overlay is loaded. */

/* ---------------------------------------------------------------------------------------------------
   The clock. ticks counts 60 a second on every machine, from the 60 Hz interrupt (the C64's raster
   interrupt, with one more each fifth frame on PAL; the Apple II's VBL, or its own count, where tick_poll
   keeps it). The core calls tick_poll in its wait loops. */
extern unsigned int ticks;
void tick_poll(void);

/* The time connected to the parlor, shown as "0:12" or "1:05:33": the seconds the parlor gave (TIM, asked by
   clk_sync about every 32 seconds), counted on between by the ticks. On the 6502 machines this is assembly
   shared by both (src/common/6502/clock.inc and clockvars.inc). */
extern unsigned char clk_t[3];          /* seconds */
extern unsigned char clk_sub;           /* ticks left in this second */
extern unsigned char clk_shown;         /* clk_t[0] when last shown; 255 to show it again */
void __fastcall__ clk_set(const char *secs);   /* from decimal seconds */
const char *clk_text(void);             /* right-aligned in 8 characters (clk_buf) */
extern char clk_buf[9];
void clk_sync(void);                    /* TIM, when 32 seconds have turned over */
extern unsigned char clk_tim_off, clk_tim_at;
void clk_show(void);                    /* at the right end of the graphics title row, when it changed */

/* ---------------------------------------------------------------------------------------------------
   Start-up and shutdown */

/* Everything the machine needs before the title screen: its memory and interrupts, the screen, the settings
   kept on its disk (through the setup overlay), the mouse and the line (serial_open). */
void sys_init(void);
/* Quit from the start screen (Commodore-Q, Open-Apple-Q): back to ProDOS or the Finder, or the C64 started
   afresh. Does not return. */
void sys_quit(void);

#endif
