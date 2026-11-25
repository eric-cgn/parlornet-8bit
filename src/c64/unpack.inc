; ParlorNet on the Commodore 64: unpacking a file packed by tools/c64_pack.py, from (UP_SRC) to (UP_DST),
; which may overlap as c64_pack.py's place() arranges. The including file defines UP_SRC, UP_DST and UP_REF
; (two bytes each in zero page) and UP_CNT (a byte). Uses A, X and Y.

unpack:
        ldy     #0
        lda     (UP_SRC),y
        beq     up_done
        inc     UP_SRC
        bne     :+
        inc     UP_SRC+1
:       tax
        bmi     up_copy
        stx     UP_CNT          ; that many bytes as they are
:       lda     (UP_SRC),y
        sta     (UP_DST),y
        iny
        cpy     UP_CNT
        bne     :-
        tya
        clc
        adc     UP_SRC
        sta     UP_SRC
        bcc     up_dst
        inc     UP_SRC+1
        bcs     up_dst
up_copy:
        cpx     #$C0
        bcs     up_long
        txa                     ; (C clear)
        and     #$3F
        adc     #2
        sta     UP_CNT
        lda     (UP_SRC),y      ; d - 1: from dst + (-(d-1) - 1)
        eor     #$FF
        clc
        adc     UP_DST
        sta     UP_REF
        lda     UP_DST+1
        adc     #$FF
        sta     UP_REF+1
        inc     UP_SRC
        bne     up_do
        inc     UP_SRC+1
        bne     up_do
up_long:
        txa
        and     #$3F
        clc
        adc     #3
        sta     UP_CNT
        lda     UP_DST          ; d: from dst - d
        sec
        sbc     (UP_SRC),y
        sta     UP_REF
        iny
        lda     UP_DST+1
        sbc     (UP_SRC),y
        sta     UP_REF+1
        lda     UP_SRC
        clc
        adc     #2
        sta     UP_SRC
        bcc     up_do
        inc     UP_SRC+1
up_do:  ldy     #0
:       lda     (UP_REF),y
        sta     (UP_DST),y
        iny
        cpy     UP_CNT
        bne     :-
up_dst: tya
        clc
        adc     UP_DST
        sta     UP_DST
        bcc     unpack
        inc     UP_DST+1
        jmp     unpack
up_done:
        rts
