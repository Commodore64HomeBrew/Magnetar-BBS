#include "bbs-bank.h"
#include "bbs-post.h"

static unsigned char
bbs_post_bank_init(void)
{
  bbs_post_init();
  return 1u;
}

static void
bbs_post_bank_deinit(void)
{
  bbs_post_deinit();
}

#pragma rodata-name("BANKHDR")

const bbs_bank_hdr_t bank_hdr = {
  { 'B', 'B', 'K', '2' },
  bbs_post_bank_init,
  bbs_post_bank_deinit,
  NULL,
  NULL
};

#pragma rodata-name("CODE")
