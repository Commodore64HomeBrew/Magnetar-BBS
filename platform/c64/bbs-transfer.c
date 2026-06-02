/* bbs-transfer.c — xfer cd/md/$ (bank 1). d/u + XMODEM in bank 5 (bbs-xfer-xmodem.c). */
#pragma static-locals(off)
#pragma rodata-name("CODE")

#ifndef BBS_BANK_BUILD
#error "bbs-transfer.c is linked only as bank overlay bbs-transfer-bank.o"
#endif

#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-transfer.h"
#include "bbs-bank.h"
#include "bbs-bank-macros.h"
#include <cbm.h>
#include <string.h>

#define XFER_CHN  2u
#define XFER_CMD  15u

#define XFER_SCRATCH_LEN  40u

typedef struct bbs_xfer_state {
  char scratch[XFER_SCRATCH_LEN];
} bbs_xfer_state_t;

static bbs_xfer_state_t *xfer;

static void bbs_xfer_dispatch(void);

void
bbs_xfer_set_op(const char *cmd)
{
  if(xfer == NULL || cmd == NULL) {
    return;
  }
  strncpy(xfer->scratch, cmd, XFER_SCRATCH_LEN - 1u);
  xfer->scratch[XFER_SCRATCH_LEN - 1u] = '\0';
  bbs_xfer_dispatch();
}

void
bbs_xfer_feed(unsigned char c)
{
  (void)c;
}

static unsigned char
xfer_dos_cmd(const char *cmd)
{
  unsigned char dev;

  dev = board.transfer_device;
  if(dev == 0u) {
    return 0u;
  }
  if(cbm_open(XFER_CMD, dev, XFER_CMD, cmd) != 0) {
    return 0u;
  }
  cbm_close(XFER_CMD);
  return 1u;
}

static unsigned char
xfer_cd_transfer(void)
{
  bbs_xfer_state_t *s = xfer;
  char *p;

  if(s == NULL) {
    return 0u;
  }
  cbm_close(XFER_CHN);

  strcpy(s->scratch, "CD:");
  strcat(s->scratch, board.transfer_prefix);
  p = s->scratch + strlen(s->scratch) - 1;
  if(p > s->scratch + 4 && *p == '/') {
    *p = 0;
  }
  if(s->scratch[5] >= 'a' && s->scratch[5] <= 'z') {
    s->scratch[5] = (char)(s->scratch[5] - 'a' + 'A');
  }
  if(xfer_dos_cmd(s->scratch) == 0u) {
    return 0u;
  }
  if(xfer_cwd[0] == 0) {
    return 1u;
  }
  strcpy(s->scratch, "CD:");
  strcat(s->scratch, xfer_cwd);
  p = s->scratch + strlen(s->scratch) - 1;
  if(p > s->scratch + 2 && *p == '/') {
    *p = 0;
  }
  return xfer_dos_cmd(s->scratch);
}

static unsigned char
xfer_name_ok(const char *s)
{
  unsigned char n = 0u;

  while(s[n] != 0 && n < 16u) {
    unsigned char c = (unsigned char)s[n];
    if(c == '/' || c == ':' || c == '"' || c < 0x20u) {
      return 0u;
    }
    ++n;
  }
  return (n > 0u && s[n] == 0) ? 1u : 0u;
}

static const char *
xfer_op_arg_after(const char *op, unsigned char skip)
{
  if(op == NULL) {
    return "";
  }
  op += skip;
  while(*op == ' ') {
    ++op;
  }
  return op;
}

static unsigned char
xfer_bank_cd(const char *path)
{
  unsigned char len;

  if(path[0] == 0 || xfer_name_ok(path) == 0u) {
    return 0u;
  }
  strcpy(xfer_cwd, path);
  len = (unsigned char)strlen(xfer_cwd);
  if(len > 0u && xfer_cwd[len - 1u] != '/') {
    strcat(xfer_cwd, "/");
  }
  return xfer_cd_transfer();
}

static unsigned char
xfer_bank_md(const char *path)
{
  bbs_xfer_state_t *s = xfer;

  if(s == NULL || path[0] == 0 || xfer_name_ok(path) == 0u) {
    return 0u;
  }
  strcpy(s->scratch, "MD:");
  strcat(s->scratch, path);
  return xfer_dos_cmd(s->scratch);
}

static unsigned int
xfer_buf_room(void)
{
  if(buf.used >= buf.size) {
    return 0u;
  }
  return (unsigned int)(buf.size - buf.used);
}

