/* ParlorNet on the Apple IIe: Double Hi-Res drawing (dhr.s), a study linked into the Backgammon overlay.
   Positions are color pixels (0 to 139) and lines; colors are 0 to 15. A mask is its width (up to 8) and
   height, then a byte a row, bit 7 its first pixel. */
#ifndef DHR_H
#define DHR_H

extern unsigned char dh_x, dh_y, dh_w, dh_h, dh_c, dh_xor;
extern unsigned char dh_c2;            /* the color for odd lines (a dither), or 0x80 for dh_c */
extern const unsigned char *dh_src;
extern unsigned char dh_tfg, dh_tbg;    /* text in these colors while below 0x80 */
extern unsigned char dhr_on;            /* (hgr.s) 0x80 while Double Hi-Res is on */
void dhr_enter(void);                   /* Double Hi-Res on and cleared; hgr.s's routines draw in it */
void dhr_mask(void);                    /* the mask dh_src at (dh_x, dh_y), its pixels in dh_c */
void dhr_save(void);                    /* keep what is under dh_w (up to 6) by dh_h (up to 11) at (dh_x, dh_y) */
void dhr_restore(void);                 /* and put it back */
/* dh_w groups of 7 pixels from group dh_x (pixel 7 * dh_x), dh_h lines from dh_y, drawn a byte at a time: on
   each line the row of dh_src (bits 7 to 1, the same in each group) in dh_c and the other pixels in dh_b, or
   left as they are with dh_b 0xFF; every pixel dh_b with dh_src 0. dh_c2 and dh_b2: odd lines' colors. */
extern unsigned char dh_b, dh_b2;
void dhr_cells(void);

#endif
