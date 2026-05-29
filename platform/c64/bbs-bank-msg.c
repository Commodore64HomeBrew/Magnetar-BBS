#include "bbs-bank.h"
#include "bbs-read.h"
#include "bbs-setboard.h"

static unsigned char
bbs_msg_bank_init(void)
{
  bbs_read_init();
  bbs_setboard_init();
  return 1u;
}

static void
bbs_msg_bank_deinit(void)
{
  bbs_read_deinit();
  bbs_setboard_deinit();
}

#pragma rodata-name("BANKHDR")

const bbs_bank_hdr_t bank_hdr = {
  { 'B', 'B', 'K', '3' },
  bbs_msg_bank_init,
  bbs_msg_bank_deinit,
  NULL,
  NULL
};

#pragma rodata-name("CODE")
