/* bbs-transfer.c — xfer commands + XMODEM (bank 1 overlay only; core in bbs-xmodem.S). */
#pragma static-locals(off)
#pragma rodata-name("CODE")

#ifndef BBS_BANK_BUILD
#error "bbs-transfer.c is linked only as bank overlay bbs-transfer-bank.o"
#endif

#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-transfer.h"
#include "bbs-xmodem.h"
#include "bbs-bank.h"
#include "bbs-bank-macros.h"
#include <cbm.h>
#ifdef BBS_SERIAL_TRANSPORT
#include <serial.h>
#endif
#include <string.h>

#define XFER_CHN  2u
#define XFER_CMD  15u
#define XFER_WAIT (CLOCK_SECOND * 3)

/* Longest xfer op: "cd " + 16-char name + NUL */
#define BBS_XFER_OP_COPY_LEN  24u

typedef struct bbs_xfer_state {
  unsigned int rx_head, rx_tail;
  unsigned char *rx_base;
  unsigned int rx_size;
  char cwd[BBS_XFER_PATH_LEN];
  char op_copy[BBS_XFER_OP_COPY_LEN];
  char pathbuf[BBS_FILE_PATH_BUFLEN];
  char outc;
} bbs_xfer_state_t;

static bbs_xfer_state_t *xfer;
unsigned char bbs_xmodem_inbyte;

static void bbs_xfer_dispatch(void);

void
bbs_xfer_set_op(const char *cmd)
{
  if(xfer == NULL || cmd == NULL) {
    return;
  }
  strncpy(xfer->op_copy, cmd, BBS_XFER_OP_COPY_LEN - 1u);
  xfer->op_copy[BBS_XFER_OP_COPY_LEN - 1u] = '\0';
  bbs_xfer_dispatch();
}

void
bbs_xfer_feed(unsigned char c)
{
  bbs_xfer_state_t *s = xfer;
  unsigned int n;

  if(s == NULL || s->rx_base == NULL) {
    return;
  }
  n = s->rx_head + 1u;
  if(n >= s->rx_size) {
    n = 0u;
  }
  if(n != s->rx_tail) {
    s->rx_base[s->rx_head] = c;
    s->rx_head = n;
  }
}

static unsigned char
xfer_rx_pop(unsigned char *c)
{
  bbs_xfer_state_t *s = xfer;

  if(s == NULL || s->rx_tail == s->rx_head) {
    return 0u;
  }
  *c = s->rx_base[s->rx_tail];
  ++s->rx_tail;
  if(s->rx_tail >= s->rx_size) {
    s->rx_tail = 0u;
  }
  return 1u;
}

static void
xfer_flush_rx(void)
{
  unsigned char c;

  while(xfer_rx_pop(&c) != 0u) {
  }
}

unsigned char
bbs_xmodem_poll(void)
{
  bbs_xfer_state_t *s = xfer;
  clock_time_t t0;
  unsigned char c;

  if(s == NULL) {
    return 0u;
  }
  t0 = clock_time();
  while((clock_time_t)(clock_time() - t0) < XFER_WAIT) {
    if(xfer_rx_pop(&c) != 0u) {
      bbs_xmodem_inbyte = c;
      return 1u;
    }
    bbs_transport_poll();
  }
  return 0u;
}

void
bbs_xmodem_putc(unsigned char c)
{
  bbs_xfer_state_t *s = xfer;
  clock_time_t t0;

  if(s == NULL) {
    return;
  }
  s->outc = (char)c;
  (void)buf_append(&s->outc, 1);
#ifdef BBS_SERIAL_TRANSPORT
  if(bbs_serial_flush_outbound != NULL) {
    bbs_serial_flush_outbound();
  }
#endif
  t0 = clock_time();
  while(buf.used != 0u &&
        (clock_time_t)(clock_time() - t0) < (XFER_WAIT * 4u)) {
    bbs_transport_poll();
  }
}

unsigned char
bbs_xmodem_read_block(void)
{
  int n;
  unsigned int p;

  n = cbm_read(XFER_CHN, &BBS_XMODEM_RBUF[2], 128);
  if(n < 0) {
    n = 0;
  }
  if((unsigned int)n < 128u) {
    for(p = (unsigned int)n; p < 128u; ++p) {
      BBS_XMODEM_RBUF[2 + p] = 0u;
    }
    return 1u;
  }
  return 0u;
}

void
bbs_xmodem_write_block(void)
{
  (void)cbm_write(XFER_CHN, &BBS_XMODEM_RBUF[2], 128);
}

