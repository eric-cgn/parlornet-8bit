/* ParlorNet on the Apple IIe and IIgs: the machine's own calls (sys.s, hgr.s, boot.s, line.c), beyond the machine
   interface (core/machine.h) and its constants (machdefs.h), for the Apple's own files and its drawing. */
#ifndef APPLE2_H
#define APPLE2_H

/* Super Serial Card */
void __fastcall__ ser_open(unsigned char slot, unsigned char control, unsigned char command);
void ser_close(void);
unsigned char ser_used(void);
extern unsigned char ser_flow;          /* 0 none, 1 RTS/CTS, 2 XON/XOFF */
extern unsigned char ser_lost;

/* The text screen: whether the machine has MouseText (txt_mt, machine.h). */
void txt_detect(void);

/* The line's settings (line.c): the Super Serial Card's slot, speed (baud_name), bits and parity
   (parity_name) and flow control; cfg_conn picks the Uthernet II instead (eth.h). */
extern unsigned char cfg_slot, cfg_baud, cfg_parity, cfg_flow;

/* The Apple joystick (joy.s): 1 when the settings turn it on */
extern unsigned char joy_on;

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
