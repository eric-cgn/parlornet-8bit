; ParlorNet on the Commodore 64: the serial line. Three ways, our own drivers all:
;
; - The user port, bit by bit (SER_USER), wired as the KERNAL's RS-232 is: RXD on PB0 and FLAG2, TXD on PA2,
;   RTS PB1, DTR PB2. A start bit's falling edge on FLAG2 starts CIA2's timer B, whose NMIs sample PB0 near
;   the middle of each bit (FLAG2's interrupt is off while a byte comes in: each falling edge in it would
;   interrupt). CIA1's timer A sends a byte a bit at a time, on the IRQ (sys.s calls irq_tx), with two stop
;   bits, so that the other end finds each start bit however late an interrupt comes. 300 to 2400 baud, both
;   ways at once. (Sending on CIA2's timer A as well, by NMI, loses an underflow now and then: an NMI is
;   taken on an edge, and one that comes as the interrupt register is read can slip by. The IRQ is taken on
;   a level.)
; - The user port's UP9600 wiring (SER_UP9600), 9600 baud: RXD also on SP2 (CIA2's serial port) and FLAG2,
;   with PB7 wired to CNT2, and TXD on SP1 (CIA1's). A start bit on FLAG2 starts CIA2's timer B toggling
;   PB7, whose rising edges in the middle of the eight data bits shift them into CIA2's shift register; its
;   interrupt then has the byte, the first bit last. To send, CIA1's shift register, clocked by its timer
;   A at the bit rate, shifts out the start bit and the first seven bits, then the last bit and the stop bit.
; - A SwiftLink or Turbo232 cartridge's 6551 (SER_SWIFT at $DE00, SER_SWIFT_DF at $DF00), received by
;   interrupt as on the Apple II's Super Serial Card; its crystal makes the rates twice the 6551's own.
;
; Received bytes go into a 256-byte ring from the NMI; bytes to send bit by bit go through a ring of their
; own, and the other ways send them as they come. The NMI (sys.s jumps through nmi_vec) has a handler for each
; way; they use no zero page.

        .importzp       tmp1, tmp2, tmp3
        .import         popa, _pal
        .import         _eth_on, _eth_get, _eth_put, _eth_drop
        .export         _ser_open, _ser_close, _ser_get, _ser_put, _ser_used, _ser_drop
        .export         _ser_pause, _ser_resume, _ser_flow, _ser_lost, _ser_iface
        .export         nmi_vec, irq_tx, _ser_irq

CIA1    = $DC00
CIA2    = $DD00
SER_USER   = 0
SER_UP9600 = 1
SER_SWIFT  = 2

        .segment "RING"
ring:   .res    256
        .segment "TXRING"
txring: .res    256

        .segment "DATA"
nmi_vec:        .word   nmi_none
_ser_irq:       .byte   0       ; 1 while CIA1's timer A sends bit by bit (sys.s: its IRQ is ours)
_ser_flow:      .byte   0       ; 0 none, 1 RTS/CTS
_ser_lost:      .byte   0       ; bytes dropped because the ring was full
_ser_iface:     .byte   $FF     ; the way in use ($FF: none open)
rhead:  .byte   0               ; written by the NMI
rtail:  .byte   0
held:   .byte   0               ; 1 while RTS is off because the ring is nearly full
paused: .byte   0
icr:    .byte   0
; the user port, bit by bit
bitlo:  .byte   0               ; a bit's cycles less one
bithi:  .byte   0
c0lo:   .byte   0               ; from the start bit's interrupt to the first sample
c0hi:   .byte   0
t0lo:   .byte   0               ; from the start bit sent to timer A's first underflow
t0hi:   .byte   0
rxcnt:  .byte   0               ; data bits still to come (0: waiting for a start bit)
rxwas:  .byte   0
rxbyte: .byte   0
txhead: .byte   0
txtail: .byte   0
txbusy: .byte   0
txcnt:  .byte   0
txbyte: .byte   0
txbase: .byte   0               ; CIA2's port A with TXD low (the start bit, a 0)
txout:  .byte   0               ; CIA2's port A for CIA1's timer A's next underflow, made ready beforehand
; UP9600
srpend: .byte   0               ; bytes in CIA1's shift register still to go
revtmp: .byte   0
; the 6551
cmdreg: .byte   $09

        .segment "RODATA"
; Cycles a bit, less one, at 300, 1200, 2400 and 4800 baud: PAL (985248 Hz), then NTSC (1022727 Hz). Then
; timer B's first count after the start bit's interrupt: a bit and a half, less the time from the edge to
; starting the timer (about 60 cycles) and from an underflow to the sample (about 40), so that each sample
; comes near the middle of its bit, with room for a late interrupt (a bad line, sprites, the other timer).
bit_pal_lo:     .byte   <3283, <820, <409, <204
bit_pal_hi:     .byte   >3283, >820, >409, >204
bit_ntsc_lo:    .byte   <3408, <851, <425, <212
bit_ntsc_hi:    .byte   >3408, >851, >425, >212
c0_pal_lo:      .byte   <(4926-100), <(1231-100), <(615-100), <(307-100)
c0_pal_hi:      .byte   >(4926-100), >(1231-100), >(615-100), >(307-100)
c0_ntsc_lo:     .byte   <(5113-100), <(1278-100), <(639-100), <(319-100)
c0_ntsc_hi:     .byte   >(5113-100), >(1278-100), >(639-100), >(319-100)
; Timer A's first count after a start bit is sent: 0.6 of a bit, so that each bit after it goes out 0.4 of a
; bit early, and a late interrupt still puts it on the line before the middle of its bit (where the other end
; samples it), up to about 0.9 of a bit late.
t0_pal_lo:      .byte   <1970, <493, <246, <123
t0_pal_hi:      .byte   >1970, >493, >246, >123
t0_ntsc_lo:     .byte   <2045, <511, <256, <128
t0_ntsc_hi:     .byte   >2045, >511, >256, >128
; The 6551's rate codes for the baud_name indexes (300 ... 38400), with the SwiftLink's doubled crystal.
acia_rate:      .byte   $05, $07, $08, $0A, $0C, $0E, $0F

        .segment "CODE"

; void __fastcall__ ser_open(unsigned char iface, unsigned char baud)
_ser_open:
        sta     tmp2            ; baud
        jsr     popa
        sta     tmp1            ; iface
        jsr     _ser_close
        lda     #0
        sta     rhead
        sta     rtail
        sta     held
        sta     paused
        sta     txhead
        sta     txtail
        sta     txbusy
        sta     srpend
        sta     rxcnt
        lda     tmp1
        sta     _ser_iface
        cmp     #SER_SWIFT
        bcc     :+
        jmp     open_acia
:       ; the user port: DTR and RTS on (PB1, PB2 out, high), TXD idle (PA2 out, high)
        lda     #$06
        sta     CIA2+3
        sta     CIA2+1
        lda     CIA2+2
        ora     #$04
        sta     CIA2+2
        lda     CIA2
        ora     #$04
        sta     CIA2
        lda     #0
        sta     CIA2+$0E        ; timers stopped
        sta     CIA2+$0F
        lda     tmp1
        cmp     #SER_UP9600
        beq     open_up9600
        ; bit by bit: the rate's timings (300 to 4800)
        ldx     tmp2
        cpx     #4
        bcc     :+
        ldx     #2
:       lda     _pal
        beq     :+
        lda     bit_pal_lo,x
        sta     bitlo
        lda     bit_pal_hi,x
        sta     bithi
        lda     c0_pal_lo,x
        sta     c0lo
        lda     c0_pal_hi,x
        sta     c0hi
        lda     t0_pal_lo,x
        sta     t0lo
        lda     t0_pal_hi,x
        sta     t0hi
        jmp     :++
:       lda     bit_ntsc_lo,x
        sta     bitlo
        lda     bit_ntsc_hi,x
        sta     bithi
        lda     c0_ntsc_lo,x
        sta     c0lo
        lda     c0_ntsc_hi,x
        sta     c0hi
        lda     t0_ntsc_lo,x
        sta     t0lo
        lda     t0_ntsc_hi,x
        sta     t0hi
:       lda     #<nmi_bits
        ldx     #>nmi_bits
        jmp     on_cia2

open_up9600:
        jsr     up_tx
        ; CIA2 receives: its timer B's latch at half a bit, the shift register in
        stx     CIA2+6
        lda     #0
        sta     CIA2+7
        lda     CIA2+$0E
        and     #$BF            ; serial port in
        sta     CIA2+$0E
        lda     #<nmi_up9600
        ldx     #>nmi_up9600
        jmp     on_cia2

; UP9600: CIA1 sends, its timer A at half a bit and the shift register out. Again after a pause, since the
; pause and the KERNAL's disk calls read CIA1's interrupt flags, which ser_put watches for the register.
; Returns the timer's count in X.
up_tx:
        ldx     #50             ; PAL: 2 * 51 cycles a bit, 9659 baud
        lda     _pal
        bne     :+
        ldx     #52             ; NTSC: 2 * 53, 9648 baud
:       stx     CIA1+4
        lda     #0
        sta     CIA1+5
        lda     #$51            ; start, force load, continuous, serial port out
        sta     CIA1+$0E
        lda     CIA1+$0D        ; (its interrupts stay off; ser_put watches the flag)
        lda     #$FF            ; the line idle
        sta     CIA1+$0C
        lda     #1
        sta     srpend
        rts

on_cia2:
        sta     nmi_vec
        stx     nmi_vec+1
; The interrupts as they are between bytes: CIA2's FLAG (a start bit), and bit by bit CIA1's timer A (sending,
; by IRQ); with UP9600, CIA2's FLAG and shift register.
cia2_idle:
        lda     #$7F
        sta     CIA2+$0D
        lda     CIA2+$0D
        ldx     #$98
        lda     _ser_iface
        bne     :+
        lda     #$7F
        sta     CIA1+$0D
        lda     CIA1+$0D
        lda     #$81
        sta     CIA1+$0D
        lda     #1
        sta     _ser_irq
        ldx     #$90
:       stx     CIA2+$0D
        rts

open_acia:
        ldx     #$DE
        cmp     #SER_SWIFT
        beq     :+
        ldx     #$DF
:       stx     a_s1+2          ; the registers' page in each access
        stx     a_d1+2
        stx     a_c1+2
        stx     a_s2+2
        stx     a_d2+2
        stx     a_r1+2
        stx     a_k1+2
        stx     a_d3+2
a_r1:   sta     $DE01           ; any write to the status register resets the 6551
        ldx     tmp2
        lda     acia_rate,x
        ora     #$10            ; 8 bits, 1 stop bit, the internal clock
a_k1:   sta     $DE03
        lda     #$09            ; DTR on, RTS on, the receive interrupt on, no parity
        sta     cmdreg
        jsr     acia_cmd
a_d3:   lda     $DE00           ; drop anything already there
        lda     #<nmi_acia
        sta     nmi_vec
        lda     #>nmi_acia
        sta     nmi_vec+1
        rts

; void ser_close(void): the interrupts off; DTR stays on, so a modem keeps its call.
_ser_close:
        lda     _ser_iface
        bmi     sc_done
        cmp     #SER_SWIFT
        bcs     :+
        lda     #$7F
        sta     CIA2+$0D
        sta     CIA1+$0D
        lda     #0
        sta     _ser_irq
        sta     CIA2+$0E
        sta     CIA2+$0F
        sta     CIA1+$0E
        lda     CIA2+$0D
        lda     CIA1+$0D
        jmp     sc_off
:       lda     cmdreg
        ora     #$02            ; the 6551's receive interrupt off
        jsr     acia_cmd
sc_off: lda     #<nmi_none
        sta     nmi_vec
        lda     #>nmi_none
        sta     nmi_vec+1
        lda     #$FF
        sta     _ser_iface
sc_done:
        rts

; ---------------------------------------------------------------------------------------------------
; The NMI handlers (sys.s jumps through nmi_vec with nothing saved). RESTORE comes here too, and is ignored.

nmi_none:
        rti

; Bit by bit: timer B (a sample) and FLAG (a start bit).
nmi_bits:
        pha
        lda     CIA2+$0D        ; what happened (reading clears it)
        sta     icr
        and     #$02            ; timer B: a sample (only while a byte comes in)
        beq     nb_flag
        lda     CIA2+1          ; a data bit from PB0, the first in bit 0
        lsr     a
        ror     rxbyte
        dec     rxcnt
        bne     nb_tx
        lda     #0              ; the eighth: timer B stopped, its interrupt off, FLAG's on
        sta     CIA2+$0F
        lda     #$02
        sta     CIA2+$0D
        lda     #$90
        sta     CIA2+$0D
        txa
        pha
        lda     rxbyte
        jsr     rx_store
        pla
        tax
        jmp     nb_tx
nb_flag:
        lda     rxcnt           ; (FLAG is on only between bytes, but its flag is set by every falling edge)
        bne     nb_tx
        lda     icr
        and     #$10
        beq     nb_tx
        lda     #$10            ; a start bit: FLAG off, timer B to the middle of the first data bit
        sta     CIA2+$0D
        lda     c0lo
        sta     CIA2+6
        lda     c0hi
        sta     CIA2+7
        lda     #$11            ; start, force load, continuous
        sta     CIA2+$0F
        lda     bitlo           ; then a bit at a time
        sta     CIA2+6
        lda     bithi
        sta     CIA2+7
        lda     #$82
        sta     CIA2+$0D
        lda     #8
        sta     rxcnt
nb_tx:
nb_done:
        pla
        rti

; Bit by bit, CIA1's timer A (the IRQ, sys.s, with A, X and Y saved): the bit made ready goes out, and the
; next one is made ready. Sent bits go out early enough (0.4 of a bit) to stand some delay.
irq_tx: lda     txbusy
        beq     :+
        lda     txout
        sta     CIA2
        jmp     tx_ready
:       rts

; The next bit out (txout) after the one just sent: txcnt counts down from 11 (the start bit sent): 10 to 3
; the data bits, 2 and 1 the stop bits, 0 the next byte's start bit, or the end (timer A stopped; the line
; stays at the stop bits' level). Uses A and X.
tx_ready:
        dec     txcnt
        beq     tr_next
        lda     txcnt
        cmp     #3
        bcc     tr_one
        lsr     txbyte
        lda     txbase
        bcc     :+
tr_one: lda     txbase
        ora     #$04
:       sta     txout
        rts
tr_next:
        ldx     txtail
        cpx     txhead
        beq     tr_idle
        lda     txring,x
        sta     txbyte
        inc     txtail
        lda     #11
        sta     txcnt
        lda     txbase          ; its start bit
        sta     txout
        rts
tr_idle:
        lda     #0
        sta     CIA1+$0E        ; timer A stopped
        sta     txbusy
        rts

; UP9600: FLAG (a start bit) starts timer B toggling PB7 (CNT2): low after its first count, then high (a
; sample) every bit from the middle of the first data bit. The shift register's interrupt has the byte, the
; first bit in bit 7: reversed, it goes into the ring.
nmi_up9600:
        pha
        lda     CIA2+$0D
        sta     icr
        txa
        pha
        lda     icr
        and     #$08
        beq     nu_flag
        lda     #0
        sta     CIA2+$0F        ; timer B stopped
        sta     rxcnt
        lda     CIA2+$0C
        ldx     #8
:       lsr     a
        rol     revtmp
        dex
        bne     :-
        lda     CIA2+1          ; (reading port B: VICE counts PB7's changes otherwise)
        lda     #$90            ; FLAG on again
        sta     CIA2+$0D
        lda     revtmp
        jsr     rx_store
        jmp     nu_done
nu_flag:
        lda     icr
        and     #$10
        beq     nu_done
        lda     rxcnt
        bne     nu_done
        lda     #$10            ; FLAG off until the byte is in
        sta     CIA2+$0D
        ldx     #52             ; to the first rising edge: a bit and a half from the start bit's edge
        lda     _pal
        bne     *+4
        ldx     #56
        stx     CIA2+6
        lda     #0
        sta     CIA2+7
        lda     #$17            ; start, force load, continuous, PB7 toggles
        sta     CIA2+$0F
        ldx     #50             ; then half a bit a count
        lda     _pal
        bne     *+4
        ldx     #52
        stx     CIA2+6
        lda     #1
        sta     rxcnt
nu_done:
        pla
        tax
        pla
        rti

; The 6551: a received byte into the ring.
nmi_acia:
        pha
        txa
        pha
a_s1:   lda     $DE01
        and     #$08
        beq     :+
a_d1:   lda     $DE00
        jsr     rx_store
:       pla
        tax
        pla
        rti

; A (a received byte) into the ring; when it is nearly full and flow control is on, RTS off. Uses X.
rx_store:
        ldx     rhead
        sta     ring,x
        inx
        cpx     rtail
        beq     rs_full
        stx     rhead
        txa
        sec
        sbc     rtail
        cmp     #192
        bcc     rs_done
        lda     held
        bne     rs_done
        lda     _ser_flow
        beq     rs_done
        lda     #1
        sta     held
        jmp     rts_off
rs_full:
        inc     _ser_lost
rs_done:
        rts

; RTS off and on: PB1 on the user port, the command register's bits 2-3 on the 6551.
rts_off:
        lda     _ser_iface
        cmp     #SER_SWIFT
        bcs     :+
        lda     CIA2+1
        and     #$FD
        sta     CIA2+1
        rts
:       lda     cmdreg
        and     #$F3
        jmp     acia_cmd

rts_on: lda     _ser_iface
        cmp     #SER_SWIFT
        bcs     :+
        lda     CIA2+1
        ora     #$02
        sta     CIA2+1
        rts
:       lda     cmdreg
acia_cmd:
a_c1:   sta     $DE02
        rts

; ---------------------------------------------------------------------------------------------------

; int ser_get(void): the next received byte, or -1.
_ser_get:
        lda     _eth_on         ; over a network connection instead (eth.h)
        beq     :+
        jmp     _eth_get
:       ldx     rtail
        cpx     rhead
        beq     :++
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
        lda     #0
        sta     held
        lda     paused
        bne     :+
        jsr     rts_on
:       pla
        ldx     #0
        rts
:       lda     #$FF
        tax
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
:       lda     rhead
        sta     rtail
        lda     held
        beq     :+
        lda     #0
        sta     held
        jsr     rts_on
:       rts

; void __fastcall__ ser_put(unsigned char c)
_ser_put:
        ldx     _eth_on
        beq     :+
        jmp     _eth_put
:       ldx     _ser_iface
        bmi     sp_done
        cpx     #SER_UP9600
        bne     :+
        jmp     put_up9600
:       bcc     :+
        jmp     put_acia
:       ; bit by bit: into the send ring (waiting while it is full), and the sender started if idle
        ldx     txhead
        inx
:       cpx     txtail
        beq     :-
        ldx     txhead
        sta     txring,x
        inc     txhead
        lda     txbusy
        bne     sp_done
        ; The start bit and the timer that times the bits after it, with the IRQ held off between them: a
        ; raster interrupt there (sprites and the keyboard, thousands of cycles) would stretch the start bit
        ; over several bits, and the other end would read the bytes after it wrong.
        php
        sei
        lda     #1
        sta     txbusy
        ldx     txtail
        lda     txring,x
        sta     txbyte
        inc     txtail
        lda     CIA2
        and     #$FB
        sta     txbase
        lda     #11
        sta     txcnt
        jsr     tx_ready        ; bit 0 made ready
        lda     t0lo
        sta     CIA1+4
        lda     t0hi
        sta     CIA1+5
        lda     txbase          ; the start bit now, and timer A for the rest: 0.6 of a bit, then bits
        sta     CIA2
        lda     #$11
        sta     CIA1+$0E
        lda     bitlo
        sta     CIA1+4
        lda     bithi
        sta     CIA1+5
        plp
sp_done:
        rts

; UP9600: two bytes through CIA1's shift register, the first bit first: the start bit and bits 0 to 6,
; then bit 7 and the stop bit (and idle ones). The register holds one while it shifts another.
put_up9600:
        sta     tmp1
        ldx     #8
:       lsr     a
        rol     tmp2
        dex
        bne     :-
        lda     tmp2            ; bit 0 first
        lsr     a               ; the start bit (0) before it
        jsr     sr_out
        lda     tmp1
        ora     #$7F
sr_out: ldx     srpend
        cpx     #2
        bcc     :++
        pha
:       lda     CIA1+$0D
        and     #$08
        beq     :-
        dec     srpend
        pla
:       sta     CIA1+$0C
        inc     srpend
        rts

; The 6551: when its transmitter is free (up to about a second).
put_acia:
        sta     tmp3
        lda     #0
        sta     tmp1
        sta     tmp2
:
a_s2:   lda     $DE01
        and     #$10
        bne     :+
        inc     tmp1
        bne     :-
        inc     tmp2
        lda     tmp2
        cmp     #24
        bcc     :-
        rts
:       lda     tmp3
a_d2:   sta     $DE00
        rts

; void ser_pause(void), void ser_resume(void): around the KERNAL's disk calls and spr_load, whose timing or
; banking the line's interrupts would upset: RTS off (the other end holds what it has, with flow control) and
; the interrupts off; then on again.
_ser_pause:
        lda     _ser_iface
        bmi     pz_done
        lda     #1
        sta     paused
        jsr     rts_off
        lda     _ser_iface
        cmp     #SER_SWIFT
        bcs     :++
:       lda     txbusy          ; the last byte sent first
        bne     :-
        lda     #$7F
        sta     CIA2+$0D
        sta     CIA1+$0D
        lda     #0
        sta     _ser_irq
        sta     CIA2+$0F
        sta     rxcnt
        lda     CIA1+$0D
        rts
:       lda     cmdreg
        ora     #$02
        and     #$F3
        jmp     acia_cmd
pz_done:
        rts

_ser_resume:
        lda     _ser_iface
        bmi     pz_done
        lda     #0
        sta     paused
        lda     _ser_iface
        cmp     #SER_SWIFT
        bcs     rs_acia
        cmp     #SER_UP9600
        bne     :+
        jsr     up_tx
:       jsr     cia2_idle
        lda     held
        bne     pz_done
        jmp     rts_on
rs_acia:
        lda     held
        bne     :+
        lda     cmdreg
        jmp     acia_cmd
:       lda     cmdreg
        and     #$F3
        jmp     acia_cmd
