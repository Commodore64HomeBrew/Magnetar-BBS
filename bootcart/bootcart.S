; Minimal C64 8K autostart cartridge: load MAGBBS from device 8 and run.
;
; KERNAL detects the CBM80 signature at $8004 and jumps through the vector at
; $8000 before normal BASIC startup, so we finish initialization here ourselves.

        .setcpu "6502"

        .include "cbm_kernal.inc"

SAL     := $AC                  ; start address of last LOAD

.segment "STARTUP"

; --- CBM80 autostart header ---------------------------------------------------
        .word   start           ; cold start vector
        .word   start           ; warm start vector
        .byte   $c3, $c2, $cd, $38, $30   ; PETSCII "CBM80"

start:
        sei
        ldx     #$ff
        txs
        cld

        jsr     IOINIT
        jsr     RAMTAS
        jsr     RESTOR

        lda     #0              ; logical file number (ignored by LOAD)
        ldx     #8              ; device 8
        ldy     #1              ; secondary address 1 = absolute load (LOAD ",8,1")
        jsr     SETLFS

        lda     #filename_len
        ldx     #<filename
        ldy     #>filename
        jsr     SETNAM

        lda     #0
        ldx     #0
        ldy     #0
        jsr     LOAD
        bcs     load_failed

        ; SAL ($AC/$AD) holds the load address after an absolute binary load.
        lda     SAL+1
        pha
        lda     SAL
        pha
        rts

load_failed:
        inc     $d020           ; red border on load failure
        jmp     load_failed

filename_len = filename_end - filename
filename:
        .byte   "MAGBBS"
filename_end:
