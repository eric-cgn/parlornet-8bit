; ParlorNet on the Commodore 64 cartridge: copying a file out of the cartridge's ROM. The cartridge is a Magic
; Desk one: eight 8K banks or more, each seen at $8000-$9FFF while the cartridge is on, the bank chosen by a
; write to $DE00 ($80 switches the cartridge off, for the RAM under it). tools/c64_crt.py lays the files out one
; after another from bank 1 on, each as a C64 file is (its load address, then its bytes), and puts their
; directory in bank 0 at $8800: five bytes a file, its bank, its offset in that bank (two bytes) and its length
; with the load address (two bytes; 0 for none). The loader copies the directory to CART_DIR in RAM at start.
;
; The including file defines CART_DIR and, in zero page, CC_SRC, CC_DST, CC_LEN (two bytes each) and CC_BANK.
; cart_copy runs with the cartridge on, $01 at $37 (the C64 shows an 8K cartridge's ROM only with LORAM and HIRAM
; both on) and the interrupts off, and reads nothing in $8000-$9FFF but the ROM: it may be called from anywhere
; below $8000. Its writes go to the RAM, under any ROM.

; cart_copy: file A of the directory copied to where its load address says; C clear when copied, set when
; the directory has no such file. CC_DST is then the address after its last byte. Leaves the cartridge on, at
; the bank of the file's end. Uses A, X and Y.
cart_copy:
        sta     CC_BANK
        asl     a
        asl     a
        clc
        adc     CC_BANK         ; (five bytes a file)
        tax
        lda     CART_DIR+3,x    ; its length
        sta     CC_LEN
        lda     CART_DIR+4,x
        sta     CC_LEN+1
        ora     CC_LEN
        bne     :+
        sec                     ; (no such file)
        rts
:       lda     CART_DIR,x
        sta     CC_BANK
        sta     $DE00
        lda     CART_DIR+1,x
        sta     CC_SRC
        lda     CART_DIR+2,x
        clc
        adc     #$80
        sta     CC_SRC+1
        jsr     cc_byte         ; the load address
        sta     CC_DST
        jsr     cc_byte
        sta     CC_DST+1
        lda     CC_LEN
        sec
        sbc     #2
        sta     CC_LEN
        bcs     cc_loop
        dec     CC_LEN+1
cc_loop:
        lda     CC_LEN
        ora     CC_LEN+1
        beq     cc_done
        jsr     cc_byte
        ldy     #0
        sta     (CC_DST),y      ; (a write under the ROM goes to the RAM there)
        inc     CC_DST
        bne     :+
        inc     CC_DST+1
:       lda     CC_LEN
        bne     :+
        dec     CC_LEN+1
:       dec     CC_LEN
        jmp     cc_loop
cc_done:
        clc
        rts

; The next byte of the ROM, on into the next bank at $A000.
cc_byte:
        ldy     #0
        lda     (CC_SRC),y
        inc     CC_SRC
        bne     cb_done
        inc     CC_SRC+1
        ldy     CC_SRC+1
        cpy     #$A0
        bne     cb_done
        ldy     #$80
        sty     CC_SRC+1
        inc     CC_BANK
        ldy     CC_BANK
        sty     $DE00
cb_done:
        rts
