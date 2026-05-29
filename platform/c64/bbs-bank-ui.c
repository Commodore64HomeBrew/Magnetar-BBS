#include "bbs-bank.h"
#include "bbs-msg-extra.h"

unsigned char
bbs_ui_bank_init(void)
{
  bbs_msg_extra_init();
  return 1u;
}

void
bbs_ui_bank_deinit(void)
{
  bbs_msg_extra_deinit();
}

