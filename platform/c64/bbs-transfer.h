#ifndef BBS_TRANSFER_H_
#define BBS_TRANSFER_H_

#include "contiki.h"
#if defined(BBS_XFER_MODULE)
#include "bbs-modules.h"
#endif

PROCESS_NAME(bbs_xfer_process);
#if defined(BBS_XFER_MODULE)
unsigned char bbs_xfer_bind(const bbs_module_ctx_t *ctx);
#endif
void bbs_xfer_set_op(const char *cmd);
unsigned char bbs_xfer_init(void);
void bbs_xfer_deinit(void);
void bbs_xfer_feed(unsigned char c);

#endif /* BBS_TRANSFER_H_ */
