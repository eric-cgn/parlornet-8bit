; ParlorNet: what differs between the Apple IIe and the Apple IIgs builds of the same assembly source.
;
; The IIgs program (built with IIGS defined) runs in a 64K bank of its own, in the 65816's native mode
; with 8-bit registers and the data bank set to that bank (gs.s), so the 6502 code works there as it is.
; Only the screen and the soft switches are elsewhere: in bank $E0, where the IIgs video reads its text
; and Hi-Res pages. These macros reach them with long addresses on the IIgs and are the plain
; instructions on the IIe, whose build stays as it was.

.ifdef IIGS
        .setcpu "65816"
        .a8
        .i8
.else
        .setcpu "6502"
.endif

; lda/sta of a soft switch at $C0xx.
.macro  io_lda  addr
.ifdef IIGS
        lda     f:$E00000 | (addr)
.else
        lda     addr
.endif
.endmacro

.macro  io_sta  addr
.ifdef IIGS
        sta     f:$E00000 | (addr)
.else
        sta     addr
.endif
.endmacro

; (p),y on the screen: a three-byte pointer [p],y on the IIgs, whose third byte is $E0.
.macro  scr_lda p
.ifdef IIGS
        lda     [p],y
.else
        lda     (p),y
.endif
.endmacro

.macro  scr_sta p
.ifdef IIGS
        sta     [p],y
.else
        sta     (p),y
.endif
.endmacro

.macro  scr_eor p
.ifdef IIGS
        eor     [p],y
.else
        eor     (p),y
.endif
.endmacro
