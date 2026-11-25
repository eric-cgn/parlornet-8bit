; ParlorNet on the Commodore 64 cartridge: its first bank (cart.cfg, tools/c64_crt.py). At power-on the KERNAL
; finds the cartridge's mark (CBM80) and jumps here at once, before it has set anything up. The start sets up
; what the KERNAL would (the I/O, the RAM's top, its vectors, the screen), then copies the loader to $C800 and
; runs it there: the loader copies the directory to $02A7, the character set to $C000 and the program, packed,
; to where its file says, switches the cartridge off, unpacks the program to $1500 and starts it, as the disk's
; loader (loader.s) does. Nothing is read from a disk, so there is no progress bar.

IOINIT  = $FF84
RAMTAS  = $FF87
RESTOR  = $FF8A
CINT    = $FF81
MAIN    = $1500
CART_DIR = $02A7
N_CHARS = 13                    ; the directory's files: the overlays 0 to 12, then these two
N_MAIN  = 14
UP_SRC  = $FB
UP_DST  = $FD
UP_REF  = $22
UP_CNT  = $02
CC_SRC  = $57
CC_DST  = $59
CC_LEN  = $5B
CC_BANK = $5D

        .import         __LOADER_LOAD__, __LOADER_RUN__, __LOADER_SIZE__

        .segment "CART"
        .word   cold            ; the start at power-on and RESET
        .word   cold            ; (RESTORE with the cartridge on: start again)
        .byte   $C3, $C2, $CD, $38, $30         ; "CBM80"

cold:   sei
        ldx     #$FF
        txs
        cld
        stx     $D016
        jsr     IOINIT
        jsr     RAMTAS
        jsr     RESTOR
        jsr     CINT
        ldx     #0              ; the loader to $C800 (at most three pages)
:       lda     __LOADER_LOAD__,x
        sta     __LOADER_RUN__,x
        lda     __LOADER_LOAD__+$100,x
        sta     __LOADER_RUN__+$100,x
        lda     __LOADER_LOAD__+$200,x
        sta     __LOADER_RUN__+$200,x
        inx
        bne     :-
        jmp     load

        .segment "LOADER"
load:   ldx     #15 * 5 - 1     ; the directory, from $8800
:       lda     $8800,x
        sta     CART_DIR,x
        dex
        bpl     :-
        lda     #N_CHARS
        jsr     cart_copy
        lda     #N_MAIN
        jsr     cart_copy
        lda     #$80            ; the cartridge off: RAM at $8000-$9FFF
        sta     $DE00
        lda     CC_DST          ; the program's last two bytes: where its packed data starts
        sec
        sbc     #2
        sta     UP_SRC
        lda     CC_DST+1
        sbc     #0
        sta     UP_SRC+1
        ldy     #1
        lda     (UP_SRC),y
        tax
        dey
        lda     (UP_SRC),y
        sta     UP_SRC
        stx     UP_SRC+1
        lda     #<MAIN
        sta     UP_DST
        lda     #>MAIN
        sta     UP_DST+1
        jsr     unpack
        lda     #8              ; the drive for the settings, if there is one
        sta     $BA
        cli
        jmp     MAIN

        .include "cartcopy.inc"
        .include "unpack.inc"
