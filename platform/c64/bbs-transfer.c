/* bbs-transfer.c — transfer dir commands + XMODEM glue (core in bbs-xmodem.s) */
#pragma static-locals(off)
#ifdef BBS_BANK_BUILD
#pragma rodata-name("CODE")
#endif

#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-transfer.h"
#include "bbs-xmodem.h"
#include "bbs-telnetd.h"
#ifdef BBS_BANK_BUILD
#include "bbs-bank.h"
#elif defined(BBS_XFER_MODULE)
#include "bbs-modules.h"
#endif
#include <cbm.h>
#ifdef BBS_SERIAL_TRANSPORT
#include <serial.h>
#endif
#if !defined(BBS_XFER_MODULE) && !defined(BBS_BANK_BUILD)
#include <stdlib.h>
#endif
#include <string.h>

#if defined(BBS_BANK_BUILD)
#include "bbs-bank-macros.h"
#elif defined(BBS_XFER_MODULE)
static BBS_BOARD_REC *xfer_board;
static BBS_STATUS_REC *xfer_status;
static BBS_BUFFER *xfer_buf;
static int *xfer_shell_event_inputp;
static void (*xfer_shell_output_str)(struct shell_command *c, char *str1, char *str2);
static void (*xfer_shell_prompt)(char *prompt);
static void (*xfer_shell_register_command)(struct shell_command *c);
static void (*xfer_shell_unregister_command)(struct shell_command *c);
static void (*xfer_transport_poll)(void);
static int (*xfer_buf_append)(const char *data, int len);
static unsigned long (*xfer_clock_time)(void);
static void (*xfer_serial_flush_outbound)(void);

#define board (*xfer_board)
#define bbs_status (*xfer_status)
#define buf (*xfer_buf)
#define shell_event_input (*xfer_shell_event_inputp)
#define shell_output_str xfer_shell_output_str
#define shell_prompt xfer_shell_prompt
#define shell_register_command xfer_shell_register_command
#define shell_unregister_command xfer_shell_unregister_command
#define bbs_transport_poll xfer_transport_poll
#define buf_append xfer_buf_append
#define clock_time xfer_clock_time
#define bbs_serial_flush_outbound xfer_serial_flush_outbound
#else
extern BBS_BOARD_REC board;
extern BBS_STATUS_REC bbs_status;
extern BBS_BUFFER buf;
#endif

#define XFER_CHN  2u
#define XFER_CMD  15u
#define XFER_WAIT (CLOCK_SECOND * 3)

#if defined(BBS_BANK_BUILD)
typedef struct bbs_xfer_state {
  unsigned int rx_head, rx_tail;
  unsigned char *rx_base;
  unsigned int rx_size;
  char cwd[BBS_XFER_PATH_LEN];
  const char *op_line;
  char op_copy[TELNETD_CONF_LINELEN + 1];
  char pathbuf[BBS_FILE_PATH_BUFLEN];
  char line[40];
  char outc;
} bbs_xfer_state_t;
#else
typedef struct bbs_xfer_state {
  unsigned int rx_head, rx_tail;
  unsigned char *rx_base;
  unsigned int rx_size;
  char cwd[BBS_XFER_PATH_LEN];
  const char *op_line;
  char pathbuf[BBS_FILE_PATH_BUFLEN];
  char line[40];
  char cmd[24];
  char outc;
#ifdef BBS_SERIAL_TRANSPORT
  char ser_c;
#endif
} bbs_xfer_state_t;
#endif

static bbs_xfer_state_t *xfer;
unsigned char bbs_xmodem_inbyte;

