; ParlorNet: the time signed in to the parlor (clk_*, machine.h); clk_tick is called for each tick.
; Machine-neutral 6502 code, .included by each machine's sys.s where it belongs among its own code and data.

; The time signed in: the seconds the parlor last gave (TIM: clk_sync, clk_set), or 0 when sign-in
; completes, and the seconds of 60 ticks since. clk_tick: one tick more. Keeps X and Y.
clk_tick:
        dec     _clk_sub
        bne     ck_done
        lda     #60
        sta     _clk_sub
        inc     _clk_t
        bne     ck_done
        inc     _clk_t+1
        bne     ck_done
        inc     _clk_t+2
ck_done:
        rts

; void __fastcall__ clk_set(const char *secs): the time signed in from the parlor's decimal seconds.
_clk_set:
        sta     ptr1
        stx     ptr1+1
        ldy     #0
        sty     _clk_t
        sty     _clk_t+1
        sty     _clk_t+2
cs_dig: lda     (ptr1),y
        sec
        sbc     #'0'
        cmp     #10
        bcs     cs_end
        pha
        jsr     ck_dbl          ; t * 2, kept in w
        ldx     #2
:       lda     _clk_t,x
        sta     ck_w,x
        dex
        bpl     :-
        jsr     ck_dbl          ; t * 8
        jsr     ck_dbl
        ldx     #$FD            ; + t * 2
        clc
:       lda     _clk_t-$FD,x
        adc     ck_w-$FD,x
        sta     _clk_t-$FD,x
        inx
        bne     :-
        pla                     ; + the digit
        ldx     #$FD
        clc
:       adc     _clk_t-$FD,x
        sta     _clk_t-$FD,x
        lda     #0
        inx
        bne     :-
        iny
        bne     cs_dig
cs_end: lda     #60
        sta     _clk_sub
        rts
ck_dbl: asl     _clk_t
        rol     _clk_t+1
        rol     _clk_t+2
        rts

; unsigned char clk_min(void): the minutes within the hour (0 to 59) of the time signed in, which change
; once a minute; w: the whole hours. X is 0 after.
_clk_min:
        ldy     #2
:       lda     _clk_t,y
        sta     ck_w,y
        dey
        bpl     :-
        jsr     div60           ; (the seconds)
        jsr     div60           ; the minutes; w: the hours
        ldx     #0
        rts

; const char *clk_text(void): the time signed in, in whole minutes in clk_buf, "0m", "59m" or "1h 5m" (up to
; 99 hours), right-aligned in 8 characters; notes its minutes as shown (clk_shown).
_clk_text:
        jsr     _clk_min
        sta     _clk_shown
        ldx     #8
        pha
        lda     #'m'
        jsr     ct_chr
        pla
        jsr     ct_num
        lda     ck_w
        beq     ct_pad          ; under an hour: the minutes alone
        lda     #' '
        jsr     ct_chr
        lda     #'h'
        jsr     ct_chr
        lda     ck_w
        jsr     ct_num
ct_pad: txa
        beq     ct_end
        lda     #' '
        jsr     ct_chr
        bne     ct_pad          ; (Z: x is 0)
ct_end: lda     #<_clk_buf
        ldx     #>_clk_buf
        rts
ct_num: jsr     ct_dig          ; A (under 100) in one digit or two, before x
        beq     ct_ret
ct_put: ora     #'0'
ct_chr: dex
        sta     _clk_buf,x
ct_ret: rts
ct_dig: ldy     #0              ; the units of A (under 100) before x; A = the tens
:       cmp     #10
        bcc     :+
        sbc     #10
        iny
        bne     :-
:       jsr     ct_put
        tya
        rts
div60:  lda     #0              ; w = w / 60, A = the remainder; keeps X
        ldy     #24
:       asl     ck_w
        rol     ck_w+1
        rol     ck_w+2
        rol     a
        cmp     #60
        bcc     :+
        sbc     #60
        inc     ck_w
:       dey
        bne     :--
cy_ret: rts                     ; (clk_sync's, too: its own end is out of a branch's reach)

; void clk_sync(void): the time signed in from the parlor, which counts it: TIM answers "TIM secs now".
; Asked at the lobby's first poll (the sign-in sets clk_tim_at to $FF), then along with the lobby's and
; the table's polls once in each 32 seconds; the ticks count the seconds in between, and alone after an
; ERR (from a parlor that does not know TIM: ERR UNKNOWN). The program's own count since sign-in comes
; first: TIM's secs are taken when they are less, or at most 30 seconds over it.
_clk_sync:
        lda     _clk_tim_off
        bne     cy_ret
        lda     _clk_t
        and     #$E0
        cmp     _clk_tim_at
        beq     cy_ret
        lda     #<s_tim
        ldx     #>s_tim
        jsr     _q_set
        jsr     _net_req
        cmp     #1              ; R_OK, with "TIM secs now"
        bne     cy_err
        lda     _r_n            ; (its one line, in r_text)
        beq     cy_at
        lda     #<_r_text
        ldx     #>_r_text
        sta     ptr1
        stx     ptr1+1
        ldy     #0
        lda     (ptr1),y
        cmp     #'T'
        bne     cy_at
        ldy     #3              ; the count since sign-in (clk_t and clk_sub after it), kept in o
:       lda     _clk_t,y
        sta     ck_o,y
        dey
        bpl     :-
        lda     ptr1
        clc
        adc     #4
        bcc     :+
        inx
:       jsr     _clk_set
        sec                     ; TIM's secs less o: TIM is taken when under o or at most 30 over
        lda     _clk_t
        sbc     ck_o
        tay
        lda     _clk_t+1
        sbc     ck_o+1
        tax
        lda     _clk_t+2
        sbc     ck_o+2
        bcc     cy_at
        bne     cy_own
        txa
        bne     cy_own
        cpy     #31
        bcc     cy_at
cy_own: ldy     #3              ; more: the count since sign-in again
:       lda     ck_o,y
        sta     _clk_t,y
        dey
        bpl     :-
        bmi     cy_at
cy_err: cmp     #2              ; R_ERR
        bne     cy_at
        sta     _clk_tim_off
cy_at:  lda     _clk_t
        and     #$E0
        sta     _clk_tim_at
        rts
s_tim:  .byte   "TIM", 0

; void clk_show(void): when signed in and the minute has changed (or clk_shown is 255, after ui_cls), the
; time signed in at the right end of the Hi-Res title row, columns 32 to 39, in inverse (the lobby and the
; tables: ui_poll). Once a minute, so the pointer, hidden while it is drawn, does not flicker.
_clk_show:
        lda     _signed_in
        beq     cs_done
        jsr     _clk_min
        cmp     _clk_shown
        beq     cs_done
        jsr     _ui_hide
        lda     #0              ; F_BLACK on F_WHITE
        jsr     pusha
        lda     #1
        jsr     _ui_ink
        lda     #32
        sta     _hg_x
        lda     #0
        sta     _hg_y
        jsr     _clk_text
        jsr     _hgr_text
        lda     #1              ; F_WHITE on F_BLACK
        jsr     pusha
        lda     #0
        jsr     _ui_ink
        jmp     _ui_show
cs_done:
        rts
