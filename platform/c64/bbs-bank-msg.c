#include "bbs-bank.h"
#include "bbs-read.h"
#include "bbs-setboard.h"

unsigned char
bbs_msg_bank_init(void)
{
  bbs_read_init();
  bbs_setboard_init();
  return 1u;
}

void
bbs_msg_bank_deinit(void)
{
  bbs_read_deinit();
  bbs_setboard_deinit();
}