static void
xfer_msg_result(unsigned char code)
{
  if(code == 0u) {
    shell_output_str(NULL, "\n\rtransfer ok\n\r", "");
  } else if(code == 0xfeu) {
    shell_output_str(NULL, "\n\raborted\n\r", "");
  } else {
    shell_output_str(NULL, "\n\rtransfer error\n\r", "");
  }
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

/* SD2IEC: CD to transfer partition then virtual subdir (OPEN uses cwd-relative names). */
static unsigned char
xfer_cd_transfer(void)
{
  bbs_xfer_state_t *s = xfer;
  char *p;

  if(s == NULL) {
    return 0u;
  }
  cbm_close(XFER_CHN);

  /* transfer_prefix "//t/" -> "CD://T" on cmd channel 15 */
  strcpy(s->pathbuf, "CD:");
  strcat(s->pathbuf, board.transfer_prefix);
  p = s->pathbuf + strlen(s->pathbuf) - 1;
  if(p > s->pathbuf + 4 && *p == '/') {
    *p = 0;
  }
  if(s->pathbuf[5] >= 'a' && s->pathbuf[5] <= 'z') {
    s->pathbuf[5] = (char)(s->pathbuf[5] - 'a' + 'A');
  }
  if(xfer_dos_cmd(s->pathbuf) == 0u) {
    return 0u;
  }
  if(s->cwd[0] == 0) {
    return 1u;
  }
  strcpy(s->pathbuf, "CD:");
  strcat(s->pathbuf, s->cwd);
  p = s->pathbuf + strlen(s->pathbuf) - 1;
  if(p > s->pathbuf + 2 && *p == '/') {
    *p = 0;
  }
  return xfer_dos_cmd(s->pathbuf);
}

#define xfer_flush_outbound() bbs_transport_flush_outbound()

void
bbs_xmodem_io_begin(void)
{
  if(xfer == NULL) {
    return;
  }
  /* Exclusive screen RAM: leave 1 KiB output layout, use xfer partition only now. */
  xfer_flush_outbound();
  bbs_scr_layout_xfer();
  xfer->rx_base = (unsigned char *)BBS_XFER_SCR_RX_BASE;
  xfer->rx_size = BBS_XFER_SCR_RX_SIZE;
  xfer->rx_head = xfer->rx_tail = 0u;
  bbs_status.status = STATUS_XFER;
  xfer_flush_rx();
}

void
bbs_xmodem_io_end(void)
{
  bbs_status.status = STATUS_LOCK;
  if(xfer != NULL) {
    xfer->rx_base = NULL;
  }
  bbs_scr_layout_output();
}

static void
xfer_do_send(const char *path)
{
  unsigned char r;
  unsigned char dev;

  dev = board.transfer_device;
  if(xfer_cd_transfer() == 0u) {
    shell_output_str(NULL, "\n\rfile open failed\n\r", "");
    return;
  }
  if(cbm_open(XFER_CHN, dev, XFER_CHN, path) != 0) {
    shell_output_str(NULL, "\n\rfile open failed\n\r", "");
    return;
  }
  shell_output_str(NULL, "\n\rXMODEM send - start receiver (CRC)\n\r", "");
  xfer_flush_outbound();
  bbs_xmodem_io_begin();
  r = bbs_xmodem_send();
  cbm_close(XFER_CHN);
  bbs_xmodem_io_end();
  xfer_msg_result(r);
  xfer_flush_outbound();
}

static void
xfer_do_recv(const char *path)
{
  unsigned char r;
  unsigned char dev;

  dev = board.transfer_device;
  shell_output_str(NULL, "\n\rstart upload\r\n", "");
  xfer_flush_outbound();
  if(xfer_cd_transfer() == 0u) {
    shell_output_str(NULL, "\n\rfile create failed\n\r", "");
    return;
  }
  if(cbm_open(XFER_CHN, dev, CBM_WRITE, path) != 0) {
    shell_output_str(NULL, "\n\rfile create failed\n\r", "");
    return;
  }
  bbs_xmodem_io_begin();
  r = bbs_xmodem_recv();
  cbm_close(XFER_CHN);
  bbs_xmodem_io_end();
  xfer_msg_result(r);
  xfer_flush_outbound();
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
  bbs_xfer_state_t *s = xfer;
  unsigned char len;

  if(s == NULL || path[0] == 0 || xfer_name_ok(path) == 0u) {
    return 0u;
  }
  strcpy(s->cwd, path);
  len = (unsigned char)strlen(s->cwd);
  if(len > 0u && s->cwd[len - 1u] != '/') {
    strcat(s->cwd, "/");
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
  strcpy(s->pathbuf, "MD:");
  strcat(s->pathbuf, path);
  return xfer_dos_cmd(s->pathbuf);
}

static void
xfer_dirlist(void)
{
  bbs_xfer_state_t *s = xfer;
  unsigned char listed = 0u;
  unsigned char li = 0u;
  unsigned char c;
  unsigned char dev;
  int nr;

  if(s == NULL || xfer_cd_transfer() == 0u) {
    shell_output_str(NULL, "\n\r (list failed)\n\r", "");
    return;
  }
  dev = board.transfer_device;
  cbm_close(XFER_CHN);
  if(cbm_open(XFER_CHN, dev, 0u, "$") != 0) {
    cbm_close(XFER_CHN);
    if(cbm_open(XFER_CHN, dev, XFER_CHN, "$") != 0) {
      shell_output_str(NULL, "\n\r (list failed)\n\r", "");
      return;
    }
  }
  shell_output_str(NULL, "\n\r", "");
  for(;;) {
    nr = cbm_read(XFER_CHN, &c, 1);
    if(nr <= 0) {
      break;
    }
    if(c == 0x0du || c == 0x0au) {
      if(li > 0u && s->pathbuf[0] != 'B') {
        s->pathbuf[li] = 0;
        shell_output_str(NULL, s->pathbuf, "");
        listed = 1u;
        xfer_flush_outbound();
      }
      li = 0u;
    } else if(li < (BBS_FILE_PATH_BUFLEN - 1u)) {
      s->pathbuf[li++] = (char)c;
    }
  }
  cbm_close(XFER_CHN);
  if(listed == 0u) {
    shell_output_str(NULL, " (empty)\n\r", "");
  }
}

static void
xfer_dispatch_file(const char *op, unsigned char skip, const char *usage,
    void (*run)(const char *))
{
  bbs_xfer_state_t *s = xfer;
  const char *path;

  if(s == NULL) {
    return;
  }
  path = xfer_op_arg_after(op, skip);
  if(path[0] == 0) {
    shell_output_str(NULL, (char *)usage, "");
    return;
  }
  if(xfer_name_ok(path) == 0u) {
    shell_output_str(NULL, "\n\rinvalid name\n\r", "");
    return;
  }
  strcpy(s->pathbuf, path);
  run(s->pathbuf);
}

static void
bbs_xfer_dispatch(void)
{
  bbs_xfer_state_t *s = xfer;
  const char *op;
  const char *path;

  if(s == NULL || s->op_copy[0] == 0) {
    return;
  }
  op = s->op_copy;

  if(op[0] == (char)'d' && (op[1] == 0 || op[1] == ' ')) {
    xfer_dispatch_file(op, 1u, "\n\rusage: d filename\n\r", xfer_do_send);
    return;
  }

  if(op[0] == (char)'u' && (op[1] == 0 || op[1] == ' ')) {
    xfer_dispatch_file(op, 1u, "\n\rusage: u filename\n\r", xfer_do_recv);
    return;
  }

  if(op[0] == (char)'c' && op[1] == (char)'d' &&
      (op[2] == 0 || op[2] == ' ')) {
    path = xfer_op_arg_after(op, 2u);
    if(xfer_bank_cd(path) != 0u) {
      strcpy(s->pathbuf, "\n\rnow: ");
      strcat(s->pathbuf, board.transfer_prefix);
      strcat(s->pathbuf, s->cwd);
      shell_output_str(NULL, s->pathbuf, "\n\r");
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
SHELL_COMMAND(bbs_xfer_dl_command, "d", "d : download (d name)", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_ul_command, "u", "u : upload (u name)", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_cd_command, "cd", "cd : change dir", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_md_command, "md", "md : make dir", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_ls_command, "$", "$ : list dir", &bbs_xfer_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_xfer_process, ev, data)
{
  PROCESS_BEGIN();
  /* Commands run via bbs_bank_set_op -> bbs_xfer_set_op -> bbs_xfer_dispatch. */
  PROCESS_END();
}

unsigned char
bbs_xfer_init(void)
{
  static bbs_xfer_state_t xfer_state;

  xfer = &xfer_state;
  xfer->cwd[0] = 0;
  xfer->rx_head = xfer->rx_tail = 0u;
  xfer->rx_base = NULL;
  xfer->rx_size = 0u;
  (void)xfer_cd_transfer();
  shell_register_command(&bbs_xfer_dl_command);
  shell_register_command(&bbs_xfer_ul_command);
  shell_register_command(&bbs_xfer_cd_command);
  shell_register_command(&bbs_xfer_md_command);
  shell_register_command(&bbs_xfer_ls_command);
  return 1u;
}

void
bbs_xfer_deinit(void)
{
  if(xfer != NULL) {
    xfer->rx_base = NULL;
    xfer->rx_size = 0u;
  }
  shell_unregister_command(&bbs_xfer_dl_command);
  shell_unregister_command(&bbs_xfer_ul_command);
  shell_unregister_command(&bbs_xfer_cd_command);
  shell_unregister_command(&bbs_xfer_md_command);
  shell_unregister_command(&bbs_xfer_ls_command);
  xfer = NULL;
}
