; ParlorNet for the Apple IIe: the Uthernet II's registers (eth.c).
;
; Every access is an absolute LDA or STA, with the slot's addresses written into the instructions by
; w_slot. An indexed or indirect store would make the 6502 read the address first, and a read of the
; data register moves the W5100's address on, so the byte would land in the wrong place.

        .setcpu "6502"
        .export         _w_slot, _w_at, _w_get, _w_put, _w_mode, _w_mode_get

        .segment "CODE"

; void __fastcall__ w_slot(unsigned char slot)
_w_slot:
        asl     a
        asl     a
        asl     a
        asl     a
        clc
        adc     #$84
        sta     p_m1+1
        sta     p_m2+1
        adc     #1
        sta     p_ah+1
        adc     #1
        sta     p_al+1
        adc     #1
        sta     p_d1+1
        sta     p_d2+1
        rts

; void __fastcall__ w_at(unsigned int address)
_w_at:
p_ah:   stx     $C095
p_al:   sta     $C096
        rts

; unsigned char w_get(void): the byte at the address, which then moves on by one
_w_get:
p_d1:   lda     $C097
        ldx     #0
        rts

; void __fastcall__ w_put(unsigned char v)
_w_put:
p_d2:   sta     $C097
        rts

; void __fastcall__ w_mode(unsigned char v): the mode register
_w_mode:
p_m1:   sta     $C094
        rts

_w_mode_get:
p_m2:   lda     $C094
        ldx     #0
        rts
