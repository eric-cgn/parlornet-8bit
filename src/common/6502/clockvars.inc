; ParlorNet: the connected time's variables (clock.inc), .included in the DATA segment.

_clk_t:         .byte   0,0,0   ; the connected time in seconds (clk_tick, clk_set)
_clk_sub:       .byte   60      ; ticks left in this second
_clk_tim_off:   .byte   0       ; not 0: the parlor does not know TIM
_clk_tim_at:    .byte   0       ; clk_t's 32 seconds when it was last asked
_clk_shown:     .byte   255     ; clk_t's low byte when it was last shown
_clk_buf:       .res    9       ; clk_text (the 0 at the end is never written)
ck_w:   .byte   0,0,0
