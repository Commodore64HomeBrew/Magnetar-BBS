#ifndef BBS_TRANSFER_H_
#define BBS_TRANSFER_H_

#include "contiki.h"

PROCESS_NAME(bbs_xfer_process);
void bbs_xfer_set_op(const char *cmd);
void bbs_xfer_init(void);
void bbs_xfer_feed(unsigned char c);

#endif /* BBS_TRANSFER_H_ */
