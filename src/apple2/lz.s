; ParlorNet for the Apple IIe: unpacking an overlay or a help packed by tools/apple2_lz.py
; (docs/overlay-packing.md), in place.
;
; The packed piece fills pages lz_from to lz_from + lz_n - 1. Its first byte says where the packed data
; starts in the first page (0 for the next page's start), and the data ends at the end of the last page.
; The output goes up from page lz_to, under the input; tools/apple2_lz.py has checked that it never
; reaches input still to be read. Uses ptr1 (the input), ptr2 (the output), ptr3 (a copy's source),
; ptr4 (a count) and tmp1 to tmp3.

        .export         _lz_unpack, _lz_from, _lz_n, _lz_to
        .importzp       ptr1, ptr2, ptr3, ptr4, tmp1, tmp2, tmp3

        .segment "BSS"
_lz_from:       .res    1
_lz_n:          .res    1
_lz_to:         .res    1

        .segment "CODE"

; void lz_unpack(void)
_lz_unpack:
        lda     _lz_from
        sta     ptr1+1
        clc
        adc     _lz_n
        sta     tmp2            ; the page where the input ends
        ldy     #0
        sty     ptr1
        sty     ptr2
        lda     (ptr1),y        ; where the data starts in the first page
        sta     ptr1
        bne     :+
        inc     ptr1+1
:       lda     _lz_to
        sta     ptr2+1
token:  lda     ptr1+1          ; the end of the input?
        cmp     tmp2
        bcc     :+
        rts
:
        lda     (ptr1),y
        sta     tmp3
        jsr     next
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        and     #7
        cmp     #7
        bne     :+
        jsr     count           ; 7 or more literals
        jmp     lits
:       sta     ptr4
        sty     ptr4+1
lits:   lda     ptr1            ; copy them from the input
        sta     ptr3
        lda     ptr1+1
        sta     ptr3+1
        jsr     copy
        lda     ptr3
        sta     ptr1
        lda     ptr3+1
        sta     ptr1+1
        cmp     tmp2            ; the end, after the last literals
        bcc     :+
        rts
:
        lda     (ptr1),y        ; the distance
        jsr     next
        bit     tmp3
        bmi     far
        sta     tmp1            ; one byte: d + 1 back
        lda     ptr2
        clc
        sbc     tmp1
        sta     ptr3
        lda     ptr2+1
        sbc     #0
        sta     ptr3+1
        jmp     length
far:    sta     tmp1            ; two bytes, low first
        lda     (ptr1),y
        jsr     next
        sta     ptr3+1
        lda     ptr2
        sec
        sbc     tmp1
        sta     ptr3
        lda     ptr2+1
        sbc     ptr3+1
        sta     ptr3+1
length: lda     tmp3
        and     #15
        cmp     #15
        bne     :+
        jsr     count
        jmp     :++
:       sta     ptr4
        sty     ptr4+1
:       lda     ptr4            ; plus 3
        clc
        adc     #3
        sta     ptr4
        bcc     :+
        inc     ptr4+1
:       jsr     copy
        jmp     token
done:   rts

; The next input byte: ptr1 one on (A and Y kept, Y being 0).
next:   inc     ptr1
        bne     :+
        inc     ptr1+1
:       rts

; A count whose token field (A) is full: ptr4 is the field plus the next byte, or when that byte is 255 the
; two bytes after it.
count:  sta     tmp1
        lda     (ptr1),y
        jsr     next
        cmp     #255
        beq     @wide
        clc
        adc     tmp1
        sta     ptr4
        tya
        adc     #0
        sta     ptr4+1
        rts
@wide:  lda     (ptr1),y
        sta     ptr4
        jsr     next
        lda     (ptr1),y
        sta     ptr4+1
        jmp     next

; ptr4 bytes from ptr3 to ptr2, a byte at a time from the first (so a copy may overlap what it makes); ptr3
; and ptr2 end one past what they copied. Leaves Y 0.
copy:   ldx     ptr4+1
        beq     @rest
@page:  lda     (ptr3),y
        sta     (ptr2),y
        iny
        bne     @page
        inc     ptr3+1
        inc     ptr2+1
        dex
        bne     @page
@rest:  ldx     ptr4
        beq     @done
@byte:  lda     (ptr3),y
        sta     (ptr2),y
        iny
        dex
        bne     @byte
        tya
        clc
        adc     ptr3
        sta     ptr3
        bcc     :+
        inc     ptr3+1
:       tya
        clc
        adc     ptr2
        sta     ptr2
        bcc     :+
        inc     ptr2+1
:       ldy     #0
@done:  rts
