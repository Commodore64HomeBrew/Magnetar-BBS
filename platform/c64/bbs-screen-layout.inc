; Screen RAM layout (must match bbs-defs.h). Exclusive modes:
;   Output: $0400-$07E7 = 1000 B telnet ring
;   Xfer:   [RX $0400 128 B][TX ..][XMODEM $0764 132 B]
BBS_XMODEM_RBUF_BASE = $0764
