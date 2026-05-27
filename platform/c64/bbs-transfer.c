/* bbs-transfer.c — transfer dir commands + XMODEM glue (core in bbs-xmodem.s) */
#pragma static-locals(off)

#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-transfer.h"
#include "bbs-xmodem.h"
#include "bbs-telnetd.h"
#include <cbm.h>
#ifdef BBS_SERIAL_TRANSPORT
#include <serial.h>
#endif
#include <stdlib.h>
#include <string.h>

extern BBS_BOARD_REC board;
extern BBS_STATUS_REC bbs_status;

#define XFER_CHN  2u
#define XFER_CMD  15u
#define XFER_WAIT (CLOCK_SECOND * 3)
typedef struct bbs_xfer_state {
  unsigned int rx_head, rx_tail;
  unsigned char *rx_base;
  unsigned int rx_size;
  char cwd[BBS_XFER_PATH_LEN];
  unsigned char nfiles;
  const char *op_line;
  char pick_name[BBS_XFER_NAME_LEN];
  char tab[BBS_XFER_MAX_FILES][BBS_XFER_NAME_LEN];
  char pathbuf[BBS_FILE_PATH_BUFLEN];
  char line[40];
  char sort_tmp[BBS_XFER_NAME_LEN];
  char list_ln[24];
  char cmd[24];
  char outc;
#ifdef BBS_SERIAL_TRANSPORT
  char ser_c;
#endif
} bbs_xfer_state_t;
static bbs_xfer_state_t *xfer;
unsigned char bbs_xmodem_inbyte;
extern BBS_BUFFER buf;

static unsigned short xfer_atou(const char *s)
{
  unsigned short v = 0u;
  while(*s >= '0' && *s <= '9') {
    v = (unsigned short)(v * 10u + (unsigned short)(*s - '0'));
    ++s;
  }
  return v;
}

void bbs_xfer_set_op(const char *cmd)
{
  if(xfer != NULL) {
    xfer->op_line = cmd;
  }
}

void bbs_xfer_feed(unsigned char c)
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

static unsigned char xfer_rx_pop(unsigned char *c)
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

static void xfer_flush_rx(void)
{
  unsigned char c;
  while(xfer_rx_pop(&c) != 0u) {
  }
#ifdef BBS_SERIAL_TRANSPORT
  if(xfer != NULL) {
    while(ser_get(&xfer->ser_c) == SER_ERR_OK) {
    }
  }
#endif
}

void bbs_xmodem_io_begin(void)
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

void bbs_xmodem_io_end(void)
{
  bbs_status.status = STATUS_LOCK;
  if(xfer != NULL) {
    xfer->rx_base = NULL;
  }
}

unsigned char bbs_xmodem_poll(void)
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
#ifdef BBS_SERIAL_TRANSPORT
    if(ser_get(&s->ser_c) == SER_ERR_OK) {
      bbs_xmodem_inbyte = (unsigned char)s->ser_c;
      return 1u;
    }
#endif
    bbs_transport_poll();
  }
  return 0u;
}

