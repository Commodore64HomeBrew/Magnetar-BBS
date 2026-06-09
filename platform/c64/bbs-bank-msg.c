#include "bbs-bank.h"
#include "bbs-read.h"
#include "bbs-setboard.h"
#include "bbs-sys-stats.h"
#include "bbs-msg-extra.h"

unsigned char
bbs_msg_bank_init(void)
{
  bbs_read_init();
  bbs_setboard_init();
  bbs_sys_stats_init();
  bbs_msg_extra_init();
  return 1u;
}

void
bbs_msg_bank_deinit(void)
{
  bbs_msg_extra_deinit();
  bbs_sys_stats_deinit();
  bbs_read_deinit();
  bbs_setboard_deinit();
}
