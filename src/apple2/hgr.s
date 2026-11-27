; ParlorNet for the Apple IIe and IIgs: the graphics screen's primitives.
;
; On the IIe the screen is Double Hi-Res, and everything that draws on it is in the kit (dhr.s), which runs
; in the auxiliary memory at $0800. The routines here are its entries (kcall). The parameters are the same
; as the IIgs's Hi-Res routines take (hg_*), in the zero page on the IIe, where the kit sees them.
;
; On the IIgs the screen is Hi-Res page 1 in bank $E0, drawn by the routines below. Positions are byte
; columns (0-39, 7 pixels each) and lines (0-191), except for lines and the pointer, which take a pixel
; column (0-279). Color fills are patterns per byte: one for even columns and one for odd ones, and a
; second pair for odd lines, so a fill can be a color, white, black or a dither.

        .include "a2gs.inc"
        .importzp       ptr1, ptr2, tmp1, tmp2, tmp3, tmp4
; HP points into the screen: a long pointer into bank $E0 on the IIgs (a2gs.inc).
.ifdef IIGS
        .importzp       _gs_hp
        .define HP _gs_hp
.endif
        .import         _font

        .export         _hgr_clear, _hgr_rect, _hgr_blit, _hgr_char, _hgr_text, _hgr_vline, _hgr_hline
        .export         _hgr_ptr
        .export         _hgr_ylo, _hgr_yhi
.ifdef IIGS
        .export         _hg_x, _hg_y, _hg_w, _hg_h, _hg_p, _hg_src, _hg_px, _hg_fg, _hg_bg, _hg_xor, _hg_bold
        .export         _hg_key
        .segment "DATA"
.else
        .exportzp       _hg_x, _hg_y, _hg_w, _hg_h, _hg_p, _hg_src, _hg_px, _hg_fg, _hg_bg, _hg_xor, _hg_bold
        .exportzp       _hg_key, zka, zmono, _hg_m
        .export         _dhr_call, ktick, _dhr_install
        .segment "ZEROPAGE"
zka:    .res    1               ; kcall: the entry's A, for the kit
zmono:  .res    1               ; and mono (machine.h)
_hg_m:  .res    3               ; d_hrow: which pixels of each of its bytes to write (dhr.h)
.endif
_hg_x:  .res    1
_hg_y:  .res    1
_hg_w:  .res    1
_hg_h:  .res    1
_hg_p:  .res    4               ; fill: on the IIgs, even line even column, even line odd column, odd line ...;
                                ; on the IIe the first is the fill's number (ui.c)
_hg_fg: .res    2               ; text: foreground for even and odd columns (on the IIe, its number)
_hg_bg: .res    2               ; text: background
_hg_xor:.res    1               ; 1: lines and blits are exclusive-or
_hg_bold:.res   1               ; 1: text in bold
_hg_key:.res    1               ; characters still to draw in bold as a key legend (hgr_char)
_hg_src:.res    2
_hg_px: .res    2

        .segment "RODATA"
ylo:    .repeat 192, ln
        .byte   <($2000 + (ln .mod 8) * $400 + ((ln / 8) .mod 8) * $80 + (ln / 64) * $28)
        .endrepeat
yhi:    .repeat 192, ln
        .byte   >($2000 + (ln .mod 8) * $400 + ((ln / 8) .mod 8) * $80 + (ln / 64) * $28)
        .endrepeat

_hgr_ylo = ylo                  ; the line address tables, for the kit (dhr.s) and for drawing in C
_hgr_yhi = yhi

        .segment "DATA"
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


        .segment "CODE"

.ifndef IIGS
        .import         _mono, _tick_poll, _dl_x, _dl_buf
        .importzp       PARMN
        .import         __DHRKIT_LOAD__, __DHRKIT_RUN__, __DHRKIT_SIZE__, __AUXART_SIZE__
        .import         __MIRROR_RUN__, __MIRROR_SIZE__
        .importzp       ptr3, ptr4

KIT     = $0800                 ; the kit's entry, in the auxiliary memory
KPLACE  = 7                     ; the kit's entries from here take the parameters at $0200 (dhr.h)
KPXLINE = 10                    ; and this one dl_buf too
KVBAR   = 15                    ; (but not this one or those after it, which take only the hg_ parameters)

; The entries: X the kit's entry number, A passed on to it (hgr_char's character).
_hgr_clear:
        ldx     #0
        .byte   $2C             ; (BIT abs: skips the next ldx)
_hgr_rect:
        ldx     #1
        .byte   $2C
_hgr_blit:
        ldx     #2
        .byte   $2C
