; ParlorNet on the Commodore 64: drawing on the bitmap (320 by 200 at $E000, two colors in each 8 by 8 cell,
; the colors at $CC00: the high nybble for set pixels, the low one for clear ones).
;
; Positions are cell columns (0-39) and lines (0-199), except for lines and dots, which take a pixel column
; (0-319). The parameters are globals (hg_*), set by the caller, as on the Apple II (hgr.s).

        .importzp       ptr1, ptr2, tmp1, tmp2, tmp3, tmp4
        .export         _hg_x, _hg_y, _hg_w, _hg_h, _hg_fg, _hg_bg, _hg_kfg, _hg_key, _hg_src, _hg_px
        .export         _hg_pat, _hg_xor, _hg_bold, _hg_kbg
        .export         _hgr_clear, _hgr_char, _hgr_text, _hgr_rect, _hgr_color, _hgr_blit
        .export         _hgr_hline, _hgr_vline, _hgr_plot, _hgr_line_addr, _hgr_rowlo, _hgr_rowhi
        .export         _spr_load

BITMAP  = $E000
BMCOL   = $CC00
CHARS   = $C000

        .segment "DATA"
_hg_x:  .byte   0
_hg_y:  .byte   0
_hg_w:  .byte   0
_hg_h:  .byte   0
_hg_fg: .byte   1               ; colors for text and fills (0-15)
_hg_bg: .byte   6
_hg_kfg:.byte   7               ; a key's color in a legend (on the usual background)
_hg_key:.byte   0               ; characters still to draw as a key (hgr_char)
_hg_pat:.byte   0               ; the fill's pattern (hgr_rect)
_hg_xor:.byte   0               ; 1: lines, dots and blits are exclusive-or
_hg_bold:.byte  0               ; (the Apple's; the C64's font is bold already)
_hg_src:.word   0
_hg_px: .word   0
_hg_kbg:.byte   6               ; the background a key's color is used on (the usual one: ui.c)

        .segment "RODATA"
_hgr_rowlo:
        .repeat 25, r
        .byte   <(BITMAP + r * 320)
        .endrepeat
_hgr_rowhi:
        .repeat 25, r
        .byte   >(BITMAP + r * 320)
        .endrepeat
crowlo: .repeat 25, r
        .byte   <(BMCOL + r * 40)
        .endrepeat
crowhi: .repeat 25, r
        .byte   >(BMCOL + r * 40)
        .endrepeat
bits:   .byte   $80,$40,$20,$10,$08,$04,$02,$01

        .segment "CODE"

; ptr1 = the bitmap's byte for cell column X... more simply: line Y's byte in column A (0-39).
; In: A column, Y line. Out: ptr1. Keeps X.
lineaddr:
        sta     tmp4
        tya
        lsr     a
        lsr     a
        lsr     a
        sty     tmp3
        tay
        lda     tmp3
        and     #7
        clc
        adc     _hgr_rowlo,y
        sta     ptr1
        lda     _hgr_rowhi,y
        adc     #0
        sta     ptr1+1
        lda     tmp4            ; + column * 8
        asl     a
        asl     a
        asl     a
        bcc     :+
        inc     ptr1+1
        clc
:       adc     ptr1
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       ldy     tmp3
        rts

; unsigned char *hgr_line_addr(void): the byte of line hg_y in column hg_x (for drawing in C).
_hgr_line_addr:
        lda     _hg_x
        ldy     _hg_y
        jsr     lineaddr
        lda     ptr1
        ldx     ptr1+1
        rts

; void hgr_clear(void): all clear, every cell in hg_fg on hg_bg.
_hgr_clear:
        lda     #<BITMAP
        sta     ptr1
        lda     #>BITMAP
        sta     ptr1+1
        lda     #0
        tay
        ldx     #$1F            ; $1F40 bytes: 31 pages and $40
:       sta     (ptr1),y
        iny
        bne     :-
        inc     ptr1+1
        dex
        bne     :-
:       sta     (ptr1),y
        iny
        cpy     #$40
        bne     :-
        jsr     colbyte
        ldx     #0
:       sta     BMCOL,x
        sta     BMCOL+$100,x
        sta     BMCOL+$200,x
        sta     BMCOL+$2E8,x
        inx
        bne     :-
        rts

; A = hg_fg * 16 + hg_bg
colbyte:
        lda     _hg_fg
        asl     a
        asl     a
        asl     a
        asl     a
        ora     _hg_bg
        rts

