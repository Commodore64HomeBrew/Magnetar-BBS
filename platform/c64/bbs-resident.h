#ifndef BBS_RESIDENT_H_
#define BBS_RESIDENT_H_

#define BBS_USE_RESIDENT 1
#include "bbs-bank.h"

/* Core globals live in bbs_shared_data (DATA), below the $B000 bank window. */
#define board           (BBS_SHARED->s_board)
#define bbs_config      (BBS_SHARED->s_config)
#define bbs_status      (BBS_SHARED->s_status)
#define bbs_user        (BBS_SHARED->s_user)
#define bbs_usrstats    (BBS_SHARED->s_usrstats)
#define bbs_sysstats    (BBS_SHARED->s_sysstats)
#define bbs_time        (BBS_SHARED->s_time)

#endif /* BBS_RESIDENT_H_ */
