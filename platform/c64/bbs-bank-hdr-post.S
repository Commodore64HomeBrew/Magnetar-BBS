.include "bbs-bank-entry.inc"

.import _bbs_post_init, _bbs_post_deinit

.segment "BANKHDR"

bank_hdr:
        bank_hdr_sig $32

        .org $06
        jmp _bbs_post_init
        .org $09
        jmp _bbs_post_deinit
        .org $0C
        rts
        .res 2, $ea
        rts
        .res 2, $ea
        rts
        .res 2, $ea
