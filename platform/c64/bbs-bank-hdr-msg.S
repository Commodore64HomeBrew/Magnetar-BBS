.include "bbs-bank-entry.inc"

.import _bbs_msg_bank_init, _bbs_msg_bank_deinit, _bbs_msg_system_stats

.segment "BANKHDR"

bank_hdr:
        bank_hdr_sig $33

        .org $06
        jmp _bbs_msg_bank_init
        .org $09
        jmp _bbs_msg_bank_deinit
        .org $0C
        rts
        .res 2, $ea
        rts
        .res 2, $ea
        .org $12
        jmp _bbs_msg_system_stats