; void hgr_color(void): cells hg_x.. (hg_w of them) on rows hg_y / 8 to (hg_y + hg_h - 1) / 8 in hg_fg
; on hg_bg.
_hgr_color:
        lda     _hg_h
        beq     hc_done
        lda     _hg_w
        beq     hc_done
        lda     _hg_y
        lsr     a
        lsr     a
        lsr     a
        sta     tmp1            ; the first row
        lda     _hg_y
        clc
        adc     _hg_h
        sec
        sbc     #1
        lsr     a
        lsr     a
        lsr     a
        sta     tmp2            ; the last
        jsr     colbyte
        sta     tmp3
hc_row: ldx     tmp1
        cpx     #25
        bcs     hc_done
        lda     crowlo,x
        sta     ptr1
        lda     crowhi,x
        sta     ptr1+1
        ldy     _hg_x
        ldx     _hg_w
        lda     tmp3
:       cpy     #40
        bcs     :+
        sta     (ptr1),y
        iny
        dex
        bne     :-
:       inc     tmp1
        lda     tmp2
        cmp     tmp1
        bcs     hc_row
hc_done:
        rts

; void hgr_rect(void): lines hg_y.. (hg_h of them) of cells hg_x.. (hg_w) filled with the byte hg_pat;
; the cells touched in hg_fg on hg_bg.
_hgr_rect:
        jsr     _hgr_color
        lda     _hg_w
        beq     rr_done
        lda     _hg_h
        beq     rr_done
        sta     tmp2
        lda     _hg_y
        sta     rr_y
rr_line:
        ldy     rr_y
        cpy     #200
        bcs     rr_done
        lda     _hg_x
        jsr     lineaddr
        ldx     _hg_w
        ldy     #0
        lda     _hg_x
        sta     tmp1
