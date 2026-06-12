#include "bbs-bank.h"

#if sizeof(bbs_shared_t) > BBS_SHARED_SIZE
#error bbs_shared_t exceeds BBS_SHARED_SIZE — bump BBS_SHARED_SIZE in bbs-bank.h and c64-bbs-core.cfg
#endif

unsigned char bbs_shared_size_bytes[sizeof(bbs_shared_t)];
