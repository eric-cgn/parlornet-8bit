; ParlorNet: the time connected to the parlor (clk_*, machine.h); clk_tick is called for each tick.
; Machine-neutral 6502 code, .included by each machine's sys.s where it belongs among its own code and data.

; The time connected to the parlor: the seconds the parlor last gave (TIM: clk_sync, clk_set), or 0 at
; connecting, and the seconds of 60 ticks since. clk_tick: one tick more. Keeps X and Y.
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

; void __fastcall__ clk_set(const char *secs): the connected time from the parlor's decimal seconds.
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

; unsigned char clk_min(void): the connected time's minutes within the hour (0 to 59), which change once a
; minute; w: the whole hours. X is 0 after.
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

; const char *clk_text(void): the connected time in whole minutes in clk_buf, "0m", "59m" or "1h 5m" (up to
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
        rts

; void clk_sync(void): the connected time from the parlor, which counts it: TIM answers "TIM secs now".
; Asked at connecting (the sign-in sets clk_tim_at to $FF), then along with the lobby's and the table's
; polls once in each 32 seconds; the ticks count the seconds in between, and alone after an ERR (from a
; parlor that does not know TIM: ERR UNKNOWN).
_clk_sync:
        lda     _clk_tim_off
        bne     cy_done
        lda     _clk_t
        and     #$E0
        cmp     _clk_tim_at
        beq     cy_done
        lda     #<s_tim
        ldx     #>s_tim
        jsr     _q_set
        jsr     _net_req
        cmp     #1              ; R_OK, with "TIM secs now"
        bne     cy_err
        lda     _r_n
        beq     cy_at
        lda     _r_line
        ldx     _r_line+1
        sta     ptr1
        stx     ptr1+1
        ldy     #0
        lda     (ptr1),y
        cmp     #'T'
        bne     cy_at
        lda     ptr1
        clc
        adc     #4
        bcc     :+
        inx
:       jsr     _clk_set
        jmp     cy_at
cy_err: cmp     #2              ; R_ERR
        bne     cy_at
        sta     _clk_tim_off
cy_at:  lda     _clk_t
        and     #$E0
        sta     _clk_tim_at
cy_done:
        rts
s_tim:  .byte   "TIM", 0

; void clk_show(void): when connected and the minute has changed (or clk_shown is 255, after ui_cls), the
; connected time at the right end of the Hi-Res title row, columns 32 to 39, in inverse (the lobby and the
; tables: ui_poll). Once a minute, so the pointer, hidden while it is drawn, does not flicker.
_clk_show:
        lda     _connected
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
