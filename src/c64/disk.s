; ParlorNet on the Commodore 64: the disk, through the KERNAL. For each call the KERNAL is switched in
; ($01 = $36; its IRQ and NMI come to ours through $0314 and $0318) and the serial line paused (RTS off, its
; interrupts off), then all is put back. The KERNAL's messages are off ($9D), so nothing is printed.

        .importzp       ptr1, ptr2, ptr3, tmp1
        .import         popax, popa, _ser_pause, _ser_resume
        .export         _k_load, _k_readfile, _k_writefile, _k_cmd, _k_device, _k_end, _k_unpack, _k_bar
        .export         _k_nodrive
.ifdef C64_CART
        .export         _c_load
        .importzp       ptr4, tmp2, tmp3
.endif
        .export         pb_on, pb_update
        .import         _txt_screen

SETLFS  = $FFBA
SETNAM  = $FFBD
OPEN    = $FFC0
CLOSE   = $FFC3
CHKIN   = $FFC6
CHKOUT  = $FFC9
CLRCHN  = $FFCC
CHRIN   = $FFCF
CHROUT  = $FFD2
LOAD    = $FFD5
READST  = $FFB7

        .segment "DATA"
_k_device:      .byte   8       ; the drive the program came from (sys_init: the KERNAL's $BA)
_k_end:         .word   0       ; where the last load ended (the byte after it)
_k_bar:         .byte   0       ; 1: the next k_load shows a progress bar (pb_update)
_k_nodrive:     .byte   0       ; 1: no drive answered the last k_readfile (device not present)
pb_on:  .byte   0
pb_st:  .word   0               ; where the file loads from
pb_step:.word   0               ; the bytes for each of the bar's 40 cells
pb_next:.word   0               ; where the next cell fills
pb_n:   .byte   0               ; the cells filled
k_n:    .byte   0
k_max:  .byte   0
k_err:  .byte   0

        .segment "CODE"

kernal_in:
        jsr     _ser_pause
        sei
        lda     #$36
        sta     $01
        lda     #0
        sta     $9D             ; no messages
        sta     $90             ; status
        cli
        rts

kernal_out:
        sei
        lda     #$35
        sta     $01
        cli
        jmp     _ser_resume

; SETNAM with the 0-ended name at ptr2.
setname:
        ldy     #0
:       lda     (ptr2),y
        beq     :+
        iny
        bne     :-
:       tya
        ldx     ptr2
        ldy     ptr2+1
        jmp     SETNAM

; unsigned char __fastcall__ k_load(const char *name, void *addr): the file loaded at addr (its own load
; address skipped), or where its load address says when addr is 0. 0 when it loaded, else the KERNAL's
; error; k_end is the address after its last byte.
_k_load:
        sta     ptr1
        stx     ptr1+1
        jsr     popax
        sta     ptr2
        stx     ptr2+1
        jsr     kernal_in
        lda     _k_bar
        beq     :+
        lda     #0
        sta     $AE             ; (the KERNAL's load pointer: 0 until the file's address is read)
        sta     $AF
        sta     pb_st+1
        sta     pb_n
        lda     #1
        sta     pb_on
:       jsr     setname
        lda     ptr1
        ora     ptr1+1
        cmp     #1              ; (C: 0 for the address given, 1 for the file's own)
        lda     #0
        rol     a
        eor     #1
        tay
        lda     #1
        ldx     _k_device
        jsr     SETLFS
        lda     #0
        ldx     ptr1
        ldy     ptr1+1
        jsr     LOAD
        stx     _k_end
        sty     _k_end+1
        php
        lda     #0
        sta     pb_on
        sta     _k_bar
        plp
        bcc     :+
        ora     #$80            ; (A: the error, 1 to 9; bit 7 so it is never 0)
        bne     :++
:       lda     #0
:       sta     k_err
        jsr     kernal_out
        lda     k_err
        ldx     #0
        rts

; unsigned char __fastcall__ k_cmd(const char *cmd): a command on the drive's command channel ("S0:NAME"
; scratches a file). 0 when the channel opened.
_k_cmd: sta     ptr2
        stx     ptr2+1
        jsr     kernal_in
        jsr     setname
        lda     #15
        ldx     _k_device
        ldy     #15
        jsr     SETLFS
        jsr     OPEN
        lda     #0
        rol     a
        sta     k_err
        lda     #15
        jsr     CLOSE
        jsr     kernal_out
        lda     k_err
        ldx     #0
        rts

; unsigned char __fastcall__ k_readfile(const char *name, unsigned char *buf, unsigned char max): up to max
; bytes of the file ("NAME,S,R"); the bytes read, 0 when there is no such file.
_k_readfile:
        sta     k_max
        jsr     popax
        sta     ptr1
        stx     ptr1+1
        jsr     popax
        sta     ptr2
        stx     ptr2+1
        jsr     kernal_in
        lda     #0
        sta     k_n
        jsr     setname
        lda     #2
        ldx     _k_device
        ldy     #2
        jsr     SETLFS
        jsr     OPEN
        bcs     rf_close
        ldx     #2
        jsr     CHKIN
        bcs     rf_close
rf_byte:
        jsr     CHRIN
        sta     tmp1
        jsr     READST
        and     #$BF            ; anything but the end of the file: an error (no such file, say)
        bne     rf_close
        ldy     k_n
        cpy     k_max
        bcs     rf_close
        lda     tmp1
        sta     (ptr1),y
        inc     k_n
        jsr     READST
        and     #$40
        beq     rf_byte
