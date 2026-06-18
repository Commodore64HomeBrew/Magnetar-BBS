#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-defs.h"
#include "bbs-msg-extra.h"
#include "bbs-fmt.h"

#ifndef BBS_BANK_BUILD
#error "bbs-msg-extra.c is built for bank overlays only (BBS_BANK_BUILD)"
#endif

#include "bbs-bank-macros.h"

static unsigned char stats_dec_buf[6];

void
bbs_msg_info(void)
{
  bbs_banner(board.sys_prefix, BBS_BANNER_INFO, bbs_status.encoding_suffix, board.sys_device, 0);
}

void
bbs_msg_user_stats(void)
{
  shell_output_str(NULL, "\r\n\x9estats for \x05", bbs_user.user_name);

  shell_output_str(NULL, "\r\n\x9eyour msgs:\x05 ", "");
  bbs_u16_to_dec(bbs_usrstats.num_msgs, stats_dec_buf);
  shell_output_str(NULL, (char *)stats_dec_buf, "");

  shell_output_str(NULL, "\x9eyour calls:\x05 ", "");
  bbs_u16_to_dec(bbs_usrstats.num_calls, stats_dec_buf);
  shell_output_str(NULL, (char *)stats_dec_buf, "");
}
