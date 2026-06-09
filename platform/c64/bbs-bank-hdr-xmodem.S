.include "bbs-bank-entry.inc"

.import _bbs_xmodem_xfer_init, _bbs_xmodem_xfer_deinit
.import _bbs_xmodem_xfer_set_op, _bbs_xmodem_xfer_feed

.segment "BANKHDR"

bank_hdr:
        bank_hdr_sig $35

        .org $06
        jmp _bbs_xmodem_xfer_init
        .org $09
        jmp _bbs_xmodem_xfer_deinit
        .org $0C
        jmp _bbs_xmodem_xfer_set_op
        .org $0F
        jmp _bbs_xmodem_xfer_feed
        .res 2, $ea
        rts
