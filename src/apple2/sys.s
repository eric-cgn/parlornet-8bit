; ParlorNet for the Apple IIe: the machine. The Super Serial Card (6551 ACIA, received bytes taken by
; an interrupt into a 256-byte ring), the 40-column text screen and the switch to Double Hi-Res, the
; keyboard, a 60 Hz tick from the vertical blank, and the AppleMouse II card's firmware calls.
;
; The program runs with the language card's RAM switched in, so the 6502 vectors at $FFFA are ours.
;
; The same source builds the Apple IIgs program's machine layer (IIGS defined; a2gs.inc): there the
; serial calls go straight to the Marinetti connection (mt.c), the text screen and the soft switches are
; reached in bank $E0, the mouse is the IIgs's own (gsos.c), and the interrupts stay GS/OS's.

        .include "a2gs.inc"
        .importzp       ptr1, ptr2, tmp1, tmp2, tmp3, tmp4
        .import         popa
        .import         _eth_on, _eth_get, _eth_put, _eth_drop
.ifdef IIGS
        .import         _gs_poll, _mono
.endif
        .import         joy_keyed, joy_idle

        .export         _ser_open, _ser_close, _ser_get, _ser_put, _ser_used, _ser_drop
        .export         _ser_flow, _ser_lost
        .export         _txt_clear, _txt_goto, _txt_putc, _txt_puts, _txt_clreol, _txt_clrrow
        .export         _txt_x, _txt_y, _txt_inv, _txt_top, _txt_bot, _txt_cursor, _txt_peek
        .export         _kbd_get, _kbd_mods, _tick_poll, _ticks
        .export         _clk_t, _clk_sub, _clk_set, _clk_text, _clk_buf, _clk_shown, _clk_show, _clk_sync, _clk_min
        .export         _clk_tim_off, _clk_tim_at
        .import         _q_set, _net_req, _r_n, _r_line
        .import         _signed_in, _ui_hide, _ui_show, _ui_ink, _hgr_text, _hg_x, _hg_y, pusha
        .export         _txt_mode, _gr_mode, _txt_detect, _txt_mt
.ifndef IIGS
        .export         _mouse_find, _mouse_call, _mouse_slot
.endif

.ifndef IIGS
; ---------------------------------------------------------------------------------------------------
; Serial

        .segment "RING"
ring:   .res    256

        .segment "DATA"
_ser_flow:      .byte   0       ; 0 none, 1 RTS/CTS, 2 XON/XOFF
_ser_lost:      .byte   0       ; bytes dropped because the ring was full
rhead:  .byte   0               ; written by the interrupt
rtail:  .byte   0
held:   .byte   0               ; 1 while we have asked the other end to stop
rxoff:  .byte   0               ; 1 after the other end sent XOFF
cmdreg: .byte   $09
opened: .byte   0

        .segment "CODE"

; void __fastcall__ ser_open(unsigned char slot, unsigned char control, unsigned char command)
; command is the 6551 command register as for sending: parity bits, $09 (DTR on, RTS on, receiver
; interrupt on, transmitter interrupt off).
_ser_open:
        sta     cmdreg
        jsr     popa
        sta     tmp2            ; control
        jsr     popa            ; slot
        asl     a
        asl     a
        asl     a
        asl     a
        sta     tmp1
        sei
        ; Patch the registers' addresses: data, status, command, control.
        ldx     #npatch-1
:       lda     pt_lo,x
        sta     ptr1
        lda     pt_hi,x
        sta     ptr1+1
        lda     pt_reg,x
        clc
        adc     tmp1
        ldy     #0
        sta     (ptr1),y
        dex
        bpl     :-
        lda     #0
        sta     rhead
        sta     rtail
        sta     held
        sta     rxoff
a_s1:   sta     $C0A9           ; any write to the status register resets the ACIA
        lda     tmp2
a_k1:   sta     $C0AB
        lda     cmdreg
a_c1:   sta     $C0AA
a_d1:   lda     $C0A8           ; drop anything already there
        lda     #<irq
        sta     $FFFE
        lda     #>irq
        sta     $FFFF
        lda     #1
        sta     opened
        cli
        rts

        .segment "RODATA"
pt_lo:  .byte   <(a_d1+1),<(a_d2+1),<(a_d3+1),<(a_d4+1),<(a_d5+1),<(a_s1+1),<(a_s2+1),<(a_s3+1),<(a_s4+1)
        .byte   <(a_c1+1),<(a_c2+1),<(a_c3+1),<(a_k1+1)
