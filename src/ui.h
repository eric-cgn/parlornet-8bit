/* ParlorNet for the Apple IIe: the Hi-Res screen kit. Text in the 7 by 8 font, fills, the pointer and
   the things it can click (targets), keys and the mouse. */
#ifndef UI_H
#define UI_H

/* Fills: a color on a color monitor; F_GREY and F_LIGHT are dithers for monochrome. */
enum { F_BLACK, F_WHITE, F_GREEN, F_VIOLET, F_ORANGE, F_BLUE, F_GREY, F_LIGHT, F_DARK, F_HATCH };

extern unsigned char mono;              /* 1: the monochrome look */

void __fastcall__ ui_fill(unsigned char f);
void __fastcall__ ui_ink(unsigned char fg, unsigned char bg);   /* text colors (fills) */
void ui_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h, unsigned char f);
void ui_text(unsigned char x, unsigned char y, const char *s);
void ui_textn(unsigned char x, unsigned char y, const char *s, unsigned char w); /* padded/cut to w */
void ui_box(unsigned int px, unsigned char y, unsigned int w, unsigned char h);  /* outline, pixels */
void ui_button(unsigned char x, unsigned char y, const char *s, unsigned char id);
void ui_button_draw(unsigned char x, unsigned char y, const char *s);  /* without a target */
void ui_cls(void);
void ui_status(const char *s);         /* the bottom line */
void ui_msg(const char *s);            /* the line above it (messages, chat) */

/* Targets: center (pixel x / 2, line y), half size, and an id the screen chose. */
#define MAXT 250
extern unsigned char t_n, t_cur;
extern unsigned char t_x[MAXT], t_y[MAXT], t_hw[MAXT], t_hh[MAXT], t_id[MAXT];
void ui_targets_clear(void);
unsigned char ui_target(unsigned char x2, unsigned char y, unsigned char hw, unsigned char hh,
                        unsigned char id);
unsigned char ui_find(unsigned char id);   /* index of the target with this id, or 255 */
void __fastcall__ ui_point(unsigned char i);    /* move the pointer to target i */

/* The pointer: hide it before drawing under it, show it after. */
void ui_hide(void);
void ui_show(void);

/* Input: a key (arrows move the pointer and are not returned), or U_CLICK when a target is clicked
   (ui_clicked is its id), or 0 when nothing happened. Option/Closed-Apple keys come with bit 7 set
   for Open-Apple (U_OA | key). */
#define U_CLICK 0xFF
#define U_OA    0x80
extern unsigned char ui_clicked;
extern unsigned char ui_retkey;          /* 1: Return is a key for the screen, not a click */
unsigned char ui_poll(void);
void ui_mouse_init(void);
extern unsigned char ui_hasmouse;

/* Line editing on the Hi-Res screen at (x, y), w characters wide. Returns 1 on Return, 0 on Escape. */
unsigned char ui_input(unsigned char x, unsigned char y, unsigned char w, char *buf, unsigned char max,
                       unsigned char secret);

void ui_wait_key(void);

#endif
