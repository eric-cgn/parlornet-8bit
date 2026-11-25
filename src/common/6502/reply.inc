; ParlorNet: reading a reply from the line (ser_reply, net.c). Machine-neutral 6502 code, .included by each
; machine's sys.s where it belongs among its own code and data.

; unsigned int ser_reply(void): read a reply into sr_buf (at most sr_max bytes), as lines ended by a 0,
; until a line that starts with OK, ERR, ASK or NO C (a modem's NO CARRIER) has ended, or, with sr_any
; set, any line; or until sr_ticks pass without a byte. Empty lines are skipped, a line is kept to 80
; characters, and what does not fit is dropped. Once part of the reply has come, sr_gap ticks without a
; byte end it too (a reply comes in one piece, so a pause in it means a byte was lost on the line). sr_final is the final line's first character (O, E, A or
; N) when one came, 0 when none did, so the kind is known even if the line did not fit. Returns the
; bytes stored. A tight loop, so that a card without its interrupt switch keeps up.
        .export         _ser_reply, _sr_buf, _sr_max, _sr_ticks, _sr_gap, _sr_any, _sr_final

        .segment "DATA"
_sr_buf:        .word   0
_sr_max:        .word   0
_sr_ticks:      .word   0
_sr_gap:        .word   0
_sr_any:        .byte   0
_sr_final:      .byte   0
sr_len: .byte   0               ; the current line's length
sr_kw:  .byte   0,0,0,0         ; its first four characters
sr_cnt: .word   0
sr_t0:  .word   0
sr_lim: .word   0               ; sr_ticks, or sr_gap once a byte is kept

        .segment "CODE"
_ser_reply:
        lda     _sr_buf
        sta     ptr2
        lda     _sr_buf+1
        sta     ptr2+1
        lda     #0
        sta     sr_cnt
        sta     sr_cnt+1
        sta     sr_len
        sta     _sr_final
sr_new: lda     _ticks
        sta     sr_t0
        lda     _ticks+1
        sta     sr_t0+1
sr_loop:
        jsr     _ser_get
        cpx     #0
        beq     sr_byte
        jsr     _tick_poll
        lda     _sr_ticks       ; the limit: sr_ticks, or sr_gap once a byte is kept
        ldx     _sr_ticks+1
        ldy     sr_cnt
        bne     :+
        ldy     sr_cnt+1
        beq     :++
:       lda     _sr_gap
        ldx     _sr_gap+1
:       sta     sr_lim
        stx     sr_lim+1
        lda     _ticks          ; ticks - t0 > the limit: give up
        sec
        sbc     sr_t0
        tax
        lda     _ticks+1
        sbc     sr_t0+1
        cmp     sr_lim+1
        bcc     sr_loop
        bne     sr_done
        cpx     sr_lim
        bcc     sr_loop
        beq     sr_loop
        jmp     sr_done
sr_byte:
        cmp     #13
        beq     sr_eol
        cmp     #10
        beq     sr_eol
        and     #$7F
        cmp     #32
        bcc     sr_new
        ldx     sr_len
        cpx     #80
        bcs     sr_new
        cpx     #4
        bcs     :+
        sta     sr_kw,x
:       inc     sr_len
        jsr     sr_store
        jmp     sr_new
sr_eol: lda     sr_len
        beq     sr_new
        lda     #0
        jsr     sr_store
        ldx     sr_len
        lda     #0
        sta     sr_len
        cpx     #4
        bcs     :+
        sta     sr_kw,x         ; a short line: clear what follows it
        cpx     #3
        bcs     :+
        sta     sr_kw+2
:       jsr     sr_isfinal
        bcs     :+
        lda     _sr_any
        bne     sr_done
        jmp     sr_new
:       lda     sr_kw
        sta     _sr_final
sr_done:
        lda     sr_cnt
        ldx     sr_cnt+1
        rts

; Store A unless the buffer is full (one byte is kept for the caller's 0).
sr_store:
        pha
        lda     sr_cnt
        clc
        adc     #1
        tax
        lda     sr_cnt+1
        adc     #0
        cmp     _sr_max+1
        bcc     ss_ok
        bne     ss_full
        cpx     _sr_max
        bcs     ss_full
ss_ok:  pla
        ldy     #0
        sta     (ptr2),y
        inc     ptr2
        bne     :+
        inc     ptr2+1
:       inc     sr_cnt
        bne     :+
        inc     sr_cnt+1
:       rts
ss_full:
        pla
        rts

; C set when sr_kw starts a final line: "OK" alone or "OK ", "ERR", "ASK ", "NO C".
sr_isfinal:
        lda     sr_kw
        cmp     #'O'
        bne     :+
        lda     sr_kw+1
        cmp     #'K'
        bne     sr_no
        lda     sr_kw+2
        beq     sr_yes
        cmp     #' '
        beq     sr_yes
        bne     sr_no
:       cmp     #'E'
        bne     :+
        lda     sr_kw+1
        cmp     #'R'
        bne     sr_no
        lda     sr_kw+2
        cmp     #'R'
        beq     sr_yes
        bne     sr_no
:       cmp     #'A'
        bne     :+
        lda     sr_kw+1
        cmp     #'S'
        bne     sr_no
        lda     sr_kw+2
        cmp     #'K'
        beq     sr_yes
        bne     sr_no
:       cmp     #'N'
        bne     sr_no
        lda     sr_kw+1
        cmp     #'O'
        bne     sr_no
        lda     sr_kw+3
        cmp     #'C'
        beq     sr_yes
sr_no:  clc
        rts
sr_yes: sec
        rts