pt_hi:  .byte   >(a_d1+1),>(a_d2+1),>(a_d3+1),>(a_d4+1),>(a_d5+1),>(a_s1+1),>(a_s2+1),>(a_s3+1),>(a_s4+1)
        .byte   >(a_c1+1),>(a_c2+1),>(a_c3+1),>(a_k1+1)
pt_reg: .byte   $88,$88,$88,$88,$88,$89,$89,$89,$89,$8A,$8A,$8A,$8B
npatch = 13
        .segment "CODE"

; void ser_close(void): interrupts off at the ACIA (DTR stays on, so a modem keeps its call).
_ser_close:
        lda     opened
        beq     :+
        sei
        lda     cmdreg
        ora     #$02
a_c2:   sta     $C0AA
        lda     #0
        sta     opened
:       rts

; Take a received byte, if one waits, into the ring. Call with interrupts off. Uses A, X.
rx_take:
a_s2:   lda     $C0A9
        and     #$08
        beq     rx_none
a_d2:   lda     $C0A8
rx_store:
        ldx     _ser_flow
        cpx     #2
        bne     :++
        cmp     #$13
        bne     :+
        stx     rxoff           ; (X = 2, non-zero)
        rts
:       cmp     #$11
        bne     :+
        ldx     #0
        stx     rxoff
        rts
:       ldx     rhead
        sta     ring,x
        inx
        cpx     rtail
        beq     rx_full
        stx     rhead
        ; Nearly full: ask the other end to stop.
        txa
        sec
        sbc     rtail
        cmp     #192
        bcc     rx_none
        lda     held
        bne     rx_none
        lda     _ser_flow
        beq     rx_none
        lda     #1
        sta     held
        lda     _ser_flow
        cmp     #1
        bne     :+
        lda     cmdreg
        and     #$F3            ; RTS off
        jmp     setcmd
:       lda     #$13
        jmp     sendnow
rx_full:
        inc     _ser_lost
rx_none:
        rts

setcmd:
a_c3:   sta     $C0AA
        rts

; Send A at once if the transmitter is free (for XON and XOFF).
sendnow:
        tax
a_s3:   lda     $C0A9
        and     #$10
        beq     :+
a_d3:   stx     $C0A8
:       rts

irq:    pha
        txa
        pha
        jsr     rx_take
        pla
        tax
        pla
        rti

; int ser_get(void): the next received byte, or -1.
_ser_get:
        lda     _eth_on         ; over the Uthernet II instead (eth.c)
        beq     :+
        jmp     _eth_get
:
        ldx     rtail
        cpx     rhead
        bne     sg_take
        sei                     ; nothing in the ring: look at the ACIA itself, in case the card's
        jsr     rx_take         ; interrupt switch is off
        cli
        ldx     rtail
        cpx     rhead
        beq     :++
sg_take:
        lda     ring,x
        inc     rtail
        pha
        lda     held
        beq     :+
        lda     rhead
        sec
        sbc     rtail
        cmp     #64
        bcs     :+
        jsr     release
:       pla
        ldx     #0
        rts
:       lda     #$FF
        tax
        rts

release:
        sei
        lda     #0
        sta     held
        lda     _ser_flow
        cmp     #1
        bne     :+
        lda     cmdreg
        jsr     setcmd
        cli
        rts
:       lda     #$11
        jsr     sendnow
        cli
        rts

; unsigned char ser_used(void): bytes waiting in the ring.
_ser_used:
        lda     rhead
        sec
        sbc     rtail
        ldx     #0
        rts

; void ser_drop(void): forget everything received.
_ser_drop:
        lda     _eth_on
        beq     :+
        jmp     _eth_drop
:
        sei
        lda     rhead
        sta     rtail
        cli
        lda     held
        beq     :+
        jsr     release
:       rts

.else
; ---------------------------------------------------------------------------------------------------
; The IIgs: no serial card; the serial calls are the Marinetti connection's (mt.c).
        .import         incsp2

        .segment "DATA"
_ser_flow:      .byte   0
_ser_lost:      .byte   0

        .segment "CODE"
_ser_open:
        jmp     incsp2          ; (slot and control on the C stack)
_ser_close:
        rts
_ser_get:
        jmp     _eth_get
_ser_put:
        jmp     _eth_put
_ser_drop:
        jmp     _eth_drop
_ser_used:
        lda     #0
        tax
        rts
.endif

        .include "reply.inc"

.ifndef IIGS
; void __fastcall__ ser_put(unsigned char c). Waits for the transmitter, up to about a second.
_ser_put:
        ldx     _eth_on
        beq     :+
        jmp     _eth_put