void bbs_xmodem_putc(unsigned char c)
{
  bbs_xfer_state_t *s = xfer;
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

unsigned char bbs_xmodem_read_block(void)
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

void bbs_xmodem_write_block(void)
{
  (void)cbm_write(XFER_CHN, &bbs_xmodem_rbuf[2], 128);
}

static void xfer_msg_result(unsigned char code)
{
  if(code == 0u) {
    shell_output_str(NULL, "\n\rtransfer ok\n\r", "");
  } else if(code == 0xfeu) {
    shell_output_str(NULL, "\n\raborted\n\r", "");
  } else {
    shell_output_str(NULL, "\n\rtransfer error\n\r", "");
  }
}

static int xfer_do_send(const char *path)
{
  unsigned char r;
  if(cbm_open(XFER_CHN, board.transfer_device, XFER_CHN, path) != 0) {
    shell_output_str(NULL, "\n\rfile open failed\n\r", "");
    return -1;
  }
  shell_output_str(NULL, "\n\rXMODEM send - start receiver (CRC)\n\r", "");
  bbs_xmodem_io_begin();
  r = bbs_xmodem_send();
  cbm_close(XFER_CHN);
  bbs_xmodem_io_end();
  xfer_msg_result(r);
  return (r == 0u) ? 0 : -1;
}

static int xfer_do_recv(const char *path)
{
  unsigned char r;
  if(cbm_open(XFER_CHN, board.transfer_device, CBM_WRITE, path) != 0) {
    shell_output_str(NULL, "\n\rfile create failed\n\r", "");
    return -1;
  }
  shell_output_str(NULL, "\n\rXMODEM recv - send file (CRC)\n\r", "");
  bbs_xmodem_io_begin();
  r = bbs_xmodem_recv();
  cbm_close(XFER_CHN);
  bbs_xmodem_io_end();
  xfer_msg_result(r);
  return (r == 0u) ? 0 : -1;
}

static void xfer_path_dir(char *out)
{
  if(xfer == NULL) {
    out[0] = 0;
    return;
  }
  strcpy(out, board.transfer_prefix);
  strcat(out, xfer->cwd);
  strcat(out, "$");
}

static void xfer_path_file(char *out, const char *name)
{
  if(xfer == NULL) {
    out[0] = 0;
    return;
  }
  strcpy(out, board.transfer_prefix);
  strcat(out, xfer->cwd);
  strcat(out, name);
}

static unsigned char xfer_dos_cmd(const char *cmd)
{
  if(cbm_open(XFER_CMD, board.transfer_device, XFER_CMD, cmd) != 0) {
    return 0u;
  }
  cbm_close(XFER_CMD);
  return 1u;
}

static unsigned char xfer_name_ok(const char *s)
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

static void xfer_sort_names(char names[][BBS_XFER_NAME_LEN], unsigned char n)
{
  bbs_xfer_state_t *s = xfer;
  unsigned char i, j;
  if(s == NULL) {
    return;
  }
  for(i = 0u; i < n; ++i) {
    for(j = (unsigned char)(i + 1u); j < n; ++j) {
      if(strcmp(names[i], names[j]) > 0) {
        strcpy(s->sort_tmp, names[i]);
        strcpy(names[i], names[j]);
        strcpy(names[j], s->sort_tmp);
      }
    }
  }
}

static unsigned char xfer_dir_is_entry(const char *line, unsigned char len)
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

static void xfer_scan_dir(char names[][BBS_XFER_NAME_LEN])
{
  bbs_xfer_state_t *s = xfer;
  unsigned char li, c;
  unsigned char in_q;
  int n;
  if(s == NULL) {
    return;
  }

  s->nfiles = 0u;
  xfer_path_dir(s->pathbuf);
  if(cbm_open(XFER_CHN, board.transfer_device, XFER_CHN, s->pathbuf) != 0) {
    return;
  }
  li = 0u;
  in_q = 0u;
  s->line[0] = 0;
  while(s->nfiles < BBS_XFER_MAX_FILES) {
    n = cbm_read(XFER_CHN, &c, 1);
    if(n <= 0) {
      break;
    }
    if(c == 0x0du || c == 0x0au) {
      if(li > 0u) {
        s->line[li] = 0;
        if(in_q == 2u && xfer_dir_is_entry(s->line, li) != 0u) {
          unsigned char i = 0u;
          while(s->line[i] != '"' && s->line[i] != 0) {
            ++i;
          }
          if(s->line[i] == '"') {
            unsigned char j = 0u;
            ++i;
            while(s->line[i] != '"' && s->line[i] != 0 && j < 16u) {
              names[s->nfiles][j++] = s->line[i++];
            }
            names[s->nfiles][j] = 0;
            if(j > 0u) {
              ++s->nfiles;
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
  xfer_sort_names(names, s->nfiles);
}

static void xfer_list_print(void)
{
  bbs_xfer_state_t *s = xfer;
  unsigned char i;
  if(s == NULL) {
    shell_output_str(NULL, "transfer module not ready", "");
    return;
  }
  xfer_scan_dir(s->tab);
  shell_output_str(NULL, "\n\r", "");
  if(s->nfiles == 0u) {
    shell_output_str(NULL, " (no files)\n\r", "");
    return;
  }
  for(i = 0u; i < s->nfiles; ++i) {
    s->list_ln[0] = 0;
    if((i + 1u) < 10u) {
      s->list_ln[0] = (char)('0' + (i + 1u));
      s->list_ln[1] = ' ';
      s->list_ln[2] = 0;
    } else {
      s->list_ln[0] = '1';
      s->list_ln[1] = (char)('0' + (i + 1u - 10u));
      s->list_ln[2] = ' ';
      s->list_ln[3] = 0;
    }
    strcat(s->list_ln, s->tab[i]);
    strcat(s->list_ln, "\n\r");
    shell_output_str(NULL, s->list_ln, "");
  }
}

static unsigned char xfer_pick_file(unsigned char num, char names[][BBS_XFER_NAME_LEN])
{
  if(xfer == NULL || num < 1u || num > xfer->nfiles) {
    return 0u;
  }
  strcpy(xfer->pick_name, names[num - 1u]);
  return 1u;
}

static unsigned char xfer_cd_local(const char *arg)
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

/*---------------------------------------------------------------------------*/
PROCESS(bbs_xfer_process, "xfer");
SHELL_COMMAND(bbs_xfer_list_command, "$", "$ : list transfer files", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_dl_command, "d", "d : download file #", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_ul_command, "u", "u : upload file", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_cd_command, "cd", "cd : change transfer dir", &bbs_xfer_process);
SHELL_COMMAND(bbs_xfer_md_command, "md", "md : make transfer dir", &bbs_xfer_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_xfer_process, ev, data)
{
  bbs_xfer_state_t *s = xfer;
  struct shell_input *input;
  const char *op;
  unsigned short num;

  PROCESS_BEGIN();
  if(s == NULL) {
    shell_output_str(NULL, "\n\rtransfer module unavailable\n\r", "");
    PROCESS_EXIT();
  }
  op = s->op_line;
  shell_output_str(NULL, PETSCII_LOWER, PETSCII_WHITE);

  if(op != NULL && op[0] == (char)'$') {
    xfer_list_print();
    PROCESS_EXIT();
  }

  if(op != NULL && op[0] == (char)'d' && op[1] == 0) {
    xfer_scan_dir(s->tab);
    if(s->nfiles == 0u) {
      shell_output_str(NULL, "\n\rno files\n\r", "");
      PROCESS_EXIT();
    }
    shell_prompt("\n\rselect file #: ");
    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
    input = data;
    num = xfer_atou(input->data1);
    if(xfer_pick_file((unsigned char)num, s->tab) != 0u) {
      xfer_path_file(s->pathbuf, s->pick_name);
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

  PROCESS_END();
}

unsigned char bbs_xfer_init(void)
{
  if(xfer == NULL) {
    xfer = (bbs_xfer_state_t *)malloc(sizeof(*xfer));
    if(xfer == NULL) {
      return 0u;
    }
  }
  xfer->cwd[0] = 0;
  xfer->nfiles = 0u;
  xfer->op_line = NULL;
  xfer->rx_head = xfer->rx_tail = 0u;
  xfer->rx_base = NULL;
  xfer->rx_size = 0u;
  shell_register_command(&bbs_xfer_list_command);
  shell_register_command(&bbs_xfer_dl_command);
  shell_register_command(&bbs_xfer_ul_command);
  if(board.dir_boost == 1u) {
    shell_register_command(&bbs_xfer_cd_command);
    shell_register_command(&bbs_xfer_md_command);
  }
  return 1u;
}

void bbs_xfer_deinit(void)
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
  if(xfer != NULL) {
    free(xfer);
    xfer = NULL;
  }
}
