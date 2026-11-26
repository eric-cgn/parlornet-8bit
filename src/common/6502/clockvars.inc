; ParlorNet: the variables of the time signed in (clock.inc), .included in the DATA segment.

_clk_t:         .byte   0,0,0   ; the time signed in, in seconds (clk_tick, clk_set)
_clk_sub:       .byte   60      ; ticks left in this second (right after clk_t: clk_sync keeps the two)
_clk_tim_off:   .byte   0       ; not 0: the parlor does not know TIM
_clk_tim_at:    .byte   0       ; clk_t's 32 seconds when it was last asked
_clk_shown:     .byte   255     ; clk_min when it was last shown; 255 to show it again
_clk_buf:       .res    9       ; clk_text (the 0 at the end is never written)
ck_w:   .byte   0,0,0
ck_o:   .res    4       ; clk_t and clk_sub before TIM (clk_sync)
