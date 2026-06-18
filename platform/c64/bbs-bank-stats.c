#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-bank.h"
#include "bbs-bank-stats.h"
#include "bbs-sys-stats.h"
#include "bbs-msg-extra.h"
#include "bbs-bank-macros.h"

PROCESS(bbs_msg_nop_process, "");
PROCESS_THREAD(bbs_msg_nop_process, ev, data)
{
  PROCESS_BEGIN();
  (void)ev;
  (void)data;
  PROCESS_END();
}

SHELL_COMMAND(sys_stats_command, "x", "", &bbs_msg_nop_process);
SHELL_COMMAND(usr_stats_command, "y", "", &bbs_msg_nop_process);
SHELL_COMMAND(info_command, "i", "", &bbs_msg_nop_process);

void
bbs_stats_set_op(const char *cmd)
{
  if(cmd == NULL || cmd[0] == '\0') {
    return;
  }
  switch((unsigned char)cmd[0]) {
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
    return;
  }
  shell_prompt(bbs_status.prompt);
}

unsigned char
bbs_stats_bank_init(void)
{
  shell_register_command(&sys_stats_command);
  shell_register_command(&usr_stats_command);
  shell_register_command(&info_command);
  return 1u;
}

void
bbs_stats_bank_deinit(void)
{
  shell_unregister_command(&info_command);
  shell_unregister_command(&usr_stats_command);
  shell_unregister_command(&sys_stats_command);
}
