#include "bbs-bank.h"
#include "bbs-shell.h"
#include "bbs-telnetd.h"
#include "bbs-file.h"
#include "sys/clock.h"
#include "cfs/cfs.h"
#include <string.h>

static unsigned long
bbs_shared_clock_time(void)
{
  return (unsigned long)clock_time();
}

extern BBS_BUFFER buf;
extern int shell_event_input;

static unsigned char bbs_bank_loaded;
static unsigned char bbs_bank_cur_id;

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
  default:
    return NULL;
  }
}

void
bbs_shared_publish(void)
{
  *(volatile bbs_shared_t **)(BBS_SHARED_MAILBOX) = &bbs_shared_data;

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
  BBS_SHARED->buf_append = buf_append;
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

void
bbs_shared_sync_back(void)
{
  /* Embedded structs: no copy; publish only refreshes pointers/callbacks. */
}

unsigned char
bbs_bank_load(unsigned char bank_id)
{
  int fd;
  unsigned int total;
  unsigned int n;
  unsigned char *dst;
  const char *filename;
  unsigned char expect_sig;
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

  bbs_shared_publish();

  fd = cfs_open(filename, CFS_READ);
  if(fd < 0) {
    goto load_failed;
  }

  dst = (unsigned char *)BBS_BANK_BASE;
  total = 0u;
  while(total < (unsigned int)BBS_BANK_SIZE) {
    n = (unsigned int)cfs_read(fd, dst + total, (unsigned int)BBS_BANK_SIZE - total);
    if(n == 0u) {
      break;
    }
    total += n;
  }
  cfs_close(fd);

  if(total < sizeof(bbs_bank_hdr_t)) {
    goto load_failed;
  }
  expect_sig = (unsigned char)('0' + bank_id);
  if(BBS_BANK_HDR->sig[0] != 'B' || BBS_BANK_HDR->sig[1] != 'B' ||
      BBS_BANK_HDR->sig[2] != 'K' || BBS_BANK_HDR->sig[3] != expect_sig) {
    goto load_failed;
  }
  if(BBS_BANK_HDR->init == NULL) {
    goto load_failed;
  }

  *(volatile unsigned char *)BBS_BANK_HW_REG = bank_id;
  BBS_SHARED->active_bank = bank_id;
  bbs_bank_loaded = 1u;
  bbs_bank_cur_id = bank_id;

  if(BBS_BANK_HDR->init() == 0u) {
    bbs_bank_unload();
    goto load_failed;
  }
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
  *(volatile unsigned char *)BBS_BANK_HW_REG = BBS_BANK_HW_DISABLE;
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
