#include "bbs-bank.h"
#include "bbs-post.h"

unsigned char
bbs_post_bank_init(void)
{
  bbs_post_init();
  return 1u;
}

void
bbs_post_bank_deinit(void)
{
  bbs_post_deinit();
}

