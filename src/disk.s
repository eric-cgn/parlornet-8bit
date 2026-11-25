; ParlorNet for the Apple IIe: C calls into the boot block's sector reader.

        .export         _disk_read, _disk_motor_off

        .segment "CODE"

; unsigned char disk_read(void): read RD_COUNT sectors from linear sector RD_LIN into pages from
; RD_PAGE up. 0 on success. Interrupts are held off while the disk turns, so the reader keeps time.
_disk_read:
        php
        sei
        jsr     $0804
        lda     #0
        rol     a
        plp
        ldx     #0
        rts

_disk_motor_off:
        jmp     $0807
