.segment "BANKHDR"

.import _bbs_ui_bank_init, _bbs_ui_bank_deinit, _bbs_ui_set_op, _bbs_msg_system_stats

bank_hdr:
        .byte $42, $42, $4b, $34
        .word _bbs_ui_bank_init
        .word _bbs_ui_bank_deinit
        .word _bbs_ui_set_op
        .word $0000
        .word _bbs_msg_system_stats
