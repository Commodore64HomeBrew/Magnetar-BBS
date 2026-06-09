#ifndef BBS_BANK_MACROS_H_
#define BBS_BANK_MACROS_H_

#include "bbs-bank.h"
#include "bbs-api.h"

#define board              (BBS_SHARED->s_board)
#define bbs_config         (BBS_SHARED->s_config)
#define bbs_status         (BBS_SHARED->s_status)
#define bbs_user           (BBS_SHARED->s_user)
#define bbs_usrstats       (BBS_SHARED->s_usrstats)
#define bbs_sysstats       (BBS_SHARED->s_sysstats)
#define bbs_time           (BBS_SHARED->s_time)
#define buf                (*BBS_SHARED->s_buf)
#define shell_event_input  (*(BBS_SHARED->s_shell_ev))
#define xfer_cwd             (BBS_SHARED->xfer_cwd)

#endif /* BBS_BANK_MACROS_H_ */
