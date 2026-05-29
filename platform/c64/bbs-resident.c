#include "bbs-bank.h"

/* Shared state in BSS below $B000; mailbox at $A986 is filled in bbs_shared_publish(). */
bbs_shared_t bbs_shared_data;
