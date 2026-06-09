#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-defs.h"
#include "bbs-msg-extra.h"
#include "bbs-fmt.h"
#include <string.h>

#ifndef BBS_BANK_BUILD
#error "bbs-msg-extra.c is built for bank overlays only (BBS_BANK_BUILD)"
#endif

#include "bbs-bank-macros.h"

void
bbs_msg_info(void)
{
  bbs_banner(board.sys_prefix, BBS_BANNER_INFO, bbs_status.encoding_suffix, board.sys_device, 0);
}

void
bbs_msg_user_stats(void)
{
  unsigned char nbuf[6];
  unsigned char message[32];

  shell_output_str(NULL, "\r\n\x9estats for \x05", bbs_user.user_name);

  shell_output_str(NULL, "\r\n\x9eyour msgs:\x05 ", "");
  bbs_u16_to_dec(bbs_usrstats.num_msgs, nbuf);
  shell_output_str(NULL, (char *)nbuf, "");

  shell_output_str(NULL, "\x9eyour calls:\x05 ", "");
  bbs_u16_to_dec(bbs_usrstats.num_calls, nbuf);
  shell_output_str(NULL, (char *)nbuf, "");
  (void)message;
}

PROCESS(usr_stats_process, "usrstats");
//SHELL_COMMAND(usr_stats_command, "y", "y : your stats", &usr_stats_process);
SHELL_COMMAND(usr_stats_command, "y", "", &usr_stats_process);


PROCESS(info_process, "info");
//SHELL_COMMAND(info_command, "i", "i : bbs info", &info_process);
SHELL_COMMAND(info_command, "i", "", &info_process);

PROCESS_THREAD(usr_stats_process, ev, data)
{
  PROCESS_BEGIN();
  (void)ev;
  (void)data;
  bbs_msg_user_stats();
  PROCESS_END();
}

PROCESS_THREAD(info_process, ev, data)
{
  PROCESS_BEGIN();
  (void)ev;
  (void)data;
  bbs_msg_info();
  PROCESS_END();
}

void
bbs_msg_extra_init(void)
{
  shell_register_command(&usr_stats_command);
  shell_register_command(&info_command);
}

void
bbs_msg_extra_deinit(void)
{
  shell_unregister_command(&usr_stats_command);
  shell_unregister_command(&info_command);
}