:       sta     tmp3
        lda     held
        beq     :+
        jsr     release
:       lda     #0
        sta     tmp1
        sta     tmp2
sp_wait:
        sei
        lda     rxoff           ; the other end said XOFF: wait (taking what comes)
        bne     sp_busy
a_s4:   lda     $C0A9
        sta     tmp4
        and     #$08
        beq     :+
a_d4:   lda     $C0A8
        jsr     rx_store
:       lda     tmp4
        and     #$10
        bne     sp_send
sp_busy:
        cli
        inc     tmp1
        bne     sp_wait
        inc     tmp2
        lda     tmp2
        cmp     #24
        bcc     sp_wait
        lda     #0              ; give up on XOFF after a while
        sta     rxoff
        rts
sp_send:
        lda     tmp3
a_d5:   sta     $C0A8
        cli
        rts

.endif

; ---------------------------------------------------------------------------------------------------
; Text screen (page 1, 40 columns)

.ifdef IIGS
        .importzp       _gs_t1, _gs_t2  ; long pointers into bank $E0 (gs.s)
        .define T1 _gs_t1
        .define T2 _gs_t2
.else
        .define T1 ptr1
        .define T2 ptr2
.endif

        .segment "DATA"
_txt_x:         .byte   0
_txt_y:         .byte   0
_txt_inv:       .byte   0
_txt_top:       .byte   0       ; the scrolling window: rows top to bot-1
_txt_bot:       .byte   24
_txt_mt:        .byte   0       ; 1: MouseText (txt_detect)
_ticks:         .word   0
vblwas: .byte   0
        .include "clockvars.inc"

        .segment "RODATA"
rowlo:  .repeat 24, r
        .byte   <($0400 + (r .mod 8) * $80 + (r / 8) * $28)
        .endrepeat
rowhi:  .repeat 24, r
        .byte   >($0400 + (r .mod 8) * $80 + (r / 8) * $28)
        .endrepeat

        .segment "CODE"

_txt_mode:
.ifndef IIGS
        sta     $C00C           ; 40 columns
        sta     $C05F           ; AN3 on: no Double Hi-Res
        sta     $C000           ; 80STORE off
.endif
        io_sta  $C051           ; text
        io_sta  $C054           ; page 1
        rts

_gr_mode:
.ifndef IIGS
        sta     $C00D           ; 80 columns
        sta     $C05E           ; AN3 off: Double Hi-Res
.endif
        io_sta  $C057           ; hi-res
        io_sta  $C052           ; full screen
        io_sta  $C054           ; page 1
        io_sta  $C050           ; graphics
        rts

; void txt_clear(void): clear the window and put the cursor at its top left.
_txt_clear:
        lda     _txt_top
        sta     _txt_y
:       jsr     _txt_clrrow_y
        inc     _txt_y
        lda     _txt_y
        cmp     _txt_bot
        bcc     :-
        lda     _txt_top
        sta     _txt_y
        lda     #0
        sta     _txt_x
        rts

; void __fastcall__ txt_clrrow(unsigned char row)
_txt_clrrow:
        tax
        bpl     clrx
_txt_clrrow_y:
        ldx     _txt_y
clrx:   lda     rowlo,x
        sta     T1
        lda     rowhi,x
        sta     T1+1
        ldy     #39
        lda     #$A0
:       scr_sta T1
        dey
        bpl     :-
        rts

; void txt_clreol(void)
_txt_clreol:
        ldx     _txt_y
        lda     rowlo,x
        sta     T1
        lda     rowhi,x
        sta     T1+1
        ldy     _txt_x
        lda     #$A0
:       cpy     #40
        bcs     :+
        scr_sta T1
        iny
        bne     :-
:       rts

; void __fastcall__ txt_goto(unsigned char x, unsigned char y)
_txt_goto:
        sta     _txt_y
        jsr     popa
        sta     _txt_x
        rts

; unsigned char __fastcall__ txt_peek(void): the screen code at the cursor.
_txt_peek:
        ldx     _txt_y
        lda     rowlo,x
        sta     T1
        lda     rowhi,x
        sta     T1+1
        ldy     _txt_x
        scr_lda T1
        ldx     #0
        rts

; void __fastcall__ txt_cursor(unsigned char code): put a screen code at the cursor (for a cursor).
_txt_cursor:
        pha
        ldx     _txt_y
        lda     rowlo,x
        sta     T1
        lda     rowhi,x
        sta     T1+1
        ldy     _txt_x
        pla
        scr_sta T1
        rts

