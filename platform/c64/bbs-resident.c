#include "bbs-bank.h"

#pragma bss-name("MAILBOX")
volatile bbs_shared_t *bbs_shared_mailbox;

#pragma bss-name("BSS")
/* Shared state in BSS below $B000. */
bbs_shared_t bbs_shared_data;
