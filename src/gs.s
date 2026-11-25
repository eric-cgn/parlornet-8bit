; ParlorNet for the Apple IIgs: the program's start and its calls into the IIgs toolbox and GS/OS.
;
; PARLORNET (gsshell.s) gives the program a 64K bank of its own and copies it in at the addresses the
; IIe program uses, then jumps here in the 65816's native mode with 8-bit registers, the data bank set
; to that bank and the direct page GS/OS gave the application (cc65's zero page is $80-$BF in it). So
; the 6502 code from cc65 runs as it is: absolute addresses land in our bank, and the interrupts and
; everything in banks $00, $01, $E0 and $E1 stay GS/OS's.
;
; Two things the 6502 code does would go wrong in native mode and are replaced here: the runtime's
; callax, a JMP (ptr1), which in native mode reads its pointer from bank 0 rather than the direct page;
; and the start-up, which on the IIe sets the stack pointer.
;
; Toolbox calls take their parameters on the stack in 16-bit words (tl_call); GS/OS calls a parameter
; block's long address (gs_call). Long addresses of our own data are gs_bank:address.

        .setcpu "65816"
        .a8
        .i8
        .export         __STARTUP__ : absolute = 1
        .import         _main, zerobss, initlib, popax, jmpvec
        .import         __CSTACK_START__, __CSTACK_SIZE__
        .importzp       sp, ptr1
        .export         callax
        .export         _tl_call, _tl_in, _tl_out, _tl_nin, _tl_nout, _tl_err
        .export         _gs_call, _gs_bank, _gs_uid, _gs_video, _gs_poll
        .export         _disk_read, _disk_motor_off
        .export         _gs_scr_get, _gs_scr_put
        .exportzp       _gs_t1, _gs_t2, _gs_hp

TOOLS   = $E10000               ; the Tool Locator's dispatcher
GSOS    = $E100B0               ; GS/OS's stack-based entry
UIDADDR = $0810                 ; where PARLORNET leaves our user ID (gsshell.s)

        .segment "ZEROPAGE"
_gs_t1: .res    3               ; long pointers into bank $E0: the text screen (sys.s) ...
_gs_t2: .res    3
_gs_hp: .res    3               ; ... and Hi-Res (hgr.s)
scrp:   .res    3               ; ... and for C (gs_scr_get, gs_scr_put)

        .segment "STARTUP"
        cld
        lda     #<(__CSTACK_START__ + __CSTACK_SIZE__)
        sta     sp
        lda     #>(__CSTACK_START__ + __CSTACK_SIZE__)
        sta     sp+1
        jsr     zerobss
        phk
        pla
        sta     _gs_bank
        lda     UIDADDR
        sta     _gs_uid
        lda     UIDADDR+1
        sta     _gs_uid+1
        lda     #$E0
        sta     _gs_t1+2
        sta     _gs_t2+2
        sta     _gs_hp+2
        sta     scrp+2
        lda     #1
        jsr     _gs_video
        jsr     initlib
        jmp     _main

        .segment "DATA"
_gs_bank:       .word   0       ; our bank (high byte 0)
_gs_uid:        .word   0       ; our Memory Manager user ID
_tl_nin:        .word   0       ; words of parameters in tl_in
_tl_nout:       .word   0       ; words of results to take into tl_out
_tl_err:        .word   0
_tl_in:         .res    24
_tl_out:        .res    8
tl_fn:          .word   0
gs_num:         .word   0
save_nv:        .byte   0
save_tb:        .byte   0
save_mc:        .byte   0       ; MONOCOLOR ($C021)
save_ac:        .byte   0       ; RDALTCHAR ($C01E)

        .segment "CODE"

; callax: call the function at A/X (a function pointer), as the runtime's, without JMP (abs).
callax: sta     jmpvec+1
        stx     jmpvec+2
        jmp     jmpvec

; unsigned int __fastcall__ tl_call(unsigned int fn): a toolbox call. Pushes tl_nout words of space
; for the results, then the tl_nin words of tl_in in order (a long as its high word first), and calls
; tool call fn; the results go into tl_out, the first word off the stack first. Returns the error code.
_tl_call:
        sta     tl_fn
        stx     tl_fn+1
        rep     #$30
        .a16
        .i16
        lda     #0
        ldx     _tl_nout
        beq     :++
:       pha
        dex
        bne     :-
:       ldy     #0
        ldx     _tl_nin
        beq     :++
:       lda     _tl_in,y
        pha
        iny
        iny
        dex
        bne     :-
:       ldx     tl_fn
        jsl     TOOLS
        sta     _tl_err
        ldy     #0
        ldx     _tl_nout
        beq     :++
:       pla
        sta     _tl_out,y
        iny
        iny
        dex
        bne     :-
:       sep     #$30
        .a8
        .i8
        lda     _tl_err
        ldx     _tl_err+1
        rts

; unsigned int __fastcall__ gs_call(unsigned int num, void *parms): GS/OS call num (class 1) with the
; parameter block at parms in our bank. Returns GS/OS's error code.
_gs_call:
        sta     ptr1
        stx     ptr1+1
        jsr     popax
        sta     gs_num
        stx     gs_num+1
        rep     #$30
        .a16
        .i16
        lda     _gs_bank
        pha
        lda     ptr1
        pha
        lda     gs_num
        pha
        jsl     GSOS
        sta     _tl_err
        sep     #$30
        .a8
        .i8
        lda     _tl_err
        ldx     _tl_err+1
        rts

; void gs_poll(void): TCPIPPoll, once a tick while connected (sys.s). Keeps every register, since the
; reply reader calls tick_poll in the middle of its work.
_gs_poll:
        php
        rep     #$30
        .a16
        .i16
        pha
        phx
        phy
        ldx     #$2236          ; TCPIPPoll
        jsl     TOOLS
        ply
        plx
        pla
        plp
        .a8
        .i8
        rts

; void __fastcall__ gs_video(unsigned char on): 1: the screen as the program wants it (the Apple II
; text and Hi-Res modes rather than Super Hi-Res, 40 columns, the alternate characters, white on black);
; 0: back as it was (with the monochrome switch and the character set).
_gs_video:
        cmp     #0
        beq     vrestore
        lda     f:$E0C029       ; NEWVIDEO
        sta     save_nv
        and     #$1F            ; Super Hi-Res off, color
        sta     f:$E0C029
        lda     f:$E0C021       ; the monochrome switch (sys.s sets it from the display setting)
        sta     save_mc
        lda     f:$E0C01E       ; the character set
        sta     save_ac
        lda     f:$E0C022       ; the text and background colors
        sta     save_tb
        lda     #$F0
        sta     f:$E0C022
        sta     f:$E0C000       ; 80STORE off
        sta     f:$E0C00C       ; 40 columns
        sta     f:$E0C00F       ; the alternate characters (inverse lower case)
        sta     f:$E0C051       ; text
        rts
vrestore:
        lda     save_tb
        sta     f:$E0C022
        lda     save_mc
        sta     f:$E0C021
        lda     save_ac
        bmi     :+
        sta     f:$E0C00E       ; the primary characters, as they were
:
        lda     save_nv
        sta     f:$E0C029
        rts

; unsigned char __fastcall__ gs_scr_get(const unsigned char *a): the byte at a in bank $E0.
_gs_scr_get:
        sta     scrp
        stx     scrp+1
        lda     [scrp]
        ldx     #0
        rts

; void __fastcall__ gs_scr_put(unsigned char *a, unsigned char v): v at a in bank $E0.
_gs_scr_put:
        pha
        jsr     popax
        sta     scrp
        stx     scrp+1
        pla
        sta     [scrp]
        rts

; The IIe's sector reader is not here: the overlays come from PARLOR.DAT (prodos.c, gsos.c).
_disk_read:
        lda     #1
        ldx     #0
        rts
_disk_motor_off:
        rts
