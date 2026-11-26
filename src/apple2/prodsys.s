; ParlorNet for the Apple IIe: PARLOR.SYSTEM, the start of the program under ProDOS.
;
; ProDOS loads this at $2000 and runs it. It turns on the alternate character set (inverse lower case,
; as the boot block does), keeping the one it found at $0820 for quitting (prodos.c), checks for 128K, disconnects the /RAM disk (the program
; uses the auxiliary memory it lives in), makes sure the prefix is set, and reads the program from
; PARLOR.DAT beside it into $4000. Then it fills in the overlay table at $0821 (each game's first page
; in PARLOR.DAT) and marks the disk number at $080A 0, so the program knows it runs under ProDOS, keeps
; ProDOS's stack pointer at $081F, switches to the auxiliary zero page, stack and language card, puts
; the program's 6502 vectors there and starts it. tools/apple2_disk.py fills in NPAGES and OVLTAB.

        .setcpu "6502"

MLI     = $BF00
COUT    = $FDED
RDKEY   = $FD0C
HOME    = $FC58

        .segment "LOADER"

        jmp     start
        .byte   "PN"            ; $2003
npages: .byte   0               ; $2005: the program's pages, then one of vectors, then the games
ovltab: .res    80, 0           ; $2006: as the boot block's, the start a page in PARLOR.DAT

start:  cld
        lda     $C01E           ; RDALTCHAR: the character set as we found it, for quitting (prodos.c)
        sta     $0820
        sta     $C00C           ; 40 columns
        sta     $C00F           ; the alternate characters (inverse lower case), as boot.s sets them
        jsr     HOME
        lda     $BF98           ; MACHID: 128K?
        and     #$30
        cmp     #$30
        beq     k128
        ldx     #m_128-msgs
        jmp     fail
k128:
        ; /RAM is slot 3, drive 2: take it out of the device list (Apple's ProDOS 8 Technical Note 10)
        lda     $BF26
        cmp     $BF10
        bne     ram_on
        lda     $BF27
        cmp     $BF11
        beq     ram_off
ram_on: ldx     $BF31           ; DEVCNT: the last entry's index
find:   lda     $BF32,x
        and     #$F0
        cmp     #$B0
        beq     shift
        dex
        bpl     find
        bmi     ram_gone
shift:  cpx     $BF31
        beq     last
        lda     $BF33,x
        sta     $BF32,x
        inx
        bne     shift
last:   dec     $BF31
ram_gone:
        lda     $BF10           ; "no device" in its slot
        sta     $BF26
        lda     $BF11
        sta     $BF27
ram_off:
        ; the prefix: when none is set, the volume ProDOS last used
        jsr     MLI
        .byte   $C7
        .word   p_getpfx
        lda     pfx
        bne     have_pfx
        lda     $BF30
        sta     ol_unit
        jsr     MLI
        .byte   $C5
        .word   p_online
        bcc     :+
        jmp     nofile
:       lda     pfx+1
        and     #$0F
        tax
        lda     #'/'
        sta     pfx+1
        sta     pfx+2,x
        inx
        inx
        stx     pfx
        jsr     MLI
        .byte   $C6
        .word   p_setpfx
have_pfx:
        jsr     MLI
        .byte   $C8
        .word   p_open
        bcc     :+
        jmp     nofile
:
        lda     o_ref
        sta     r_ref
        sta     c_ref
        lda     npages          ; the program, at $4000
        sta     r_len+1
        jsr     MLI
        .byte   $CA
        .word   p_read
        bcc     :+
        jmp     nofile
:
        lda     #$00            ; its vectors, into $0D00 for now
        sta     r_buf
        lda     #$0D
        sta     r_buf+1
        lda     #1
        sta     r_len+1
        jsr     MLI
        .byte   $CA
        .word   p_read
        bcc     :+
        jmp     nofile
:
        jsr     MLI
        .byte   $CC
        .word   p_close
        ldx     #79
:       lda     ovltab,x
        sta     $0821,x
        dex
        bpl     :-
        lda     #0
        sta     $080A
        sei
        tsx
        stx     $081F
        sta     $C009           ; the auxiliary zero page, stack and language card
        lda     $C083           ; its RAM, bank 2, to read and write
        lda     $C083
        ldx     #0
:       lda     $0D00,x
        sta     $FF00,x
        inx
        bne     :-
        jmp     $4000

nofile: ldx     #m_file-msgs
fail:   lda     msgs,x
        beq     :+
        ora     #$80
        jsr     COUT
        inx
        bne     fail
:       jsr     RDKEY
        jsr     MLI
        .byte   $65
        .word   p_quit
        brk

msgs:
m_128:  .byte   "PARLORNET NEEDS AN APPLE IIE WITH 128K.", $0D, "PRESS A KEY.", 0
m_file: .byte   "PARLOR.DAT IS NOT BESIDE PARLOR.SYSTEM.", $0D, "PRESS A KEY.", 0

p_getpfx:
        .byte   1
        .word   pfx
p_online:
        .byte   2
ol_unit:.byte   0
        .word   pfx+1
p_setpfx:
        .byte   1
        .word   pfx
p_open: .byte   3
        .word   datname
        .word   $0900
o_ref:  .byte   0
p_read: .byte   4
r_ref:  .byte   0
r_buf:  .word   $4000
r_len:  .word   0
        .word   0
p_close:.byte   1
c_ref:  .byte   0
p_quit: .byte   4, 0
        .word   0
        .byte   0
        .word   0
datname:.byte   10, "PARLOR.DAT"
pfx:    .res    64, 0
