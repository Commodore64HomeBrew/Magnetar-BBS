#include "bbs-bank.h"
#include "bbs-resident.h"
#include "bbs-shell.h"
#include "bbs-telnetd.h"

extern int buf_putc_raw(unsigned char c);
#include "bbs-file.h"
#include "sys/clock.h"
#include <cbm.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define BBS_BANK_LOAD_CHN  BBS_FILE_CHANNEL

/* Off: screen RAM is telnet outbound; XMODEM uses a partition during STATUS_XFER. */
#define BBS_BANK_DEBUG  0

static unsigned char
bbs_bank_sig_ok(unsigned char bank_id)
{
  const unsigned char *p = (const unsigned char *)BBS_BANK_BASE;
  unsigned char expect;

  expect = (unsigned char)(0x30u + bank_id);
  return (p[0] == 0x42u && p[1] == 0x42u && p[2] == 0x4bu && p[3] == expect) ? 1u : 0u;
}

#if BBS_BANK_DEBUG
#define BBS_BANK_DBG_SCR_BASE  0x0400u
#define BBS_BANK_DBG_SCR_SIZE  0x0400u
#define BBS_BANK_DBG_SCR_COLS  40u

static unsigned int bbs_bank_dbg_pos;

static unsigned char
bbs_bank_dbg_screencode(unsigned char c)
{
  if(c >= (unsigned char)'a' && c <= (unsigned char)'z') {
    c = (unsigned char)(c - (unsigned char)'a' + (unsigned char)'A');
  }
  if(c >= (unsigned char)'A' && c <= (unsigned char)'Z') {
    return (unsigned char)(c - (unsigned char)'A' + 1u);
  }
  if(c == (unsigned char)'\n' || c == (unsigned char)'\r') {
    return 0u;
  }
  return c;
}

static void
bbs_bank_dbg_clear(void)
{
  memset((void *)BBS_BANK_DBG_SCR_BASE, 0x20u, (size_t)BBS_BANK_DBG_SCR_SIZE);
  bbs_bank_dbg_pos = 0u;
}

static void
bbs_bank_dbg_newline(void)
{
  unsigned int row;

  row = bbs_bank_dbg_pos / (unsigned int)BBS_BANK_DBG_SCR_COLS;
  bbs_bank_dbg_pos = (row + 1u) * (unsigned int)BBS_BANK_DBG_SCR_COLS;
  if(bbs_bank_dbg_pos >= (unsigned int)BBS_BANK_DBG_SCR_SIZE) {
    bbs_bank_dbg_pos = 0u;
  }
}

static void
bbs_bank_dbg_putc(unsigned char c)
{
  volatile unsigned char *scr;
  unsigned char ch;

  ch = bbs_bank_dbg_screencode(c);
  if(ch == 0u) {
    bbs_bank_dbg_newline();
    return;
  }
  if(bbs_bank_dbg_pos >= (unsigned int)BBS_BANK_DBG_SCR_SIZE) {
    bbs_bank_dbg_pos = 0u;
  }
  scr = (volatile unsigned char *)(BBS_BANK_DBG_SCR_BASE + bbs_bank_dbg_pos);
  *scr = ch;
  ++bbs_bank_dbg_pos;
}

static void
bbs_bank_dbg_puts(const char *msg)
{
  while(msg != NULL && *msg != 0) {
    bbs_bank_dbg_putc((unsigned char)*msg);
    ++msg;
  }
  bbs_bank_dbg_newline();
}

static void
bbs_bank_dbg_sig_fail(unsigned char bank_id)
{
  char line[64];
  const unsigned char *p = (const unsigned char *)BBS_BANK_BASE;
  unsigned char expect;

  expect = (unsigned char)(0x30u + bank_id);
  sprintf(line,
      "bank id=%u sig fail got=%02x %02x %02x %02x want=42 42 4b %02x",
      (unsigned int)bank_id,
      (unsigned int)p[0], (unsigned int)p[1], (unsigned int)p[2], (unsigned int)p[3],
      (unsigned int)expect);
  bbs_bank_dbg_puts(line);
}

