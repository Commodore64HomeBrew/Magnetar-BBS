; Fixed resident data at BBS_SHARED_BASE (bbs-bank.h). Sized to sizeof(bbs_shared_t).
.export _bbs_shared_data

.segment "SHARED"

_bbs_shared_data:
        .res 800, 0
