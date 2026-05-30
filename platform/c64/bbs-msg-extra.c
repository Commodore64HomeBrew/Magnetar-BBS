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
#define buf_putc_raw(c) (BBS_SHARED->buf_putc_raw((unsigned char)(c)))

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

void
bbs_msg_system_stats(void)
{
  unsigned short total_msgs;
  unsigned char message[40];
  unsigned char day_ptr, stats_days, day_offset;
  unsigned char j, k, c, d;
  unsigned short dm;
  static const unsigned char day_colour[7] =
      { 0x1c, 0x81, 0x9e, 0x99, 0x9f, 0x9a, 0x9c };

  shell_output_str(NULL, "\r\n\x9clast callers:\r\n\r\n", "");

  k = (unsigned char)(bbs_sysstats.caller_ptr + 1u);
  if(k >= BBS_STATS_USRS) { k = 0; }

  for(j = 0u; j < BBS_STATS_USRS; ++j) {
    shell_output_str(NULL, "\x99  -> \x05", bbs_sysstats.last_callers[k++]);
    if(k >= BBS_STATS_USRS) { k = 0; }
  }

  stats_days = (unsigned char)(bbs_status.width > 2 ? (bbs_status.width - 2u) : 1u);
  if(stats_days > BBS_STATS_DAYS) { stats_days = BBS_STATS_DAYS; }

  shell_output_str(NULL, "\r\n\x0d\x9fposts per day:", "");

  if(buf_putc_raw(ISO_cr) < 0) { goto stats_chart_done; }
  if(buf_putc_raw(0x05) < 0) { goto stats_chart_done; }

  c = 0x39;
  for(k = 0u; k < 9u; ++k) {
    if(buf_putc_raw(c--) < 0) { goto stats_chart_done; }
    if(buf_putc_raw(0xab) < 0) { goto stats_chart_done; }
    if(buf_putc_raw(ISO_cr) < 0) { goto stats_chart_done; }
  }

  if(buf_putc_raw(PETSCII_RIGHT) < 0) { goto stats_chart_done; }
  if(buf_putc_raw(0xed) < 0) { goto stats_chart_done; }
  for(k = 0u; k < stats_days; ++k) {
    if(buf_putc_raw(0xb1) < 0) { goto stats_chart_done; }
  }

  if(buf_putc_raw(ISO_cr) < 0) { goto stats_chart_done; }
  if(buf_putc_raw(PETSCII_UP) < 0) { goto stats_chart_done; }
  if(buf_putc_raw(PETSCII_RIGHT) < 0) { goto stats_chart_done; }
  if(buf_putc_raw(PETSCII_RIGHT) < 0) { goto stats_chart_done; }
  if(buf_putc_raw(PETSCII_RIGHT) < 0) { goto stats_chart_done; }
  if(buf_putc_raw(PETSCII_REVON) < 0) { goto stats_chart_done; }

  day_offset = (unsigned char)(BBS_STATS_DAYS - stats_days + 1u);
  day_ptr = (unsigned char)(bbs_sysstats.day_ptr + day_offset);
  if(day_ptr >= BBS_STATS_DAYS) { day_ptr = (unsigned char)(day_ptr - BBS_STATS_DAYS); }

  d = 0u;
  for(k = 0u; k < stats_days; ++k) {
    if(buf_putc_raw(day_colour[d++]) < 0) { goto stats_chart_done; }
    if(d > 6u) { d = 0u; }

    dm = bbs_sysstats.daily_msgs[day_ptr];
    if(dm > 28u) { dm = 28u; }

    for(j = 0u; j < (unsigned char)dm; ++j) {
      if(buf_putc_raw(PETSCII_UP) < 0) { goto stats_chart_done; }
      if(buf_putc_raw(PETSCII_LEFT) < 0) { goto stats_chart_done; }
      if(buf_putc_raw(0xe3) < 0) { goto stats_chart_done; }
    }

    if(buf_putc_raw(0x90) < 0) { goto stats_chart_done; }
    for(j = 0u; j < (unsigned char)dm; ++j) {
      if(buf_putc_raw(PETSCII_DOWN) < 0) { goto stats_chart_done; }
    }
    if(buf_putc_raw(PETSCII_RIGHT) < 0) { goto stats_chart_done; }

    ++day_ptr;
    if(day_ptr >= BBS_STATS_DAYS) { day_ptr = 0u; }
  }

  if(buf_putc_raw(PETSCII_REVOFF) < 0) { goto stats_chart_done; }
  if(buf_putc_raw(ISO_cr) < 0) { goto stats_chart_done; }
  if(buf.used < buf.size) {
    buf.bufmem[(buf.head + buf.used) % buf.size] = 0;
  }

stats_chart_done:
  total_msgs = 0u;
  for(k = 1u; k <= BBS_MAX_BOARDS; ++k) {
    total_msgs = (unsigned short)(total_msgs + bbs_config.msg_id[k]);
  }

  shell_output_str(NULL, "\r\n\x9etotal msgs:\x05 ", "");
  bbs_u16_to_dec(total_msgs, message);
  shell_output_str(NULL, (char *)message, "");
  bbs_transport_flush_outbound();
}

PROCESS(sys_stats_process, "sysstats");
SHELL_COMMAND(sys_stats_command, "x", "x : bbs stats", &sys_stats_process);

PROCESS(usr_stats_process, "usrstats");
SHELL_COMMAND(usr_stats_command, "y", "y : your stats", &usr_stats_process);

PROCESS(info_process, "info");
SHELL_COMMAND(info_command, "i", "i : bbs info", &info_process);

PROCESS_THREAD(sys_stats_process, ev, data)
{
  PROCESS_BEGIN();
  (void)ev;
  (void)data;
  bbs_msg_system_stats();
  PROCESS_END();
}

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
  shell_register_command(&sys_stats_command);
  shell_register_command(&usr_stats_command);
  shell_register_command(&info_command);
}

void
bbs_msg_extra_deinit(void)
{
  shell_unregister_command(&sys_stats_command);
  shell_unregister_command(&usr_stats_command);
  shell_unregister_command(&info_command);
}
