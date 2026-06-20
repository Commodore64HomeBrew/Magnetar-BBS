/* bbs-xfer-xmodem.c — d/u download upload + XMODEM (bank 5). cd/md/$ in bank 1. */
#pragma static-locals(off)
#pragma rodata-name("CODE")

#ifndef BBS_BANK_BUILD
#error "bbs-xfer-xmodem.c is linked only as bank overlay bbs-xfer-xmodem-bank.o"
#endif

#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-xfer-xmodem.h"
#include "bbs-xmodem.h"
#include "bbs-bank.h"
#include "bbs-bank-macros.h"
#include <cbm.h>
#include <string.h>
#ifdef BBS_SERIAL_TRANSPORT
#include <serial.h>
#endif

#define XFER_CHN  2u
#define XFER_CMD  15u
#define XFER_WAIT (CLOCK_SECOND * 3)
#define XFER_SCRATCH_LEN  40u

typedef struct bbs_xmodem_xfer_state {
  unsigned int rx_head, rx_tail;
  unsigned char *rx_base;
  unsigned int rx_size;
  char scratch[XFER_SCRATCH_LEN];
  char outc;
} bbs_xmodem_xfer_state_t;

static bbs_xmodem_xfer_state_t *xm;
unsigned char bbs_xmodem_inbyte;

static void bbs_xmodem_xfer_dispatch(void);

void
bbs_xmodem_xfer_set_op(const char *cmd)
{
  if(xm == NULL || cmd == NULL) {
    return;
  }
  strncpy(xm->scratch, cmd, XFER_SCRATCH_LEN - 1u);
  xm->scratch[XFER_SCRATCH_LEN - 1u] = '\0';
  bbs_xmodem_xfer_dispatch();
}

void
bbs_xmodem_xfer_feed(unsigned char c)
{
  bbs_xmodem_xfer_state_t *s = xm;
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
xm_rx_pop(unsigned char *c)
{
  bbs_xmodem_xfer_state_t *s = xm;

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
xm_flush_rx(void)
{
  unsigned char c;

  while(xm_rx_pop(&c) != 0u) {
  }
}

unsigned char
bbs_xmodem_poll(void)
{
  bbs_xmodem_xfer_state_t *s = xm;
  clock_time_t t0;
  unsigned char c;

  if(s == NULL) {
    return 0u;
  }
  t0 = clock_time();
  while((clock_time_t)(clock_time() - t0) < XFER_WAIT) {
    if(xm_rx_pop(&c) != 0u) {
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
  bbs_xmodem_xfer_state_t *s = xm;
  clock_time_t t0;

  if(s == NULL) {
    return;
  }
  s->outc = (char)c;
  (void)buf_append(&s->outc, 1);
#ifdef BBS_SERIAL_TRANSPORT
  bbs_serial_flush_outbound();
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
xm_msg_result(unsigned char code)
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
xm_dos_cmd(const char *cmd)
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
xm_cd_transfer(void)
{
  bbs_xmodem_xfer_state_t *s = xm;
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
  if(xm_dos_cmd(s->scratch) == 0u) {
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
  return xm_dos_cmd(s->scratch);
}

#define xm_flush_outbound() bbs_transport_flush_outbound()

void
bbs_xmodem_io_begin(void)
{
  if(xm == NULL) {
    return;
  }
  xm_flush_outbound();
  bbs_scr_layout_xfer();
  xm->rx_base = (unsigned char *)BBS_XFER_SCR_RX_BASE;
  xm->rx_size = BBS_XFER_SCR_RX_SIZE;
  xm->rx_head = xm->rx_tail = 0u;
  bbs_status.status = STATUS_XFER;
  xm_flush_rx();
}

void
bbs_xmodem_io_end(void)
{
  bbs_status.status = STATUS_LOCK;
  if(xm != NULL) {
    xm->rx_base = NULL;
  }
  bbs_scr_layout_output();
}

static void
xm_do_send(const char *path)
{
  unsigned char r;
  unsigned char dev;

  dev = board.transfer_device;
  if(xm_cd_transfer() == 0u) {
    shell_output_str(NULL, "\n\rfile open failed\n\r", "");
    return;
  }
  if(cbm_open(XFER_CHN, dev, XFER_CHN, path) != 0) {
    shell_output_str(NULL, "\n\rfile open failed\n\r", "");
    return;
  }
  shell_output_str(NULL, "\n\rXMODEM send - start receiver (CRC)\n\r", "");
  xm_flush_outbound();
  bbs_xmodem_io_begin();
  r = bbs_xmodem_send();
  cbm_close(XFER_CHN);
  bbs_xmodem_io_end();
  xm_msg_result(r);
  xm_flush_outbound();
}

static void
xm_do_recv(const char *path)
{
  unsigned char r;
  unsigned char dev;

  dev = board.transfer_device;
  shell_output_str(NULL, "\n\rstart upload\r\n", "");
  xm_flush_outbound();
  if(xm_cd_transfer() == 0u) {
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
  xm_msg_result(r);
  xm_flush_outbound();
}

static unsigned char
xm_name_ok(const char *s)
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
xm_op_arg_after(const char *op, unsigned char skip)
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

static void
xm_dispatch_file(const char *op, unsigned char skip, const char *usage,
    void (*run)(const char *))
{
  const char *path;

  if(xm == NULL) {
    return;
  }
  path = xm_op_arg_after(op, skip);
  if(path[0] == 0) {
    shell_output_str(NULL, (char *)usage, "");
    return;
  }
  if(xm_name_ok(path) == 0u) {
    shell_output_str(NULL, "\n\rinvalid name\n\r", "");
    return;
  }
  run(path);
}

static void
bbs_xmodem_xfer_dispatch(void)
{
  bbs_xmodem_xfer_state_t *s = xm;
  const char *op;

  if(s == NULL || s->scratch[0] == 0) {
    return;
  }
  op = s->scratch;

  if(op[0] == (char)'d' && (op[1] == 0 || op[1] == ' ')) {
    xm_dispatch_file(op, 1u, "\n\rusage: d filename\n\r", xm_do_send);
    return;
  }

  if(op[0] == (char)'u' && (op[1] == 0 || op[1] == ' ')) {
    xm_dispatch_file(op, 1u, "\n\rusage: u filename\n\r", xm_do_recv);
  }
}

/*---------------------------------------------------------------------------*/
PROCESS(bbs_xmodem_xfer_process, "xmodem");
SHELL_COMMAND(bbs_xmodem_dl_command, "d", "", &bbs_xmodem_xfer_process);
SHELL_COMMAND(bbs_xmodem_ul_command, "u", "", &bbs_xmodem_xfer_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_xmodem_xfer_process, ev, data)
{
  PROCESS_BEGIN();
  PROCESS_END();
}

unsigned char
bbs_xmodem_xfer_init(void)
{
  static bbs_xmodem_xfer_state_t xm_state;

  xm = &xm_state;
  xm->rx_head = xm->rx_tail = 0u;
  xm->rx_base = NULL;
  xm->rx_size = 0u;
  (void)xm_cd_transfer();
  shell_register_command(&bbs_xmodem_dl_command);
  shell_register_command(&bbs_xmodem_ul_command);
  return 1u;
}

void
bbs_xmodem_xfer_deinit(void)
{
  if(xm != NULL) {
    xm->rx_base = NULL;
    xm->rx_size = 0u;
  }
  shell_unregister_command(&bbs_xmodem_dl_command);
  shell_unregister_command(&bbs_xmodem_ul_command);
  xm = NULL;
}