; void __fastcall__ txt_putc(char c): CR or LF start a new line, 8 goes back one, the rest print.
_txt_putc:
        cmp     #13
        beq     newline
        cmp     #10
        beq     newline
        cmp     #8
        bne     :+
        lda     _txt_x
        beq     pc_done
        dec     _txt_x
        rts
:       cmp     #32
        bcc     pc_done
        cmp     #127
        bcs     pc_apple
        ldx     _txt_inv
        beq     pc_norm
        cmp     #$60            ; lower case: inverse in the alternate character set ($60-$7F)
        bcs     pc_put
        cmp     #$40
        bcc     pc_put
        sbc     #$40
        jmp     pc_put
pc_norm:
        ora     #$80
pc_put: pha
        ldx     _txt_y
        lda     rowlo,x
        sta     T1
        lda     rowhi,x
        sta     T1+1
        ldy     _txt_x
        pla
        scr_sta T1
        iny
        sty     _txt_x
        cpy     #40
        bcc     pc_done
newline:
        lda     #0
        sta     _txt_x
        ldx     _txt_y
        inx
        cpx     _txt_bot
        bcs     scroll
        stx     _txt_y
pc_done:
        rts

; $7F, the Open-Apple key, and $80, the Closed-Apple key: MouseText's apples ($41 and $40, with the
; alternate characters on), or "OA-" and "CA-" without MouseText.
pc_apple:
        cmp     #$81
        bcs     pc_done
        and     #$01            ; 1 open, 0 closed
        ldx     _txt_mt
        beq     :+
        ora     #$40
        jmp     pc_put
:       tax
        lda     #'C'
        cpx     #0
        beq     :+
        lda     #'O'
:       jsr     _txt_putc
        lda     #'A'
        jsr     _txt_putc
        lda     #'-'
        jmp     _txt_putc

; void txt_detect(void): txt_mt 1 on a machine with MouseText: an enhanced IIe, a IIc or a IIgs. These have
; a 65C02 or a 65816, where $1A is INC A; on the older IIe's 6502 it does nothing.
_txt_detect:
        lda     #0
        .byte   $1A
        sta     _txt_mt
        rts

; Scroll the window up a row; the cursor stays on the (now clear) bottom row.
scroll: ldx     _txt_top
sc1:    inx
        cpx     _txt_bot
        bcs     sc2
        lda     rowlo,x
        sta     T2
        lda     rowhi,x
        sta     T2+1
        lda     rowlo-1,x
        sta     T1
        lda     rowhi-1,x
        sta     T1+1
        ldy     #39
:       scr_lda T2
        scr_sta T1
        dey
        bpl     :-
        jmp     sc1
sc2:    dex
        stx     _txt_y
        jmp     _txt_clrrow_y

; void __fastcall__ txt_puts(const char *s)
_txt_puts:
        sta     tp+1
        stx     tp+2
tp:     lda     $FFFF
        beq     :+
        jsr     _txt_putc
        inc     tp+1
        bne     tp
        inc     tp+2
        bne     tp
:       rts

.ifndef IIGS
; ---------------------------------------------------------------------------------------------------
; The language card's bank 1 ($D000-$DFFF), where the IIe keeps the command line, the dialogs and the pages
; of text (frame.c, the overlay OVL_FRAME).

        .importzp       ptr4
        .import         callptr4
        .export         _bank1_call, _bank1_copy

; void bank1_call(void): cc65's wrapper for every call into bank 1 (frame.h), with the function in ptr4
; and its last argument in A and X. Bank 1 is switched in for the call, and bank 2 back after it unless the
; caller was in bank 1 itself; the function's result in A and X is kept.
_bank1_call:
        tay
        lda     $C011           ; RDLCBNK2: bit 7 set while bank 2 is in
        pha
        bit     $C08B           ; the RAM of bank 1, to read and write (two reads)
        bit     $C08B
        tya
        jsr     callptr4
        tay
        pla
        bpl     :+
        bit     $C083           ; the RAM of bank 2 again
        bit     $C083
:       tya
        rts

; void bank1_copy(void): $D000-$DFFF of bank 2 (the overlay just loaded) into bank 1, a byte at a time.
_bank1_copy:
        lda     #0
        sta     ptr1
        lda     #$D0
        sta     ptr1+1
        ldy     #0
bc_lp:  bit     $C083
        bit     $C083
        lda     (ptr1),y
        bit     $C08B
        bit     $C08B
        sta     (ptr1),y
        iny
        bne     bc_lp
        inc     ptr1+1
        lda     ptr1+1
        cmp     #$E0
        bne     bc_lp
        bit     $C083
        bit     $C083
        rts
