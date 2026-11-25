; ParlorNet on the Commodore 64: the disk's boot sector (track 1, sector 0), for a C128, which reads it to $0B00
; at power-on or on BOOT and, seeing "CBM", prints BOOTING PARLORNET... and calls the code after the strings.
; ParlorNet is a C64 program, so that code switches the C128 to 64 mode (the KERNAL's C64MODE, as GO64
; does) and leaves behind it what starts the program there: a few bytes at $8000 with the "CBM80" mark of a
; cartridge. The C64 KERNAL's reset finds the mark and jumps to them; they do the rest of the KERNAL's
; start-up (I/O, memory, vectors, screen), take the mark away (a later reset is an ordinary one), and load
; and start PN.BOOT, the loader (loader.s), from the drive the C128 booted from.

C64MODE = $FF4D
MMUCR   = $FF00
SETLFS  = $FFBA
SETNAM  = $FFBD
LOAD    = $FFD5
CHROUT  = $FFD2

        .segment "BOOT"
        .org    $0B00
        .byte   "CBM"
        .word   0               ; no more sectors to read
        .byte   0
        .byte   0
        .byte   "PARLORNET", 0  ; BOOTING PARLORNET...
        .byte   0               ; no file to load
boot:   sei
        lda     #$3E            ; RAM bank 0 throughout (the I/O stays in)
        sta     MMUCR
        ldx     #starter_len - 1
:       lda     starter_at,x
        sta     $8000,x
        dex
        bpl     :-
        lda     $BA             ; the drive the C128 booted from
        cmp     #8
        bcs     :+
        lda     #8
:       sta     s_dev
        lda     #$00
        sta     MMUCR
        jmp     C64MODE

starter_at = *
        .org    $8000
        .word   s_cold, s_cold
        .byte   $C3, $C2, $CD, $38, $30         ; "CBM80"
s_cold: jsr     $FDA3           ; the C64 KERNAL's start-up, the part after the cartridge check: I/O,
        jsr     $FD50           ; memory, vectors, the screen
        jsr     $FD15
        jsr     $FF5B
        cli
        lda     #0
        sta     $8004
        ldx     #0
:       lda     s_msg,x
        beq     :+
        jsr     CHROUT
        inx
        bne     :-
:       lda     #7
        ldx     #<s_name
        ldy     #>s_name
        jsr     SETNAM
        lda     #1
        ldx     s_dev
        ldy     #1
        jsr     SETLFS
        lda     #0
        jsr     LOAD
        bcs     :+
        jmp     $C800
:       jmp     ($A000)         ; (no disk: BASIC)
s_dev:  .byte   8
s_name: .byte   "PN.BOOT"
s_msg:  .byte   13, "PARLORNET, IN 64 MODE...", 13, 0
starter_len = * - $8000
