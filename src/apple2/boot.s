; ParlorNet for the Apple IIe: the boot block and the sector reader.
;
; The Disk II boot ROM reads physical sectors 0 to N-1 of track 0 into $0800 upwards, where N is the
; first byte of sector 0, and jumps to $0801. This block checks for the auxiliary memory that Double
; Hi-Res needs, then reads the program from the disk with its own 6-and-2 sector reader, switches the
; language card's RAM in, and starts the program. The reader stays in memory: the program loads the games
; through it (rd_range, below).
;
; The disk holds no operating system. Sectors are counted linearly from track 0 physical sector 0
; (linear sector = track * 16 + physical sector); tools/apple2_disk.py lays the program out and fills
; in the load table at LOADTAB.

        .setcpu "6502"

BOOTSECT = 6                    ; this block is 6 sectors, $0800-$0DFF
HOME    = $FC58                 ; the monitor's: clear the text screen
COUT    = $FDED                 ; and print a character

; Zero page used by the reader ($F0-$FF; the C program keeps out of it).
slot16  = $F0                   ; boot slot * 16
curht   = $F1                   ; the head's half-track
lin     = $F2                   ; 2 bytes: next linear sector to read
count   = $F4                   ; sectors left to read
dpage   = $F5                   ; page for the next sector
tries   = $F6                   ; 2 bytes: patience counters
left    = $F8                   ; sectors still wanted on this track
tmp     = $F9
trk     = $FA                   ; track wanted
asec    = $FB                   ; sector of the address field just read
atrk    = $FC                   ; track of the address field just read
dst     = $FD                   ; 2 bytes: decode target
retry   = $FF                   ; recalibrations left

RAW     = $0C00                 ; the 256 six-bit nibbles of a sector (after the code)
AUX     = $0D00                 ; and its 86 two-bit nibbles
XLATE   = $0D56                 ; disk byte -> 6-bit value, indexed from $96 (106 bytes)

        .segment "BOOT"

        .byte   BOOTSECT
entry:  jmp     boot            ; $0801
        jmp     rd_range        ; $0804: read [lin] count sectors to page dpage; C set on failure
        jmp     motor_off       ; $0807
diskid: .byte   1               ; $080A: which disk this is (1 or 2), set by the disk maker
        .byte   "PN"            ; $080B
; $080D: the load table, set by the disk maker: up to 4 entries of linear sector (2 bytes), count,
; first page; an entry with count 0 ends it. Then the start address. An entry for page $D0 goes into the
; language card's bank 1 (the Double Hi-Res kit), the others where they say, with bank 2 in.
LOADTAB:
        .res    16, 0           ; $080D-$081C
start:  .word   0               ; $081D
        .res    2, 0            ; $081F-$0820 (spare; under ProDOS PARLOR.SYSTEM keeps its values there)
; $0821: the overlays (the games, and the sign-in), set by the disk maker: 20 entries of linear sector
; (2 bytes), count and disk number. Each loads at $D000.
OVLTAB: .res    80, 0           ; $0821-$0870

