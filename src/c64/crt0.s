; ParlorNet on the Commodore 64: the start of the program (PN.MAIN at $1400), which the loader (loader.s)
; jumps to with the KERNAL and BASIC in, the character set at $C000 and the KERNAL's screen on.
; It takes the machine over (sys_start: BASIC and the KERNAL out, our interrupts, the screens in the VIC's
; bank 3), sets up cc65's C stack, clears the BSS and calls main, which never returns.

        .export         __STARTUP__ : absolute = 1
        .import         _main, zerobss, initlib, sys_start
        .import         __CSTACK_START__, __CSTACK_SIZE__
        .importzp       sp

        .segment "STARTUP"

        sei
        cld
        ldx     #$FF
        txs
        jsr     sys_start
        lda     #<(__CSTACK_START__ + __CSTACK_SIZE__)
        sta     sp
        lda     #>(__CSTACK_START__ + __CSTACK_SIZE__)
        sta     sp+1
        jsr     zerobss
        jsr     initlib
        cli
        jmp     _main
