/* ParlorNet on the Commodore 64: the bitmap (gfx.s), the sprites (sys.s, gfx.s) and the colors, for the
   screen kit (ui.c) and the games' drawing (draw/). */
#ifndef GFX_H
#define GFX_H

/* The C64's colors. */
enum { C_BLACK, C_WHITE, C_RED, C_CYAN, C_PURPLE, C_GREEN, C_BLUE, C_YELLOW,
       C_ORANGE, C_BROWN, C_LRED, C_DGREY, C_GREY, C_LGREEN, C_LBLUE, C_LGREY };

/* Drawing's parameters (as the Apple II's hgr.s has them): a cell column, a line, a width in cells (or pixels
   for lines), a height in lines; colors for set and clear pixels; a fill's byte; a picture. */
extern unsigned char hg_x, hg_y, hg_w, hg_h, hg_fg, hg_bg, hg_kfg, hg_kbg, hg_key, hg_pat, hg_xor, hg_bold;
extern const unsigned char *hg_src;
extern unsigned int hg_px;
void hgr_clear(void);                   /* all clear, the cells in hg_fg on hg_bg */
void hgr_color(void);                   /* the cells over lines hg_y.. hg_h, columns hg_x.. hg_w */
void hgr_rect(void);                    /* those lines filled with hg_pat, and colored */
void hgr_blit(void);                    /* the picture hg_src: width (bytes), height, rows */
void __fastcall__ hgr_char(char c);
void __fastcall__ hgr_text(const char *s);
void hgr_hline(void);                   /* pixels hg_px.. hg_w long at line hg_y */
void hgr_vline(void);
void hgr_plot(void);
unsigned char *hgr_line_addr(void);     /* line hg_y's byte in column hg_x */
extern const unsigned char hgr_rowlo[], hgr_rowhi[];

/* The VIC's border color. */
#define VIC_BORDER (*(unsigned char *)0xD020)

/* Sprites: position (x 0-511, y), shape (a block: 64 is $D000), color, multicolor bits, on bits. The
   interrupt sets them in the VIC each frame. A pixel at (x, y) on the bitmap is sprite position x + 24,
   y + 50. */
extern unsigned int spr_x[8];
extern unsigned char spr_y[8], spr_ptr[8], spr_col[8], spr_mc, spr_on;
void __fastcall__ spr_load(unsigned char block, const unsigned char *shape);
#define SPR_ARROW 64                    /* the pointer and its outline (ui.c) */
#define SPR_GAME 66                     /* the games' own, 66 up */

/* The 1351 mouse and the joystick, as the interrupt reads them (sys.s). */
extern unsigned char has_mouse, mouse_moved, mouse_btn, mouse_y, joy_at;
extern unsigned char mouse_on;          /* 1: read a 1351 in port 1 (the settings) */
extern unsigned int mouse_x;

/* Colors through the monochrome setting (ui.c): a C64 color, a fill's color, and text in two colors. */
unsigned char __fastcall__ ui_col(unsigned char c);
unsigned char __fastcall__ ui_fcol(unsigned char f);
void __fastcall__ ui_inkc(unsigned char fg, unsigned char bg);

void kbd_flush(void);
extern unsigned char txt_screen;        /* 1 while the text screen shows */

#endif
