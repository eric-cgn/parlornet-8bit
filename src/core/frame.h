/* ParlorNet: what the lobby and the tables share above the screen kit (frame.c): the command line, the
   dialogs, and pages of text drawn over the screen. The words and the keys are those of docs/design.md in
   the ParlorNet repository. */
#ifndef FRAME_H
#define FRAME_H

/* On the Apple IIe these are in the language card's bank 1 (ovl.h, OVL_FRAME), and every call goes through
   bank1_call (sys.s), which switches bank 1 in for the call and back after. So they must not be handed
   anything in $D000-$DFFF of the overlay in bank 2: a game's words for a dialog go in dlg_words first. */
#ifdef BANK1
void bank1_call(void);
#pragma wrapped-call (push, bank1_call, 0)
#endif

/* A dialog over the screen: its words, wrapped in a box, and two buttons. A confirmation has Yes and No and
   starts on No; an offer (D_OFFER) has Accept and Decline and starts on Decline, or on Accept with
   D_FIRST. Y and N answer it, the arrows and Tab move between the buttons, Return or Space presses the one
   marked, and a click presses either. Esc (or the second button of a joystick or a mouse) answers No to a
   confirmation and puts an offer away unanswered. Returns D_YES for Yes or Accept, D_NO for No or Decline,
   D_AWAY for an offer put away. The caller draws its screen again afterwards. */
#define D_OFFER 1
#define D_FIRST 2
#define D_NO 0
#define D_YES 1
#define D_AWAY 2
unsigned char __fastcall__ dialog(const char *words, unsigned char how);
extern char dlg_words[128];             /* (in the program, table.c) a dialog's words, made by a game */

/* Pages of text over the screen, from title row to message line: the lines, each wrapped at the screen's
   width, a line "\f" starting the next page. The title row says title, and ", page 1 of 2" when there are
   more. Space or Right turns the page, Left turns back, and Esc or Return closes; the buttons along the
   bottom do the same for a pointer. The caller draws its screen again afterwards. */
void __fastcall__ pages(const char *title, char **lines, unsigned char n);

/* The command line: "/" typed on the message line, where every key types until Return sends the line or Esc
   clears it. lit draws what shows keys (the buttons and the status line), unlit while the line is open
   (ui_unlit). table is where it was typed, or 0 in the lobby. cmd_send runs a line as if it had been typed:
   CMD [table] /line, or the program's own screen for /help, /leave and /tutorial. A reply of one line of up
   to 80 characters goes into cmd_said for the message line and the status line; a longer one is shown as
   pages. A confirmation (DLG) opens its
   dialog, and Yes sends the confirmed form. The flags returned say what happened. */
#define CMD_MAX 60
#define C_SAID 1                        /* cmd_said has a line for the message line */
#define C_REDRAW 2                      /* the screen was drawn over */
#define C_SENT 4                        /* a command went to the parlor: what it did comes as events */
#define C_HELP 8                        /* /help */
#define C_LEAVE 16                      /* /leave */
#define C_TUTORIAL 32                   /* /tutorial */
#define C_OUT 64                        /* signed out (/quit, confirmed) */
#define C_CLEARED 128                   /* Esc cleared the line */
extern char cmd_said[81];
void cmd_show(void);                    /* cmd_said on the message line, and its second line on the status line */
unsigned char __fastcall__ cmd_line(unsigned int table, void (*lit)(void));
unsigned char __fastcall__ cmd_send(unsigned int table, const char *line);

#ifdef BANK1
#pragma wrapped-call (pop)
#endif

#endif