static void
xfer_dirlist_format(char *line, unsigned int blocks, const char *name)
{
  char *p = line;
  unsigned char col;
  unsigned int n = blocks;
  unsigned char digits = 0u;
  char tmp[6];

  if(n == 0u) {
    tmp[digits++] = '0';
  } else {
    while(n > 0u && digits < 6u) {
      tmp[digits++] = (char)('0' + (n % 10u));
      n /= 10u;
    }
  }
  for(col = 0u; col < (unsigned char)(6u - digits); ++col) {
    *p++ = ' ';
  }
  while(digits > 0u) {
    *p++ = tmp[--digits];
  }
  *p++ = ' ';
  *p++ = ' ';
  while(*name != 0) {
    *p++ = *name++;
  }
  *p = 0;
}

static void
xfer_dirlist_drain(void)
{
  if(xfer_buf_room() >= 64u) {
    return;
  }
  bbs_transport_poll_send();
}

static void
xfer_dirlist(void)
{
  struct cbm_dirent dirent;
  bbs_xfer_state_t *s = xfer;
  unsigned char prev_status;
  unsigned char dev;

  if(s == NULL || xfer_cd_transfer() == 0u) {
    return;
  }

  dev = board.transfer_device;
  cbm_close(XFER_CHN);
  if(cbm_opendir(XFER_CHN, dev, "$") != 0u) {
    shell_output_str(NULL, "\r\n$ failed\r\n", "");
    return;
  }

  prev_status = bbs_status.status;
  bbs_status.status = STATUS_DIRLIST;
  shell_output_str(NULL, "\r\nblocks  name\r\n", "");
  shell_output_str(NULL, "----------------------\r\n", "");

  while(cbm_readdir(XFER_CHN, &dirent) == 0u) {
    if(dirent.name[0] == 0) {
      continue;
    }
    xfer_dirlist_format(s->scratch, dirent.size, dirent.name);
    shell_output_str(NULL, s->scratch, "");
    xfer_dirlist_drain();
  }

  cbm_closedir(XFER_CHN);
  bbs_status.status = prev_status;
}

static void
bbs_xfer_dispatch(void)
{
  bbs_xfer_state_t *s = xfer;
  const char *op;
  const char *path;

  if(s == NULL || s->scratch[0] == 0) {
    return;
  }
  op = s->scratch;

  if(op[0] == (char)'c' && op[1] == (char)'d' &&
      (op[2] == 0 || op[2] == ' ')) {
    path = xfer_op_arg_after(op, 2u);
    if(xfer_bank_cd(path) != 0u) {
      strcpy(s->scratch, "\n\rnow: ");
      strcat(s->scratch, board.transfer_prefix);
      strcat(s->scratch, xfer_cwd);
      shell_output_str(NULL, s->scratch, "\n\r");
    } else {
      shell_output_str(NULL, "\n\rcd failed\n\r", "");
    }
    return;
  }

  if(op[0] == (char)'m' && op[1] == (char)'d' &&
      (op[2] == 0 || op[2] == ' ')) {
    path = xfer_op_arg_after(op, 2u);
    if(xfer_bank_md(path) != 0u) {
      shell_output_str(NULL, "\n\rok\n\r", "");
    } else {
      shell_output_str(NULL, "\n\rmd failed\n\r", "");
    }
    return;
  }

  if(op[0] == (char)'$' && op[1] == 0) {
    xfer_dirlist();
  }
}

/*---------------------------------------------------------------------------*/
PROCESS(bbs_xfer_process, "xfer");
SHELL_COMMAND(bbs_xfer_cd_command, "cd", "cd : change dir", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_md_command, "md", "md : make dir", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_ls_command, "$", "$ : disk directory", &bbs_xfer_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_xfer_process, ev, data)
{
  PROCESS_BEGIN();
  PROCESS_END();
}

unsigned char
bbs_xfer_init(void)
{
  static bbs_xfer_state_t xfer_state;

  xfer = &xfer_state;
  (void)xfer_cd_transfer();
  shell_register_command(&bbs_xfer_cd_command);
  shell_register_command(&bbs_xfer_md_command);
  shell_register_command(&bbs_xfer_ls_command);
  return 1u;
}

void
bbs_xfer_deinit(void)
{
  cbm_closedir(XFER_CHN);
  cbm_close(XFER_CHN);
  shell_unregister_command(&bbs_xfer_cd_command);
  shell_unregister_command(&bbs_xfer_md_command);
  shell_unregister_command(&bbs_xfer_ls_command);
  xfer = NULL;
}