.endif

; ---------------------------------------------------------------------------------------------------
; Keyboard and time

; unsigned char kbd_get(void): a key (bit 7 clear), or 0. The joystick (joy.s) sees each key, and gives
; its own when there is none.
_kbd_get:
        jsr     _tick_poll
        io_lda  $C000
        bpl     :+
        io_sta  $C010
        and     #$7F
        jmp     joy_keyed
:       jmp     joy_idle

; unsigned char kbd_mods(void): bit 0 Open-Apple held, bit 1 Closed-Apple (Option) held.
_kbd_mods:
.ifdef IIGS
        io_lda  $C062           ; (no long LDX)
        asl     a
        lda     #0
        rol     a
        asl     a
        sta     tmp1
        io_lda  $C061
        asl     a
        lda     #0
        rol     a
        ora     tmp1
.else
        lda     #0
        ldx     $C062
        bpl     :+
        ora     #2
:       ldx     $C061
        bpl     :+
        ora     #1
.endif
:       ldx     #0
        rts

; void tick_poll(void): count vertical blanks (60 a second) in ticks. Call it often.
_tick_poll:
        io_lda  $C019           ; bit 7 low during the vertical blank (IIe; high on the IIgs)
        and     #$80
        cmp     vblwas
        beq     :+
        sta     vblwas
        cmp     #0
        bne     :+
        inc     _ticks
        bne     tk_new
        inc     _ticks+1
tk_new:
        jsr     clk_tick
.ifdef IIGS
        ; the IIgs's monochrome switch (MONOCOLOR, $C021 bit 7) follows the display setting, so a color
        ; monitor shows the monochrome look in black and white
        lda     _mono
        cmp     #1              ; C: monochrome
        php
        io_lda  $C021
        asl     a
        plp
        ror     a
        io_sta  $C021
        lda     _eth_on         ; a new tick while connected: Marinetti's housekeeping (gs.s)
        beq     :+
        jmp     _gs_poll
.endif
:       rts

        .include "clock.inc"

.ifndef IIGS
; ---------------------------------------------------------------------------------------------------
; AppleMouse II: find the card, and call its firmware.

        .segment "DATA"
_mouse_slot:    .byte   0
mjump:  .word   0

        .segment "CODE"

; unsigned char mouse_find(void): the slot of a mouse card (1-7), or 0.
_mouse_find:
        ldx     #7
mf1:    txa
        ora     #$C0
        sta     ptr1+1
        lda     #0
        sta     ptr1
        ldy     #$05
        lda     (ptr1),y
        cmp     #$38
        bne     mf2
        ldy     #$07
        lda     (ptr1),y
        cmp     #$18
        bne     mf2
        ldy     #$0B
        lda     (ptr1),y
        cmp     #$01
        bne     mf2
        ldy     #$0C
        lda     (ptr1),y
        cmp     #$20
        bne     mf2
        ldy     #$FB
        lda     (ptr1),y
        cmp     #$D6
        bne     mf2
        stx     _mouse_slot
        txa
        ldx     #0
        rts
mf2:    dex
        bne     mf1
        lda     #0
        sta     _mouse_slot
        tax
        rts

; void __fastcall__ mouse_call(unsigned int offset_and_a): call the firmware routine whose entry is
; at $Cn00 + (low byte), with A = high byte. ($12 SETMOUSE, $14 READMOUSE, $17 CLAMPMOUSE,
; $19 INITMOUSE.)
_mouse_call:
        stx     tmp1            ; A for the call
        tay
        lda     _mouse_slot
        beq     :+
        ora     #$C0
        sta     ptr1+1
        sta     mjump+1
        lda     #0
        sta     ptr1
        lda     (ptr1),y
        sta     mjump
        lda     _mouse_slot
        asl     a
        asl     a
        asl     a
        asl     a
        tay                     ; Y = $n0
        ldx     mjump+1         ; X = $Cn
        lda     mjump
        sta     mgo+1
        lda     mjump+1
        sta     mgo+2
        lda     tmp1
        php
        sei
mgo:    jsr     $FFFF
        plp
:       rts

        .segment "VECTORS"
        .word   nmi             ; $FFFA NMI
        .word   $FA62           ; $FFFC reset (the IIe reads it from ROM anyway)
        .word   irq             ; $FFFE IRQ and BRK

        .segment "CODE"
nmi:    rti
.endif
