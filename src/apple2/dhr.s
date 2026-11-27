; ParlorNet for the Apple IIe: the Double Hi-Res kit, everything that draws on the graphics screen.
;
; Double Hi-Res (DHR) shows 560 dots a line from two 8K pages at $2000, one in the auxiliary memory and one
; in the main memory. Along a line their bytes take turns, aux 0, main 0, aux 1, main 1 and so on, byte k of
; the line being byte k / 2 of its page; each byte gives 7 dots, bit 0 first (bit 7 is not shown). On a
; color monitor each 4 dots are one of 16 colors, 140 color pixels a line; in monochrome every dot shows.
; Seven color pixels are 28 dots, exactly four bytes, so the program lays its boards out in groups of 28
; dots, which are drawn a whole byte at a time (d_place, d_column). A text column is 14 dots, two bytes.
;
; The kit runs in the auxiliary memory at $0800, with the auxiliary memory switched in for reading and
; writing (RAMRD, RAMWRT). The loaders put it at $2800 and the program copies it there at start-up
; (dhr_install, hgr.s), with the games' Double Hi-Res masks after it (AUXART). Its entry is at its start,
; which hgr.s's kcall calls with the entry's number in X, after copying the parameters below ($0200 on)
; into the auxiliary memory. What else it reads from the program is in the zero page (hg_*, zka, zmono),
; which RAMRD does not move, or copied to the same address in the auxiliary memory at start-up (the font,
; the line tables, and the code that switches memory, MIRROR). The language card is not moved either, so
; it reads an overlay's Hi-Res pictures as they are. The kit polls the clock (ktick) often enough that no
; tick is missed, at most a few thousand cycles apart, and no more often.
;
; The auxiliary page is reached through 80STORE and HIRES, which kcall turns on: with both on, PAGE2
; ($C055) puts the auxiliary memory at $2000-$3FFF (and $0400-$07FF) and $C054 the main memory, without
; changing the page shown. Every routine here ends with the main memory in.
;
; Colors are 0 to 15: 0 black, 1 magenta, 2 dark blue, 3 purple, 4 dark green, 5 gray, 6 medium blue,
; 7 light blue, 8 brown, 9 orange, 10 gray, 11 pink, 12 green, 13 yellow, 14 aqua, 15 white. The dot at
; 4n + j shows the color's bit (j + 1) mod 4. The fills of ui.c (F_BLACK ...) are colors here too, a pair
; of them for even and odd lines (fce, fco), and a fill from 16 up is the color fill - 16.

        .setcpu "6502"
        .importzp       ptr1, ptr2, tmp1, tmp2, tmp3, tmp4
        .import         _hgr_ylo, _hgr_yhi, _font, ktick
        .importzp       _hg_x, _hg_y, _hg_w, _hg_h, _hg_p, _hg_src, _hg_px, _hg_fg, _hg_bg, _hg_xor
        .importzp       _hg_bold, _hg_key, zka, zmono, _hg_m
        .export         _dl_x, _dl_y, _dl_w, _dl_h, _dl_n, _dl_m, _dl_ms, _dl_ce, _dl_co, _dl_sx, _dl_buf
        .export         _dl_cn, _dl_col, _dl_sh, _dl_v
        .exportzp       PARMN

AUXON   = $C055                 ; PAGE2 with 80STORE: the auxiliary page
MAINON  = $C054

; ---------------------------------------------------------------------------------------------------
; The parameters (dhr.h), in main memory at $0200, where the kit sees them with either bank in.

        .segment "DHRPARM"
_dl_x:  .res    1               ; d_place: the first group (28 dots each, 0 to 19)
_dl_y:  .res    1               ; the first line
_dl_w:  .res    1               ; groups across
_dl_h:  .res    1               ; lines down
_dl_n:  .res    1               ; layers, 1 to 4
_dl_m:  .res    8               ; each layer's mask (4 bytes a line, as the screen's), or 0 for every dot
_dl_ms: .res    4               ; how far its mask moves on each line: 4, 0 for one line used for all, or
                                ; -4 to go up it
