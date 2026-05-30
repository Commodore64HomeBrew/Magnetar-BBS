#include "bbs-bank.h"
#include "bbs-bank-macros.h"
#include "bbs-msg-extra.h"

void
bbs_ui_set_op(const char *cmd)
{
  if(cmd == 0 || cmd[0] == 0) {
    return;
  }
  switch(cmd[0]) {
  case 'x':
    bbs_msg_system_stats();
    break;
  case 'y':
    bbs_msg_user_stats();
    break;
  case 'i':
    bbs_msg_info();
    break;
  default:
    break;
  }
  bbs_transport_flush_outbound();
}

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