; Messages for the boot screen (text page 1, inverse/normal screen codes made by the code).
title:  .byte   "PARLORNET", 0
msg2:   .byte   "THIS IS DISK 2. BOOT DISK 1.", 0
msgerr: .byte   "DISK ERROR", 0
; (40 characters to the space, so that the screen's own wrap ends the first line there)
msg128: .byte   "ParlorNet needs an Apple IIe with 128K, or a IIc or IIgs.", 0

boot:
        stx     slot16
        ; Clear the text screen and say what this is.
        lda     $C051           ; text
        sta     $C00C           ; 40 columns
        sta     $C00F           ; the alternate characters (inverse lower case)
        sta     $C001           ; 80STORE
        jsr     HOME
        ; Double Hi-Res needs the auxiliary 64K. With 80STORE and HIRES on, PAGE2 puts its $2000 in place
        ; of the main memory's, so a value written there must still be there after the main one is, and
        ; after its $2400 is, which the 1K of an 80-column card without the 64K repeats at $2000.
        lda     $C057           ; HIRES (the text screen still shows)
        sta     $C055           ; the auxiliary memory
        lda     #$A5
        sta     $2000
        lsr     a
        sta     $2400
        sta     $C054           ; the main memory
        sta     $2000
        sta     $C055
        lda     $2000
        sta     $C054           ; (page 1: PAGE2 is off once 80STORE is)
        sta     $C000           ; 80STORE off (HIRES stays on, which text mode does not mind)
        cmp     #$A5
        beq     :+
        ldx     #0
no128:  lda     msg128,x
        beq     hang
        ora     #$80
        jsr     COUT
        inx
        bne     no128
:       ldx     #0
:       lda     title,x
        beq     :+
        ora     #$80
        sta     $0400+15,x
        inx
        bne     :-
:
        ; Patch the slot into the data latch reads.
        ldx     #npatch-1
:       lda     patchlo,x
        sta     dst
        lda     patchhi,x
        sta     dst+1
        ldy     #0
        lda     #$8C
        clc
        adc     slot16
        sta     (dst),y
        dex
        bpl     :-
        ; The disk byte table: XLATE[b - $96] = the 6-bit value of disk byte b.
        ldx     #63
:       lda     wtab,x
        tay
        txa
        sta     XLATE-$96,y
        dex
        bpl     :-
        ; All stepper phases off; the boot ROM left the head on track 0.
        ldx     slot16
        lda     $C080,x
        lda     $C082,x
        lda     $C084,x
        lda     $C086,x
        lda     #0
        sta     curht
        ; The language card's RAM, bank 2, for reading and writing.
        lda     $C083
        lda     $C083
        ; Disk 2 boots only to say so.
        lda     LOADTAB+2
        bne     ld
        ldx     #0
:       lda     msg2,x
        beq     hang
        ora     #$80
        sta     $0500+6,x
        inx
        bne     :-
hang:   jsr     motor_off
:       jmp     :-
ld:     ldx     #0
ld1:    lda     LOADTAB+2,x
        beq     go
        sta     count
        lda     LOADTAB,x
        sta     lin
        lda     LOADTAB+1,x
        sta     lin+1
        lda     LOADTAB+3,x
        sta     dpage
        cmp     #$D0
        bne     :+
        bit     $C08B           ; bank 1, to read and write
        bit     $C08B
:       txa
        pha
        jsr     rd_range
        bit     $C083           ; bank 2 again
        bit     $C083
        pla
        bcs     fail
        clc
        adc     #4
        tax
        cpx     #16
        bcc     ld1
go:     jsr     motor_off
        jmp     (start)
fail:   ldx     #0
:       lda     msgerr,x
        beq     hang
        ora     #$80
        sta     $0500+15,x
        inx
        bne     :-
        beq     hang

motor_off:
        ldx     slot16
        lda     $C088,x
        rts

; rd_range: read count sectors from linear sector lin into pages dpage upwards. C set on failure.
rd_range:
        ldx     slot16
        lda     $C089,x         ; motor on
        lda     $C08A,x         ; drive 1
        lda     $C08E,x         ; read mode
        lda     #3
        sta     retry
rr_track:
        lda     count
        bne     :+
        clc
        rts
:       ; trk = lin / 16; want[] = pages of the sectors wanted on it
        lda     lin+1
        sta     tmp
        lda     lin
        lsr     tmp
        ror     a
        lsr     tmp
        ror     a
        lsr     tmp
        ror     a
        lsr     tmp
        ror     a
        sta     trk
        ldx     #15
        lda     #0
:       sta     want,x
        dex
        bpl     :-
        sta     left
        lda     lin
        and     #15
        tax
:       lda     dpage
        sta     want,x
        inc     dpage
        inc     left
        inc     lin
        bne     :+
        inc     lin+1
:       dec     count
        beq     :+
        inx
        cpx     #16
        bcc     :--
:       lda     trk
        jsr     seek
rr_sec: lda     #0              ; up to 256 bad fields before recalibrating
        sta     tries
rr_addr:
        jsr     rdaddr
        bcs     rr_bad
        lda     atrk
        cmp     trk
        bne     rr_recal
        ldx     asec
        lda     want,x
        beq     rr_addr
        sta     dst+1
        jsr     rddata
        bcs     rr_bad
        jsr     decode
        ldx     asec
        lda     #0
        sta     want,x
        dec     left
        bne     rr_addr
        beq     rr_track
rr_bad: dec     tries
        bne     rr_addr
rr_recal:
        dec     retry
        bne     :+
        sec
        rts
:       lda     #80             ; step out past track 0 and start again
        sta     curht
        lda     #0
        jsr     seek
        lda     trk
        jsr     seek
        jmp     rr_sec

; seek: move the head to track A, a half-track at a time.
seek:   asl     a
        sta     tmp
sk1:    lda     curht
        cmp     tmp
        beq     sk_done
        and     #3
        asl     a
        sta     dst             ; the phase now on (its off switch)
        lda     curht
        cmp     tmp
        bcs     :+
        inc     curht
        bne     :++
:       dec     curht
:       lda     curht
        and     #3
        asl     a
        ora     #1
        ora     slot16
        tay
        lda     $C080,y         ; the next phase on
        lda     #40
        jsr     delay
        lda     dst
        ora     slot16
        tay
        lda     $C080,y         ; the last one off
        lda     #10
        jsr     delay
        jmp     sk1
sk_done:
        lda     #200            ; let the head settle
        jsr     delay
        lda     curht
        and     #3
        asl     a
        ora     slot16
        tay
        lda     $C080,y
        ldx     slot16
        rts

; delay: about A * 100 microseconds.
delay:  ldy     #19
:       dey
        bne     :-
        sec
        sbc     #1
        bne     delay
        rts

; rdaddr: find the next address field; C clear and atrk, asec set, or C set if none came.
rdaddr: lda     #$E0            ; give up after 32 * 256 disk bytes (more than a turn)
        sta     dst
        ldy     #0
ra0:    iny
        bne     ra1
        inc     dst
        beq     ra_fail
ra1:
p0:     lda     $C0EC
        bpl     ra1
ra2:    cmp     #$D5
        bne     ra0
p1:     lda     $C0EC
        bpl     p1
        cmp     #$AA
        bne     ra2
p2:     lda     $C0EC
        bpl     p2
        cmp     #$96
        bne     ra2
        ldy     #0
ra3:
p3:     lda     $C0EC
        bpl     p3
        sec
        rol     a
        sta     dst
p4:     lda     $C0EC
        bpl     p4
        and     dst
        sta     afield,y
        iny
        cpy     #4
        bne     ra3
        lda     afield+1
        sta     atrk
        lda     afield+2
        cmp     #16
        bcs     ra_fail
        sta     asec
        eor     afield
        eor     afield+1
        cmp     afield+3
        bne     ra_fail
        clc
        rts
ra_fail:
        sec
        rts

; rddata: read the data field that follows; C set on a bad field.
rddata: ldy     #32
rd0:    dey
        beq     rd_fail
p5:     lda     $C0EC
        bpl     p5
rd1:    cmp     #$D5
        bne     rd0
p6:     lda     $C0EC
        bpl     p6
        cmp     #$AA
        bne     rd1
p7:     lda     $C0EC
        bpl     p7
        cmp     #$AD
        bne     rd1
        ldx     #0
        lda     #0
rd2:
p8:     ldy     $C0EC
        bpl     rd2
        eor     XLATE-$96,y
        sta     AUX,x
        inx
        cpx     #86
        bne     rd2
        ldx     #0
rd3:
p9:     ldy     $C0EC
        bpl     rd3
        eor     XLATE-$96,y
        sta     RAW,x
        inx
        bne     rd3
p10:    ldy     $C0EC
        bpl     p10
        eor     XLATE-$96,y
        bne     rd_fail
        clc
        rts
rd_fail:
        sec
        rts

; decode: the 342 nibbles to the 256 bytes of page dst+1. AUX[j] holds the low two bits (reversed) of
; bytes j, j+86 and j+172.
decode: lda     #0
        sta     dst
        tay
dc0:    ldx     #0
dc1:    lda     RAW,y
        lsr     AUX,x
        rol     a
        lsr     AUX,x
        rol     a
        sta     (dst),y
        iny
        beq     dc2
        inx
        cpx     #86
        bne     dc1
        beq     dc0
dc2:    rts

; The 64 disk bytes of 6-and-2 encoding, by value.
wtab:   .byte   $96,$97,$9A,$9B,$9D,$9E,$9F,$A6,$A7,$AB,$AC,$AD,$AE,$AF,$B2,$B3
        .byte   $B4,$B5,$B6,$B7,$B9,$BA,$BB,$BC,$BD,$BE,$BF,$CB,$CD,$CE,$CF,$D3
        .byte   $D6,$D7,$D9,$DA,$DB,$DC,$DD,$DE,$DF,$E5,$E6,$E7,$E9,$EA,$EB,$EC
        .byte   $ED,$EE,$EF,$F2,$F3,$F4,$F5,$F6,$F7,$F9,$FA,$FB,$FC,$FD,$FE,$FF

patchlo: .byte  <(p0+1),<(p1+1),<(p2+1),<(p3+1),<(p4+1),<(p5+1),<(p6+1),<(p7+1),<(p8+1),<(p9+1),<(p10+1)
patchhi: .byte  >(p0+1),>(p1+1),>(p2+1),>(p3+1),>(p4+1),>(p5+1),>(p6+1),>(p7+1),>(p8+1),>(p9+1),>(p10+1)
npatch = 11

afield: .res    4
want:   .res    16

        .assert * <= RAW, error, "boot code overlaps its buffers"
