; Fixed bank header layout at $B000 (size $15). Include after .segment "BANKHDR"
.macro bank_hdr_sig id
        .byte $42, $42, $4b, id
        .word $0100
.endmacro

.macro bank_ent_jmp off, sym
        .org off
        jmp sym
.endmacro

.macro bank_ent_rts off
        .org off
        rts
.endmacro

.macro bank_hdr_finish
        .org $15
.endmacro
