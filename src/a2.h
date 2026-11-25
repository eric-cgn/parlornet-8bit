/* ParlorNet for the Apple IIe: the machine (sys.s, hgr.s, boot.s). */
#ifndef A2_H
#define A2_H

/* Super Serial Card */
void __fastcall__ ser_open(unsigned char slot, unsigned char control, unsigned char command);
void ser_close(void);
int ser_get(void);                      /* the next byte, or -1 */
void __fastcall__ ser_put(unsigned char c);
unsigned char ser_used(void);
void ser_drop(void);
extern unsigned char ser_flow;          /* 0 none, 1 RTS/CTS, 2 XON/XOFF */
extern unsigned char ser_lost;

/* 40-column text screen */
void txt_mode(void);
void gr_mode(void);
void txt_clear(void);                   /* the window, txt_top to txt_bot - 1 */
void __fastcall__ txt_goto(unsigned char x, unsigned char y);
void __fastcall__ txt_putc(char c);
void __fastcall__ txt_puts(const char *s);
void txt_clreol(void);
void __fastcall__ txt_clrrow(unsigned char row);
void __fastcall__ txt_cursor(unsigned char code);
unsigned char txt_peek(void);
extern unsigned char txt_x, txt_y, txt_inv, txt_top, txt_bot;

/* The Open-Apple and Closed-Apple keys in text: MouseText's apples on an enhanced IIe, a IIc or a IIgs
   (txt_mt), "OA-" and "CA-" on an older IIe (txt_putc); the Hi-Res font has them on every machine. */
#define OA "\x7F"
#define CA "\x80"
/* Key legends on the Hi-Res screen: the next 1, 3 or 6 characters are a key, drawn in bold in monochrome
   (as the Open-Apple glyph and the character after it are). K1 "D" done, K3 "Esc", K6 "Return". */
#define K1 "\x01"
#define K3 "\x03"
#define K6 "\x06"
void txt_detect(void);                  /* sets txt_mt: 1 when the machine has MouseText */
extern unsigned char txt_mt;

/* keyboard and time */
unsigned char kbd_get(void);            /* a key, or 0 */
unsigned char kbd_mods(void);           /* 1 Open-Apple, 2 Closed-Apple */
void tick_poll(void);
extern unsigned int ticks;              /* 60 a second, while tick_poll is called */

/* The time connected to the parlor (sys.s): the seconds the parlor gave (TIM: clk_sync), and the ticks
   since; the ticks alone with a parlor that has no TIM. */
extern unsigned char clk_t[3];          /* seconds */
extern unsigned char clk_sub;           /* ticks left in this second */
extern unsigned char clk_shown;         /* clk_t[0] when shown; 255 to show it again */
void __fastcall__ clk_set(const char *secs);  /* from decimal seconds */
const char *clk_text(void);             /* "0:12" or "1:05:33", right-aligned in 8 (clk_buf) */
extern char clk_buf[9];
void clk_sync(void);                    /* the connected time from the parlor (TIM) */
extern unsigned char clk_tim_off, clk_tim_at;   /* (clk_sync) */
void clk_show(void);                    /* on the Hi-Res title row's right end, when it changed */

#define K_LEFT  8
#define K_RIGHT 21
#define K_UP    11
#define K_DOWN  10
#define K_RET   13
#define K_ESC   27
#define K_TAB   9
#define K_DEL   127

/* AppleMouse II */
unsigned char mouse_find(void);
void __fastcall__ mouse_call(unsigned int call);     /* low byte: entry offset, high byte: A */
extern unsigned char mouse_slot;

/* Hi-Res page 1 */
extern unsigned char hg_x, hg_y, hg_w, hg_h, hg_xor, hg_bold, hg_key;
extern unsigned char hg_p[4], hg_fg[2], hg_bg[2];
extern const unsigned char *hg_src;
extern unsigned int hg_px;
void hgr_clear(void);
void hgr_rect(void);
void hgr_blit(void);
void __fastcall__ hgr_char(char c);
void __fastcall__ hgr_text(const char *s);
void hgr_vline(void);
void hgr_hline(void);
void hgr_ptr(void);

/* the sector reader in the boot block */
#define RD_LIN   (*(unsigned int *)0xF2)
#define RD_COUNT (*(unsigned char *)0xF4)
#define RD_PAGE  (*(unsigned char *)0xF5)
#define DISK_ID  (*(unsigned char *)0x080A)
unsigned char disk_read(void);          /* RD_* set; 0 on success */
void disk_motor_off(void);

#ifdef IIGS
/* On the IIgs the Hi-Res page is in bank $E0 (gs.s): a byte of it, by its address there, for C code that
   draws straight on the screen. */
unsigned char __fastcall__ gs_scr_get(const unsigned char *a);
void __fastcall__ gs_scr_put(unsigned char *a, unsigned char v);
#endif

#endif
