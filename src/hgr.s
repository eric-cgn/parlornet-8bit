; ParlorNet for the Apple IIe: Hi-Res page 1 drawing.
;
; Positions are byte columns (0-39, 7 pixels each) and lines (0-191), except for lines and the pointer,
; which take a pixel column (0-279). The parameters are globals (hg_*), set by the caller.
; Color fills are patterns per byte: one for even columns and one for odd ones, and a second pair for
; odd lines, so a fill can be a color, white, black or a dither.

        .include "a2gs.inc"
        .importzp       ptr1, ptr2, tmp1, tmp2, tmp3, tmp4
; HP points into the screen: ptr1 on the IIe, a long pointer into bank $E0 on the IIgs (a2gs.inc).
.ifdef IIGS
        .importzp       _gs_hp
        .define HP _gs_hp
.else
        .define HP ptr1
.endif
        .import         _font
        .import         _tick_poll      ; (the IIe: in the longer loops, so that no tick is missed)

        .export         _hg_x, _hg_y, _hg_w, _hg_h, _hg_p, _hg_src, _hg_px, _hg_fg, _hg_bg, _hg_xor, _hg_bold
        .export         _hg_key
        .export         _hgr_clear, _hgr_rect, _hgr_blit, _hgr_char, _hgr_text, _hgr_vline, _hgr_hline
        .export         _hgr_ptr
        .export         _hgr_ylo, _hgr_yhi

        .segment "DATA"
_hg_x:  .byte   0
_hg_y:  .byte   0
_hg_w:  .byte   0
_hg_h:  .byte   0
_hg_p:  .byte   0,0,0,0         ; fill: even line even column, even line odd column, odd line ...
_hg_fg: .byte   $7F,$7F         ; text: foreground for even and odd columns
_hg_bg: .byte   0,0             ; text: background
_hg_xor:.byte   0               ; 1: lines and blits are exclusive-or
_hg_bold:.byte  0               ; 1: text in bold
_hg_key:.byte   0               ; characters still to draw in bold as a key legend (hgr_char)
_hg_src:.word   0
_hg_px: .word   0

        .segment "RODATA"
ylo:    .repeat 192, ln
        .byte   <($2000 + (ln .mod 8) * $400 + ((ln / 8) .mod 8) * $80 + (ln / 64) * $28)
        .endrepeat
yhi:    .repeat 192, ln
        .byte   >($2000 + (ln .mod 8) * $400 + ((ln / 8) .mod 8) * $80 + (ln / 64) * $28)
        .endrepeat

_hgr_ylo = ylo                  ; the line address tables, for drawing in C (splash.c)
_hgr_yhi = yhi

        .segment "CODE"

; Set HP to the start of line X.
rowx:   lda     ylo,x
        sta     HP
        lda     yhi,x
        sta     HP+1
        rts

; void hgr_clear(void)
_hgr_clear:
        lda     #$20
        sta     HP+1
        lda     #0
        sta     HP
        tay
:       scr_sta HP
        iny
        bne     :-
        inc     HP+1
.ifndef IIGS
        jsr     _tick_poll
        lda     #0
.endif
        ldx     HP+1
        cpx     #$40
        bcc     :-
        rts

; void hgr_rect(void): fill hg_w columns by hg_h lines at (hg_x, hg_y) with hg_p.
_hgr_rect:
        lda     _hg_h
        beq     rr_done
        sta     tmp2
        ldx     _hg_y
rr_line:
.ifndef IIGS
        jsr     _tick_poll
.endif
        cpx     #192
        bcs     rr_done
        jsr     rowx
        txa
        and     #1
        asl     a
        sta     tmp1            ; 0 or 2: which pair of patterns
        ldy     _hg_x
        lda     _hg_w
        sta     tmp3
rr_col: cpy     #40
        bcs     rr_next
        tya
        and     #1
        ora     tmp1
        stx     tmp4
        tax
        lda     _hg_p,x
        ldx     tmp4
        scr_sta HP
        iny
        dec     tmp3
        bne     rr_col
rr_next:
        inx
        dec     tmp2
        bne     rr_line
rr_done:
        rts

; void hgr_blit(void): draw the sprite at hg_src (width, height, rows) at (hg_x, hg_y).
_hgr_blit:
        lda     _hg_src
        sta     ptr2
        lda     _hg_src+1
        sta     ptr2+1
        ldy     #0
        lda     (ptr2),y
        sta     tmp3            ; width
        iny
        lda     (ptr2),y
        sta     tmp2            ; height
        lda     ptr2
        clc
        adc     #2
        sta     ptr2
        bcc     :+
        inc     ptr2+1