:       lda     tmp1
        cmp     #40
        bcs     :+
        lda     _hg_pat
        sta     (ptr1),y
        tya
        clc
        adc     #8
        tay
        bcc     rr_next
        inc     ptr1+1          ; (past 32 cells, the line's next page)
rr_next:
        inc     tmp1
        dex
        bne     :-
:       inc     rr_y
        dec     tmp2
        bne     rr_line
rr_done:
        rts
        .segment "DATA"
rr_y:   .byte   0
        .segment "CODE"

; void hgr_blit(void): the picture at hg_src (width in bytes, height in lines, then the rows, top first,
; each byte 8 pixels, the leftmost in bit 7) at (hg_x, hg_y); exclusive-or with hg_xor. Colors stay.
_hgr_blit:
        lda     _hg_src
        sta     ptr2
        lda     _hg_src+1
        sta     ptr2+1
        ldy     #0
        lda     (ptr2),y
        sta     bl_w
        iny
        lda     (ptr2),y
        sta     tmp2
        lda     ptr2
        clc
        adc     #2
        sta     ptr2
        bcc     :+
        inc     ptr2+1
:       lda     _hg_y
        sta     rr_y
bl_line:
        lda     tmp2
        beq     bl_done
        ldy     rr_y
        cpy     #200
        bcs     bl_done
        lda     _hg_x
        jsr     lineaddr
        ldx     #0              ; source column
        ldy     #0              ; destination offset (8 a column)
bl_col: stx     tmp1
        txa
        clc
        adc     _hg_x
        cmp     #40
        bcs     bl_next
        sty     tmp3
        ldy     tmp1
        lda     (ptr2),y
        ldy     tmp3
        ldx     _hg_xor
        beq     :+
        eor     (ptr1),y
:       sta     (ptr1),y
        tya
        clc
        adc     #8
        tay
        ldx     tmp1
        inx
        cpx     bl_w
        bcc     bl_col
bl_next:
        lda     ptr2
        clc
        adc     bl_w
        sta     ptr2
        bcc     :+
        inc     ptr2+1
:       inc     rr_y
        dec     tmp2
        jmp     bl_line
bl_done:
        rts
        .segment "DATA"
bl_w:   .byte   0
        .segment "CODE"

; void __fastcall__ hgr_char(char c): a character in cell column hg_x at line hg_y, in hg_fg on hg_bg (a
; key's characters in hg_kfg, when on the usual background); hg_x moves on by one. A code 1 to 31 (K1 ...
; in machdefs.h) marks that many characters after it as a key, and takes no cell; so does the C= logo ($7F),
; for itself and the character after it.
_hgr_char:
        cmp     #$7F
        bne     :+
        ldx     #2
        stx     _hg_key
:       cmp     #32
        bcs     :+
        sta     _hg_key
        rts
:       ldx     _hg_x
        cpx     #40
        bcc     :+
        jmp     ch_done
:       ; the glyph: CHARS + c * 8
        ldx     #0
        stx     ptr2+1
        asl     a
        rol     ptr2+1
        asl     a
        rol     ptr2+1
        asl     a
        rol     ptr2+1
        sta     ptr2
        lda     ptr2+1
        clc
        adc     #>CHARS
        sta     ptr2+1
        ; its colors
        lda     _hg_fg
        ldx     _hg_key
        beq     :+
        dec     _hg_key
        ldx     _hg_bg
        cpx     _hg_kbg
        bne     :+
        lda     _hg_kfg
:       asl     a
        asl     a
        asl     a
        asl     a
        ora     _hg_bg
        sta     tmp2
        lda     _hg_y
        lsr     a
        lsr     a
        lsr     a
        tax
        jsr     cellcol
        lda     _hg_y
        and     #7
        beq     :+
        inx                     ; (across two rows of cells)
        jsr     cellcol
        ; the 8 lines
:       lda     _hg_x
        ldy     _hg_y
        jsr     lineaddr
        lda     _hg_y
        and     #7
        sta     tmp1            ; lines left in the first cell, as 8 - this
        ldy     #0
ch_row: lda     (ptr2),y
        sty     tmp3
        ldy     #0
        sta     (ptr1),y
        ldy     tmp3
        ; the next line: the next byte, or the next row of cells (+ 320 - 7)
        inc     ptr1
        bne     :+
        inc     ptr1+1
:       inc     tmp1
        lda     tmp1
        cmp     #8
        bne     :+
        lda     ptr1
        clc
        adc     #<(320 - 8)
        sta     ptr1
        lda     ptr1+1
        adc     #>(320 - 8)
        sta     ptr1+1
:       iny
        cpy     #8
        bne     ch_row
ch_done:
        inc     _hg_x
        rts

; The cell at row X, column hg_x, in color tmp2. Keeps X.
cellcol:
        cpx     #25
        bcs     :+
        lda     crowlo,x
        sta     ptr1
        lda     crowhi,x
        sta     ptr1+1
        ldy     _hg_x
        lda     tmp2
        sta     (ptr1),y
:       rts

; void __fastcall__ hgr_text(const char *s): characters from (hg_x, hg_y) on.
_hgr_text:
        sta     ht+1
        stx     ht+2
ht:     lda     $FFFF
        beq     :+
        jsr     _hgr_char
        inc     ht+1
        bne     ht
        inc     ht+2
        bne     ht
:       rts

; Pixel hg_px, line Y: ptr1 its byte, A its bit.
pixaddr:
        lda     _hg_px+1
        lsr     a
        lda     _hg_px
        ror     a
        lsr     a
        lsr     a               ; the column
        jsr     lineaddr
        lda     _hg_px
        and     #7
        tax
        lda     bits,x
        rts

; void hgr_plot(void): the dot at (hg_px, hg_y): set, or flipped with hg_xor.
_hgr_plot:
        ldy     _hg_y
        cpy     #200
        bcs     :++
        jsr     pixaddr
        ldy     #0
        ldx     _hg_xor
        beq     :+
        eor     (ptr1),y
        sta     (ptr1),y
        rts
:       ora     (ptr1),y
        sta     (ptr1),y
:       rts

; void hgr_hline(void): a line hg_w pixels long (up to 255) across from (hg_px, hg_y).
_hgr_hline:
        lda     _hg_w
        beq     hl_done
        sta     tmp2
        lda     _hg_px
        pha
        lda     _hg_px+1
        pha
:       lda     _hg_px+1        ; (within the screen)
        beq     :+
        lda     _hg_px
        cmp     #<320
        bcs     :++
:       jsr     _hgr_plot
        inc     _hg_px
        bne     :+
        inc     _hg_px+1
:       dec     tmp2
        bne     :---
        pla
        sta     _hg_px+1
        pla
        sta     _hg_px
hl_done:
        rts

; void hgr_vline(void): a line hg_h long down from (hg_px, hg_y).
_hgr_vline:
        lda     _hg_h
        beq     vl_done
        sta     tmp2
        lda     _hg_y
        pha
:       jsr     _hgr_plot
        inc     _hg_y
        dec     tmp2
        bne     :-
        pla
        sta     _hg_y
vl_done:
        rts

; void __fastcall__ spr_load(unsigned char block, const unsigned char *shape): 63 bytes of a sprite's shape
; into block (64 up: $D000 up, under the I/O, where the VIC sees them).
        .import popa, _ser_pause, _ser_resume
_spr_load:
        sta     ptr2
        stx     ptr2+1
        jsr     popa
        ldx     #0
        stx     ptr1
        lsr     a
        ror     ptr1
        lsr     a
        ror     ptr1
        clc
        adc     #$C0
        sta     ptr1+1
        jsr     _ser_pause      ; (the line's interrupts need the I/O)
        php
        sei
        lda     #$34
        sta     $01
        ldy     #62
:       lda     (ptr2),y
        sta     (ptr1),y
        dey
        bpl     :-
        lda     #$35
        sta     $01
        plp
        jmp     _ser_resume
