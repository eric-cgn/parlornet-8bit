; ParlorNet for the Apple IIe: ProDOS MLI calls, when the program was started by PARLOR.SYSTEM.
;
; Under ProDOS the program runs in the auxiliary zero page, stack and language card (ALTZP on), with
; the games in the auxiliary language card, while ProDOS stays in the main one. An MLI call switches to
; ProDOS's side (the main zero page, stack and language card, the ROM switched in, interrupts held off)
; for the call, and back after it.

        .setcpu "6502"
        .import         popa
        .export         _mli, _pd_main_sp

        .segment "CODE"

; unsigned char __fastcall__ mli(unsigned char cmd, void *params): ProDOS's error code, 0 if none.
_mli:
        sta     m_parm
        stx     m_parm+1
        jsr     popa
        sta     m_cmd
        php
        sei
        tsx
        stx     aux_sp
        sta     $C008           ; the main zero page, stack and language card
        ldx     _pd_main_sp
        txs
        lda     $C082           ; the ROM, as ProDOS expects it
        jsr     $BF00
m_cmd:  .byte   0
m_parm: .word   0
        sta     m_res
        tsx
        stx     _pd_main_sp
        sta     $C009           ; ours again
        ldx     aux_sp
        txs
        lda     $C083           ; the language card's RAM, bank 2, to read and write
        lda     $C083
        plp
        lda     m_res
        ldx     #0
        rts

        .segment "DATA"
aux_sp:         .byte   0
m_res:          .byte   0
_pd_main_sp:    .byte   $FF     ; ProDOS's stack pointer, from PARLOR.SYSTEM ($081F)
