; ParlorNet for the Apple IIe: the start of the program, which the boot block jumps to.
; It sets up the 6502 stack and cc65's C stack, clears the BSS, and calls main, which never returns.

        .export         __STARTUP__ : absolute = 1
        .import         _main, zerobss, initlib
        .import         __CSTACK_START__, __CSTACK_SIZE__
        .importzp       sp

        .segment "STARTUP"

        sei
        cld
        ldx     #$FF
        txs
        lda     #<(__CSTACK_START__ + __CSTACK_SIZE__)
        sta     sp
        lda     #>(__CSTACK_START__ + __CSTACK_SIZE__)
        sta     sp+1
        jsr     zerobss
        jsr     initlib
        jmp     _main
