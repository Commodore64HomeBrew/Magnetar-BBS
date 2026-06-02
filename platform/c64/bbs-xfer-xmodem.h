#ifndef BBS_XFER_XMODEM_H_
#define BBS_XFER_XMODEM_H_

#include "contiki.h"

PROCESS_NAME(bbs_xmodem_xfer_process);
void bbs_xmodem_xfer_set_op(const char *cmd);
unsigned char bbs_xmodem_xfer_init(void);
void bbs_xmodem_xfer_deinit(void);
void bbs_xmodem_xfer_feed(unsigned char c);

#endif /* BBS_XFER_XMODEM_H_ */
