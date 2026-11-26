; ParlorNet for the Apple IIe and IIgs: the Apple joystick, read as keys when the settings turn it on (J).
;
; kbd_get (sys.s) comes here: joy_keyed with a key it has taken, joy_idle when there was none. Pushing the
; stick past a dead zone in the middle gives the arrow key for that way, as the C64's joystick does, and
; holding it there repeats it as the IIe's keys repeat. The stick is read about 15 times a second, and a
; reading takes up to about 3 ms. The buttons are the same lines as the Open-Apple and Closed-Apple keys:
; button 0 pressed and let go with no key between gives Return, button 1 the second button (K_BACK, which
; cancels as Esc does but never leaves a table), and with a key the button was the Apple key held for it,
; so Open-Apple shortcuts work as before. A game port with no joystick in it never ends its
; timers' pulses, so a reading that runs out means no joystick, and then the stick and the buttons do
; nothing. Some IIe owners have no joystick, and that is why the setting starts off.
;
; On the IIe this is in the page of the 6502 vectors ($FF00, the language card's RAM), which the start-up
; loads with them, since the program itself has no room left below $BF00. On the IIgs it is part of the
; program, and the stick is read at the IIgs's slow speed, at which its timing is the IIe's.

        .include "a2gs.inc"
        .import         _ticks
        .export         joy_keyed, joy_idle, _joy_on

; The dead zone, in the reading's counts (17 cycles each, so 11/17 of the PDL value): the stick counts as
; pushed below about PDL 64 or above about PDL 192.
JLO     = 41
JHI     = 125
K_LEFT  = 8
K_RIGHT = 21
K_UP    = 11
K_DOWN  = 10

        .segment "DATA"         ; (in the program: the vectors' page has room for the code alone)
_joy_on:.byte   0               ; the setting: 1 to read the joystick
jlast:  .byte   0               ; ticks (low byte) at the last reading
jdir:   .byte   0               ; the arrow key the stick gave at the last reading, or 0
jrpt:   .byte   0               ; readings until it repeats
jbtn:   .byte   0               ; 1 while button 0 is down, 2 while button 1 is
jused:  .byte   0               ; 1 when a key came while a button was down
jmiss:  .byte   1               ; 1 while no joystick answers (before the first reading too)
jx:     .byte   0               ; the last reading's counts, $FF when it ran out
jy:     .byte   0

.ifdef IIGS
        .segment "CODE"
.else
        .segment "JOYCODE"
.endif

; A key from kbd_get (in A, kept): with a button down, the button is an Apple key held for it.
joy_keyed:
        ldx     _joy_on
        beq     jk_done
        pha
        io_lda  $C061
        bmi     jk_btn
        io_lda  $C062
        bpl     jk_up
jk_btn: lda     #1
        sta     jbtn
        sta     jused
jk_up:  pla
jk_done:ldx     #0
        rts

; No key from kbd_get: a key from the joystick, or 0.
joy_idle:
        lda     _joy_on
        bne     :+
        tax
        rts
:
        io_lda  $C061
        bmi     jdown
        io_lda  $C062
        bmi     jdown2
        ldy     jbtn
        beq     jstick
        lda     #0              ; a button let go, unless a key came while it was down: button 0 is
        sta     jbtn            ; Return, button 1 the second button (K_BACK), which cancels as Esc does
        ldx     jused
        sta     jused
        txa
        ora     jmiss
        bne     jnone
        lda     #13
        dey
        beq     :+
        lda     #28
:       rts
jdown2: lda     #2
        bne     jd
jdown:  lda     #1              ; (the stick waits while a button is down)
jd:     sta     jbtn
        bne     jnone

jstick: lda     _ticks
        sec
        sbc     jlast
        cmp     #4
        bcc     jnone
        lda     _ticks
        sta     jlast
        jsr     jread
        lda     jx              ; both ran out: no joystick
        and     jy
        cmp     #$FF
        lda     #0
        rol     a
        sta     jmiss
        ldy     #K_UP           ; the way it is pushed, up and down first, as on the C64; an axis that
        lda     jy              ; ran out counts as the middle
        cmp     #JLO
        bcc     jgot
        ldy     #K_DOWN
        cmp     #JHI
        bcc     :+
        cmp     #$FF
        bne     jgot
:       ldy     #K_LEFT
        lda     jx
        cmp     #JLO
        bcc     jgot
        ldy     #K_RIGHT
        cmp     #JHI
        bcc     jmid
        cmp     #$FF
        bne     jgot
jmid:   ldy     #0
jgot:   tya
        beq     jstill
        cpy     jdir
        beq     jsame
        sty     jdir            ; a new way: its key now, and again after about half a second
        lda     #8
        sta     jrpt
        bne     jkey
jsame:  dec     jrpt
        bne     jnone
        inc     jrpt            ; then at each reading, 15 a second
jkey:   tya
        ldx     #0
        rts
jstill: sta     jdir
jnone:  lda     #0
        tax
        rts

; Read the stick: trigger the game port's timers, then count until each one's pulse ends, both in the
; same loop of 17 cycles. A count that reaches $FF has run out.
jread:
.ifdef IIGS
        io_lda  $C036           ; the IIgs at its slow speed while it counts
        pha
        and     #$7F
        io_sta  $C036
.endif
        io_lda  $C070
        ldy     #0
jr_b:   io_lda  $C064
        bpl     jr_x
        io_lda  $C065
        bpl     jr_y
        iny
        bne     jr_b
        dey                     ; (both ran out: x has $FF, and y runs out at once below)
jr_x:   sty     jx              ; x has ended: y counts on
        ldx     #1
        bne     jr_1
jr_y:   sty     jy              ; y has ended: x counts on
        ldx     #0
jr_1:
.ifdef IIGS
        lda     f:$E0C064,x
.else
        lda     $C064,x
.endif
        bpl     jr_1e
        io_lda  $C064           ; (as long as the loop for both)
        nop
        iny
        bne     jr_1
        dey
jr_1e:  tya
        sta     jx,x
jr_end:
.ifdef IIGS
        pla
        io_sta  $C036
.endif
        rts