:       ldx     _hg_y
bl_line:
.ifndef IIGS
        jsr     _tick_poll
.endif
        lda     tmp2
        beq     bl_done
        cpx     #192
        bcs     bl_done
        jsr     rowx
        lda     HP
        clc
        adc     _hg_x
        sta     HP
        ldy     #0
bl_col: lda     _hg_xor
        beq     :+
        lda     (ptr2),y
        scr_eor HP
        jmp     :++
:       lda     (ptr2),y
:       scr_sta HP
        iny
        cpy     tmp3
        bcc     bl_col
        lda     ptr2
        clc
        adc     tmp3
        sta     ptr2
        bcc     :+
        inc     ptr2+1
:       inx
        dec     tmp2
        jmp     bl_line
bl_done:
        rts

; void __fastcall__ hgr_char(char c): a character in the cell at (hg_x, hg_y), in hg_fg on hg_bg;
; hg_x moves on by one. Keys are marked in bold (in monochrome; in color all text is): the Open-Apple
; glyph and the character after it, and after a code 1 to 31 (K1 ... in a2.h) that many characters,
; which takes no cell.
_hgr_char:
.ifndef IIGS
        pha
        jsr     _tick_poll
        pla
.endif
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
:
        cmp     #97             ; (to $80: the Open-Apple and Closed-Apple glyphs at $7F and $80)
        bcs     ch_done
        ; ptr2 = _font + c * 8
        ldx     #0
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
        bcs     ch_done
        and     #1
        tax
        lda     _hg_fg,x
        sta     tmp1
        lda     _hg_bg,x
        sta     tmp2
        ora     tmp1
        and     #$80
        sta     tmp3            ; the palette bit
        lda     #0
        sta     tmp4            ; glyph line
ch_line:
        lda     _hg_y
        clc
        adc     tmp4
        cmp     #192
        bcs     ch_next
        tax
        jsr     rowx
        ldy     tmp4
        lda     (ptr2),y        ; the glyph's line g
        ldx     _hg_bold
        bne     ch_bold
        ldx     _hg_key
        beq     :+
ch_bold:
        sta     ch_t            ; bold: each pixel doubled to the right, so strokes are white in color
        asl     a
        ora     ch_t
        and     #$7F
:       tax
        eor     #$7F            ; out = (bg & ~g) | (fg & g) | the palette bit
        and     tmp2
        and     #$7F
        sta     ch_t
        txa
        and     tmp1
        and     #$7F
        ora     ch_t
        ora     tmp3
        ldy     _hg_x
        scr_sta HP
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

        .segment "DATA"
ch_t:   .byte   0
tx_ptr: .word   0
        .segment "CODE"

; void __fastcall__ hgr_text(const char *s): characters from (hg_x, hg_y) on.
_hgr_text:
        sta     tx_ptr
        stx     tx_ptr+1
:       ldy     #0
        lda     tx_ptr
        sta     ptr2
        lda     tx_ptr+1
        sta     ptr2+1
        lda     (ptr2),y
        beq     :+
        jsr     _hgr_char
        inc     tx_ptr
        bne     :-
        inc     tx_ptr+1
        bne     :-
:       rts

; Split the pixel column hg_px into a byte column (tmp3) and a bit (tmp4 = the mask).
pxsplit:
        lda     _hg_px
        ldx     _hg_px+1
        ldy     #0
ps1:    cpx     #0
        bne     ps2
        cmp     #7
        bcc     ps3
ps2:    sec
        sbc     #7
        bcs     :+
        dex
:       iny
        bne     ps1
ps3:    sty     tmp3
        tax
        lda     #1
        cpx     #0
        beq     ps4
:       asl     a
        dex
        bne     :-
ps4:    sta     tmp4
        rts

; void hgr_vline(void): a line hg_h long down from pixel (hg_px, hg_y); set, or flipped with hg_xor.
_hgr_vline:
        jsr     pxsplit
        lda     tmp3
        cmp     #40
        bcs     vl_done
        lda     _hg_h
        sta     tmp2
        ldx     _hg_y
vl1:    lda     tmp2
        beq     vl_done
        cpx     #192
        bcs     vl_done
        jsr     rowx
        ldy     tmp3
        lda     _hg_xor
        bne     :+
        scr_lda HP
        ora     tmp4
        scr_sta HP
        jmp     :++