#ifdef BBS_XFER_MODULE
unsigned char
bbs_xfer_bind(const bbs_module_ctx_t *ctx)
{
  if(ctx == NULL || ctx->bbsm_board == NULL || ctx->bbsm_status == NULL ||
      ctx->bbsm_buffer == NULL || ctx->bbsm_shell_event_input == NULL ||
      ctx->bbsm_shell_output_str == NULL ||
      ctx->bbsm_shell_prompt == NULL || ctx->bbsm_shell_register_command == NULL ||
      ctx->bbsm_shell_unregister_command == NULL || ctx->bbsm_transport_poll == NULL ||
      ctx->bbsm_buf_append == NULL || ctx->bbsm_clock_time == NULL) {
    return 0u;
  }
  xfer_board = (BBS_BOARD_REC *)ctx->bbsm_board;
  xfer_status = (BBS_STATUS_REC *)ctx->bbsm_status;
  xfer_buf = (BBS_BUFFER *)ctx->bbsm_buffer;
  xfer_shell_event_inputp = ctx->bbsm_shell_event_input;
  xfer_shell_output_str = ctx->bbsm_shell_output_str;
  xfer_shell_prompt = ctx->bbsm_shell_prompt;
  xfer_shell_register_command = ctx->bbsm_shell_register_command;
  xfer_shell_unregister_command = ctx->bbsm_shell_unregister_command;
  xfer_transport_poll = ctx->bbsm_transport_poll;
  xfer_buf_append = ctx->bbsm_buf_append;
  xfer_clock_time = ctx->bbsm_clock_time;
  xfer_serial_flush_outbound = ctx->bbsm_serial_flush_outbound;
  return 1u;
}
#endif

void
bbs_xfer_set_op(const char *cmd)
{
  if(xfer == NULL || cmd == NULL) {
    return;
  }
#ifdef BBS_BANK_BUILD
  strncpy(xfer->op_copy, cmd, TELNETD_CONF_LINELEN);
  xfer->op_copy[TELNETD_CONF_LINELEN] = '\0';
  xfer->op_line = xfer->op_copy;
#else
  xfer->op_line = cmd;
#endif
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
#if defined(BBS_SERIAL_TRANSPORT) && !defined(BBS_BANK_BUILD)
  if(xfer != NULL) {
    while(ser_get(&xfer->ser_c) == SER_ERR_OK) {
    }
  }
#endif
}

void
bbs_xmodem_io_begin(void)
{
  if(xfer == NULL) {
    return;
  }
  xfer->rx_base = buf.bufmem;
  xfer->rx_size = buf.size;
  buf.used = 0u;
  buf.head = 0u;
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
#if defined(BBS_SERIAL_TRANSPORT) && !defined(BBS_BANK_BUILD)
    if(ser_get(&s->ser_c) == SER_ERR_OK) {
      bbs_xmodem_inbyte = (unsigned char)s->ser_c;
      return 1u;
    }
#endif
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

  n = cbm_read(XFER_CHN, &bbs_xmodem_rbuf[2], 128);
  if(n < 0) {
    n = 0;
  }
  if((unsigned int)n < 128u) {
    for(p = (unsigned int)n; p < 128u; ++p) {
      bbs_xmodem_rbuf[2 + p] = 0u;
    }
    return 1u;
  }
  return 0u;
}