_hgr_char:
        ldx     #3
        .byte   $2C
_hgr_vline:
        ldx     #4
        .byte   $2C
_hgr_hline:
        ldx     #5
        .byte   $2C
_hgr_ptr:
        ldx     #6
        jmp     kcall

; void __fastcall__ dhr_call(unsigned char entry): the kit's entry of that number (dhr.h).
_dhr_call:
        tax
        jmp     kcall

; The code that runs while the auxiliary memory is switched in for reading is here, and the start-up copies
; it to the same addresses there (dhr_install), so that it goes on running whichever memory is read.
        .segment "MIRROR"

; Call the kit's entry X with A. 80STORE and HIRES go on for its PAGE2, which puts the auxiliary memory at
; $2000-$3FFF only with both on (on the text screen HIRES changes nothing that shows). The kit runs with the
; auxiliary memory switched in for reading and writing (RAMRD, RAMWRT), so it sees the parameters at $0200
; there: they are copied over first, with dl_buf for the entries that read it.
kcall:  sta     zka
        sta     $C001           ; 80STORE
        bit     $C057           ; HIRES
        lda     _mono
        sta     zmono
        cpx     #KPLACE
        bcc     @go
        cpx     #KVBAR
        bcs     @go
        sta     $C005           ; RAMWRT: writes go to the auxiliary memory
        ldy     #PARMN - 1
:       lda     _dl_x,y
        sta     _dl_x,y
        dey
        bpl     :-
        cpx     #KPXLINE
        bne     @copied
        ldy     #192
:       lda     _dl_buf-1,y
        sta     _dl_buf-1,y
        lda     _dl_buf+191,y
        sta     _dl_buf+191,y
        dey
        bne     :-
@copied:sta     $C004
@go:    sta     $C003           ; RAMRD: reads from the auxiliary memory (this code is there too)
        sta     $C005
        jsr     KIT
        sta     $C002
        sta     $C004
        rts

; For the kit: the clock (tick_poll), with the main memory switched in for it.
ktick:  sta     $C002
        sta     $C004
        jsr     _tick_poll
        sta     $C005
        sta     $C003
        rts

        .segment "ONCE"

; void dhr_install(void), at start-up: the kit and the games' pictures from where the loaders put them
; ($2800, DHRSTAGE) into the
; auxiliary memory at $0800 (AUXK), and there too at their own addresses the font, the line tables and the
; code above, which the kit and kcall use with the auxiliary memory read. The parameters in the zero page
; start at 0.
_dhr_install:
        lda     #0
        ldx     #_hg_px + 1 - _hg_x
:       sta     _hg_x,x
        dex
        bpl     :-
        sta     $C005
        lda     #<__DHRKIT_LOAD__
        ldx     #>__DHRKIT_LOAD__
        ldy     #<__DHRKIT_RUN__
        sty     ptr4
        ldy     #>__DHRKIT_RUN__
        sty     ptr4+1
        ldy     #>(__DHRKIT_SIZE__ + __AUXART_SIZE__)
        sty     tmp2
        ldy     #<(__DHRKIT_SIZE__ + __AUXART_SIZE__)
        jsr     copy
        ldy     #>(97 * 8)
        sty     tmp2
        lda     #<_font
        ldx     #>_font
        ldy     #<(97 * 8)
        jsr     same
        ldy     #>384
        sty     tmp2
        lda     #<ylo
        ldx     #>ylo
        ldy     #<384
        jsr     same
        ldy     #>__MIRROR_SIZE__
        sty     tmp2
        lda     #<__MIRROR_RUN__
        ldx     #>__MIRROR_RUN__
        ldy     #<__MIRROR_SIZE__
        jsr     same
        sta     $C004
        rts
; tmp2 * 256 + Y bytes from A X to the same address (copy), or to ptr4 (copy).
same:   sta     ptr4
        stx     ptr4+1
copy:   sta     ptr3
        stx     ptr3+1
        sty     tmp1
        ldy     #0
@lp:    lda     tmp1
        ora     tmp2
        beq     @done
        lda     (ptr3),y
        sta     (ptr4),y
        inc     ptr3
        bne     :+
        inc     ptr3+1
:       inc     ptr4
        bne     :+
        inc     ptr4+1
:       lda     tmp1
        bne     :+
        dec     tmp2
:       dec     tmp1
        jmp     @lp
@done:  rts

        .segment "CODE"

.else
        .segment "DATA"
ch_t:   .byte   0
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
; glyph and the character after it, and after a code 1 to 31 (K1 ... in machdefs.h) that many characters,
; which takes no cell.
_hgr_char:
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
.endif
