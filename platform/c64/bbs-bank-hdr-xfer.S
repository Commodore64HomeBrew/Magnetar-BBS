.include "bbs-bank-entry.inc"

.import _bbs_xfer_init, _bbs_xfer_deinit, _bbs_xfer_set_op

.segment "BANKHDR"

bank_hdr:
        bank_hdr_sig $31

        .org $06
        jmp _bbs_xfer_init
        .org $09
        jmp _bbs_xfer_deinit
        .org $0C
        jmp _bbs_xfer_set_op
        .org $0F
        rts
        .res 2, $ea
        rts
        .res 2, $ea