_dl_ce: .res    4               ; its color on even lines
_dl_co: .res    4               ; and on odd lines
_dl_sx: .res    2               ; d_glide: the sprite's first dot
_dl_cn: .res    1               ; d_column: how many places
_dl_col:.res    8               ; and their kinds
_dl_sh: .res    1               ; d_num: where a single digit goes
_dl_v:  .res    1               ; d_num, d_pips: the number
PARMN   = * - _dl_x                 ; (the parameters' bytes, which kcall gives the kit)
_dl_buf:.res    384             ; the masks, d_types's kinds, and for d_pxline a line of 140 colors

        .segment "ZEROPAGE"
mp:     .res    2               ; a mask's line
po:     .res    4               ; a line's 4 bytes for a group
pt:     .res    1
kp:     .res    2               ; the kinds of place (d_types)

; ---------------------------------------------------------------------------------------------------
        .segment "DHRKIT"

; The entry, which kcall (hgr.s) calls with the entry's number in X and its A in zka.
        lda     kth,x
        pha
        lda     ktl,x
        pha
        lda     zka
        rts
; (0 to 6 hgr.s's, from 7 on dhr.h's)
ktl:    .lobytes d_clear-1, d_rect-1, d_blit-1, d_char-1, d_vline-1, d_hline-1, d_ptr-1, d_place-1
        .lobytes d_glide-1, d_unglide-1, d_pxline-1, d_types-1, d_column-1, d_num-1, d_pips-1, d_vbar-1, d_hrow-1
kth:    .hibytes d_clear-1, d_rect-1, d_blit-1, d_char-1, d_vline-1, d_hline-1, d_ptr-1, d_place-1
        .hibytes d_glide-1, d_unglide-1, d_pxline-1, d_types-1, d_column-1, d_num-1, d_pips-1, d_vbar-1, d_hrow-1

; Each color's byte for a byte k of the line, by k mod 4: colpat + color * 4 + k mod 4.
colpat: .repeat 64, i
        .byte   ((((i / 4) >> ((7 * (i .mod 4) + 0 + 1) .mod 4)) & 1) << 0) | ((((i / 4) >> ((7 * (i .mod 4) + 1 + 1) .mod 4)) & 1) << 1) | ((((i / 4) >> ((7 * (i .mod 4) + 2 + 1) .mod 4)) & 1) << 2) | ((((i / 4) >> ((7 * (i .mod 4) + 3 + 1) .mod 4)) & 1) << 3) | ((((i / 4) >> ((7 * (i .mod 4) + 4 + 1) .mod 4)) & 1) << 4) | ((((i / 4) >> ((7 * (i .mod 4) + 5 + 1) .mod 4)) & 1) << 5) | ((((i / 4) >> ((7 * (i .mod 4) + 6 + 1) .mod 4)) & 1) << 6)
        .endrepeat
; A Hi-Res byte g (a font glyph's line, a Hi-Res picture's byte) doubled to 14 dots: the first 7 (the
; auxiliary byte) are lo16 + (g & 15), and the last 7 (the main one) hi16 + (g >> 3).
lo16:   .repeat 16, v
        .byte   (v & 1) * 3 | ((v >> 1) & 1) * 12 | ((v >> 2) & 1) * 48 | ((v >> 3) & 1) * 64
        .endrepeat
hi16:   .repeat 16, v
        .byte   (v & 1) * 1 | ((v >> 1) & 1) * 6 | ((v >> 2) & 1) * 24 | ((v >> 3) & 1) * 96
        .endrepeat
; The same for a font glyph's line g, whose bit 0 is never set, by g / 2.
fl64:   .repeat 64, v
        .byte   ((v >> 0) & 1) * 12 | ((v >> 1) & 1) * 48 | ((v >> 2) & 1) * 64
        .endrepeat
fh64:   .repeat 64, v
        .byte   ((v >> 2) & 1) * 1 | ((v >> 3) & 1) * 6 | ((v >> 4) & 1) * 24 | ((v >> 5) & 1) * 96
        .endrepeat
; ui.c's fills as colors on even and odd lines: black, white, green, violet (purple), orange, blue, gray
; (a checkerboard of dots), light, dark and hatch.
fce:    .byte   0, 15, 12, 3, 9, 6, 5, 15, 5, 15
fco:    .byte   0, 15, 12, 3, 9, 6, 10, 5, 0, 0
himask: .byte   $7F, $7E, $7C, $78, $70, $60, $40
lomask: .byte   $01, $03, $07, $0F, $1F, $3F, $7F

; Fill A as colors: even lines' in A, odd lines' in X.
fcol:   cmp     #16
        bcs     :+
        tax
        lda     fco,x
        pha
        lda     fce,x
        tay
        pla
        tax
        tya
        rts
:       and     #15
        tax
        rts

; ptr1 to the start of line X (the same offset in either page).
row:    lda     _hgr_ylo,x
        sta     ptr1
        lda     _hgr_yhi,x
        sta     ptr1+1
        rts

; ---------------------------------------------------------------------------------------------------
; hgr_clear: both pages black.
d_clear:
        sta     AUXON
        jsr     clr8
        sta     MAINON
clr8:   lda     #0
        tax
@lp:    .repeat 32, i
        sta     $2000 + i * $100, x
        .endrepeat
        inx
        beq     @done
        txa
        and     #$0F
        bne     :+
        jsr     ktick
:       lda     #0
        jmp     @lp
@done:  rts

; ---------------------------------------------------------------------------------------------------
; hgr_rect: hg_w text columns (14 dots) by hg_h lines at (hg_x, hg_y) in the fill hg_p[0].
        .segment "DHRKIT"
rp:     .res    8               ; even line: aux and main byte of an even column, then of an odd one;
                                ; then the odd line's
rcnt:   .res    1

d_rect: lda     _hg_p
        jsr     fcol
        stx     tmp4
        ldx     #0
        jsr     rpat
        lda     tmp4
        ldx     #4
        jsr     rpat
        lda     _hg_x
        cmp     #40
        bcs     rc_done
        clc
        adc     _hg_w
        cmp     #41
        bcc     :+
        lda     #40
:       sec
        sbc     _hg_x
        beq     rc_done
        sta     rcnt
        lda     _hg_h
        beq     rc_done
        sta     tmp3
        ldx     _hg_y
rc_line:
        cpx     #192
        bcs     rc_done
        stx     tmp2
        jsr     row
        txa
        and     #1
        asl     a
        asl     a
        tax
        lda     rp,x
        sta     tmp1
        lda     rp+2,x
        sta     tmp4
        sta     AUXON
        jsr     rc_fill
        sta     MAINON
        lda     rp+1,x
        sta     tmp1
        lda     rp+3,x
        sta     tmp4
        jsr     rc_fill
        jsr     ktick
        ldx     tmp2
        inx
        dec     tmp3
        bne     rc_line
rc_done:
        rts

; The bytes of one page for the columns from hg_x: tmp1 for even columns, tmp4 for odd ones. Keeps X.
rc_fill:
        txa
        pha
        ldy     _hg_x
        ldx     rcnt
        tya
        and     #1
        beq     @e
@o:     lda     tmp4
        sta     (ptr1),y
        iny
        dex
        beq     @d
@e:     lda     tmp1
        sta     (ptr1),y
        iny
        dex
        bne     @o
@d:     pla
        tax
        rts

; The color A's bytes for even and odd columns into rp + X: colpat + c * 4 + 0 to 3.
rpat:   asl     a
        asl     a
        tay
        lda     colpat,y
        sta     rp,x
        lda     colpat+1,y
        sta     rp+1,x
        lda     colpat+2,y
        sta     rp+2,x
        lda     colpat+3,y
        sta     rp+3,x
        rts

; ---------------------------------------------------------------------------------------------------
; hgr_char: the character in A in the text cell at (hg_x, hg_y), in the fill hg_fg[0] on hg_bg[0], each of
; the font's 7 pixels two dots; hg_x moves on. Keys are in bold as in hgr.s: the Open-Apple glyph and the
; character after it, and after a code 1 to 31 that many characters, which take no cell.
chf:    .res    1               ; a cell's auxiliary byte's k mod 4 (0 or 2)
chi:    .res    1               ; the cell's column's parity * 2
chb:    .res    1               ; not 0: bold
; For the colors last used (lfg, lbg), by the column's parity * 2 + the line's: foreground ^ background for
; the auxiliary byte (dxa) and the main one (dxm), and the background's bytes (cba, cbm).
dxa:    .res    4
dxm:    .res    4
cba:    .res    4
cbm:    .res    4
cfc:    .res    2               ; the foreground's colors on even and odd lines
cbc:    .res    2               ; the background's
lfg:    .byte   $FF
lbg:    .byte   $FF

d_char: pha
        jsr     ktick
        pla
        cmp     #$7F
        bne     :+
        ldx     #2
        stx     _hg_key
:       sec
        sbc     #32
        bcs     :+
        adc     #32
        sta     _hg_key
        rts
:       cmp     #97             ; (to $80: the Open-Apple and Closed-Apple glyphs)
        bcc     :+
        jmp     ch_done
:       ldx     #0
        stx     ptr2+1
        asl     a
        rol     ptr2+1
        asl     a
        rol     ptr2+1
        asl     a
        rol     ptr2+1
        clc
        adc     #<_font
        sta     ptr2
        lda     ptr2+1
        adc     #>_font
        sta     ptr2+1
        lda     _hg_x
        cmp     #40
        bcc     :+
        jmp     ch_done
:       and     #1
        asl     a
        sta     chi
        lda     _hg_fg
        cmp     lfg
        bne     :+
        lda     _hg_bg
        cmp     lbg
        beq     @ok
:       lda     _hg_fg
        sta     lfg
        jsr     fcol
        sta     cfc
        stx     cfc+1
        lda     _hg_bg
        sta     lbg
        jsr     fcol
        sta     cbc
        stx     cbc+1
        ldx     #3
@par:   txa
        and     #2
        sta     chf
        txa
        and     #1
        tay
        lda     cbc,y
        jsr     cpair
        sta     cba,x
        tya
        sta     cbm,x
        txa
        and     #1
        tay
        lda     cfc,y
        jsr     cpair
        eor     cba,x
        sta     dxa,x
        tya
        eor     cbm,x
        sta     dxm,x
        dex
        bpl     @par
@ok:    lda     _hg_bold
        ora     _hg_key
        sta     chb
        lda     #0
        sta     tmp4            ; the glyph's line
ch_line:
        lda     _hg_y
        clc
        adc     tmp4
        cmp     #192
        bcs     ch_next
        tax
        and     #1
        ora     chi
        sta     tmp3
        lda     _hgr_ylo,x
        sta     ptr1
        lda     _hgr_yhi,x
        sta     ptr1+1
        ldy     tmp4
        lda     (ptr2),y
        ldx     chb
        beq     :+
        sta     tmp1            ; bold: each pixel doubled to the right
        asl     a
        ora     tmp1
:       lsr     a               ; (the font's pixels are bits 1 to 5, and in bold to 6)
        tax
        ldy     tmp3
        lda     fh64,x
        and     dxm,y
        eor     cbm,y
        sta     tmp1
        lda     fl64,x
        and     dxa,y
        eor     cba,y
        ldy     _hg_x
        sta     AUXON
        sta     (ptr1),y
        sta     MAINON
        lda     tmp1
        sta     (ptr1),y
ch_next:
        inc     tmp4
        lda     tmp4
        cmp     #8
        bcc     ch_line
        inc     _hg_x
ch_done:
        lda     _hg_key
        beq     :+
        dec     _hg_key
:       rts

; Color A's bytes for a cell (chf): its auxiliary byte in A and its main byte in Y. Keeps X.
cpair:  asl     a
        asl     a
        ora     chf
        tay
        lda     colpat,y
        pha
        lda     colpat+1,y
        tay
        pla
        rts

; ---------------------------------------------------------------------------------------------------
; hgr_blit: a Hi-Res picture (its width in bytes, its height, then the rows) at text column hg_x, line
; hg_y, each pixel two dots (or flipped with hg_xor). In color a byte whose palette bit (bit 7) is clear
; goes one dot to the left, as the IIe shows such a byte half a Hi-Res pixel left of one with the bit
; set, so that its colors stay those of Hi-Res: green and violet rather than orange and blue. Its first
; dot, when set, goes into the last dot of the byte before.
bw:     .res    1
bh:     .res    1
bln:    .res    1
bla:    .res    1               ; a byte's dots, in the auxiliary byte and the main one
blm:    .res    1
bsp:    .res    1               ; $40: the dot that goes into the byte before
bhb:    .res    1               ; bit 7: the palette bit

d_blit: lda     _hg_src
        sta     ptr2
        lda     _hg_src+1
        sta     ptr2+1
        ldy     #0
        lda     (ptr2),y
        sta     bw
        iny
        lda     (ptr2),y
        sta     bh
        lda     ptr2
        clc
        adc     #2
        sta     ptr2
        bcc     :+
        inc     ptr2+1
:       ldx     _hg_y
bl_line:
        lda     bh
        beq     :+
        cpx     #192
        bcc     :++
:       jmp     bl_done
:       stx     bln
        jsr     row
        ldy     #0
bl_col: tya
        clc
        adc     _hg_x
        cmp     #40
        bcs     bl_skip
        sta     tmp1
        sty     tmp2
        lda     (ptr2),y
        sta     bhb
        and     #$7F
        sta     tmp3
        and     #15
        tax
        lda     lo16,x
        sta     bla
        lda     tmp3
        lsr     a
        lsr     a
        lsr     a
        tax
        lda     hi16,x
        sta     blm
        lda     #0
        sta     bsp
        bit     bhb
        bmi     bl_put2         ; the palette bit set, or monochrome: as it is
        lda     zmono
        bne     bl_put2
        lsr     blm             ; one dot to the left: the main byte's first dot to the end of the
        lda     bla             ; auxiliary one, and the auxiliary one's first to the byte before
        bcc     :+
        ora     #$80
:       lsr     a
        sta     bla
        bcc     bl_put2
        lda     #$40
        sta     bsp
bl_put2:sta     AUXON
        lda     bla
        jsr     bl_put
        sta     MAINON
        lda     blm
        jsr     bl_put
        bit     bhb
        bmi     bl_next
        lda     zmono
        bne     bl_next
        ldy     tmp1            ; the dot before, the last of the main byte to the left
        beq     bl_next
        dey
        lda     _hg_xor
        beq     :+
        lda     (ptr1),y
        eor     bsp
        sta     (ptr1),y
        jmp     bl_next
:       lda     (ptr1),y        ; set, never cleared, so that the byte before keeps its own last dot
        ora     bsp
        sta     (ptr1),y
bl_next:ldy     tmp2
bl_skip:
        iny
        cpy     bw
        bcs     :+
        jmp     bl_col
:
        lda     ptr2
        clc
        adc     bw
        sta     ptr2
        bcc     :+
        inc     ptr2+1
:       jsr     ktick
        ldx     bln
        inx
        dec     bh
        jmp     bl_line
bl_done:
        jmp     ktick
bl_put: ldy     _hg_xor
        beq     :+
        ldy     tmp1
        eor     (ptr1),y
        sta     (ptr1),y
        rts
:       ldy     tmp1
        sta     (ptr1),y
        rts

; ---------------------------------------------------------------------------------------------------
; d_hrow: three Hi-Res bytes at text column hg_x of line hg_y, from hg_p[0] to hg_p[2], each written only
; where its mask, hg_m[0] to hg_m[2], has a pixel, the rest of the line kept. A byte goes to the screen as
; hgr_blit draws it, its palette bit set by the caller. It is what a game that wrote parts of Hi-Res bytes
; itself (Grid Words' tiles) does in Double Hi-Res.
hk:     .res    1
hma:    .res    1               ; the mask's dots, in the auxiliary byte and the main one
hmm:    .res    1
hms:    .res    1               ; $40: the mask's dot that goes into the byte before

d_hrow: ldx     _hg_y
        cpx     #192
        bcc     :+
        rts
:
        jsr     row
        ldx     #0
hr_k:   stx     hk
        lda     _hg_m,x
        bne     :+
        jmp     hr_next
:
        tay
        txa
        clc
        adc     _hg_x
        cmp     #40
        bcc     :+
        jmp     hr_next
:       sta     tmp1            ; the column
        tya                     ; the mask's dots
        and     #15
        tax
        lda     lo16,x
        sta     hma
        tya
        lsr     a
        lsr     a
        lsr     a
        tax
        lda     hi16,x
        sta     hmm
        ldx     hk              ; the byte's dots
        lda     _hg_p,x
        sta     bhb
        and     #$7F
        tay
        and     #15
        tax
        lda     lo16,x
        and     hma
        sta     bla
        tya
        lsr     a
        lsr     a
        lsr     a
        tax
        lda     hi16,x
        and     hmm
        sta     blm
        lda     #0
        sta     bsp
        sta     hms
        bit     bhb             ; the palette bit clear, in color: one dot to the left, as hgr_blit
        bmi     hr_put
        lda     zmono
        bne     hr_put
        lsr     blm
        lda     bla
        bcc     :+
        ora     #$80
:       lsr     a
        sta     bla
        bcc     :+
        lda     #$40
        sta     bsp
:       lsr     hmm
        lda     hma
        bcc     :+
        ora     #$80
:       lsr     a
        sta     hma
        bcc     hr_put
        lda     #$40
        sta     hms
hr_put: sta     AUXON
        ldy     tmp1
        lda     hma
        eor     #$FF
        and     (ptr1),y
        ora     bla
        sta     (ptr1),y
        sta     MAINON
        lda     hmm
        eor     #$FF
        and     (ptr1),y
        ora     blm
        sta     (ptr1),y
        lda     hms             ; the dot that goes into the byte before
        beq     hr_next
        dey
        bmi     hr_next
        eor     #$FF
        and     (ptr1),y
        ora     bsp
        sta     (ptr1),y
hr_next:ldx     hk
        inx
        cpx     #3
        beq     :+
        jmp     hr_k
:       rts

; ---------------------------------------------------------------------------------------------------
; Lines, in white or flipped with hg_xor: hgr_hline, hg_w Hi-Res pixels across from (hg_px, hg_y), and
; hgr_vline, hg_h lines down from it. A Hi-Res pixel is two dots; in color a line covers whole color
; pixels, so that it shows white.
hd0:    .res    2               ; the first dot and the last
hd1:    .res    2
hcnt:   .res    1
hln:    .res    1
k0:     .res    1               ; a span: its first byte along the line (0 to 79) and that byte's mask,
m0:     .res    1
k1:     .res    1               ; its last byte and mask
m1:     .res    1
c4:     .res    1               ; its color * 4
hcol:   .res    1               ; a line's color * 4 (white but for d_vbar's)
sxor:   .res    1               ; 1: flipped
par:    .res    1
ya:     .res    1
yb:     .res    1

d_hline:
        jsr     white
        lda     _hg_w
        beq     hl_out
        jsr     hl_d0
        jsr     hl_d1
        ldx     _hg_y
        cpx     #192
        bcs     hl_out
        jmp     hspan
hl_out: rts
hl_d0:  lda     _hg_px
        asl     a
        sta     hd0
        lda     _hg_px+1
        rol     a
        sta     hd0+1
        rts
hl_d1:  lda     _hg_w           ; hd1: the last dot of hg_w pixels from hd0
        asl     a
        sta     tmp1
        lda     #0
        rol     a
        sta     tmp2
        lda     hd0
        clc
        adc     tmp1
        sta     hd1
        lda     hd0+1
        adc     tmp2
        sta     hd1+1
        lda     hd1
        bne     :+
        dec     hd1+1
:       dec     hd1
        rts

white:  lda     #15 * 4
        sta     hcol
        rts

; d_vbar: hg_w pixels across and hg_h lines down from (hg_px, hg_y), as hgr_vline is one pixel across, in
; the color hg_p[0].
d_vbar: lda     _hg_p
        asl     a
        asl     a
        sta     hcol
        lda     _hg_w
        beq     hl_out
        jsr     hl_d0
        jsr     hl_d1
        jmp     vl_go

d_vline:
        jsr     white
        jsr     hl_d0
        lda     hd0
        sta     hd1
        inc     hd1
        lda     hd0+1
        sta     hd1+1
vl_go:  lda     _hg_h
        beq     hl_out
        sta     hcnt
        jsr     hprep           ; the same bytes and masks on every line
        ldx     _hg_y
:       cpx     #192
        bcs     hl_out
        stx     hln
        jsr     span
        ldx     hln
        inx
        dec     hcnt
        bne     :-
        rts

; Dots hd0 to hd1 of line X.
hspan:  stx     hln
        jsr     hprep
        ldx     hln
        jmp     span

; The span of dots hd0 to hd1: its bytes k0 and k1, their masks, the color and whether it flips.
hprep:  lda     zmono
        bne     :+
        lda     hd0
        and     #$FC
        sta     hd0
        lda     hd1
        ora     #3
        sta     hd1
:       lda     hd1+1           ; at most dot 559
        cmp     #2
        bcc     :+
        lda     hd1
        cmp     #$30
        bcc     :+
        lda     #$2F
        sta     hd1
        lda     #2
        sta     hd1+1
:       lda     hd0
        ldx     hd0+1
        jsr     div7
        sty     k0
        tax
        lda     himask,x
        sta     m0
        lda     hd1
        ldx     hd1+1
        jsr     div7
        sty     k1
        tax
        lda     lomask,x
        sta     m1
        lda     hcol
        sta     c4
        lda     _hg_xor
        sta     sxor
        rts

; A dot (A low, X high; under 560) as its byte along the line (Y) and its bit (A).
div7:   ldy     #0
@lp:    cpx     #0
        bne     @sub
        cmp     #7
        bcc     @done
@sub:   sec
        sbc     #7
        bcs     :+
        dex
:       iny
        bne     @lp
@done:  rts

; Fill the bytes k0 to k1 of line X (k0's bits from m0, k1's to m1) with the color c4 / 4, or flip them
; when sxor is set.
span:   jsr     row
        lda     #0
        sta     par
        sta     AUXON
sp_pass:
        ; ya: the first byte of this page in the span, yb the last, as offsets in the page
        lda     k0
        eor     par
        lsr     a               ; C: k0 is in the other page
        lda     k0
        adc     #0
        sta     tmp2            ; the first k of this page
        lsr     a
        sta     ya
        lda     k1
        eor     par
        eor     #1
        lsr     a               ; C: k1 is in this page
        lda     k1
        sbc     #0              ; (one less when it is not)
        bcc     sp_next         ; (k1 0 in the other page)
        sta     tmp4            ; the last k of this page
        cmp     tmp2
        bcc     sp_next         ; none in this page
        lsr     a
        sta     yb
        ldx     tmp2
        jsr     edge
        ldy     ya
        cpy     yb
        beq     sp_next
        ldx     tmp4
        jsr     edge2
        ldy     ya
        iny
        cpy     yb
        bcs     sp_next
        lda     sxor
        bne     sp_xmid
        lda     par
        ora     c4
        tax
        lda     colpat,x        ; offset y is byte 2y + par, so k mod 4 is par at even offsets
        sta     tmp1
        lda     colpat+2,x      ; and par + 2 at odd ones
        sta     tmp3
        tya
        lsr     a
        bcc     sp_even
sp_odd: lda     tmp3
        sta     (ptr1),y
        iny
        cpy     yb
        bcs     sp_next
sp_even:lda     tmp1
        sta     (ptr1),y
        iny
        cpy     yb
        bcc     sp_odd
        bcs     sp_next
sp_xmid:lda     (ptr1),y
        eor     #$7F
        sta     (ptr1),y
        iny
        cpy     yb
        bcc     sp_xmid
sp_next:
        lda     par
        bne     :+
        inc     par
        sta     MAINON
        jmp     sp_pass
:       rts

; One byte at the ends of a span: k in X, at offset ya (edge) or yb (edge2).
edge2:  ldy     yb
        bpl     ed1
edge:   ldy     ya
ed1:    lda     #$7F
        cpx     k0
        bne     :+
        and     m0
:       cpx     k1
        bne     :+
        and     m1
:       sta     tmp3
        lda     sxor
        bne     ed_x
        txa
        and     #3
        ora     c4
        tax
        lda     colpat,x
        eor     (ptr1),y
        and     tmp3
        eor     (ptr1),y
        sta     (ptr1),y
        rts
ed_x:   lda     tmp3
        eor     (ptr1),y
        sta     (ptr1),y
        rts

; ---------------------------------------------------------------------------------------------------
; Sprites: a picture at any dot, drawn over what is there, which is kept and put back. Slot 0 is the
; pointer's, slot 1 the gliding piece's. A sprite is up to 15 lines of up to 28 dots, in one or two layers
; drawn in turn, each a mask (4 bytes a line, as d_place's) and a color.
sx:     .res    2               ; the first dot
sy:     .res    1
sh:     .res    1
sn:     .res    1               ; layers
sm:     .res    4               ; their masks
sc:     .res    2               ; and colors
skk:    .res    1               ; the first byte along the line
ss:     .res    1               ; and the dot in it
sr:     .res    1               ; the line in the sprite
sv:     .res    1               ; the place in the kept bytes
sli:    .res    1
sb:     .res    5               ; a line's 5 bytes of the screen
t5:     .res    5               ; a mask's line, moved to the sprite's dot
slk:    .res    2               ; for each slot: where it was drawn, to put it back
sly:    .res    2
slh:    .res    2
sav0:   .res    5 * 15
sav1:   .res    5 * 12

; Draw the sprite (sx ... sc) in slot X.
spr_draw:
        lda     sx
        ldx     sx+1
        jsr     div7
        sty     skk
        sta     ss
        ldx     sli             ; (the slot, set by the caller)
        tya
        sta     slk,x
        lda     sy
        sta     sly,x
        lda     sh
        sta     slh,x
        jsr     savptr
        lda     #0
        sta     sr
        sta     sv
sd_line:
        lda     sy
        clc
        adc     sr
        cmp     #192
        bcc     :+
        rts
:       tax
        jsr     row
        jsr     rd5
        ldx     #0
sd_lay: stx     tmp3
        txa
        asl     a
        tax
        lda     sm,x
        sta     mp
        lda     sm+1,x
        sta     mp+1
        lda     sr
        asl     a
        asl     a
        tay
        lda     (mp),y
        sta     t5
        iny
        lda     (mp),y
        sta     t5+1
        iny
        lda     (mp),y
        sta     t5+2
        iny
        lda     (mp),y
        sta     t5+3
        lda     #0
        sta     t5+4
        jsr     shl5
        ldx     tmp3
        lda     sc,x
        asl     a
        asl     a
        sta     tmp1
        ldx     #0
:       txa
        clc
        adc     skk
        and     #3
        ora     tmp1
        tay
        lda     sb,x
        eor     colpat,y
        and     t5,x
        eor     sb,x
        sta     sb,x
        inx
        cpx     #5
        bcc     :-
        ldx     tmp3
        inx
        cpx     sn
        bcc     sd_lay
        jsr     wr5
        jsr     ktick      ; (a line takes up to 1,000 cycles: no tick is missed)
        inc     sr
        lda     sr
        cmp     sh
        bcs     sd_done
        jmp     sd_line
sd_done:
        jmp     ktick

; Put back what slot X's sprite covered.
spr_back:
        lda     slk,x
        sta     skk
        lda     sly,x
        sta     sy
        lda     slh,x
        sta     sh
        jsr     savptr
        lda     #0
        sta     sr
        sta     sv
sb_line:
        lda     sy
        clc
        adc     sr
        cmp     #192
        bcs     sb_done
        tax
        jsr     row
        ldx     #0
sb_byte:txa
        clc
        adc     skk
        cmp     #80
        bcs     sb_next
        lsr     a
        sta     tmp2
        ldy     sv
        lda     (ptr2),y
        inc     sv
        ldy     tmp2
        bcs     :+
        sta     AUXON
        sta     (ptr1),y
        sta     MAINON
        jmp     sb_next
:       sta     (ptr1),y
sb_next:inx
        cpx     #5
        bcc     sb_byte
        inc     sr
        lda     sr
        cmp     sh
        bcc     sb_line
sb_done:
        rts

; ptr2 to slot X's kept bytes.
savptr: lda     #<sav0
        ldy     #>sav0
        cpx     #0
        beq     :+
        lda     #<sav1
        ldy     #>sav1
:       sta     ptr2
        sty     ptr2+1
        rts

; The line's 5 bytes from skk into sb, and kept.
rd5:    ldx     #0
@lp:    txa
        clc
        adc     skk
        cmp     #80
        bcs     @next
        lsr     a
        tay
        bcs     :+
        sta     AUXON
        lda     (ptr1),y
        sta     MAINON
        jmp     :++
:       lda     (ptr1),y
:       sta     sb,x
        ldy     sv
        sta     (ptr2),y
        inc     sv
@next:  inx
        cpx     #5
        bcc     @lp
        rts

; sb back to the screen.
wr5:    ldx     #0
@lp:    txa
        clc
        adc     skk
        cmp     #80
        bcs     @next
        lsr     a
        tay
        lda     sb,x
        bcs     :+
        sta     AUXON
        sta     (ptr1),y
        sta     MAINON
        jmp     @next
:       sta     (ptr1),y
@next:  inx
        cpx     #5
        bcc     @lp
        rts

; t5 moved ss dots on, 7 to a byte.
shl5:   ldy     ss
        beq     @done
@lp:    lda     t5
        asl     a
        cmp     #$80
        and     #$7F
        sta     t5
        lda     t5+1
        rol     a
        cmp     #$80
        and     #$7F
        sta     t5+1
        lda     t5+2
        rol     a
        cmp     #$80
        and     #$7F
        sta     t5+2
        lda     t5+3
        rol     a
        cmp     #$80
        and     #$7F
        sta     t5+3
        lda     t5+4
        rol     a
        and     #$7F
        sta     t5+4
        dey
        bne     @lp
@done:  rts

; hgr_ptr: the pointer, a white arrow edged in black with its tip at the Hi-Res pixel (hg_px, hg_y), drawn
; over what is there; called again, it puts that back (ui.c calls it in pairs). In color its tip is on the
; color pixel's first dot.
pt_on:  .res    1

d_ptr:  ldx     #0
        lda     pt_on
        eor     #1
        sta     pt_on
        bne     :+
        jmp     spr_back
:       stx     sli
        lda     _hg_px
        asl     a
        sta     sx
        lda     _hg_px+1
        rol     a
        sta     sx+1
        lda     zmono
        bne     :+
        lda     sx
        and     #$FC
        sta     sx
:       lda     _hg_y
        sta     sy
        lda     #15
        sta     sh
        lda     #2
        sta     sn
        lda     #<arrow_e
        sta     sm
        lda     #>arrow_e
        sta     sm+1
        lda     #<arrow_f
        sta     sm+2
        lda     #>arrow_f
        sta     sm+3
        lda     #0
        sta     sc
        lda     #15
        sta     sc+1
        jmp     spr_draw

; The C64's arrow, each of its pixels two dots: its black edge, and the white inside it.
arrow_e:
        .byte   $0F, $00, $00, $00
        .byte   $3F, $00, $00, $00
        .byte   $7F, $01, $00, $00
        .byte   $7F, $07, $00, $00
        .byte   $7F, $1F, $00, $00
        .byte   $7F, $7F, $00, $00
        .byte   $7F, $7F, $03, $00
        .byte   $7F, $7F, $0F, $00
        .byte   $7F, $7F, $0F, $00
        .byte   $7F, $7F, $00, $00
        .byte   $3F, $7E, $03, $00
        .byte   $0F, $7E, $03, $00
        .byte   $00, $78, $0F, $00
        .byte   $00, $78, $0F, $00
        .byte   $00, $60, $0F, $00
arrow_f:
        .byte   $00, $00, $00, $00
        .byte   $0C, $00, $00, $00
        .byte   $3C, $00, $00, $00
        .byte   $7C, $01, $00, $00
        .byte   $7C, $07, $00, $00
        .byte   $7C, $1F, $00, $00
        .byte   $7C, $7F, $00, $00
        .byte   $7C, $7F, $03, $00
        .byte   $7C, $1F, $00, $00
        .byte   $3C, $1E, $00, $00
        .byte   $0C, $78, $00, $00
        .byte   $00, $78, $00, $00
        .byte   $00, $60, $03, $00
        .byte   $00, $60, $03, $00
        .byte   $00, $00, $00, $00

; d_glide: the sprite of dl_n (1 or 2) layers, dl_m and dl_ce, dl_h lines at dot dl_sx, line dl_y, in slot 1.
; d_unglide puts back what it covered.
d_glide:
        ldx     #1
        stx     sli
        lda     _dl_sx
        sta     sx
        lda     _dl_sx+1
        sta     sx+1
        lda     _dl_y
        sta     sy
        lda     _dl_h
        sta     sh
        lda     _dl_n
        sta     sn
        ldx     #3
:       lda     _dl_m,x
        sta     sm,x
        dex
        bpl     :-
        lda     _dl_ce
        sta     sc
        lda     _dl_ce+1
        sta     sc+1
        jmp     spr_draw
d_unglide:
        ldx     #1
        jmp     spr_back

; ---------------------------------------------------------------------------------------------------
; Places: groups of 28 dots drawn a whole byte at a time. Each line starts black and takes up to 4 layers
; in turn, each a mask (4 bytes a line, as the group's bytes on the screen; or every dot) in a color, one for
; even lines and one for odd ones. A mask moves on by its stride each line: 4, 0 to use one line for all,
; or -4 to go up it. The same bytes go in every group of a place.
;
; d_place takes its layers from dl_n, dl_m, dl_ms, dl_ce and dl_co, and draws dl_w groups from dl_x and
; dl_h lines from dl_y. When no mask moves, a line is worked out once for even lines and once for odd ones.
;
; d_types takes a table of up to 10 kinds of place of 12 lines, in the auxiliary memory with the kit, at
; dl_m[0] (17 bytes each: the layers, + $80 when it must not be kept and + $40 when a color
; changes with the line's parity, then for each layer its mask, 0 for every dot, its stride and its colors,
; even + 16 * odd), and forgets what d_column kept. d_column draws dl_cn places down from line dl_y in group
; dl_x, of the kinds listed in dl_col. The first 8 kinds are kept once worked out, for lines of the same
; parity (or any, when their colors do not change with it), so that a board's many places of the same kind
; are only stored.
NTY     = 10
ln_:    .res    1               ; the layers
lml:    .res    4               ; each one's mask line (high byte 0: every dot)
lmh:    .res    4
lms:    .res    4               ; and stride
link:   .res    32              ; the colors as bytes: line parity * 16 + layer * 4 + k mod 4
lce:    .res    4
lco:    .res    4
lmov:   .res    1               ; a mask moves
ldith:  .res    1               ; a color changes with the parity
pout:   .res    8               ; d_place: an even line's bytes and an odd line's
pw:     .res    1
poff:   .res    1
pcnt:   .res    1
pln:    .res    1
ppar:   .res    1
ctag:   .res    6               ; what each slot keeps: $FF nothing, else its kind * 4 and the lines it is
                                ; good for, 0 even, 1 odd, 2 any
; The 6 slots, 48 bytes each: kinds 0 to 5, and 6 and 7 in the slots of 4 and 5. They are in the
; auxiliary memory's half of the text page (reached with PAGE2, as the auxiliary half of the screen), which
; nothing else uses, and clear of its screen holes ($xx78-$xx7F, $xxF8-$xxFF).
sbl:    .byte   $00, $30, $80, $B0, $00, $30
sbh:    .byte   $04, $04, $04, $04, $05, $05
cslx:   .res    1
cfl:    .res    1
ctyp:   .res    1
cidx:   .res    1
ctp:    .res    1
crow:   .res    1

pl_out: rts
d_place:
        ldx     #0
:       cpx     _dl_n
        bcs     :+
        txa
        asl     a
        tay
        lda     _dl_m,y
        sta     lml,x
        lda     _dl_m+1,y
        sta     lmh,x
        lda     _dl_ms,x
        sta     lms,x
        lda     _dl_ce,x
        sta     lce,x
        lda     _dl_co,x
        sta     lco,x
        inx
        bne     :-
:       lda     _dl_n
        sta     ln_
        jsr     inkset
        lda     _dl_x
        cmp     #20
        bcs     pl_out
        clc
        adc     _dl_w
        cmp     #21
        bcc     :+
        lda     #20
:       sec
        sbc     _dl_x
        beq     pl_out
        sta     pw
        lda     _dl_x
        asl     a
        sta     poff
        lda     _dl_h
        beq     pl_out
        sta     pcnt
        lda     _dl_y
        sta     pln
; pcnt lines from pln, pw groups from poff, of the layers set (ln_ ...).
prun:   lda     lmov
        bne     @line
        lda     #1              ; no mask moving: an odd line and an even line worked out, then only stored
        sta     ppar
        jsr     pline
        dec     ppar
        jsr     pline
        lda     pw
        cmp     #1
        bne     @line
        lda     pln
        and     #1
        asl     a
        asl     a
        tax
@run:   ldy     pln
        cpy     #192
        bcs     @out
        lda     _hgr_ylo,y
        sta     ptr1
        lda     _hgr_yhi,y
        sta     ptr1+1
        ldy     poff
        sta     AUXON
        lda     pout,x
        sta     (ptr1),y
        iny
        lda     pout+2,x
        sta     (ptr1),y
        sta     MAINON
        lda     pout+3,x
        sta     (ptr1),y
        dey
        lda     pout+1,x
        sta     (ptr1),y
        txa
        eor     #4
        tax
        lda     pln
        and     #7
        bne     :+
        jsr     ktick
:       inc     pln
        dec     pcnt
        bne     @run
@out:   rts
@line:  ldx     pln
        cpx     #192
        bcs     @out
        lda     _hgr_ylo,x
        sta     ptr1
        lda     _hgr_yhi,x
        sta     ptr1+1
        txa
        and     #1
        sta     ppar
        asl     a
        asl     a
        tax
        lda     lmov
        beq     :+
        jsr     pline
:       jsr     stw
        lda     pln
        and     #7
        bne     :+
        jsr     ktick
:       inc     pln
        dec     pcnt
        bne     @line
        rts

; This line's bytes (parity ppar) worked out into pout + X, X = ppar * 4.
pline:  lda     ppar
        asl     a
        asl     a
        pha
        jsr     pcomp
        pla
        tax
        lda     po
        sta     pout,x
        lda     po+1
        sta     pout+1,x
        lda     po+2
        sta     pout+2,x
        lda     po+3
        sta     pout+3,x
        rts

; The colors' bytes (link) from lce and lco, and lmov and ldith.
inkset: lda     #0
        sta     lmov
        sta     ldith
        ldx     #0
@lp:    cpx     ln_
        bcs     @done
        stx     tmp2
        lda     lmh,x
        beq     :+
        lda     lms,x
        ora     lmov
        sta     lmov
:       lda     lce,x
        cmp     lco,x
        beq     :+
        sta     ldith
:       txa
        asl     a
        asl     a
        sta     tmp3
        tax
        ldy     tmp2
        lda     lce,y
        jsr     ink4
        lda     tmp3
        ora     #16
        tax
        ldy     tmp2
        lda     lco,y
        jsr     ink4
        ldx     tmp2
        inx
        bne     @lp
@done:  rts

; Color A's 4 bytes (k mod 4 = 0 to 3) into link + X.
ink4:   asl     a
        asl     a
        tay
        lda     colpat,y
        sta     link,x
        lda     colpat+1,y
        sta     link+1,x
        lda     colpat+2,y
        sta     link+2,x
        lda     colpat+3,y
        sta     link+3,x
        rts

; This line's 4 bytes (parity ppar) into po, the masks moved on to the next line.
pcomp:  lda     #0
        sta     po
        sta     po+1
        sta     po+2
        sta     po+3
        sta     tmp2            ; the layer
        lda     ppar
        asl     a
        asl     a
        asl     a
        asl     a
        sta     tmp3
@lay:   ldx     tmp2
        cpx     ln_
        bcc     :+
        rts
:       txa
        asl     a
        asl     a
        ora     tmp3
        tay                     ; (the colors' bytes)
        lda     lmh,x
        bne     @mask
        lda     link,y
        sta     po
        lda     link+1,y
        sta     po+1
        lda     link+2,y
        sta     po+2
        lda     link+3,y
        sta     po+3
        inc     tmp2
        bne     @lay
@mask:  sta     mp+1
        lda     lml,x
        sta     mp
        tya
        tax
        ldy     #0
        lda     po
        sta     pt
        eor     link,x
        and     (mp),y
        eor     pt
        sta     po
        iny
        lda     po+1
        sta     pt
        eor     link+1,x
        and     (mp),y
        eor     pt
        sta     po+1
        iny
        lda     po+2
        sta     pt
        eor     link+2,x
        and     (mp),y
        eor     pt
        sta     po+2
        iny
        lda     po+3
        sta     pt
        eor     link+3,x
        and     (mp),y
        eor     pt
        sta     po+3
        ldx     tmp2            ; the mask moves on
        lda     lms,x
        bmi     @up
        clc
        adc     lml,x
        sta     lml,x
        bcc     :+
        inc     lmh,x
:       inc     tmp2
        jmp     @lay
@up:    clc
        adc     lml,x
        sta     lml,x
        bcs     :+
        dec     lmh,x
:       inc     tmp2
        jmp     @lay

; pout + X on this line (ptr1) in pw groups from offset poff.
stw:    ldy     poff
        lda     pw
        cmp     #1
        bne     @many
        sta     AUXON           ; (one group)
        lda     pout,x
        sta     (ptr1),y
        iny
        lda     pout+2,x
        sta     (ptr1),y
        sta     MAINON
        lda     pout+3,x
        sta     (ptr1),y
        dey
        lda     pout+1,x
        sta     (ptr1),y
        rts
@many:  sta     tmp1
        sta     AUXON
:       lda     pout,x
        sta     (ptr1),y
        iny
        lda     pout+2,x
        sta     (ptr1),y
        iny
        dec     tmp1
        bne     :-
        sta     MAINON
        ldy     poff
        lda     pw
        sta     tmp1
:       lda     pout+1,x
        sta     (ptr1),y
        iny
        lda     pout+3,x
        sta     (ptr1),y
        iny
        dec     tmp1
        bne     :-
        rts

d_types:
        lda     _dl_m
        sta     kp
        lda     _dl_m+1
        sta     kp+1
        lda     #$FF
        ldx     #5
:       sta     ctag,x
        dex
        bpl     :-
        rts
koff:   .repeat NTY, k          ; each kind's place in the table
        .byte   17 * k
        .endrepeat

; The layers of kind X (the table at kp).
tylay:  ldy     koff,x
        lda     (kp),y
        and     #$0F
        sta     ln_
        ldx     #0
@lp:    cpx     ln_
        bcs     @done
        iny
        lda     (kp),y          ; the mask
        sta     lml,x
        iny
        lda     (kp),y
        sta     lmh,x
        iny
        lda     (kp),y
        sta     lms,x
        iny
        lda     (kp),y
        and     #15
        sta     lce,x
        lda     (kp),y
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     lco,x
        inx
        bne     @lp
@done:  jmp     inkset

d_column:
        lda     _dl_y
        sta     pln
        lda     _dl_x
        asl     a
        sta     poff
        lda     #1
        sta     pw
        lda     #0
        sta     cidx
dc_ent:   ldx     cidx
        cpx     _dl_cn
        bcc     :+
        rts
:       lda     _dl_col,x
        sta     ctyp
        tax
        ldy     koff,x          ; the kept lines it needs: kind * 4 and the parity, or 2 for any
        lda     (kp),y
        sta     cfl             ; (its flags)
        lda     pln
        and     #1
        bit     cfl
        bvs     :+
        lda     #2
:       sta     ctp
        cpx     #8
        bcc     :+
        jmp     dc_free
:       cpx     #6
        bcc     :+
        dex
        dex
:       stx     cslx
        jsr     slot
        lda     ctyp
        asl     a
        asl     a
        ora     ctp
        sta     ctp
        ldx     cslx
        lda     ctag,x
        cmp     ctp
        beq     dc_put
        ldx     ctyp            ; work its 12 lines out into the slot
        jsr     tylay
        lda     #0
        sta     crow
dc_calc:  lda     pln
        clc
        adc     crow
        and     #1
        sta     ppar
        jsr     pcomp
        lda     crow
        asl     a
        asl     a
        tay
        sta     AUXON
        lda     po
        sta     (ptr2),y
        iny
        lda     po+1
        sta     (ptr2),y
        iny
        lda     po+2
        sta     (ptr2),y
        iny
        lda     po+3
        sta     (ptr2),y
        sta     MAINON
        inc     crow
        lda     crow
        cmp     #12
        bcc     dc_calc
        bit     cfl
        bmi     dc_put
        ldx     cslx
        lda     ctp
        sta     ctag,x
dc_put:   ldx     #0
dc_p1:    ldy     pln
        cpy     #192
        bcs     dc_next
        lda     _hgr_ylo,y
        sta     ptr1
        lda     _hgr_yhi,y
        sta     ptr1+1
        ldy     poff
        sta     AUXON
ks1:    lda     $0400+1,x
        sta     po+1
ks3:    lda     $0400+3,x
        sta     po+3
ks0:    lda     $0400,x
        sta     (ptr1),y
        iny
ks2:    lda     $0400+2,x
        sta     (ptr1),y
        sta     MAINON
        lda     po+3
        sta     (ptr1),y
        dey
        lda     po+1
        sta     (ptr1),y
        inc     pln
        inx
        inx
        inx
        inx
        cpx     #48
        bcc     dc_p1
dc_next:  jsr     ktick
        inc     cidx
        jmp     dc_ent
dc_free:  jsr     tylay           ; a kind not kept: its lines as d_place does them, with any more of it
        lda     #12             ; that come next
        sta     pcnt
:       ldx     cidx
        inx
        cpx     _dl_cn
        bcs     :+
        lda     _dl_col,x
        cmp     ctyp
        bne     :+
        stx     cidx
        lda     pcnt
        adc     #12 - 1         ; (C set)
        sta     pcnt
        bne     :-
:
        jsr     prun
        jmp     dc_next

; Slot X's kept lines: ptr2 to them, and the loads of d_column's dc_put from them.
slot:   lda     sbl,x
        sta     ptr2
        lda     sbh,x
        sta     ptr2+1
        ldx     #3
:       txa                     ; each load's operand: the slot + its byte of the 4
        clc
        adc     ptr2
        ldy     sopl,x
        sta     ks1+1,y
        lda     ptr2+1
        sta     ks1+2,y
        dex
        bpl     :-
        rts
sopl:   .byte   ks0 - ks1, 0, ks2 - ks1, ks3 - ks1

; ---------------------------------------------------------------------------------------------------
; d_pxline: a line of 140 color pixels (dl_buf, a color a byte) on lines dl_y to dl_y + dl_h - 1.
lb:     .res    80
dcnt:   .res    1
; A color's 4 bits in the order of its dots.

d_pxline:
        ldx     #0
        ldy     #0
        stx     tmp1
        lda     #1
        sta     tmp2
@px:    lda     _dl_buf,x       ; the color's bits in the order of its dots: bit 1 first, bit 0 last
        and     #15
        lsr     a
        bcc     :+
        ora     #8
:       sta     tmp4
        lda     #4
        sta     dcnt
@dot:   lsr     tmp4
        bcc     :+
        lda     tmp1
        ora     tmp2
        sta     tmp1
:       asl     tmp2
        bpl     :+
        lda     tmp1
        sta     lb,y
        iny
        lda     #0
        sta     tmp1
        lda     #1
        sta     tmp2
:       dec     dcnt
        bne     @dot
        inx
        cpx     #140
        bne     @px
        lda     _dl_y
        sta     pln
        lda     _dl_h
        beq     @done
        sta     pcnt
@line:  ldx     pln
        cpx     #192
        bcs     @done
        jsr     row
        sta     AUXON
        ldy     #0
        ldx     #0
:       lda     lb,x
        sta     (ptr1),y
        inx
        inx
        iny
        cpy     #40
        bne     :-
        sta     MAINON
        ldy     #0
        ldx     #1
:       lda     lb,x
        sta     (ptr1),y
        inx
        inx
        iny
        cpy     #40
        bne     :-
        jsr     ktick
        inc     pln
        dec     pcnt
        bne     @line
@done:  rts

; ---------------------------------------------------------------------------------------------------
; Numbers and dice in whole color pixels, so that they have no colored fringe, as a mask of 12 lines in
; knum (dhr.h's dhr_knum), for d_place or d_column to draw.
;
; d_num: the number dl_v (up to 99, or 100 for +) on lines 3 to 7, in digits 3 pixels by 5 lines: two at
; pixels 0 and 4, or one (or +) from pixel 4 - dl_sh. d_pips: a die's dl_v pips, each a pixel by two lines,
; on lines 2 and 3, 5 and 6, and 8 and 9, at pixels 1, 2 and 3 of a face 5 pixels wide.
        .export         _dhr_knum
_dhr_knum:
knum:   .res    48
; Digits 0 to 9 and +, a row each in bits 2 (left) to 0.
digit:  .byte   7,5,5,5,7, 2,6,2,2,7, 7,1,7,4,7, 7,1,3,1,7, 5,5,7,1,1
        .byte   7,4,7,1,7, 7,4,7,5,7, 7,1,2,2,2, 7,5,7,5,7, 7,5,7,1,7, 0,2,7,2,0
; Each die's rows of pips (pixel 1 $20, 2 $10, 3 $08).
piprow: .byte   0,$10,0, $08,0,$20, $08,$10,$20, $28,0,$28, $28,$10,$28, $28,$28,$28
; Each pixel's dots in a line's 4 bytes.
pxm:    .byte   $0F,0,0,0, $70,$01,0,0, 0,$1E,0,0, 0,$60,$03,0, 0,0,$3C,0, 0,0,$40,$07, 0,0,0,$78
nv:     .res    1
nr:     .res    1
nb:     .res    1

d_num:  lda     _dl_v
        sta     nv
        jsr     nclear
@row:   lda     nv
        cmp     #100
        bne     :+
        lda     #10
        jsr     drow            ; +
        jmp     @sh
:       cmp     #10
        bcc     @one
        ldx     #0              ; two digits: tens and ones
:       cmp     #10
        bcc     :+
        sbc     #10
        inx
        bne     :-
:       sta     tmp4
        txa
        jsr     drow
        asl     a
        asl     a
        asl     a
        asl     a
        sta     nb
        lda     tmp4
        jsr     drow
        ora     nb
        jmp     @put
@one:   jsr     drow
@sh:    ldx     _dl_sh
        beq     @put
:       asl     a
        dex
        bne     :-
@put:   ldx     nr
        ldy     nrow,x          ; (lines 3 to 7)
        jsr     pxl
        inc     nr
        lda     nr
        cmp     #5
        bcc     @row
        rts
nrow:   .byte   12, 16, 20, 24, 28

; Row nr of digit A.
drow:   sta     tmp3
        asl     a
        asl     a
        adc     tmp3            ; * 5
        adc     nr
        tax
        lda     digit,x
        rts

d_pips: lda     _dl_v
        sec
        sbc     #1
        sta     tmp3
        asl     a
        adc     tmp3            ; * 3
        sta     nv
        jsr     nclear
:       ldx     nv
        lda     piprow,x
        pha
        ldx     nr
        ldy     prow,x
        jsr     pxl
        pla
        ldx     nr
        ldy     prow+1,x
        jsr     pxl
        inc     nv
        inc     nr
        inc     nr
        lda     nr
        cmp     #6
        bcc     :-
        rts
prow:   .byte   8, 12, 20, 24, 32, 36

; knum cleared, nr 0.
nclear: lda     #0
        sta     nr
        ldx     #47
:       sta     knum,x
        dex
        bpl     :-
        rts

; The row of pixels A (bit 6 the first) into knum's line at Y (its first byte).
pxl:    sta     nb
        ldx     #0              ; the pixel * 4
@px:    asl     nb
        bpl     @next
        lda     knum,y
        ora     pxm,x
        sta     knum,y
        lda     knum+1,y
        ora     pxm+1,x
        sta     knum+1,y
        lda     knum+2,y
        ora     pxm+2,x
        sta     knum+2,y
        lda     knum+3,y
        ora     pxm+3,x
        sta     knum+3,y
@next:  inx
        inx
        inx
        inx
        cpx     #28
        bcc     @px
        rts
