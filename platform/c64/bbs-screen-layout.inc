; Screen RAM layout for telnet outbound + XMODEM (must match bbs-defs.h).
; $0400-$07E7 (1000 bytes): [RX 128][TX ...][XMODEM block 132]
BBS_XMODEM_RBUF_BASE = $0764
