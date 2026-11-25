; ParlorNet for the Apple IIgs: PARLORNET, the GS/OS application (S16) that starts the program.
;
; tools/apple2gs.py makes the application's load file (OMF) from this code followed by the program: a
; length word, the page that goes at $0800 (the overlay table, which marks the program as running
; from files, as under ProDOS) and the program itself, which goes at $4000. This code is position
; independent (relative branches and PER only), so the System Loader may put it anywhere.
;
; It starts the Tool Locator, the Memory Manager and the Miscellaneous Tool Set, asks the Memory
; Manager for a whole bank (64K, locked, outside the special memory of banks $00, $01, $E0 and $E1),
; clears it and copies the program in at the addresses it was linked for, reserves the Hi-Res page in
; bank $E0 that the program draws on, and jumps to the program (gs.s) in native mode with 8-bit
; registers, the data bank set to the new bank and the direct page GS/OS gave the application.

        .setcpu "65816"

TOOLS   = $E10000
GSOS    = $E100B0

; the direct page (the program uses $80 up, once it runs)
uid     = $00
hand    = $02                   ; a handle (4)
bp      = $06                   ; our bank's first byte (4)
src     = $0A                   ; the program in this segment (4)
msg     = $0E                   ; a message to show (4)

; NewHandle for a whole bank into hand; carry set if there is none. (A macro, not a subroutine: JSR
; takes an absolute address, which would not be position independent.)
.macro  newbank
        pha
        pha
        pea     $0001
        pea     $0000           ; 64K
        lda     uid
        pha
        pea     $C018           ; locked, fixed, not across a bank, not special memory
        pea     $0000
        pea     $0000
        ldx     #$0902          ; NewHandle
        jsl     TOOLS
        pla
        sta     hand
        pla
        sta     hand+2
.endmacro

        .segment "SHELL"
        .a16
        .i16
start:  rep     #$30
        ldx     #$0201          ; TLStartUp
        jsl     TOOLS
        pha
        ldx     #$0202          ; MMStartUp: our user ID
        jsl     TOOLS
        pla
        sta     uid
        ldx     #$0203          ; MTStartUp
        jsl     TOOLS

        ; a bank of our own
        newbank
        bcc     gotbank
        pha
        pha
        ldx     #$1F02          ; CompactMem, then try once more
        jsl     TOOLS
        pla
        pla
        newbank
        bcc     gotbank
        per     m_mem
        brl     fail
gotbank:
        ldy     #2
        lda     [hand],y
        sta     bp+2
        lda     [hand]
        sta     bp
        beq     :+              ; (a whole bank starts at $0000)
        brl     badbank
:

        ; the Hi-Res page the program draws on, $E0:2000-$3FFF
        pha
        pha
        pea     $0000
        pea     $2000
        lda     uid
        pha
        pea     $C002           ; locked, fixed, at this address
        pea     $00E0
        pea     $2000
        ldx     #$0902          ; NewHandle
        jsl     TOOLS
        pla
        pla
        bcc     :+
        per     m_hires
        brl     fail

        ; clear the bank, then copy the program in
:       lda     #0
        tay
:       sta     [bp],y
        iny
        iny
        bne     :-
        per     payload
        pla
        sta     src
        phk
        phk
        pla
        and     #$00FF
        sta     src+2
        ; the page at $0800
        lda     src+2
        pha
        lda     src
        clc
        adc     #2
        pha
        lda     bp+2
        pha
        pea     $0800
        pea     $0000
        pea     $0100
        ldx     #$2B02          ; BlockMove
        jsl     TOOLS
        ; the program at $4000
        lda     src+2
        pha
        lda     src
        clc
        adc     #2+256
        pha
        lda     bp+2
        pha
        pea     $4000
        pea     $0000
        lda     [src]
        pha
        ldx     #$2B02
        jsl     TOOLS
        ; our user ID for the program
        ldy     #$0810
        lda     uid
        sta     [bp],y

        ; go: native mode, 8-bit registers, the data bank ours, the direct page as it is
        sep     #$30
        .a8
        .i8
        lda     bp+2
        pha
        plb
        pha                     ; the bank, then $3FFF: RTL goes to bank:$4000
        pea     $3FFF
        rtl

        .a16
        .i16
badbank:
        per     m_mem
        ; fall into fail

; Show the message whose address is on the stack on the text screen, wait for a key and quit.
fail:   pla
        sta     msg
        phk
        phk
        pla
        and     #$00FF
        sta     msg+2
        sep     #$20
        .a8
        lda     f:$E0C029
        and     #$7F            ; Super Hi-Res off
        sta     f:$E0C029
        sta     f:$E0C051       ; text
        sta     f:$E0C054
        sta     f:$E0C00C       ; 40 columns
        ldx     #0
        lda     #$A0
:       sta     f:$E00400,x     ; clear the top rows
        sta     f:$E00480,x
        sta     f:$E00500,x
        inx
        cpx     #40
        bne     :-
        ldy     #0
        tyx
:       lda     [msg],y
        beq     :+
        ora     #$80
        sta     f:$E00480,x
        inx
        iny
        bra     :-
:       sta     f:$E0C010
:       lda     f:$E0C000
        bpl     :-
        sta     f:$E0C010
        rep     #$30
        .a16
        pea     $0000           ; a parameter block on the stack: no parameters
        tsc
        inc     a
        pea     $0000
        pha
        pea     $2029           ; Quit
        jsl     GSOS
        brk     $00

m_mem:  .byte   "ParlorNet needs 64K of free memory in one bank.", 0
m_hires:.byte   "ParlorNet cannot reserve the Hi-Res screen.", 0

        .segment "PAYLOAD"
payload:
