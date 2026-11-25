; ParlorNet on the Commodore 64: the machine. Start-up (BASIC and the KERNAL switched out, the VIC in bank 3),
; the raster interrupt (60 ticks a second on PAL and NTSC, the sprites, the keyboard, the joysticks and the
; 1351 mouse), the 40-column text screen, the keyboard queue, and the time connected (common/6502/clock.inc).
;
; The interrupt vectors are ours at $FFFA-$FFFF while the KERNAL is out, and at $0314 (IRQ) and $0318 (NMI)
; while it is in for the disk (disk.s); the handlers take either way in.

        .importzp       ptr1, ptr2, tmp1, tmp2, tmp3, tmp4
        .import         popa, pusha
        .import         nmi_vec, irq_tx, _ser_irq, pb_on, pb_update
        .import         _q_set, _net_req, _r_n, _r_line
        .import         _connected, _ui_hide, _ui_show, _ui_ink, _hgr_text, _hg_x, _hg_y
        .import         _ser_get

        .export         sys_start, _tick_poll, _ticks, _pal
        .export         _txt_clear, _txt_goto, _txt_putc, _txt_puts, _txt_clreol, _txt_clrrow
        .export         _txt_x, _txt_y, _txt_inv, _txt_top, _txt_bot, _txt_cursor, _txt_peek, _txt_mt
        .export         _txt_mode, _gr_mode, _txt_screen
        .export         _kbd_get, _kbd_mods, _kbd_flush
        .export         _clk_t, _clk_sub, _clk_set, _clk_text, _clk_buf, _clk_shown, _clk_show, _clk_sync
        .export         _clk_tim_off, _clk_tim_at
        .export         _spr_x, _spr_y, _spr_on, _spr_col, _spr_ptr, _spr_mc
        .export         _joy_at, _mouse_x, _mouse_y, _mouse_moved, _mouse_btn, _has_mouse, _mouse_on
        .export         irq_entry, nmi_entry, _irq_ticks, _kq_key, _kq_mod, _kq_head, _kq_tail

SCREEN  = $C800                 ; the text screen
BMCOL   = $CC00                 ; the bitmap's cell colors
CHARS   = $C000
VIC     = $D000
CIA1    = $DC00
CIA2    = $DD00
COLRAM  = $D800
TXT_FG  = 14                    ; light blue on blue, as the C64 starts
TXT_BG  = 6
BORDER  = 14

; ---------------------------------------------------------------------------------------------------
; Data

        .segment "ZEROPAGE"
iz1:    .res    1
iz2:    .res    1
iz3:    .res    1
iz4:    .res    1
iz5:    .res    1
iz6:    .res    1
izp:    .res    2

        .segment "DATA"
_txt_x:         .byte   0
_txt_y:         .byte   0
_txt_inv:       .byte   0
_txt_top:       .byte   0
_txt_bot:       .byte   24
_txt_mt:        .byte   1       ; the Commodore logo is one character
_txt_screen:    .byte   1       ; 1 while the text screen shows (0: the bitmap)
_ticks:         .word   0
_irq_ticks:     .word   0       ; counted by the interrupt; tick_poll brings ticks up to it
_pal:           .byte   0
palcnt: .byte   0
        .include "clockvars.inc"

; The sprites, as the interrupt sets them each frame: x (0-511), y, shape (the block, $D000 / 64 up),
; color, multicolor, and which are on.
_spr_x:         .word   0,0,0,0,0,0,0,0
_spr_y:         .byte   0,0,0,0,0,0,0,0
_spr_ptr:       .byte   64,65,66,67,68,69,70,71
_spr_col:       .byte   0,1,0,0,0,0,0,0
_spr_mc:        .byte   0
_spr_on:        .byte   0

; The keyboard: a queue of keys with the modifiers held, and the key held now (for repeating).
_kq_key: .res    8
_kq_mod: .res    8
_kq_head:.byte   0
_kq_tail:.byte   0
lastmod:.byte   0
held:   .byte   $FF             ; the matrix index held, $FF none
hcode:  .byte   0
hmod:   .byte   0
rpt:    .byte   0               ; frames to the next repeat
_joy_at:.byte   0               ; the joystick's bits as last read (1 = on): up, down, left, right, fire

; The 1351 mouse in port 1: the pointer's position, whether it moved, its buttons.
_mouse_on:      .byte   0       ; 1: a 1351 in port 1 (the settings)
_has_mouse:     .byte   0
_mouse_x:       .word   160
_mouse_y:       .byte   100
_mouse_moved:   .byte   0
_mouse_btn:     .byte   0
potx:   .byte   0
poty:   .byte   0
mwait:  .byte   0

        .segment "RODATA"
rowlo:  .repeat 25, r
        .byte   <(SCREEN + r * 40)
        .endrepeat
rowhi:  .repeat 25, r
        .byte   >(SCREEN + r * 40)
        .endrepeat

; The keyboard matrix (column on port A, row on port B; index column * 8 + row) to ASCII, without and with
; shift. 0 is no key; $FE, $FD and $FC are shift, C= and CTRL.
ESC = 27
keys:   .byte   127, 13, 21,  0,  0,  0,  0, 10     ; DEL RETURN CRSR-RIGHT F7 F1 F3 F5 CRSR-DOWN
        .byte   '3','w','a','4','z','s','e',$FE
        .byte   '5','r','d','6','c','f','t','x'
        .byte   '7','y','g','8','b','h','u','v'
        .byte   '9','i','j','0','m','k','o','n'
        .byte   '+','p','l','-','.',':','@',','
        .byte   '\',  '*',';',  0,$FE,'=','^','/'    ; pound HOME RSHIFT up-arrow
        .byte   '1',ESC,$FC,'2',' ',$FD,'q',ESC     ; left-arrow CTRL C= RUN/STOP
skeys:  .byte     0, 13,  8,  0,  0,  0,  0, 11
        .byte   '#','W','A','$','Z','S','E',$FE
        .byte   '%','R','D','&','C','F','T','X'
        .byte   $27,'Y','G','(','B','H','U','V'
        .byte   ')','I','J','0','M','K','O','N'
        .byte   '{','P','L','}','>','[','`','<'
        .byte   '|','*',']',  0,$FE,'=','~','?'
        .byte   '!','_',$FC,'"',' ',$FD,'Q',ESC

        .segment "CODE"

; ---------------------------------------------------------------------------------------------------
; Start-up, from crt0.s with interrupts off and the KERNAL in.

sys_start:
        ; PAL or NTSC: the last raster line's low byte is $37 on PAL (312 lines), $05 or $06 on NTSC
:       lda     $D012
:       cmp     $D012
        beq     :-
        bmi     :--
        cmp     #$20
        lda     #0
        rol     a
        sta     _pal
        ; the vectors: the KERNAL's IRQ and NMI ways in (for disk.s) and the 6502's own
        lda     #<irq_kernal
        sta     $0314
        lda     #>irq_kernal
        sta     $0315
        lda     #<nmi_entry
        sta     $0318
        lda     #>nmi_entry
        sta     $0319
        lda     #<nmi_entry
        sta     $FFFA
        lda     #>nmi_entry
        sta     $FFFB
        lda     #<reset
        sta     $FFFC
        lda     #>reset
        sta     $FFFD
        lda     #<irq_entry
        sta     $FFFE
        lda     #>irq_entry
        sta     $FFFF
        ; no CIA interrupts until the serial line is opened
        lda     #$7F
        sta     CIA1+$0D
        sta     CIA2+$0D
        lda     CIA1+$0D
        lda     CIA2+$0D
        lda     #$35            ; RAM at $A000 and $E000, I/O at $D000
        sta     $01
        ; the character set's inverse half
        ldx     #0
:       lda     CHARS,x
        eor     #$FF
        sta     CHARS+$400,x
        lda     CHARS+$100,x
        eor     #$FF
        sta     CHARS+$500,x
        lda     CHARS+$200,x
        eor     #$FF
        sta     CHARS+$600,x
        lda     CHARS+$300,x
        eor     #$FF
        sta     CHARS+$700,x
        inx
        bne     :-
        ; the VIC: bank 3 ($C000-$FFFF), no sprites yet, the raster interrupt at line 251
        lda     CIA2+$02
        ora     #$03
        sta     CIA2+$02
        lda     CIA2
        and     #$FC
        sta     CIA2
        lda     #0
        sta     VIC+$15
        sta     VIC+$17
        sta     VIC+$1D
        sta     VIC+$1B         ; sprites in front of the screen
        lda     #251
        sta     VIC+$12
        lda     #$1B
        sta     VIC+$11
        lda     #$01
        sta     VIC+$1A
        sta     VIC+$19
        lda     #BORDER
        sta     VIC+$20
        jsr     _txt_mode
        lda     #0
        sta     _txt_top
        lda     #25
        sta     _txt_bot
        jsr     _txt_clear
        lda     #24
        sta     _txt_bot
        rts

reset:  jmp     ($FFFC)         ; (never: RESET always comes from the KERNAL ROM)

; ---------------------------------------------------------------------------------------------------
; Interrupts. The KERNAL's way in has pushed A, X and Y already; ours pushes them the same way.

irq_entry:
        pha
        txa
        pha
        tya
        pha
irq_kernal:
        cld
        lda     _ser_irq        ; CIA1's timer A, sending bit by bit (serial.s)
        beq     :+
        lda     CIA1+$0D
        lsr     a
        bcc     :+
        jsr     irq_tx
:       lda     pb_on           ; a load's progress bar (disk.s)
        beq     :+
        jsr     pb_update
:       lda     VIC+$19
        bpl     irq_out
        sta     VIC+$19
        cli                     ; (the sending goes on, interrupting the rest)
        ; ticks: one a frame, and on PAL one more each fifth frame, for 60 a second
        inc     _irq_ticks
        bne     :+
        inc     _irq_ticks+1
:       lda     _pal
        beq     :++
        dec     palcnt
        bpl     :++
        lda     #4
        sta     palcnt
        inc     _irq_ticks
        bne     :+
        inc     _irq_ticks+1
:
:       jsr     sprites
        jsr     scan
irq_out:
        pla
        tay
        pla
        tax
        pla
        rti

nmi_entry:
        jmp     (nmi_vec)       ; (serial.s: the line's handler, or an RTI)

; The sprites from _spr_*.
sprites:
        ldx     #7
        ldy     #14
        lda     #0
        sta     iz1
:       lda     _spr_x,y
        sta     VIC,y
        lda     _spr_x+1,y
        lsr     a
        rol     iz1
        lda     _spr_y,x
        sta     VIC+1,y
        lda     _spr_col,x
        sta     VIC+$27,x
        lda     _spr_ptr,x
        sta     SCREEN+$3F8,x
        sta     BMCOL+$3F8,x
        dey
        dey
        dex
        bpl     :-
        lda     iz1
        sta     VIC+$10
        lda     _spr_mc
        sta     VIC+$1C
        lda     _spr_on
        sta     VIC+$15
        rts

; ---------------------------------------------------------------------------------------------------
; The keyboard, the joysticks and the mouse, each frame. (The interrupt has zero page of its own, iz*.)

scan:
        ; the joysticks: with no keyboard column chosen, a joystick in port 2 shows on port A and one in
        ; port 1 (or the mouse's buttons) on port B. While one is in use the keyboard is not read, since
        ; the joystick's lines would look like keys.
        lda     #$FF
        sta     CIA1
        lda     CIA1+1
        and     #$1F
        eor     #$1F
        sta     iz2             ; port 1
        lda     CIA1
        and     #$1F
        eor     #$1F
        sta     iz1             ; port 2
        jsr     mouse
        lda     _has_mouse
        beq     :+
        lda     iz2             ; with the mouse in port 1, its buttons are not a joystick
        and     #$10
        sta     _mouse_btn
        lda     #0
        sta     iz2
:       lda     iz1
        ora     iz2
        sta     _joy_at
        beq     keyboard
        jmp     joystick

keyboard:
        lda     #0
        sta     iz3             ; modifiers: 1 shift, 2 C=, 4 CTRL
        lda     #$FF
        sta     iz4             ; the first key (matrix index)
        ldx     #0              ; column
kb_col: lda     colsel,x
        sta     CIA1
        lda     CIA1+1
        eor     #$FF
        beq     kb_next
        sta     izp             ; the rows down in this column
        txa
        asl     a
        asl     a
        asl     a
        sta     izp+1           ; the column's first index
        ldy     #0              ; row
kb_row: lsr     izp
        bcc     kb_rnext
        sty     iz2
        tya
        ora     izp+1
        tay
        lda     keys,y
        cmp     #$FE
        bne     :+
        lda     #1
        bne     kb_mod
:       cmp     #$FD
        bne     :+
        lda     #2
        bne     kb_mod
:       cmp     #$FC
        bne     :+
        lda     #4
kb_mod: ora     iz3
        sta     iz3
        jmp     kb_rest
:       lda     iz4
        bpl     kb_rest         ; (one key at a time: the first)
        sty     iz4
kb_rest:
        ldy     iz2
kb_rnext:
        iny
        cpy     #8
        bcc     kb_row
kb_next:
        inx
        cpx     #8
        bcc     kb_col
        lda     #$7F            ; (port A left so: the SID reads port 1's potentiometers, the mouse)
        sta     CIA1
        ldy     iz4
        bmi     kb_none
        ; the key's code: shifted (not with C=), or a control code with CTRL
        lda     iz3
        cmp     #1
        bne     :+
        lda     skeys,y
        jmp     :++
:       and     #3
        cmp     #3
        bne     :+
        lda     skeys,y         ; shift and C=: as shifted
        jmp     :++
:       lda     keys,y
:       beq     kb_none
        tax
        lda     iz3
        and     #4
        beq     :+
        txa
        cmp     #'a'
        bcc     kb_none
        and     #$1F            ; CTRL and a letter
        tax
:       txa
        ldx     iz3
        jmp     press

kb_none:
        lda     #$FF
        sta     held
        rts

; A key (A) with its modifiers (X), and its matrix index in Y ($40 up for the joystick): queued when it
; is new, and again while held if it repeats (the cursor keys, DEL, space and the joystick's directions).
press:  cpy     held
        bne     pr_new
        dec     rpt
        bne     pr_done
        lda     hcode
        cmp     #127
        beq     :+
        cmp     #' '
        beq     :+
        cmp     #13             ; (Return and the joystick's fire do not repeat)
        beq     pr_done
        cmp     #27
        bcs     pr_done
:       lda     #4
        sta     rpt
        lda     hcode
        ldx     hmod
        jmp     enqueue
pr_new: sty     held
        sta     hcode
        stx     hmod
        ldy     #24
        sty     rpt
enqueue:
        ldy     _kq_head
        sta     _kq_key,y
        txa
        sta     _kq_mod,y
        iny
        tya
        and     #7
        cmp     _kq_tail
        beq     pr_done         ; (full)
        sta     _kq_head
pr_done:
        rts

; The joystick (either port): its direction as an arrow key, fire as Return.
joystick:
        lda     #$7F
        sta     CIA1
        lda     _joy_at
        ldy     #$40
        ldx     #11             ; up
        lsr     a
        bcs     :+
        iny
        ldx     #10             ; down
        lsr     a
        bcs     :+
        iny
        ldx     #8              ; left
        lsr     a
        bcs     :+
        iny
        ldx     #21             ; right
        lsr     a
        bcs     :+
        iny
        ldx     #13             ; fire
        lsr     a
        bcs     :+
        jmp     kb_none
:       txa
        ldx     #0
        jmp     press

; The 1351 mouse in port 1: the SID's POTX and POTY count its moves (bits 1 to 6, wrapping); each new value
; less the last, as -32 to 31, moves the pointer. Without a mouse they read $FF. The first move shows that a
; mouse is there.
mouse:  lda     _mouse_on       ; (on in the settings: an empty port 1 reads as noise)
        bne     :+
        rts
:       lda     $D419
        cmp     #$FF
        bne     :+
        rts
:
        ldx     mwait           ; the first reading only sets where it starts
        bne     :+
        inc     mwait
        and     #$7E
        sta     potx
        lda     $D41A
        and     #$7E
        sta     poty
        rts
:       ldy     potx
        jsr     mdelta
        sty     potx
        beq     ms_y
        ; x += delta (signed), within 0 to 319
        ldx     #1
        stx     _has_mouse
        stx     _mouse_moved
        tax
        clc
        adc     _mouse_x
        sta     _mouse_x
        txa
        bmi     :+
        lda     _mouse_x+1
        adc     #0
        jmp     mx_hi
:       lda     _mouse_x+1
        adc     #$FF
mx_hi:  sta     _mouse_x+1
        bpl     :+
        lda     #0
        sta     _mouse_x
        sta     _mouse_x+1
:       lda     _mouse_x+1
        beq     ms_y
        lda     _mouse_x
        cmp     #<319
        bcc     ms_y
        lda     #<319
        sta     _mouse_x
ms_y:   lda     $D41A
        ldy     poty
        jsr     mdelta
        sty     poty
        beq     ms_done
        ; y -= delta (the mouse counts up as it moves away), within 0 to 199
        ldx     #1
        stx     _has_mouse
        stx     _mouse_moved
        sta     iz5
        bmi     ms_down
        lda     _mouse_y        ; up: y - delta, not below 0
        sec
        sbc     iz5
        bcs     ms_sety
        lda     #0
        beq     ms_sety
ms_down:
        eor     #$FF            ; down: y + -delta, not past 199
        sec
        adc     _mouse_y
        bcs     :+
        cmp     #200
        bcc     ms_sety
:       lda     #199
ms_sety:
        sta     _mouse_y
ms_done:
        rts

; A: the pot's new value, Y: the last. Returns the delta (-32 to 31) in A (Z set when 0) and the new value
; in Y.
mdelta: and     #$7E
        sta     iz6
        sty     iz5
        sec
        sbc     iz5
        and     #$7E
        lsr     a
        cmp     #$20
        bcc     :+
        ora     #$C0            ; negative
:       ldy     iz6
        cmp     #0
        rts

        .segment "RODATA"
colsel: .byte   $FE,$FD,$FB,$F7,$EF,$DF,$BF,$7F
        .segment "CODE"

; unsigned char kbd_get(void): a key, or 0.
_kbd_get:
        jsr     _tick_poll
        ldy     _kq_tail
        cpy     _kq_head
        beq     :+
        lda     _kq_mod,y
        sta     lastmod
        lda     _kq_key,y
        pha
        iny
        tya
        and     #7
        sta     _kq_tail
        pla
        ldx     #0
        rts
:       lda     #0
        tax
        rts

; unsigned char kbd_mods(void): bit 0 when C= was held with the last key.
_kbd_mods:
        lda     lastmod
        lsr     a
        and     #1
        ldx     #0
        rts

; void kbd_flush(void): forget the keys waiting.
_kbd_flush:
        lda     _kq_head
        sta     _kq_tail
        rts

; void tick_poll(void): ticks up to the interrupt's count, the connected time with them. Call it often.
_tick_poll:
        lda     _ticks
        cmp     _irq_ticks
        bne     :+
        lda     _ticks+1
        cmp     _irq_ticks+1
        beq     tp_done
:       inc     _ticks
        bne     :+
        inc     _ticks+1
:       jsr     clk_tick
        jmp     _tick_poll
tp_done:
        rts

; ---------------------------------------------------------------------------------------------------
; The text screen: screen codes are the character set's, ASCII; inverse is $80 up.

; void txt_mode(void), void gr_mode(void): the text screen ($C800, characters at $C000) or the bitmap
; ($E000, colors at $CC00).
_txt_mode:
        lda     #$20
        sta     VIC+$18
        lda     #$1B
        sta     VIC+$11
        lda     #TXT_BG
        sta     VIC+$21
        lda     #1
        sta     _txt_screen
        rts

_gr_mode:
        lda     #$38
        sta     VIC+$18
        lda     #$3B
        sta     VIC+$11
        lda     #0
        sta     _txt_screen
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
clrx:   jsr     rowx
        ldy     #39
:       lda     #' '
        sta     (ptr1),y
        lda     #TXT_FG
        sta     (ptr2),y
        dey
        bpl     :-
        rts

; ptr1: row X of the screen; ptr2: its color RAM.
rowx:   lda     rowlo,x
        sta     ptr1
        sta     ptr2
        lda     rowhi,x
        sta     ptr1+1
        clc
        adc     #>(COLRAM - SCREEN)
        sta     ptr2+1
        rts

; void txt_clreol(void)
_txt_clreol:
        ldx     _txt_y
        jsr     rowx
        ldy     _txt_x
        lda     #' '
:       cpy     #40
        bcs     :+
        sta     (ptr1),y
        iny
        bne     :-
:       rts

; void __fastcall__ txt_goto(unsigned char x, unsigned char y)
_txt_goto:
        sta     _txt_y
        jsr     popa
        sta     _txt_x
        rts

; unsigned char txt_peek(void), void txt_cursor(unsigned char code): the code at the cursor, and putting
; one there, as on the Apple II, whose codes have inverse below $80: so a cursor of $20 is an inverse space.
_txt_peek:
        ldx     _txt_y
        jsr     rowx
        ldy     _txt_x
        lda     (ptr1),y
        eor     #$80
        ldx     #0
        rts

_txt_cursor:
        eor     #$80
        pha
        ldx     _txt_y
        jsr     rowx
        ldy     _txt_x
        pla
        sta     (ptr1),y
        rts

; void __fastcall__ txt_putc(char c): CR or LF start a new line, 8 goes back one, the rest print; $7F is the
; Commodore logo.
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
        cmp     #128
        bcc     :+
        lda     #'?'
:       ldx     _txt_inv
        beq     :+
        ora     #$80
:       pha
        ldx     _txt_y
        jsr     rowx
        ldy     _txt_x
        pla
        sta     (ptr1),y
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

; Scroll the window up a row; the cursor stays on the (now clear) bottom row.
scroll: ldx     _txt_top
sc1:    inx
        cpx     _txt_bot
        bcs     sc2
        lda     rowlo,x
        sta     ptr2
        lda     rowhi,x
        sta     ptr2+1
        lda     rowlo-1,x
        sta     ptr1
        lda     rowhi-1,x
        sta     ptr1+1
        ldy     #39
:       lda     (ptr2),y
        sta     (ptr1),y
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

; ---------------------------------------------------------------------------------------------------
; Reading a reply (net.c) and the connected time: the same code as every machine's.

        .include "reply.inc"
        .include "clock.inc"