static void
bbs_bank_dbg_fail(unsigned char bank_id, unsigned char reason,
    unsigned int total, const char *filename, unsigned char open_err)
{
  char line[64];
  const bbs_bank_hdr_t *hdr;

  hdr = (const bbs_bank_hdr_t *)BBS_BANK_BASE;

  switch(reason) {
  case 1:
    sprintf(line, "bank id=%u open fail %s dev=%u err=%u",
        (unsigned int)bank_id, filename,
        (unsigned int)board.sys_device, (unsigned int)open_err);
    break;
  case 2:
    sprintf(line, "bank id=%u read short %u need %u",
        (unsigned int)bank_id, total, (unsigned int)sizeof(bbs_bank_hdr_t));
    break;
  case 3:
    bbs_bank_dbg_sig_fail(bank_id);
    return;
  case 4:
    sprintf(line, "bank id=%u init null $%04x",
        (unsigned int)bank_id, (unsigned int)(unsigned long)hdr->init);
    break;
  case 5:
    sprintf(line, "bank id=%u init returned 0", (unsigned int)bank_id);
    break;
  default:
    sprintf(line, "bank id=%u fail reason=%u", (unsigned int)bank_id,
        (unsigned int)reason);
    break;
  }
  bbs_bank_dbg_puts(line);
}

static void
bbs_bank_dbg_ok(unsigned char bank_id, unsigned int total, const char *filename)
{
  char line[64];

  sprintf(line, "bank id=%u ok %s %u bytes",
      (unsigned int)bank_id, filename, total);
  bbs_bank_dbg_puts(line);
}
#endif /* BBS_BANK_DEBUG */

static unsigned long
bbs_shared_clock_time(void)
{
  return (unsigned long)clock_time();
}

extern BBS_BUFFER buf;
extern int shell_event_input;

static unsigned char bbs_bank_loaded;
static unsigned char bbs_bank_cur_id;
static unsigned char bbs_shared_inited;

void
bbs_bank_hw_enable_for_exec(void)
{
  /* SD2IEC: bank image runs from RAM at $B000. $DE00 cart map is future-only. */
}

void
bbs_bank_hw_disable_exec(void)
{
}

static const char *
bbs_bank_filename(unsigned char bank_id)
{
  switch(bank_id) {
  case BBS_BANK_ID_XFER:
    return "bbs-bank1.bin";
  case BBS_BANK_ID_POST:
    return "bbs-bank2.bin";
  case BBS_BANK_ID_MSG:
    return "bbs-bank3.bin";
  case BBS_BANK_ID_UI:
    return "bbs-bank4.bin";
  default:
    return NULL;
  }
}

void
bbs_shared_init(void)
{
  if(bbs_shared_inited != 0u) {
    return;
  }
  bbs_shared_inited = 1u;
  BBS_SHARED->sig0 = 'B';
  BBS_SHARED->sig1 = 'S';
  BBS_SHARED->active_bank = 0u;
  BBS_SHARED->s_buf = &buf;
  BBS_SHARED->s_shell_ev = &shell_event_input;
  BBS_SHARED->shell_output_str = shell_output_str;
  BBS_SHARED->shell_prompt = shell_prompt;
  BBS_SHARED->shell_register_command = shell_register_command;
  BBS_SHARED->shell_unregister_command = shell_unregister_command;
  BBS_SHARED->transport_poll = bbs_transport_poll;
  BBS_SHARED->transport_flush_outbound = bbs_transport_flush_outbound;
  BBS_SHARED->transport_buf_reset = bbs_transport_buf_reset;
  BBS_SHARED->scr_layout_output = bbs_scr_layout_output;
  BBS_SHARED->scr_layout_xfer = bbs_scr_layout_xfer;
  BBS_SHARED->stream_begin = bbs_stream_begin;
  BBS_SHARED->buf_append = buf_append;
  BBS_SHARED->buf_putc_raw = buf_putc_raw;
  BBS_SHARED->clock_time = bbs_shared_clock_time;
  BBS_SHARED->set_prompt = set_prompt;
  BBS_SHARED->update_time = update_time;
  BBS_SHARED->log_message = log_message;
  BBS_SHARED->file_path = file_path;
  BBS_SHARED->bbs_banner = (void (*)(unsigned char *, unsigned char *,
      unsigned char *, unsigned char, unsigned char))bbs_banner;
  BBS_SHARED->bbs_path_sys_at = bbs_path_sys_at;
#ifdef BBS_SERIAL_TRANSPORT
  BBS_SHARED->serial_flush_outbound = bbs_serial_flush_outbound;
#else
  BBS_SHARED->serial_flush_outbound = NULL;
#endif
}