:       scr_lda HP
        eor     tmp4
        scr_sta HP
:       inx
        dec     tmp2
        jmp     vl1
vl_done:
        rts

; void hgr_hline(void): a line hg_w pixels long (up to 255) across from pixel (hg_px, hg_y).
_hgr_hline:
        jsr     pxsplit
        ldx     _hg_y
        cpx     #192
        bcs     hl_done
        jsr     rowx
        lda     _hg_w
        sta     tmp2
        ldy     tmp3
hl1:    lda     tmp2
        beq     hl_done
        cpy     #40
        bcs     hl_done
        lda     _hg_xor
        bne     :+
        scr_lda HP
        ora     tmp4
        jmp     :++
:       scr_lda HP
        eor     tmp4
:       scr_sta HP
        dec     tmp2
        asl     tmp4
        lda     tmp4
        cmp     #$80
        bcc     hl1
        lda     #1
        sta     tmp4
        iny
        jmp     hl1
hl_done:
        rts

; void hgr_ptr(void): flip the pointer (a pointing hand) with its fingertip at pixel (hg_px, hg_y).
; Drawing it twice takes it away.
_hgr_ptr:
        ; the fingertip is 5 pixels into the picture
        lda     _hg_px
        sec
        sbc     #5
        sta     pt_x
        lda     _hg_px+1
        sbc     #0
        sta     pt_x+1
        bcs     :+
        lda     #0
        sta     pt_x
        sta     pt_x+1
:       lda     _hg_px
        pha
        lda     _hg_px+1
        pha
        lda     pt_x
        sta     _hg_px
        lda     pt_x+1
        sta     _hg_px+1
        jsr     pxsplit         ; tmp3 = column, tmp4 = mask of the first pixel
        pla
        sta     _hg_px+1
        pla
        sta     _hg_px
        ; the shift: which bit tmp4 is
        ldx     #0
        lda     tmp4
:       lsr     a
        beq     :+
        inx
        bne     :-
:       stx     pt_sh
        lda     #0
        sta     pt_r
pt_line:
        lda     _hg_y
        clc
        adc     pt_r
        cmp     #192
        bcs     pt_next
        tax
        jsr     rowx
        lda     pt_r
        asl     a
        tax
        lda     pointer,x
        sta     pt_v
        lda     pointer+1,x
        sta     pt_v+1
        lda     #0
        sta     pt_v+2
        ldx     pt_sh
        beq     :++
:       asl     pt_v
        rol     pt_v+1
        rol     pt_v+2
        dex
        bne     :-
:       ; the three 7-pixel groups
        lda     pt_v
        and     #$7F
        sta     pt_g
        lda     pt_v
        asl     a               ; bit 7 into C
        lda     pt_v+1
        rol     a
        and     #$7F
        sta     pt_g+1
        lda     pt_v+1
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     ch_t
        lda     pt_v+2
        asl     a
        asl     a
        ora     ch_t
        and     #$7F
        sta     pt_g+2
        ldy     tmp3
        ldx     #0
:       cpy     #40
        bcs     pt_next
        scr_lda HP
        eor     pt_g,x
        scr_sta HP
        iny
        inx
        cpx     #3
        bcc     :-
pt_next:
        inc     pt_r
        lda     pt_r
        cmp     #PTRH
        bcs     :+
        jmp     pt_line
:       rts

        .segment "DATA"
pt_x:   .word   0
pt_sh:  .byte   0
pt_r:   .byte   0
pt_v:   .byte   0,0,0
pt_g:   .byte   0,0,0

        .segment "RODATA"
; The pointing hand, 13 pixels across (bit 0 on the left), outline only.
PTRH = 13
pointer:
        .word   %0000000110000  ; ....##.......
        .word   %0000001001000  ; ...#..#......
        .word   %0000001001000  ; ...#..#......
        .word   %0000111001000  ; ...#..###....
        .word   %0011001001000  ; ...#..#..##..
        .word   %0101001001110  ; .###..#..#.#.
        .word   %0100000001001  ; #..#.......#.
        .word   %0100000000001  ; #..........#.
        .word   %0100000000010  ; .#.........#.
        .word   %0010000000100  ; ..#.......#..
        .word   %0010000001000  ; ...#......#..
        .word   %0011111111000  ; ...########..
        .word   %0000000000000
