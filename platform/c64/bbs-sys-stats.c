#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-defs.h"
#include "bbs-sys-stats.h"
#include "bbs-fmt.h"

#ifndef BBS_BANK_BUILD
#error "bbs-sys-stats.c is built for bank overlays only (BBS_BANK_BUILD)"
#endif

#include "bbs-bank-macros.h"

static unsigned char stats_msg_buf[40];

void
bbs_msg_system_stats(void)
{
  unsigned short total_msgs;
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

stats_chart_done:
  total_msgs = 0u;
  for(k = 1u; k <= BBS_MAX_BOARDS; ++k) {
    total_msgs = (unsigned short)(total_msgs + bbs_config.msg_id[k]);
  }

  shell_output_str(NULL, "\r\n\x9etotal msgs:\x05 ", "");
  bbs_u16_to_dec(total_msgs, stats_msg_buf);
  shell_output_str(NULL, (char *)stats_msg_buf, "");
}