unsigned char
bbs_bank_load(unsigned char bank_id)
{
  unsigned int total;
  unsigned int n;
  unsigned char *dst;
  const char *filename;
  unsigned char had_bank;
  unsigned char prev_id;

  filename = bbs_bank_filename(bank_id);
  if(filename == NULL) {
    return 0u;
  }

  if(bbs_bank_loaded != 0u && bbs_bank_cur_id == bank_id) {
    return 1u;
  }

  had_bank = bbs_bank_loaded;
  prev_id = bbs_bank_cur_id;

  if(bbs_bank_loaded != 0u) {
    bbs_bank_unload();
  }

  bbs_shared_init();

#if BBS_BANK_DEBUG
  bbs_bank_dbg_clear();
  {
    char line[64];
    sprintf(line, "bank load id=%u %s dev=%u",
        (unsigned int)bank_id, filename, (unsigned int)board.sys_device);
    bbs_bank_dbg_puts(line);
  }
#endif

  if(cbm_open(BBS_BANK_LOAD_CHN, board.sys_device, BBS_BANK_LOAD_CHN,
      filename) != 0u) {
#if BBS_BANK_DEBUG
    bbs_bank_dbg_fail(bank_id, 1u, 0u, filename, _oserror);
#endif
    goto load_failed;
  }
  dst = (unsigned char *)BBS_BANK_BASE;
  total = 0u;
  while(total < (unsigned int)BBS_BANK_SIZE) {
    n = (unsigned int)cbm_read(BBS_BANK_LOAD_CHN, dst + total,
        (unsigned int)BBS_BANK_SIZE - total);
    if(n == 0u) {
      break;
    }
    total += n;
  }
  cbm_close(BBS_BANK_LOAD_CHN);

  if(total < sizeof(bbs_bank_hdr_t)) {
#if BBS_BANK_DEBUG
    bbs_bank_dbg_fail(bank_id, 2u, total, filename, 0u);
#endif
    goto load_failed;
  }
  if(bbs_bank_sig_ok(bank_id) == 0u) {
#if BBS_BANK_DEBUG
    bbs_bank_dbg_fail(bank_id, 3u, total, filename, 0u);
#endif
    goto load_failed;
  }
  if(BBS_BANK_HDR->init == NULL) {
#if BBS_BANK_DEBUG
    bbs_bank_dbg_fail(bank_id, 4u, total, filename, 0u);
#endif
    goto load_failed;
  }

  BBS_SHARED->active_bank = bank_id;
  bbs_bank_loaded = 1u;
  bbs_bank_cur_id = bank_id;

  if(BBS_BANK_HDR->init() == 0u) {
#if BBS_BANK_DEBUG
    bbs_bank_dbg_fail(bank_id, 5u, total, filename, 0u);
#endif
    bbs_bank_unload();
    goto load_failed;
  }
  /* Reset ring after bank swap; flush first only when replacing another bank. */
#ifndef BBS_SERIAL_TRANSPORT
  if(had_bank != 0u) {
    bbs_transport_flush_outbound();
  }
#endif
  bbs_transport_buf_reset();
#if BBS_BANK_DEBUG
  bbs_bank_dbg_ok(bank_id, total, filename);
#endif
  return 1u;

load_failed:
  if(had_bank != 0u && prev_id != 0u && prev_id != bank_id) {
    (void)bbs_bank_load(prev_id);
  }
  return 0u;
}

void
bbs_bank_unload(void)
{
  if(bbs_bank_loaded != 0u && BBS_BANK_HDR->deinit != NULL) {
    BBS_BANK_HDR->deinit();
  }
  bbs_bank_forget();
}

void
bbs_bank_forget(void)
{
  BBS_SHARED->active_bank = 0u;
  bbs_bank_loaded = 0u;
  bbs_bank_cur_id = 0u;
}

unsigned char
bbs_bank_active(void)
{
  return bbs_bank_loaded;
}

unsigned char
bbs_bank_id_active(void)
{
  return bbs_bank_cur_id;
}

void
bbs_bank_set_op(const char *cmd)
{
  if(bbs_bank_loaded != 0u && BBS_BANK_HDR->set_op != NULL) {
    BBS_BANK_HDR->set_op(cmd);
  }
}

void
bbs_bank_feed(unsigned char c)
{
  if(bbs_bank_loaded != 0u && BBS_BANK_HDR->feed != NULL) {
    BBS_BANK_HDR->feed(c);
  }
}
