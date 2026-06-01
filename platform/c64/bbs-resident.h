#ifndef BBS_RESIDENT_H_
#define BBS_RESIDENT_H_

#define BBS_USE_RESIDENT 1
#include "bbs-bank.h"

/* State and callbacks live in the fixed block at BBS_SHARED_BASE (see bbs-bank.h). */
#define board           (BBS_SHARED->s_board)
#define bbs_config      (BBS_SHARED->s_config)
#define bbs_status      (BBS_SHARED->s_status)
#define bbs_user        (BBS_SHARED->s_user)
#define bbs_usrstats    (BBS_SHARED->s_usrstats)
#define bbs_sysstats    (BBS_SHARED->s_sysstats)
#define bbs_time        (BBS_SHARED->s_time)

#endif /* BBS_RESIDENT_H_ */
