; ParlorNet on the Commodore 64: PN.BOOT, the loader, at $C800 (where the text screen will be), started by
; PARLORNET (stub.s) or, on a C128, by the boot sector's start in 64 mode (boot128.s). Through the KERNAL it
; loads the character set (PN.CHARS) to $C000 and the program (PN.MAIN, packed by tools/c64_pack.py) where
; its file says, unpacks it to $1400 and starts it there. While the program loads (about 25 seconds from a
; 1541), a bar fills the screen's bottom row: the KERNAL's interrupt goes through ours first, which follows
; the KERNAL's load pointer from where the file loads to $C000, where it ends.

SETLFS  = $FFBA
SETNAM  = $FFBD
LOAD    = $FFD5
CHROUT  = $FFD2
MAIN    = $1400
UP_SRC  = $FB
UP_DST  = $FD
UP_REF  = $22
UP_CNT  = $02

        .segment "LOADER"
start:  ldx     #0
:       lda     msg,x
        beq     :+
        jsr     CHROUT
        inx
        bne     :-
:       lda     $BA             ; the drive PARLORNET came from
        cmp     #8
        bcs     :+
        lda     #8
        sta     $BA
:       lda     #8              ; the character set, to $C000
        ldx     #<n_chars
        ldy     #>n_chars
        jsr     SETNAM
        lda     #1
        ldx     $BA
        ldy     #0
        jsr     SETLFS
        lda     #0
        ldx     #<$C000
        ldy     #>$C000
        jsr     LOAD
        bcs     fail
        sei                     ; the bar's interrupt
        lda     #<bar
        sta     $0314
        lda     #>bar
        sta     $0315
        lda     #0
        sta     $AE
        sta     $AF
        sta     b_st+1
        sta     b_n
        cli
        lda     #7              ; the program, where its file says
        ldx     #<n_main
        ldy     #>n_main
        jsr     SETNAM
        lda     #1
        ldx     $BA
        ldy     #1
        jsr     SETLFS
        lda     #0
        jsr     LOAD
        php
        sei
        lda     #$31            ; the KERNAL's interrupt as it was
        sta     $0314
        lda     #$EA
        sta     $0315
        plp
        cli
        bcs     fail
        lda     #$36            ; BASIC out: the file ends at $C000, under BASIC's ROM (the program sets
        sta     $01             ; $01 itself, sys.s)
        stx     UP_SRC          ; its last two bytes: where the packed data starts
        sty     UP_SRC+1
        ldy     #0
        lda     UP_SRC
        bne     :+
        dec     UP_SRC+1
:       dec     UP_SRC
        lda     (UP_SRC),y
        tax
        lda     UP_SRC
        bne     :+
        dec     UP_SRC+1
:       dec     UP_SRC
        lda     (UP_SRC),y
        sta     UP_SRC
        stx     UP_SRC+1
        lda     #<MAIN
        sta     UP_DST
        lda     #>MAIN
        sta     UP_DST+1
        jsr     unpack
        jmp     MAIN

fail:   ldx     #0
:       lda     err,x
        beq     :+
        jsr     CHROUT
        inx
        bne     :-
:       rts

        .include "unpack.inc"

; The bar: 40 steps from where the program loads ($AE, once the KERNAL has read it from the file) to
; $C000, an inverse space in light green on the bottom row for each.
bar:    lda     b_st+1
        bne     b_have
        lda     $AF
        beq     b_out
        sta     b_st+1
        sta     b_next+1
        lda     $AE
        sta     b_st
        sta     b_next
        lda     #0
        sec
        sbc     b_st
        sta     b_step
        lda     #$C0
        sbc     b_st+1
        sta     b_step+1
        lda     #0
        ldx     #16
:       asl     b_step
        rol     b_step+1
        rol     a
        cmp     #40
        bcc     :+
        sbc     #40
        inc     b_step
:       dex
        bne     :--
b_have: ldx     b_n
        cpx     #40
        bcs     b_out
        lda     $AF
        cmp     b_next+1
        bcc     b_out
        bne     :+
        lda     $AE
        cmp     b_next
        bcc     b_out
:       lda     #$A0
        sta     $0400 + 24 * 40,x
        lda     #13
        sta     $D800 + 24 * 40,x
        inc     b_n
        lda     b_next
        clc
        adc     b_step
        sta     b_next
        lda     b_next+1
        adc     b_step+1
        sta     b_next+1
        jmp     b_have
b_out:  jmp     $EA31

b_st:   .word   0
b_step: .word   0
b_next: .word   0
b_n:    .byte   0

n_chars:.byte   "PN.CHARS"
n_main: .byte   "PN.MAIN"
msg:    .byte   13, "PARLORNET IS LOADING...", 13, 0
err:    .byte   "THE PARLORNET DISK IS NOT IN THE DRIVE.", 13, 0
