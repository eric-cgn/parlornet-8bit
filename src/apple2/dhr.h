/* ParlorNet on the Apple IIe: the Double Hi-Res kit (dhr.s), for drawing beyond hgr.s's routines.

   d_place draws groups of 28 dots (7 color pixels, four bytes of the screen) a whole byte at a time: each
   line starts black and takes dl_n layers in turn, each a mask (4 bytes a line, as the screen's, or 0 for
   every dot) in a color, one for even lines and one for odd ones. d_column draws a column of places of 12
   lines, each of one of the kinds d_types was given, and keeps the first 8 kinds' lines once worked out. The
   masks are read with the language card's bank 1 in, where an overlay cannot be seen, so they must be in
   dl_buf. Colors are 0 to 15 (dhr.s lists them); in monochrome a color is a pattern of its dots. */
#ifndef DHR_H
#define DHR_H

extern unsigned char dl_x, dl_y, dl_w, dl_h, dl_n;
extern const unsigned char *dl_m[4];    /* each layer's mask */
extern unsigned char dl_ms[4];          /* 4: a mask line a screen line; 0: its first line on each */
extern unsigned char dl_ce[4], dl_co[4];/* its color on even and odd lines */
extern unsigned int dl_sx;              /* a gliding sprite's first dot (0 to 559) */
extern unsigned char dl_cn, dl_col[8];  /* d_column: how many places, and their kinds */
extern unsigned char dl_buf[384];       /* where the masks are */

void __fastcall__ dhr_call(unsigned char entry);
#define DK_PLACE 21                     /* d_place */
#define DK_GLIDE 24                     /* a sprite of dl_n (1 or 2) layers, dl_m and dl_ce (dl_co unused), */
#define DK_UNGLIDE 27                   /* dl_h lines (up to 12) at dot dl_sx, line dl_y; and put back */
#define DK_PXLINE 30                    /* dl_buf as 140 colors, a color pixel each, on dl_h lines from dl_y */
#define DK_TYPES 33                     /* up to 10 kinds of place, from dl_buf (dhr.s), for d_column */
#define DK_COLUMN 36                    /* dl_cn places of 12 lines down from line dl_y in group dl_x */
#define dhr_place() dhr_call(DK_PLACE)

/* A fill of ui.c's (ui_fill, ui_ink) that is the Double Hi-Res color c */
#define F_DHR(c) (16 + (c))

#endif
