; ParlorNet on the Commodore 64: PARLORNET, the first file on the disk, which starts the program with as little
; typing as there can be. It loads PN.BOOT (loader.s) to $C800 and starts it, and comes in two ways at once:
;
; - LOAD"*",8,1 (or LOAD"PARLORNET",8,1) puts these 93 bytes at $02A7-$0303, where they end on BASIC's
;   vectors: $0300 as it was (the error routine) and $0302, the idle loop's, pointing at a_entry. When the
;   LOAD is done BASIC goes to its idle loop through $0302 and so starts it, with no RUN.
; - LOAD"*",8 (or the same name) puts them at $0801, as BASIC's program: the first bytes are a line, 10 SYS
;   2061, so RUN starts b_entry.
;
; The code between is run at either address, so it is the same both ways but for where it finds the name.

SETLFS  = $FFBA
SETNAM  = $FFBD
LOAD    = $FFD5
BOOT    = $C800                 ; PN.BOOT
A_BASE  = $02A7                 ; where ,8,1 puts it
B_BASE  = $0801                 ; where ,8 puts it
TO_B    = B_BASE - A_BASE       ; from an address at $02A7 to the same at $0801

        .segment "STUB"
        .org    A_BASE
        .word   basend + TO_B    ; the BASIC line (at $0801)
        .word   10
        .byte   $9E, "2061", 0
basend: .word   0
b_entry:                        ; RUN: here at $080D (2061)
        ldx     #<(name + TO_B)
        ldy     #>(name + TO_B)
        jmp     common + TO_B
a_entry:                        ; LOAD"*",8,1: here at its own address, from BASIC's idle loop
        lda     #$83            ; the vector as BASIC had it
        sta     $0302
        lda     #$A4
        sta     $0303
        ldx     #<name
        ldy     #>name
common: lda     #7
        jsr     SETNAM
        lda     #1
        ldx     $BA             ; the drive this came from
        ldy     #1              ; to PN.BOOT's own address
        jsr     SETLFS
        lda     #0
        jsr     LOAD
        bcs     fail
        jmp     BOOT
fail:   jmp     ($A002)         ; (BASIC's warm start: READY.)
name:   .byte   "PN.BOOT"
        .res    $0300 - *, 0
        .word   $E38B           ; $0300: BASIC's error vector, as it was
        .word   a_entry         ; $0302: the idle loop's
        .assert b_entry - A_BASE = 12, error, "the SYS address is 2061"
