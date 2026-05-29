.segment "BANKHDR"

.import _bbs_xfer_bank_init, _bbs_xfer_deinit, _bbs_xfer_set_op, _bbs_xfer_feed

bank_hdr:
        .byte $42, $42, $4b, $31
        .word _bbs_xfer_bank_init
        .word _bbs_xfer_deinit
        .word _bbs_xfer_set_op
        .word _bbs_xfer_feed
        .word $0000
