; ParlorNet's template machine: the start of the program. A machine's own start-up takes the machine over
; (its interrupts, its screen), sets up cc65's C stack, clears the BSS and calls main, which never returns.

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
