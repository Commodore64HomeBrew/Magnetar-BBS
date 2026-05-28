#include "bbs-shell.h"
#include "bbs-defs.h"
#include "bbs-msg-bind.h"
#include "bbs-msg-extra.h"

/* Intentionally avoid stdio/printf in module to save heap+CODE. */

static void msg_user_stats(void);
static void msg_system_stats(void);
static void msg_info(void);

static void u16_to_dec(unsigned short v, unsigned char *out);

unsigned char
bbs_msg_extra_bind(const bbs_module_ctx_t *ctx)
{
  if(ctx == NULL || ctx->bbsm_msg_set_handlers == NULL) {
    return 0u;
  }
  ctx->bbsm_msg_set_handlers(msg_system_stats, msg_user_stats, msg_info);
  return 1u;
}

static void
msg_info(void)
{
  bbs_banner(board.sys_prefix, BBS_BANNER_INFO, bbs_status.encoding_suffix, board.sys_device, 0);
}

static void
msg_user_stats(void)
{
  unsigned char nbuf[6];
  unsigned char message[32];

  shell_output_str(NULL, "\r\n\x9estats for \x05", bbs_user.user_name);

  shell_output_str(NULL, "\r\n\x9eyour msgs:\x05 ", "");
  u16_to_dec(bbs_usrstats.num_msgs, nbuf);
  shell_output_str(NULL, (char *)nbuf, "");

  shell_output_str(NULL, "\x9eyour calls:\x05 ", "");
  u16_to_dec(bbs_usrstats.num_calls, nbuf);
  shell_output_str(NULL, (char *)nbuf, "");
  (void)message;
}

static void
msg_system_stats(void)
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
  u16_to_dec(total_msgs, message);
  shell_output_str(NULL, (char *)message, "");
}

static void
u16_to_dec(unsigned short v, unsigned char *out)
{
  static const unsigned short pow10[5] = { 10000u, 1000u, 100u, 10u, 1u };
  unsigned char i;
  unsigned char started;

  if(out == NULL) return;

  started = 0u;
  for(i = 0u; i < 5u; ++i) {
    unsigned char digit = 0u;
    unsigned short p = pow10[i];
    while(v >= p) {
      v = (unsigned short)(v - p);
      ++digit;
    }
    if(digit != 0u || started != 0u || i == 4u) {
      *out++ = (unsigned char)('0' + digit);
      started = 1u;
    }
  }
  *out = 0;
}

