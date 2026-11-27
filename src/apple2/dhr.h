/* ParlorNet on the Apple IIe: the Double Hi-Res kit (dhr.s), for drawing beyond hgr.s's routines.

   d_place draws groups of 28 dots (7 color pixels, four bytes of the screen) a whole byte at a time: each
   line starts black and takes dl_n layers in turn, each a mask (4 bytes a line, as the screen's, or 0 for
   every dot) in a color, one for even lines and one for odd ones. d_column draws a column of places of 12
   lines, each of one of the kinds d_types was given, and keeps the first 8 kinds' lines once worked out.

   The kit runs in the auxiliary memory, with the parameters below copied there for each call. It reads the
   masks there too, so they are the games' Double Hi-Res pictures (art/apple2/NAME.dhr), which are linked
   into the auxiliary memory with it, or dhr_knum, which it makes itself; a C program passes their addresses
   and never reads them. Colors are 0 to 15 (dhr.s lists them); in monochrome a color is a pattern of its
   dots. */
#ifndef DHR_H
#define DHR_H

extern unsigned char dl_x, dl_y, dl_w, dl_h, dl_n;
extern const unsigned char *dl_m[4];    /* each layer's mask */
extern unsigned char dl_ms[4];          /* 4: a mask line a screen line; 0: its first line on each */
extern unsigned char dl_ce[4], dl_co[4];/* its color on even and odd lines */
extern unsigned int dl_sx;              /* a gliding sprite's first dot (0 to 559) */
extern unsigned char dl_cn, dl_col[8];  /* d_column: how many places, and their kinds */
extern unsigned char dl_sh, dl_v;       /* d_num, d_pips: where a single digit goes, and the number */
extern unsigned char dl_buf[384];       /* d_pxline's colors */
extern const unsigned char dhr_knum[];  /* (in the auxiliary memory) the mask d_num and d_pips make */

void __fastcall__ dhr_call(unsigned char entry);
#define DK_PLACE 7                      /* d_place */
#define DK_GLIDE 8                      /* a sprite of dl_n (1 or 2) layers, dl_m and dl_ce (dl_co unused), */
#define DK_UNGLIDE 9                    /* dl_h lines (up to 12) at dot dl_sx, line dl_y; and put back */
#define DK_PXLINE 10                    /* dl_buf as 140 colors, a color pixel each, on dl_h lines from dl_y */
#define DK_TYPES 11                     /* up to 10 kinds of place, at dl_m[0] (dhr.s), for d_column */
#define DK_COLUMN 12                    /* dl_cn places of 12 lines down from line dl_y in group dl_x */
#define DK_NUM 13                       /* dl_v in digits 3 color pixels by 5 lines, into dhr_knum (dhr.s) */
#define DK_PIPS 14                      /* a die's dl_v pips, into dhr_knum */
#define DK_VBAR 15                      /* hgr_vline hg_w pixels across, in color hg_p[0] */
#define dhr_place() dhr_call(DK_PLACE)

/* A fill of ui.c's (ui_fill, ui_ink) that is the Double Hi-Res color c */
#define F_DHR(c) (16 + (c))

#endif