void
bbs_xmodem_write_block(void)
{
  (void)cbm_write(XFER_CHN, &bbs_xmodem_rbuf[2], 128);
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

static void
xfer_do_send(const char *path)
{
  unsigned char r;

  if(cbm_open(XFER_CHN, board.transfer_device, XFER_CHN, path) != 0) {
    shell_output_str(NULL, "\n\rfile open failed\n\r", "");
    return;
  }
  shell_output_str(NULL, "\n\rXMODEM send - start receiver (CRC)\n\r", "");
  bbs_xmodem_io_begin();
  r = bbs_xmodem_send();
  cbm_close(XFER_CHN);
  bbs_xmodem_io_end();
  xfer_msg_result(r);
}

static void
xfer_do_recv(const char *path)
{
  unsigned char r;

  if(cbm_open(XFER_CHN, board.transfer_device, CBM_WRITE, path) != 0) {
    shell_output_str(NULL, "\n\rfile create failed\n\r", "");
    return;
  }
  shell_output_str(NULL, "\n\rXMODEM recv - send file (CRC)\n\r", "");
  bbs_xmodem_io_begin();
  r = bbs_xmodem_recv();
  cbm_close(XFER_CHN);
  bbs_xmodem_io_end();
  xfer_msg_result(r);
}

static void
xfer_path_dir(char *out)
{
  if(xfer == NULL) {
    out[0] = 0;
    return;
  }
  strcpy(out, board.transfer_prefix);
  strcat(out, xfer->cwd);
  strcat(out, "$");
}

static void
xfer_path_file(char *out, const char *name)
{
  if(xfer == NULL) {
    out[0] = 0;
    return;
  }
  strcpy(out, board.transfer_prefix);
  strcat(out, xfer->cwd);
  strcat(out, name);
}

static unsigned char
xfer_dos_cmd(const char *cmd)
{
  if(cbm_open(XFER_CMD, board.transfer_device, XFER_CMD, cmd) != 0) {
    return 0u;
  }
  cbm_close(XFER_CMD);
  return 1u;
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

#if defined(BBS_BANK_BUILD)

static unsigned char
xfer_bank_cd(const char *path)
{
  bbs_xfer_state_t *s = xfer;
  unsigned char len;

  if(s == NULL || path[0] == 0 || xfer_name_ok(path) == 0u) {
    return 0u;
  }
  strcpy(s->pathbuf, "CD:");
  strcat(s->pathbuf, path);
  if(xfer_dos_cmd(s->pathbuf) == 0u) {
    return 0u;
  }
  strcpy(s->cwd, path);
  len = (unsigned char)strlen(s->cwd);
  if(len > 0u && s->cwd[len - 1u] != '/') {
    strcat(s->cwd, "/");
  }
  return 1u;
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
#endif /* BBS_BANK_BUILD */

static unsigned char
xfer_dir_is_entry(const char *line, unsigned char len)
{
  unsigned char i;

  if(len < 4u) {
    return 0u;
  }
  for(i = 0u; i + 3u < len; ++i) {
    if(line[i] == 'D' && line[i + 1] == 'I' && line[i + 2] == 'R') {
      return 0u;
    }
    if(line[i] == '<' && line[i + 1] == ' ' && line[i + 2] == 'D') {
      return 0u;
    }
  }
  return 1u;
}

static void
xfer_list_print(void)
{
  bbs_xfer_state_t *s = xfer;
  unsigned char li, c, in_q, i, j;
  unsigned char listed;
  int n;
  char name[BBS_XFER_NAME_LEN];

  if(s == NULL) {
    return;
  }

  listed = 0u;
  xfer_path_dir(s->pathbuf);
  if(cbm_open(XFER_CHN, board.transfer_device, XFER_CHN, s->pathbuf) != 0) {
    shell_output_str(NULL, "\n\r (list failed)\n\r", "");
    return;
  }

  shell_output_str(NULL, "\n\r", "");
  li = 0u;
  in_q = 0u;
  s->line[0] = 0;
  while(1) {
    n = cbm_read(XFER_CHN, &c, 1);
    if(n <= 0) {
      break;
    }
    if(c == 0x0du || c == 0x0au) {
      if(li > 0u) {
        s->line[li] = 0;
        if(in_q == 2u && xfer_dir_is_entry(s->line, li) != 0u) {
          i = 0u;
          while(s->line[i] != '"' && s->line[i] != 0) {
            ++i;
          }
          if(s->line[i] == '"') {
            j = 0u;
            ++i;
            while(s->line[i] != '"' && s->line[i] != 0 && j < 16u) {
              name[j++] = s->line[i++];
            }
            name[j] = 0;
            if(j > 0u) {
              shell_output_str(NULL, name, "\n\r");
              listed = 1u;
            }
          }
        }
      }
      li = 0u;
      in_q = 0u;
    } else if(li < 39u) {
      s->line[li++] = (char)c;
      if(c == '"') {
        ++in_q;
      }
    }
  }
  cbm_close(XFER_CHN);
  if(listed == 0u) {
    shell_output_str(NULL, " (no files)\n\r", "");
  }
}

#if !defined(BBS_BANK_BUILD)
static unsigned char
xfer_cd_local(const char *arg)
{
  bbs_xfer_state_t *s = xfer;
  unsigned char len, i;

  if(s == NULL) {
    return 0u;
  }
  if(arg[0] == 0) {
    return 1u;
  }
  if(strcmp(arg, "..") == 0 || arg[0] == '_' || strcmp(arg, "^") == 0) {
    len = (unsigned char)strlen(s->cwd);
    if(len == 0u) {
      return 1u;
    }
    if(len > 0u && s->cwd[len - 1u] == '/') {
      s->cwd[len - 1u] = 0;
      len = (unsigned char)strlen(s->cwd);
    }
    for(i = len; i > 0u; --i) {
      if(s->cwd[i - 1u] == '/') {
        s->cwd[i] = 0;
        break;
      }
    }
    return xfer_dos_cmd("CD:_");
  }
  len = (unsigned char)strlen(s->cwd);
  if(len + strlen(arg) + 2u >= BBS_XFER_PATH_LEN) {
    return 0u;
  }
  if(len > 0u && s->cwd[len - 1u] != '/') {
    strcat(s->cwd, "/");
  }
  strcat(s->cwd, arg);
  strcpy(s->cmd, "CD:");
  strcat(s->cmd, arg);
  return xfer_dos_cmd(s->cmd);
}
#endif /* !BBS_BANK_BUILD */

/*---------------------------------------------------------------------------*/
PROCESS(bbs_xfer_process, "xfer");
SHELL_COMMAND(bbs_xfer_list_command, "$", "$ : list dir", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_dl_command, "d", "d : download file", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_ul_command, "u", "u : upload file", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_cd_command, "cd", "cd : change dir", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_md_command, "md", "md : make dir", &bbs_xfer_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_xfer_process, ev, data)
{
  bbs_xfer_state_t *s = xfer;
  struct shell_input *input;
  const char *op;
  const char *path;

  PROCESS_BEGIN();
  if(s == NULL) {
    PROCESS_EXIT();
  }
  op = s->op_line;
#if defined(BBS_BANK_BUILD)
  shell_output_str(NULL, PETSCII_LOWER, PETSCII_WHITE);

  if(op != NULL && op[0] == (char)'$' && op[1] == 0) {
    xfer_list_print();
    PROCESS_EXIT();
  }

  if(op != NULL && op[0] == (char)'d' &&
      (op[1] == 0 || op[1] == ' ')) {
    path = xfer_op_arg_after(op, 1u);
    if(path[0] == 0 || xfer_name_ok(path) == 0u) {
      shell_output_str(NULL, "\n\rinvalid name\n\r", "");
    } else {
      xfer_path_file(s->pathbuf, path);
      xfer_do_send(s->pathbuf);
    }
    PROCESS_EXIT();
  }

  if(op != NULL && op[0] == (char)'u' && op[1] == 0) {
    shell_prompt("\n\rfile name (max 16): ");
    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
    input = data;
    if(xfer_name_ok(input->data1) != 0u) {
      xfer_path_file(s->pathbuf, input->data1);
      xfer_do_recv(s->pathbuf);
    } else {
      shell_output_str(NULL, "\n\rinvalid name\n\r", "");
    }
    PROCESS_EXIT();
  }

  if(op != NULL && op[0] == (char)'c' && op[1] == (char)'d' &&
      (op[2] == 0 || op[2] == ' ')) {
    path = xfer_op_arg_after(op, 2u);
    if(xfer_bank_cd(path) != 0u) {
      strcpy(s->pathbuf, "\n\rnow: ");
      strcat(s->pathbuf, board.transfer_prefix);
      strcat(s->pathbuf, s->cwd);
      strcat(s->pathbuf, "\n\r");
      shell_output_str(NULL, s->pathbuf, "");
    } else {
      shell_output_str(NULL, "\n\rcd failed\n\r", "");
    }
    PROCESS_EXIT();
  }

  if(op != NULL && op[0] == (char)'m' && op[1] == (char)'d' &&
      (op[2] == 0 || op[2] == ' ')) {
    path = xfer_op_arg_after(op, 2u);
    if(xfer_bank_md(path) != 0u) {
      shell_output_str(NULL, "\n\rok\n\r", "");
    } else {
      shell_output_str(NULL, "\n\rmd failed\n\r", "");
    }
    PROCESS_EXIT();
  }
#else
  shell_output_str(NULL, PETSCII_LOWER, PETSCII_WHITE);

  if(op != NULL && op[0] == (char)'$') {
    xfer_list_print();
    PROCESS_EXIT();
  }

  if(op != NULL && op[0] == (char)'d' &&
      (op[1] == 0 || op[1] == ' ')) {
    path = xfer_op_arg_after(op, 1u);
    if(path[0] == 0 || xfer_name_ok(path) == 0u) {
      shell_output_str(NULL, "\n\rinvalid name\n\r", "");
    } else {
      xfer_path_file(s->pathbuf, path);
      xfer_do_send(s->pathbuf);
    }
    PROCESS_EXIT();
  }

  if(op != NULL && op[0] == (char)'u' && op[1] == 0) {
    shell_prompt("\n\rfile name (max 16): ");
    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
    input = data;
    if(xfer_name_ok(input->data1) != 0u) {
      xfer_path_file(s->pathbuf, input->data1);
      xfer_do_recv(s->pathbuf);
    } else {
      shell_output_str(NULL, "\n\rinvalid name\n\r", "");
    }
    PROCESS_EXIT();
  }

  if(board.dir_boost == 1u && op != NULL && op[0] == (char)'c' && op[1] == (char)'d' && op[2] == 0) {
    shell_prompt("\n\rdirectory: ");
    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
    input = data;
    if(xfer_cd_local(input->data1) == 0u) {
      shell_output_str(NULL, "\n\rcd failed\n\r", "");
    } else {
      strcpy(s->pathbuf, "\n\rnow: ");
      strcat(s->pathbuf, board.transfer_prefix);
      strcat(s->pathbuf, s->cwd);
      strcat(s->pathbuf, "\n\r");
      shell_output_str(NULL, s->pathbuf, "");
    }
    PROCESS_EXIT();
  }

  if(board.dir_boost == 1u && op != NULL && op[0] == (char)'m' && op[1] == (char)'d' && op[2] == 0) {
    shell_prompt("\n\rnew directory: ");
    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
    input = data;
    if(xfer_name_ok(input->data1) != 0u) {
      strcpy(s->cmd, "MD:");
      strcat(s->cmd, input->data1);
      if(xfer_dos_cmd(s->cmd) == 0u) {
        shell_output_str(NULL, "\n\rmd failed\n\r", "");
      } else {
        shell_output_str(NULL, "\n\rok\n\r", "");
      }
    } else {
      shell_output_str(NULL, "\n\rinvalid name\n\r", "");
    }
    PROCESS_EXIT();
  }
#endif /* BBS_BANK_BUILD */

  PROCESS_END();
}

unsigned char
bbs_xfer_init(void)
{
#if defined(BBS_XFER_MODULE) || defined(BBS_BANK_BUILD)
  static bbs_xfer_state_t module_state;
  xfer = &module_state;
#else
  if(xfer == NULL) {
    xfer = (bbs_xfer_state_t *)malloc(sizeof(*xfer));
    if(xfer == NULL) {
      return 0u;
    }
  }
#endif
  xfer->cwd[0] = 0;
  xfer->op_line = NULL;
  xfer->rx_head = xfer->rx_tail = 0u;
  xfer->rx_base = NULL;
  xfer->rx_size = 0u;
  shell_register_command(&bbs_xfer_list_command);
  shell_register_command(&bbs_xfer_dl_command);
  shell_register_command(&bbs_xfer_ul_command);
#if defined(BBS_BANK_BUILD)
  shell_register_command(&bbs_xfer_cd_command);
  shell_register_command(&bbs_xfer_md_command);
#else
  if(board.dir_boost == 1u) {
    shell_register_command(&bbs_xfer_cd_command);
    shell_register_command(&bbs_xfer_md_command);
  }
#endif
  return 1u;
}

void
bbs_xfer_deinit(void)
{
  if(xfer != NULL) {
    xfer->op_line = NULL;
    xfer->rx_base = NULL;
    xfer->rx_size = 0u;
  }
  shell_unregister_command(&bbs_xfer_list_command);
  shell_unregister_command(&bbs_xfer_dl_command);
  shell_unregister_command(&bbs_xfer_ul_command);
  shell_unregister_command(&bbs_xfer_cd_command);
  shell_unregister_command(&bbs_xfer_md_command);
#if !defined(BBS_XFER_MODULE) && !defined(BBS_BANK_BUILD)
  if(xfer != NULL) {
    free(xfer);
  }
#endif
  xfer = NULL;
}
