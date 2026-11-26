; ParlorNet for the Apple IIe: Double Hi-Res drawing, a study (the dhr-study branch's notes describe it).
;
; Double Hi-Res shows 560 dots a line from two 8K pages at $2000: the auxiliary memory's bytes and the main
; memory's bytes take turns along the line, aux 0, main 0, aux 1, main 1 and so on, each byte giving 7 dots
; (bit 0 first; bit 7 is not shown). On a color monitor each 4 dots make one of 16 colors, so a line is
; 140 color pixels. This file is linked into the Backgammon overlay, which turns it on (dhr_enter): it
; fills the vectors that hgr.s's routines jump through while dhr_on is set, so that the table's frame
; (its title, buttons, status line and chat) is drawn in Double Hi-Res by the same calls as before.
;
; The auxiliary page is reached through 80STORE: with 80STORE and HIRES on, PAGE2 ($C055) puts the
; auxiliary memory at $2000-$3FFF and $C054 the main memory, without changing the page shown. Every
; routine here does the auxiliary bytes of a line first and then the main ones, and ends with the main
; memory in.
;
; Positions: dh_x and dh_w are color pixels (0 to 139), dh_y and dh_h lines. A color is 0 to 15. A mask
; (dh_src) is its width (at most 8) and height, then a byte a row with bit 7 for its first pixel.
;
; Seven color pixels are 28 dots, four whole bytes (two in each page), so a picture whose places start at
; a multiple of 7 pixels can be drawn a byte at a time, as text is (dhr_cells), and the board's places are.

        .setcpu "6502"
        .importzp       ptr1, ptr2, tmp1, tmp2, tmp3, tmp4
        .import         _hgr_ylo, _hgr_yhi, _font, _tick_poll
        .import         _hg_x, _hg_y, _hg_w, _hg_h, _hg_p, _hg_src, _hg_px, _hg_fg, _hg_bg, _hg_xor
        .import         _hg_bold, _hg_key
        .import         _dhr_on, _dhr_vec
        .export         _dh_x, _dh_y, _dh_w, _dh_h, _dh_c, _dh_c2, _dh_xor, _dh_src, _dh_tfg, _dh_tbg
        .export         _dhr_enter, _dhr_mask, _dhr_save, _dhr_restore, _dhr_cells, _dh_b, _dh_b2

AUXON   = $C055                 ; PAGE2 with 80STORE: the auxiliary page
MAINON  = $C054

        .segment "DATA"
_dh_x:  .byte   0
_dh_y:  .byte   0
_dh_w:  .byte   0
_dh_h:  .byte   0
_dh_c:  .byte   0
_dh_c2: .byte   $80             ; the color for odd lines, for a dither; $80: dh_c's
_dh_xor:.byte   0
_dh_src:.word   0
_dh_b:  .byte   0               ; dhr_cells: the background ($FF: none, what is there stays)
_dh_b2: .byte   $80             ; and on odd lines ($80: dh_b's)
_dh_tfg:.byte   $80             ; text in these colors when below $80, else in hg_fg and hg_bg's
_dh_tbg:.byte   $80

        .segment "BSS"
k0:     .res    1               ; a span: its first byte (0 to 79 along the line) and that byte's mask,
m0:     .res    1
k1:     .res    1               ; its last byte and mask
m1:     .res    1
c4:     .res    1               ; color * 4
par:    .res    1               ; 0 the auxiliary bytes, 1 the main ones
ya:     .res    1
yb:     .res    1
sx0:    .res    1
sx1:    .res    1
ln:     .res    1
cur:    .res    1
cnt:    .res    1
bits:   .res    1
mx:     .res    1
rs:     .res    1
mw:     .res    1
fa:     .res    1
fm:     .res    1
ba:     .res    1
bm:     .res    1
glyph:  .res    1
o4:     .res    4               ; dhr_cells: a line's four bytes
dm4:    .res    4               ; and the dots its row's pixels cover
fgi:    .res    1
bgi:    .res    1
lbits:  .res    1               ; the last line's row and colors: a line the same as the last one is
lfgi:   .res    1               ; written without working its bytes out again
lbgi:   .res    1
save:   .res    6 * 11          ; dhr_save: up to 3 offsets in each page, 11 lines
psave:  .res    6 * 9           ; under the pointer
pt_on:  .res    1               ; 1 while the pointer is drawn

        .segment "RODATA"
; For pxk: the byte and bit of pixel 7g + r in its group, by r.
r7byte: .byte   0, 0, 1, 1, 2, 2, 3
r7bit:  .byte   0, 4, 1, 5, 2, 6, 3
himask: .byte   $7F, $7E, $7C, $78, $70, $60, $40
lomask: .byte   $01, $03, $07, $0F, $1F, $3F, $7F
; Each color's byte for a byte k of the line, by k mod 4. Byte k starts at dot 7k, and the dot at 4n + j
; shows the color's bit (j + 1) mod 4, which gives the colors their usual numbers: 0 black, 1 magenta,
; 2 dark blue, 3 purple, 4 dark green, 5 gray, 6 medium blue, 7 light blue, 8 brown, 9 orange, 10 gray,
; 11 pink, 12 green, 13 yellow, 14 aqua, 15 white.
colpat: .repeat 64, i
        .byte   ((((i / 4) >> ((7 * (i .mod 4) + 0 + 1) .mod 4)) & 1) << 0) | ((((i / 4) >> ((7 * (i .mod 4) + 1 + 1) .mod 4)) & 1) << 1) | ((((i / 4) >> ((7 * (i .mod 4) + 2 + 1) .mod 4)) & 1) << 2) | ((((i / 4) >> ((7 * (i .mod 4) + 3 + 1) .mod 4)) & 1) << 3) | ((((i / 4) >> ((7 * (i .mod 4) + 4 + 1) .mod 4)) & 1) << 4) | ((((i / 4) >> ((7 * (i .mod 4) + 5 + 1) .mod 4)) & 1) << 5) | ((((i / 4) >> ((7 * (i .mod 4) + 6 + 1) .mod 4)) & 1) << 6)
        .endrepeat
; A 7-pixel font or Hi-Res byte doubled to 14 dots: the first 7 dots (the auxiliary byte) and the last 7.
dbllo:  .repeat 128, g
        .byte   (g & 1) * 3 | ((g >> 1) & 1) * 12 | ((g >> 2) & 1) * 48 | ((g >> 3) & 1) * 64
        .endrepeat
dblhi:  .repeat 128, g
        .byte   ((g >> 3) & 1) * 1 | ((g >> 4) & 1) * 6 | ((g >> 5) & 1) * 24 | ((g >> 6) & 1) * 96
        .endrepeat
; dhr_cells: the dots of a group's four bytes that its 7 pixels cover, from the row's bits 7 to 1 (pixels
; 0 to 6): byte 0 has pixel 0 and three dots of pixel 1, byte 1 the last dot of pixel 1, pixel 2 and two
; dots of pixel 3, and so on.
cdot0:  .byte   $00, $70, $0F, $7F                              ; pixels 0, 1
cdot1:  .byte   $00, $60, $1E, $7E, $01, $61, $1F, $7F          ; pixels 1, 2, 3
cdot2:  .byte   $00, $40, $3C, $7C, $03, $43, $3F, $7F          ; pixels 3, 4, 5
cdot3:  .byte   $00, $78, $07, $7F                              ; pixels 5, 6
; The Hi-Res fills' colors (ui.c's fills, by their first three bytes) in Double Hi-Res colors.
fillpat:.byte   $00,$00,$00, $7F,$7F,$7F, $2A,$55,$2A, $55,$2A,$55, $AA,$D5,$AA, $D5,$AA,$D5
        .byte   $55,$2A,$2A, $7F,$7F,$55, $55,$2A,$00, $7F,$7F,$00
fillcol:.byte   0, 15, 12, 3, 9, 6, 5, 13, 2, 10
; What the hand of hgr.s becomes: a white arrow edged in black, its tip at the pointer's place.
arrow_e:.byte   7, 9
        .byte   %11000000
        .byte   %11100000
        .byte   %11110000
        .byte   %11111000
        .byte   %11111100
        .byte   %11111110
        .byte   %11111100
        .byte   %10111110
        .byte   %00001110
arrow:  .byte   6, 8
        .byte   %10000000
        .byte   %11000000
        .byte   %11100000
        .byte   %11110000
        .byte   %11111000
        .byte   %11100000
        .byte   %10110000
        .byte   %00011000
vecs:   .word   d_clear, d_rect, d_blit, d_char, d_vline, d_hline, d_ptr

        .segment "CODE"

; void dhr_enter(void): Double Hi-Res on, cleared, with hgr.s's routines drawing in it.
_dhr_enter:
        ldx     #13
:       lda     vecs,x
        sta     _dhr_vec,x
        dex
        bpl     :-
        sta     $C001           ; 80STORE
        jsr     d_clear
        sta     $C00D           ; 80 columns
        sta     $C05E           ; AN3 off: Double Hi-Res
        sta     $C057           ; hi-res
        sta     $C052           ; full screen
        sta     $C054           ; page 1
        sta     $C050           ; graphics
        lda     #$80
        sta     _dhr_on
        rts

; Both pages cleared.
d_clear:
        sta     AUXON
        jsr     clr8
        sta     MAINON
clr8:   lda     #$20
        sta     ptr1+1
        lda     #0
        sta     ptr1
        tay
:       sta     (ptr1),y
        iny
        bne     :-
        inc     ptr1+1
        ldx     ptr1+1
        cpx     #$40
        bcc     :-
        jmp     _tick_poll

; The line X: ptr1 to its start (the same offsets in either page).
row:    lda     _hgr_ylo,x
        sta     ptr1
        lda     _hgr_yhi,x
        sta     ptr1+1
        rts

; Fill the bytes k0 to k1 of line X (k0's bits from m0, k1's to m1) with the color c4 / 4, or flip
; them when dh_xor is set.
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
        ; the first byte
        ldx     tmp2
        jsr     edge
        ldy     ya
        cpy     yb
        beq     sp_next
        ; the last byte
        ldx     tmp4
        jsr     edge2
        ; the bytes between, whole
        ldy     ya
        iny
        cpy     yb
        bcs     sp_next
        lda     _dh_xor
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
        lda     _dh_xor
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

; Color pixel X's first dot: its byte along the line in A, its bit in Y.
pxk:    txa
        ldy     #0
:       cmp     #7
        bcc     :+
        sbc     #7
        iny
        bne     :-
:       tax
        tya
        asl     a
        asl     a
        clc
        adc     r7byte,x
        ldy     r7bit,x
        rts

; The span of color pixels sx0 to sx1 - 1 (sx1 at most 140) on line X.
pxspan: stx     ln
        ldx     sx0
        jsr     pxk
        sta     k0
        lda     himask,y
        sta     m0
        ldx     sx1
        dex
        jsr     pxk
        sta     k1
        tya
        clc
        adc     #3
        cmp     #7
        bcc     :+
        sbc     #7
        inc     k1
:       tay
        lda     lomask,y
        sta     m1
        ldx     ln
        jmp     span

; c4 for line X: dh_c, or dh_c2 on an odd line when it is set.
linec:  txa
        lsr     a
        lda     _dh_c
        bcc     setc
        bit     _dh_c2
        bmi     setc
        lda     _dh_c2
; c4 from the color in A.
setc:   and     #15
        asl     a
        asl     a
        sta     c4
        rts

; void dhr_cells(void): dh_w groups of 7 color pixels from group dh_x (color pixel 7 * dh_x), dh_h lines
; from dh_y: on each line the row of dh_src (bits 7 to 1; the same in every group) in dh_c and its other
; pixels in dh_b, or left as they are when dh_b is $FF. With dh_src 0 every pixel is dh_b. dh_c2 and dh_b2
; are the colors on odd lines, for a dither.
_dhr_cells:
        lda     _dh_h
        bne     :+
        rts
:       sta     cnt
        lda     _dh_y
        sta     ln
        lda     #0
        sta     mw
        lda     #$FF            ; (no line before it: no color is $FF / 4)
        sta     lfgi
cl_line:
        ldx     ln
        cpx     #192
        bcc     :+
        rts
:       jsr     row
        ; this line's colors
        txa
        lsr     a
        lda     _dh_c
        bcc     :+
        bit     _dh_c2
        bmi     :+
        lda     _dh_c2
:       asl     a
        asl     a
        sta     fgi
        txa
        lsr     a
        lda     _dh_b
        bcc     :+
        bit     _dh_b2
        bmi     :+
        lda     _dh_b2
:       asl     a
        asl     a
        sta     bgi
        ; the row's dots
        lda     #0
        ldx     _dh_src+1
        beq     cl_m
        stx     ptr2+1
        ldx     _dh_src
        stx     ptr2
        ldy     mw
        lda     (ptr2),y
cl_m:   sta     bits
        cmp     lbits
        bne     cl_new
        lda     fgi
        cmp     lfgi
        bne     cl_new
        lda     bgi
        cmp     lbgi
        bne     cl_new
        lda     _dh_b
        cmp     #$FF
        bne     cl_st
        jmp     cl_ovst
cl_new: lda     bits
        sta     lbits
        lda     fgi
        sta     lfgi
        lda     bgi
        sta     lbgi
        lda     bits
        rol     a
        rol     a
        rol     a
        and     #3
        tax
        lda     cdot0,x
        sta     dm4
        lda     bits
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        and     #7
        tax
        lda     cdot1,x
        sta     dm4+1
        lda     bits
        lsr     a
        lsr     a
        and     #7
        tax
        lda     cdot2,x
        sta     dm4+2
        lda     bits
        lsr     a
        and     #3
        tax
        lda     cdot3,x
        sta     dm4+3
        lda     _dh_b
        cmp     #$FF
        beq     cl_over
        ; the four bytes, the same in every group
        ldy     #3
:       tya
        ora     fgi
        tax
        lda     colpat,x
        and     dm4,y
        sta     tmp1
        tya
        ora     bgi
        tax
        lda     dm4,y
        eor     #$7F
        and     colpat,x
        ora     tmp1
        sta     o4,y
        dey
        bpl     :-
cl_st:  lda     _dh_x
        asl     a
        sta     tmp2            ; the first offset
        sta     AUXON
        tay
        ldx     _dh_w
:       lda     o4
        sta     (ptr1),y
        iny
        lda     o4+2
        sta     (ptr1),y
        iny
        dex
        bne     :-
        sta     MAINON
        ldy     tmp2
        ldx     _dh_w
:       lda     o4+1
        sta     (ptr1),y
        iny
        lda     o4+3
        sta     (ptr1),y
        iny
        dex
        bne     :-
cl_next:
        jsr     _tick_poll
        inc     ln
        inc     mw
        dec     cnt
        beq     :+
        jmp     cl_line
:       rts
        ; over what is there: only the row's pixels
cl_over:
        ldy     #3
:       tya
        ora     fgi
        tax
        lda     colpat,x
        sta     o4,y
        dey
        bpl     :-
cl_ovst:lda     _dh_x
        asl     a
        sta     tmp2
        sta     AUXON
        ldx     #0
        jsr     cl_ov2
        sta     MAINON
        ldx     #1
        jsr     cl_ov2
        jmp     cl_next
cl_ov2: lda     _dh_w
        sta     tmp3
        ldy     tmp2
:       lda     o4,x
        eor     (ptr1),y
        and     dm4,x
        eor     (ptr1),y
        sta     (ptr1),y
        iny
        lda     o4+2,x
        eor     (ptr1),y
        and     dm4+2,x
        eor     (ptr1),y
        sta     (ptr1),y
        iny
        dec     tmp3
        bne     :-
        rts

; void dhr_mask(void): the mask at dh_src with its top left at (dh_x, dh_y), its set pixels in dh_c (or
; flipped with dh_xor).
_dhr_mask:
        lda     _dh_src
        sta     ptr2
        lda     _dh_src+1
        sta     ptr2+1
        ldy     #1
        lda     (ptr2),y
        sta     cnt             ; height
        lda     #0
        sta     mw              ; the row
mk_row: lda     _dh_y
        clc
        adc     mw
        cmp     #192
        bcs     mk_done
        sta     ln
        tax
        jsr     linec
        ldy     mw
        iny
        iny
        lda     (ptr2),y
        sta     bits
        lda     #0
        sta     mx
mk_bit: lda     bits
        beq     mk_next
        asl     bits
        bcs     mk_run
        inc     mx
        bne     mk_bit
mk_run: lda     mx
        sta     rs
        inc     mx
:       asl     bits
        bcc     :+
        inc     mx
        bne     :-
:       lda     _dh_x
        clc
        adc     rs
        cmp     #140
        bcs     mk_skip
        sta     sx0
        lda     _dh_x
        clc
        adc     mx
        cmp     #141
        bcc     :+
        lda     #140
:       sta     sx1
        ldx     ln
        jsr     pxspan
mk_skip:inc     mx
        bne     mk_bit
mk_next:inc     mw
        dec     cnt
        bne     mk_row
mk_done:
        jmp     _tick_poll

; The offsets in the pages of color pixels dh_x to dh_x + dh_w - 1: ya to yb.
offs:   ldx     _dh_x
        jsr     pxk
        lsr     a
        sta     ya
        lda     _dh_x
        clc
        adc     _dh_w
        tax
        dex
        jsr     pxk
        sta     tmp1
        tya
        clc
        adc     #3
        cmp     #7
        lda     tmp1
        adc     #0
        lsr     a
        sta     yb
        rts

; void dhr_save(void): keep what is on the screen under dh_w (up to 6) by dh_h (up to 11) color pixels
; at (dh_x, dh_y); dhr_restore puts it back.
_dhr_save:
        lda     #0
        beq     sv
_dhr_restore:
        lda     #1
sv:     ldx     #<save
        ldy     #>save
svbuf:  stx     ptr2
        sty     ptr2+1
        sta     par
        jsr     offs
        lda     #0
        sta     mw              ; place in save
        lda     _dh_h
        sta     cnt
        ldx     _dh_y
sv_line:
        cpx     #192
        bcs     sv_done
        stx     ln
        jsr     row
        sta     AUXON
        jsr     sv_bytes
        sta     MAINON
        jsr     sv_bytes
        ldx     ln
        inx
        dec     cnt
        bne     sv_line
sv_done:
        rts
sv_bytes:
        ldy     ya
        ldx     mw
sb_lp:  stx     tmp1
        lda     par
        bne     sb_put
        lda     (ptr1),y
        sty     tmp2
        ldy     tmp1
        sta     (ptr2),y
        ldy     tmp2
        jmp     sb_nx
sb_put: sty     tmp2
        ldy     tmp1
        lda     (ptr2),y
        ldy     tmp2
        sta     (ptr1),y
sb_nx:  inx
        iny
        cpy     yb
        beq     sb_lp
        bcc     sb_lp
        stx     mw
        rts

; The color of a Hi-Res pattern (hg_fg, hg_bg: its even column's byte, X 0, and odd's, X 1) in A.
hcolor: stx     tmp3            ; (its palette bit, bit 7)
        and     #$7F
        beq     hc_black
        cmp     #$7F
        beq     hc_white
        cmp     #$2A
        bne     hc_55
        lda     #12             ; green, or orange with the palette bit
        bit     tmp3
        bpl     hc_rts
        lda     #9
        rts
hc_55:  lda     #3              ; violet, or blue
        bit     tmp3
        bpl     hc_rts
        lda     #6
hc_rts: rts
hc_black:
        lda     #0
        rts
hc_white:
        lda     #15
        rts

; hgr_char: the character in A in the cell at (hg_x, hg_y), as hgr.s's, with its 7 dots doubled to 14.
d_char: pha
        jsr     _tick_poll
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
:       cmp     #97
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
:       ; the colors: fa, fm the foreground's bytes at the cell's auxiliary and main bytes, ba, bm the
        ; background's (cell x is bytes 2x and 2x + 1, whose k mod 4 is 0 and 1, or 2 and 3)
        and     #1
        asl     a
        sta     tmp1
        lda     _dh_tfg
        bpl     :+
        ldx     _hg_fg
        lda     _hg_fg
        jsr     hcolor
:       asl     a
        asl     a
        ora     tmp1
        tax
        lda     colpat,x
        sta     fa
        lda     colpat+1,x
        sta     fm
        lda     _dh_tbg
        bpl     :+
        ldx     _hg_bg
        lda     _hg_bg
        jsr     hcolor
:       asl     a
        asl     a
        ora     tmp1
        tax
        lda     colpat,x
        sta     ba
        lda     colpat+1,x
        sta     bm
        lda     #0
        sta     tmp4
ch_line:
        lda     _hg_y
        clc
        adc     tmp4
        cmp     #192
        bcs     ch_next
        tax
        jsr     row
        ldy     tmp4
        lda     (ptr2),y
        ldx     _hg_bold
        bne     :+
        ldx     _hg_key
        beq     :++
:       sta     tmp3            ; bold: each dot doubled to the right
        asl     a
        ora     tmp3
:       and     #$7F
        sta     glyph
        tax
        ldy     _hg_x
        sta     AUXON
        lda     dbllo,x
        and     fa
        sta     tmp3
        lda     dbllo,x
        eor     #$7F
        and     ba
        ora     tmp3
        sta     (ptr1),y
        sta     MAINON
        lda     dblhi,x
        and     fm
        sta     tmp3
        lda     dblhi,x
        eor     #$7F
        and     bm
        ora     tmp3
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

; hgr_rect: hg_w columns (of 14 dots) by hg_h lines at (hg_x, hg_y) in the Hi-Res fill hg_p, in the
; Double Hi-Res color nearest it.
d_rect: ldx     #0
        ldy     #0
rf_lp:  lda     fillpat,x
        cmp     _hg_p
        bne     rf_no
        lda     fillpat+1,x
        cmp     _hg_p+1
        bne     rf_no
        lda     fillpat+2,x
        cmp     _hg_p+2
        beq     rf_got
rf_no:  inx
        inx
        inx
        iny
        cpy     #10
        bcc     rf_lp
        ldy     #0
rf_got: lda     fillcol,y
        jsr     setc
        lda     _hg_h
        beq     rc_done
        sta     cnt
        lda     _hg_x
        cmp     #40
        bcs     rc_done
        asl     a
        sta     k0
        clc
        adc     _hg_w
        adc     _hg_w
        cmp     #81
        bcc     :+
        lda     #80
:       sec
        sbc     #1
        sta     k1
        lda     #$7F
        sta     m0
        sta     m1
        lda     #0
        sta     _dh_xor
        ldx     _hg_y
rc_line:
        cpx     #192
        bcs     rc_done
        stx     cur
        jsr     span
        jsr     _tick_poll
        ldx     cur
        inx
        dec     cnt
        bne     rc_line
rc_done:
        rts

; hgr_blit: a Hi-Res picture, its dots doubled, in white on black (or flipped with hg_xor).
d_blit: lda     _hg_src
        sta     ptr2
        lda     _hg_src+1
        sta     ptr2+1
        ldy     #0
        lda     (ptr2),y
        sta     mw              ; width
        iny
        lda     (ptr2),y
        sta     cnt             ; height
        lda     #2
        sta     mx              ; the next byte of the picture
        ldx     _hg_y
bl_line:
        lda     cnt
        beq     bl_done
        cpx     #192
        bcs     bl_done
        stx     ln
        jsr     row
        lda     #0
        sta     rs
bl_col: lda     _hg_x
        clc
        adc     rs
        cmp     #40
        bcs     bl_skip
        sta     tmp1
        ldy     mx
        lda     (ptr2),y
        and     #$7F
        tax
        ldy     tmp1
        sta     AUXON
        lda     dbllo,x
        jsr     bl_put
        sta     MAINON
        lda     dblhi,x
        jsr     bl_put
bl_skip:inc     mx
        inc     rs
        lda     rs
        cmp     mw
        bcc     bl_col
        ldx     ln
        inx
        dec     cnt
        jmp     bl_line
bl_done:
        jmp     _tick_poll
bl_put: ldy     _hg_xor
        beq     :+
        ldy     tmp1
        eor     (ptr1),y
        sta     (ptr1),y
        rts
:       ldy     tmp1
        sta     (ptr1),y
        rts

; hg_px (a Hi-Res pixel, 2 dots) and the dot A (0 or 1) within it: the byte k (X) and bit (Y) of the line.
hpdot:  sta     tmp3
        lda     _hg_px
        ldx     _hg_px+1
        ldy     #0
hp_lp:  cpx     #0
        bne     hp_sub
        cmp     #7
        bcc     hp_got
hp_sub: sec
        sbc     #7
        bcs     :+
        dex
:       iny
        bne     hp_lp
hp_got: ; Y = the column (14 dots), A = the pixel in it: dot 2A + tmp3
        asl     a
        ora     tmp3
        sta     tmp3
        tya
        asl     a
        tax
        ldy     tmp3
        cpy     #7
        bcc     :+
        inx
        tya
        sbc     #7
        tay
:       rts

; hgr_hline: hg_w Hi-Res pixels across from (hg_px, hg_y), white (or flipped with hg_xor).
d_hline:
        lda     #0
        jsr     hpdot
        stx     k0
        lda     himask,y
        sta     m0
        lda     _hg_px
        pha
        lda     _hg_px+1
        pha
        lda     _hg_px
        clc
        adc     _hg_w
        sta     _hg_px
        bcc     :+
        inc     _hg_px+1
:       lda     _hg_px
        bne     :+
        dec     _hg_px+1
:       dec     _hg_px
        lda     #1
        jsr     hpdot
        stx     k1
        lda     lomask,y
        sta     m1
        pla
        sta     _hg_px+1
        pla
        sta     _hg_px
        ldx     _hg_y
        cpx     #192
        bcs     :+
        jsr     hl_set
        jsr     span
        lda     #0
        sta     _dh_xor
:       rts
hl_set: lda     _hg_xor
        sta     _dh_xor
        lda     #15 * 4
        sta     c4
        rts

; hgr_vline: hg_h lines down from (hg_px, hg_y), 2 dots wide.
d_vline:
        lda     #0
        jsr     hpdot
        stx     k0
        lda     himask,y
        sta     m0
        lda     #1
        jsr     hpdot
        stx     k1
        lda     lomask,y
        sta     m1
        jsr     hl_set
        lda     _hg_h
        beq     vl_done
        sta     cnt
        ldx     _hg_y
vl_line:
        cpx     #192
        bcs     vl_done
        stx     cur
        jsr     span
        ldx     cur
        inx
        dec     cnt
        bne     vl_line
vl_done:
        lda     #0
        sta     _dh_xor
        rts

; hgr_ptr: the pointer, a white arrow edged in black with its tip at the color pixel hg_px / 2, line hg_y,
; drawn over what is there, which is kept; called again, it puts that back (ui.c calls it in pairs, as it
; flips the Hi-Res hand).
d_ptr:  lda     _dh_x           ; (kept for the drawing the pointer may come between)
        pha
        lda     _dh_y
        pha
        lda     _dh_w
        pha
        lda     _dh_h
        pha
        lda     _dh_c
        pha
        lda     _dh_c2
        pha
        lda     _hg_px+1
        lsr     a
        lda     _hg_px
        ror     a
        sta     _dh_x
        lda     _hg_y
        sta     _dh_y
        lda     #7
        sta     _dh_w
        lda     #9
        sta     _dh_h
        lda     #0
        sta     _dh_xor
        lda     #$80
        sta     _dh_c2
        lda     pt_on
        eor     #1
        sta     pt_on
        beq     pt_back
        lda     #0              ; keep what is there
        ldx     #<psave
        ldy     #>psave
        jsr     svbuf
        lda     _dh_src
        pha
        lda     _dh_src+1
        pha
        lda     #<arrow_e
        ldx     #>arrow_e
        ldy     #0
        jsr     pt_mask
        lda     #<arrow
        ldx     #>arrow
        ldy     #15
        jsr     pt_mask
        pla
        sta     _dh_src+1
        pla
        sta     _dh_src
        jmp     pt_done
pt_back:lda     #1
        ldx     #<psave
        ldy     #>psave
        jsr     svbuf
pt_done:pla
        sta     _dh_c2
        pla
        sta     _dh_c
        pla
        sta     _dh_h
        pla
        sta     _dh_w
        pla
        sta     _dh_y
        pla
        sta     _dh_x
        rts
pt_mask:sta     _dh_src
        stx     _dh_src+1
        sty     _dh_c
        jmp     _dhr_mask
