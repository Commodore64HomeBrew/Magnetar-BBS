.segment "BANKHDR"

.import _bbs_post_bank_init, _bbs_post_bank_deinit

bank_hdr:
        .byte $42, $42, $4b, $32
        .word _bbs_post_bank_init
        .word _bbs_post_bank_deinit
        .word $0000
        .word $0000
        .word $0000
