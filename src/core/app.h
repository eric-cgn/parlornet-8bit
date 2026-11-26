/* ParlorNet: what the parts of the core share (and the machine's setup overlay uses). The machine's own part is
   in machine.h, which comes first. */
#ifndef APP_H
#define APP_H

/* Settings (main.c; the line's own are the machine's) */
extern unsigned char cfg_echo;          /* 1: the console shows what is typed */
extern unsigned char cfg_title;         /* 1: the title screen at start-up (off only under ProDOS) */
#if MACHINE_SERIAL
/* The dialer (parlor.c): P sends the init string (ATZ as it starts; nothing when it is empty) and waits for
   OK, dials ATDT (or ATDP, Pulse dial) and the number, and sends the opening on CONNECT. An empty number: P sends the opening at
   once, for a direct line. */
#define DIAL_INIT_MAX 23
#define DIAL_NUM_MAX 35
extern char cfg_init[DIAL_INIT_MAX + 1];    /* "ATZ", "" or such as "AT&F" */
extern char cfg_number[DIAL_NUM_MAX + 1];   /* "parlornet.org:6501" */
extern unsigned char cfg_pulse;         /* 1: ATDP and the number; 0: ATDT */
extern unsigned char dial_now;          /* 1: P on the start screen (the console's Open-Apple-P: 0) */
extern unsigned char dialing;           /* 1 while the line is one the dialer dialed */
#endif
void settings(void);                    /* the settings screen (the setup overlay), then the line */

/* Text screen helpers (main.c) */
void title_bar(const char *left, const char *right);
void say(const char *s);                /* txt_puts */
void sayln(const char *s);
/* (src/common/setting_line.c, in the setup overlay) a setting on a text row: its key, what it is, its value */
void setting_line(unsigned char row, const char *key, const char *what, const char *value);

/* ParlorNet mode (parlor.c): returns 1 for the console, 0 for the start screen. */
unsigned char parlor(void);
extern char me[13];                     /* the signed-in name */
#define SERVER_NAME_MAX 40             /* a host name such as parlornet.org, and longer */
extern char server_name[SERVER_NAME_MAX + 1];   /* the greeting's name, always ended by a 0 */

/* The lobby (lobby.c) and a table (table.c). */
unsigned char lobby(void);              /* 0 sign out, 1 the console, 2 close the connection, 3 signed out */
unsigned char table_play(unsigned int id);  /* 0 the lobby, 1 the console, 2 close the connection, 3 signed out */

#endif
