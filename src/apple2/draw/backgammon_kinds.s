; ParlorNet on the Apple IIe: Backgammon's kinds of place for the Double Hi-Res kit's d_column (dhr.s), in
; color and in monochrome, which draw/backgammon.c gives it with d_types. They are with the kit in the
; auxiliary memory (AUXART). A kind: its layers (+ $40 when a color changes with the line's parity), then 4
; layers of a mask (0 for every dot), its stride (4, or -4 to go up it) and its colors (even + 16 * odd).
; The kinds: a checker of seat 1 (white) or 0 (black) on a red or green lane (0 to 3), the tips pointing
; down (4, 5) and up (6, 7), and the empty lanes (8, 9); red lanes first.

        .import         _dhr_c_disc, _dhr_c_tipd, _dhr_m_disc, _dhr_m_rim, _dhr_m_tipdf, _dhr_m_tipdl
        .import         _dhr_m_side
        .export         _dhr_bg_kinds, _dhr_bg_mkinds

RED     = $11                   ; (a color on both lines: 17 * the color)
GREEN   = $CC
WOOD    = $99
WHITE   = $FF
BLACK   = $00
CHECK   = $A5                   ; monochrome: a checkerboard of dots
FULL    = 0
UP      = $FC

.macro  layer m, s, c
.ifblank m
        .word   0
        .byte   0, 0
.else
        .word   m
        .byte   s, c
.endif
.endmacro
.macro  place n, m1, s1, c1, m2, s2, c2, m3, s3, c3
        .byte   n
        layer   m1, s1, c1
        layer   m2, s2, c2
        layer   m3, s3, c3
        layer
.endmacro

        .segment "RODATA"
_dhr_bg_kinds:
        place   2, FULL, 0, RED, _dhr_c_disc, 4, WHITE
        place   2, FULL, 0, RED, _dhr_c_disc, 4, BLACK
        place   2, FULL, 0, GREEN, _dhr_c_disc, 4, WHITE
        place   2, FULL, 0, GREEN, _dhr_c_disc, 4, BLACK
        place   2, FULL, 0, WOOD, _dhr_c_tipd, 4, RED
        place   2, FULL, 0, WOOD, _dhr_c_tipd, 4, GREEN
        place   2, FULL, 0, WOOD, _dhr_c_tipd + 44, UP, RED
        place   2, FULL, 0, WOOD, _dhr_c_tipd + 44, UP, GREEN
        place   1, FULL, 0, RED
        place   1, FULL, 0, GREEN
_dhr_bg_mkinds:
        place   3, FULL, 0, BLACK, _dhr_m_rim, 4, BLACK, _dhr_m_disc, 4, WHITE
        place   3, FULL, 0, BLACK, _dhr_m_rim, 4, WHITE, _dhr_m_disc, 4, BLACK
        place   $43, FULL, 0, CHECK, _dhr_m_rim, 4, BLACK, _dhr_m_disc, 4, WHITE
        place   $43, FULL, 0, CHECK, _dhr_m_rim, 4, WHITE, _dhr_m_disc, 4, BLACK
        place   2, FULL, 0, BLACK, _dhr_m_tipdl, 4, WHITE
        place   $42, FULL, 0, BLACK, _dhr_m_tipdf, 4, CHECK
        place   2, FULL, 0, BLACK, _dhr_m_tipdl + 44, UP, WHITE
        place   $42, FULL, 0, BLACK, _dhr_m_tipdf + 44, UP, CHECK
        place   1, _dhr_m_side, 0, WHITE
        place   $41, FULL, 0, CHECK
