#include "bbs-bank.h"
#include "bbs-api.h"
#include "bbs-resident.h"
#include "bbs-shell.h"
#include "bbs-telnetd.h"
#include "bbs-file.h"
#include <cbm.h>
#include <string.h>

extern BBS_BUFFER buf;
extern int shell_event_input;

static unsigned char bbs_bank_loaded;
static unsigned char bbs_bank_cur_id;

typedef unsigned char (*bbs_bank_init_fn)(void);
typedef void (*bbs_bank_void_fn)(void);
typedef void (*bbs_bank_set_op_fn)(const char *);
typedef void (*bbs_bank_feed_fn)(unsigned char);

static unsigned char
bbs_bank_sig_ok(unsigned char bank_id)
{
  const unsigned char *p = (const unsigned char *)BBS_BANK_BASE;
  unsigned char expect;

  expect = (unsigned char)(0x30u + bank_id);
  return (p[0] == 0x42u && p[1] == 0x42u && p[2] == 0x4bu && p[3] == expect) ? 1u : 0u;
}

static unsigned char
bbs_bank_entry_is_jmp(unsigned char off)
{
  return ((const unsigned char *)BBS_BANK_BASE)[off] == 0x4cu;
}

/* cc65 BSS is not in the .bin; zero tail RAM before init. */
static void
bbs_bank_clear_bss(unsigned int loaded)
{
  unsigned char *p;
  unsigned int n;

  if(loaded >= (unsigned int)BBS_BANK_RAM_SIZE) {
    return;
  }
  p = (unsigned char *)BBS_BANK_BASE + loaded;
  n = (unsigned int)BBS_BANK_RAM_SIZE - loaded;
  memset(p, 0, n);
}

static unsigned char
bbs_bank_header_ok(unsigned char bank_id)
{
  if(bbs_bank_sig_ok(bank_id) == 0u) {
    return 0u;
  }
  if(bbs_bank_entry_is_jmp(BBS_BANK_INIT_OFF) == 0u ||
      bbs_bank_entry_is_jmp(BBS_BANK_DEINIT_OFF) == 0u) {
    return 0u;
  }
  return 1u;
}

static const char *
bbs_bank_filename(unsigned char bank_id)
{
  switch(bank_id) {
  case BBS_BANK_ID_XFER:   return "bank1";
  case BBS_BANK_ID_POST:   return "bank2";
  case BBS_BANK_ID_MSG:    return "bank3";
  case BBS_BANK_ID_STATS:  return "bank4";
  case BBS_BANK_ID_XMODEM: return "bank5";
  default:                 return NULL;
  }
}

void
bbs_api_init(void)
{
  BBS_SHARED->sig0 = 'B';
  BBS_SHARED->sig1 = 'S';
  BBS_SHARED->abi_major = BBS_ABI_MAJOR;
  BBS_SHARED->abi_minor = BBS_ABI_MINOR;
  BBS_SHARED->s_buf = &buf;
  BBS_SHARED->s_shell_ev = &shell_event_input;
}

unsigned char
bbs_bank_load(unsigned char bank_id)
{
  unsigned int total;
  unsigned int n;
  unsigned char *dst;
  const char *filename;
  char path[40];
  bbs_bank_init_fn init_fn;

  filename = bbs_bank_filename(bank_id);
  if(filename == NULL) {
    return 0u;
  }
  if(bbs_bank_loaded != 0u && bbs_bank_cur_id == bank_id) {
    return 1u;
  }
  if(bbs_bank_loaded != 0u) {
    bbs_bank_unload();
  }

  bbs_path_sys_colon(path, filename);
  if(cbm_open(BBS_FILE_CHANNEL, board.sys_device, BBS_FILE_CHANNEL,
      path) != 0u) {
    return 0u;
  }
  dst = (unsigned char *)BBS_BANK_BASE;
  total = 0u;
  while(total < (unsigned int)BBS_BANK_RAM_SIZE) {
    n = (unsigned int)cbm_read(BBS_FILE_CHANNEL, dst + total,
        (unsigned int)BBS_BANK_RAM_SIZE - total);
    if(n == 0u) {
      break;
    }
    total += n;
  }
  cbm_close(BBS_FILE_CHANNEL);

  if(total < BBS_BANK_HDR_SIZE || bbs_bank_header_ok(bank_id) == 0u) {
    return 0u;
  }
  bbs_bank_clear_bss(total);

  init_fn = (bbs_bank_init_fn)BBS_BANK_INIT_ADDR;
  BBS_SHARED->active_bank = bank_id;
  bbs_bank_loaded = 1u;
  bbs_bank_cur_id = bank_id;

  if(init_fn() == 0u) {
    bbs_bank_forget();
    return 0u;
  }
  bbs_transport_buf_reset();
  return 1u;
}

void
bbs_bank_unload(void)
{
  bbs_bank_void_fn deinit_fn;

  if(bbs_bank_loaded != 0u &&
      bbs_bank_header_ok(bbs_bank_cur_id) != 0u) {
    deinit_fn = (bbs_bank_void_fn)BBS_BANK_DEINIT_ADDR;
    deinit_fn();
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
bbs_bank_ensure_stats(void)
{
  if(bbs_bank_id_active() == BBS_BANK_ID_STATS) {
    return 1u;
  }
  return bbs_bank_load(BBS_BANK_ID_STATS);
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
  bbs_bank_set_op_fn fn;

  if(bbs_bank_loaded != 0u && cmd != NULL) {
    fn = (bbs_bank_set_op_fn)BBS_BANK_SET_OP_ADDR;
    fn(cmd);
  }
}

void
bbs_bank_feed(unsigned char c)
{
  bbs_bank_feed_fn fn;

  if(bbs_bank_loaded != 0u) {
    fn = (bbs_bank_feed_fn)BBS_BANK_FEED_ADDR;
    fn(c);
  }
}

void
bbs_bank_run_sys_stats(void)
{
  bbs_bank_void_fn fn;

  if(bbs_bank_loaded != 0u &&
      bbs_bank_entry_is_jmp(BBS_BANK_RUN_STATS_OFF) != 0u) {
    fn = (bbs_bank_void_fn)BBS_BANK_RUN_STATS_ADDR;
    fn();
  }
}
