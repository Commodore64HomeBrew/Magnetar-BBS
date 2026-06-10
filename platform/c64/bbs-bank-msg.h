#ifndef BBS_BANK_MSG_H_
#define BBS_BANK_MSG_H_

#include "contiki.h"

extern struct process bbs_msg_nop_process;

void bbs_msg_set_op(const char *cmd);

#endif /* BBS_BANK_MSG_H_ */