rf_close:
        lda     $90             ; the KERNAL's status: bit 7, the device not present
        rol     a
        lda     #0
        rol     a
        sta     _k_nodrive
        jsr     CLRCHN
        lda     #2
        jsr     CLOSE
        jsr     kernal_out
        lda     k_n
        ldx     #0
        rts

; unsigned char __fastcall__ k_writefile(const char *name, const unsigned char *buf, unsigned char n): the
; bytes into a new file ("NAME,S,W"). 0 when written.
_k_writefile:
        sta     k_max
        jsr     popax
        sta     ptr1
        stx     ptr1+1
        jsr     popax
        sta     ptr2
        stx     ptr2+1
        jsr     kernal_in
        lda     #1
        sta     k_err
        jsr     setname
        lda     #2
        ldx     _k_device
        ldy     #2
        jsr     SETLFS
        jsr     OPEN
        bcs     wf_close
        ldx     #2
        jsr     CHKOUT
        bcs     wf_close
        ldy     #0
:       cpy     k_max
        bcs     :+
        lda     (ptr1),y
        sty     k_n
        jsr     CHROUT
        ldy     k_n
        iny
        bne     :-
:       jsr     READST
        sta     k_err
wf_close:
        jsr     CLRCHN
        lda     #2
        jsr     CLOSE
        jsr     kernal_out
        lda     k_err
        ldx     #0
        rts

; void __fastcall__ k_unpack(void *dst): the file just loaded (k_load, 0), packed by tools/c64_pack.py, unpacked
; to dst; its last two bytes say where the packed data starts.
_k_unpack:
        sta     ptr2
        stx     ptr2+1
        lda     _k_end
        sec
        sbc     #2
        sta     ptr1
        lda     _k_end+1
        sbc     #0
        sta     ptr1+1
        ldy     #1
        lda     (ptr1),y
        tax
        dey
        lda     (ptr1),y
        sta     ptr1
        stx     ptr1+1
        jmp     unpack

UP_SRC = ptr1
UP_DST = ptr2
UP_REF = ptr3
UP_CNT = tmp1
        .include "unpack.inc"

; The progress bar, from the IRQ (sys.s) while pb_on: the cells of the bitmap's status row (row 23) colored
; across, or the text screen's row 24 turned inverse, as the KERNAL's load pointer ($AE) passes 40 steps from where
; the file loads to $C000, where every packed file ends (c64_pack.py's place()). Nothing is read from the
; disk for it. Uses A and X.
pb_update:
        lda     pb_st+1
        bne     pb_have
        lda     $AF
        bne     :+
        rts                     ; (the file's address not read yet)
:       sta     pb_st+1
        sta     pb_next+1
        lda     $AE
        sta     pb_st
        sta     pb_next
        lda     #0              ; step = ($C000 - start) / 40
        sec
        sbc     pb_st
        sta     pb_step
        lda     #$C0
        sbc     pb_st+1
        sta     pb_step+1
        lda     #0
        ldx     #16
:       asl     pb_step
        rol     pb_step+1
        rol     a
        cmp     #40
        bcc     :+
        sbc     #40
        inc     pb_step
:       dex
        bne     :--
pb_have:
        lda     pb_n
        cmp     #40
        bcs     pb_done
        lda     $AF
        cmp     pb_next+1
        bcc     pb_done
        bne     :+
        lda     $AE
        cmp     pb_next
        bcc     pb_done
:       ldx     pb_n
        lda     _txt_screen
        beq     :+
        lda     $C800 + 24 * 40,x   ; the text screen: the character there inverted, in light green
        ora     #$80
        sta     $C800 + 24 * 40,x
        lda     #13
        sta     $D800 + 24 * 40,x
        jmp     :++
:       lda     #$15            ; the bitmap: the status row's cell, white on green
        sta     $CC00 + 23 * 40,x
:       inc     pb_n
        lda     pb_next
        clc
        adc     pb_step
        sta     pb_next
        lda     pb_next+1
        adc     pb_step+1
        sta     pb_next+1
        jmp     pb_have
pb_done:
        rts

.ifdef C64_CART
; unsigned char __fastcall__ c_load(unsigned char k): file k of the cartridge (the overlays by their number,
; ovl.h) copied out of its ROM to where its load address says, as k_load would load it; 0 when copied, 1 when
; the cartridge has no such file. k_end is the address after its last byte. The serial line is paused and the
; interrupts are off while the cartridge is on, since their handlers and data may be under it; $01 is $37
; meanwhile (the BASIC and KERNAL ROMs in for reading, which nothing reads), as the cartridge's ROM needs.
CART_DIR = $02A7
CC_SRC  = ptr3
CC_DST  = ptr4
CC_LEN  = tmp2                  ; (and tmp3)
CC_BANK = tmp1
_c_load:
        sta     k_n             ; (the file)
        jsr     _ser_pause
        php
        sei
        lda     $01             ; the ROM at $8000 shows only with LORAM and HIRAM both on ($37); the
        pha                     ; program runs with HIRAM off ($35)
        lda     #$37
        sta     $01
        lda     k_n             ; (read while the cartridge is still off)
        jsr     cart_copy
        lda     #$80            ; the cartridge off again
        sta     $DE00
        lda     #0
        rol     a               ; (C: no such file)
        sta     k_err
        pla
        sta     $01
        plp
        lda     CC_DST
        sta     _k_end
        lda     CC_DST+1
        sta     _k_end+1
        jsr     _ser_resume
        lda     k_err
        ldx     #0
        rts

        .include "cartcopy.inc"
.endif
