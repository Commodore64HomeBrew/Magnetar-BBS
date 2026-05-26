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
#include <string.h>

extern BBS_BOARD_REC board;
extern BBS_STATUS_REC bbs_status;

#define XFER_CHN  2u
#define XFER_CMD  15u
#define XFER_WAIT (CLOCK_SECOND * 3)
static unsigned int xfer_rx_head, xfer_rx_tail;
static unsigned char *xfer_rx_base;
static unsigned int xfer_rx_size;
static char xfer_cwd[BBS_XFER_PATH_LEN];
static unsigned char xfer_nfiles;
static const char *xfer_op_line;
static char xfer_pick_name[BBS_XFER_NAME_LEN];
static char xfer_tab[BBS_XFER_MAX_FILES][BBS_XFER_NAME_LEN];
static char xfer_pathbuf[BBS_FILE_PATH_BUFLEN];
static char xfer_line[40];
static char xfer_sort_tmp[BBS_XFER_NAME_LEN];
static char xfer_list_ln[24];
static char xfer_cmd[24];
static char xfer_outc;
unsigned char bbs_xmodem_inbyte;
#ifdef BBS_SERIAL_TRANSPORT
static char xfer_ser_c;
#endif
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
  xfer_op_line = cmd;
}

void bbs_xfer_feed(unsigned char c)
{
  unsigned int n;
  if(xfer_rx_base == NULL) {
    return;
  }
  n = xfer_rx_head + 1u;
  if(n >= xfer_rx_size) {
    n = 0u;
  }
  if(n != xfer_rx_tail) {
    xfer_rx_base[xfer_rx_head] = c;
    xfer_rx_head = n;
  }
}

static unsigned char xfer_rx_pop(unsigned char *c)
{
  if(xfer_rx_tail == xfer_rx_head) {
    return 0u;
  }
  *c = xfer_rx_base[xfer_rx_tail];
  ++xfer_rx_tail;
  if(xfer_rx_tail >= xfer_rx_size) {
    xfer_rx_tail = 0u;
  }
  return 1u;
}

static void xfer_flush_rx(void)
{
  unsigned char c;
  while(xfer_rx_pop(&c) != 0u) {
  }
#ifdef BBS_SERIAL_TRANSPORT
  while(ser_get(&xfer_ser_c) == SER_ERR_OK) {
  }
#endif
}

void bbs_xmodem_io_begin(void)
{
  xfer_rx_base = buf.bufmem;
  xfer_rx_size = buf.size;
  buf.used = 0u;
  buf.head = 0u;
  xfer_rx_head = xfer_rx_tail = 0u;
  bbs_status.status = STATUS_XFER;
  xfer_flush_rx();
}

void bbs_xmodem_io_end(void)
{
  bbs_status.status = STATUS_LOCK;
  xfer_rx_base = NULL;
}

unsigned char bbs_xmodem_poll(void)
{
  unsigned char c;
  clock_time_t t0 = clock_time();
  while((clock_time_t)(clock_time() - t0) < XFER_WAIT) {
    if(xfer_rx_pop(&c) != 0u) {
      bbs_xmodem_inbyte = c;
      return 1u;
    }
#ifdef BBS_SERIAL_TRANSPORT
    if(ser_get(&xfer_ser_c) == SER_ERR_OK) {
      bbs_xmodem_inbyte = (unsigned char)xfer_ser_c;
      return 1u;
    }
#endif
    bbs_transport_poll();
  }
  return 0u;
}

void bbs_xmodem_putc(unsigned char c)
{
  clock_time_t t0;
  xfer_outc = (char)c;
  (void)buf_append(&xfer_outc, 1);
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
  strcpy(out, board.transfer_prefix);
  strcat(out, xfer_cwd);
  strcat(out, "$");
}

static void xfer_path_file(char *out, const char *name)
{
  strcpy(out, board.transfer_prefix);
  strcat(out, xfer_cwd);
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
  unsigned char i, j;
  for(i = 0u; i < n; ++i) {
    for(j = (unsigned char)(i + 1u); j < n; ++j) {
      if(strcmp(names[i], names[j]) > 0) {
        strcpy(xfer_sort_tmp, names[i]);
        strcpy(names[i], names[j]);
        strcpy(names[j], xfer_sort_tmp);
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
  unsigned char li, c;
  unsigned char in_q;
  int n;

  xfer_nfiles = 0u;
  xfer_path_dir(xfer_pathbuf);
  if(cbm_open(XFER_CHN, board.transfer_device, XFER_CHN, xfer_pathbuf) != 0) {
    return;
  }
  li = 0u;
  in_q = 0u;
  xfer_line[0] = 0;
  while(xfer_nfiles < BBS_XFER_MAX_FILES) {
    n = cbm_read(XFER_CHN, &c, 1);
    if(n <= 0) {
      break;
    }
    if(c == 0x0du || c == 0x0au) {
      if(li > 0u) {
        xfer_line[li] = 0;
        if(in_q == 2u && xfer_dir_is_entry(xfer_line, li) != 0u) {
          unsigned char i = 0u;
          while(xfer_line[i] != '"' && xfer_line[i] != 0) {
            ++i;
          }
          if(xfer_line[i] == '"') {
            unsigned char j = 0u;
            ++i;
            while(xfer_line[i] != '"' && xfer_line[i] != 0 && j < 16u) {
              names[xfer_nfiles][j++] = xfer_line[i++];
            }
            names[xfer_nfiles][j] = 0;
            if(j > 0u) {
              ++xfer_nfiles;
            }
          }
        }
      }
      li = 0u;
      in_q = 0u;
    } else if(li < 39u) {
      xfer_line[li++] = (char)c;
      if(c == '"') {
        ++in_q;
      }
    }
  }
  cbm_close(XFER_CHN);
  xfer_sort_names(names, xfer_nfiles);
}

static void xfer_list_print(void)
{
  unsigned char i;
  xfer_scan_dir(xfer_tab);
  shell_output_str(NULL, "\n\r", "");
  if(xfer_nfiles == 0u) {
    shell_output_str(NULL, " (no files)\n\r", "");
    return;
  }
  for(i = 0u; i < xfer_nfiles; ++i) {
    xfer_list_ln[0] = 0;
    if((i + 1u) < 10u) {
      xfer_list_ln[0] = (char)('0' + (i + 1u));
      xfer_list_ln[1] = ' ';
      xfer_list_ln[2] = 0;
    } else {
      xfer_list_ln[0] = '1';
      xfer_list_ln[1] = (char)('0' + (i + 1u - 10u));
      xfer_list_ln[2] = ' ';
      xfer_list_ln[3] = 0;
    }
    strcat(xfer_list_ln, xfer_tab[i]);
    strcat(xfer_list_ln, "\n\r");
    shell_output_str(NULL, xfer_list_ln, "");
  }
}

static unsigned char xfer_pick_file(unsigned char num, char names[][BBS_XFER_NAME_LEN])
{
  if(num < 1u || num > xfer_nfiles) {
    return 0u;
  }
  strcpy(xfer_pick_name, names[num - 1u]);
  return 1u;
}

static unsigned char xfer_cd_local(const char *arg)
{
  unsigned char len, i;
  if(arg[0] == 0) {
    return 1u;
  }
  if(strcmp(arg, "..") == 0 || arg[0] == '_' || strcmp(arg, "^") == 0) {
    len = (unsigned char)strlen(xfer_cwd);
    if(len == 0u) {
      return 1u;
    }
    if(len > 0u && xfer_cwd[len - 1u] == '/') {
      xfer_cwd[len - 1u] = 0;
      len = (unsigned char)strlen(xfer_cwd);
    }
    for(i = len; i > 0u; --i) {
      if(xfer_cwd[i - 1u] == '/') {
        xfer_cwd[i] = 0;
        break;
      }
    }
    return xfer_dos_cmd("CD:_");
  }
  len = (unsigned char)strlen(xfer_cwd);
  if(len + strlen(arg) + 2u >= BBS_XFER_PATH_LEN) {
    return 0u;
  }
  if(len > 0u && xfer_cwd[len - 1u] != '/') {
    strcat(xfer_cwd, "/");
  }
  strcat(xfer_cwd, arg);
  strcpy(xfer_cmd, "CD:");
  strcat(xfer_cmd, arg);
  return xfer_dos_cmd(xfer_cmd);
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
  struct shell_input *input;
  const char *op;
  unsigned short num;

  PROCESS_BEGIN();
  op = xfer_op_line;
  shell_output_str(NULL, PETSCII_LOWER, PETSCII_WHITE);

  if(op != NULL && op[0] == (char)'$') {
    xfer_list_print();
    PROCESS_EXIT();
  }

  if(op != NULL && op[0] == (char)'d' && op[1] == 0) {
    xfer_scan_dir(xfer_tab);
    if(xfer_nfiles == 0u) {
      shell_output_str(NULL, "\n\rno files\n\r", "");
      PROCESS_EXIT();
    }
    shell_prompt("\n\rselect file #: ");
    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
    input = data;
    num = xfer_atou(input->data1);
    if(xfer_pick_file((unsigned char)num, xfer_tab) != 0u) {
      xfer_path_file(xfer_pathbuf, xfer_pick_name);
      xfer_do_send(xfer_pathbuf);
    }
    PROCESS_EXIT();
  }

  if(op != NULL && op[0] == (char)'u' && op[1] == 0) {
    shell_prompt("\n\rfile name (max 16): ");
    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
    input = data;
    if(xfer_name_ok(input->data1) != 0u) {
      xfer_path_file(xfer_pathbuf, input->data1);
      xfer_do_recv(xfer_pathbuf);
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
      strcpy(xfer_pathbuf, "\n\rnow: ");
      strcat(xfer_pathbuf, board.transfer_prefix);
      strcat(xfer_pathbuf, xfer_cwd);
      strcat(xfer_pathbuf, "\n\r");
      shell_output_str(NULL, xfer_pathbuf, "");
    }
    PROCESS_EXIT();
  }

  if(board.dir_boost == 1u && op != NULL && op[0] == (char)'m' && op[1] == (char)'d' && op[2] == 0) {
    shell_prompt("\n\rnew directory: ");
    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
    input = data;
    if(xfer_name_ok(input->data1) != 0u) {
      strcpy(xfer_cmd, "MD:");
      strcat(xfer_cmd, input->data1);
      if(xfer_dos_cmd(xfer_cmd) == 0u) {
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

void bbs_xfer_init(void)
{
  xfer_cwd[0] = 0;
  xfer_rx_head = xfer_rx_tail = 0u;
  xfer_rx_base = NULL;
  shell_register_command(&bbs_xfer_list_command);
  shell_register_command(&bbs_xfer_dl_command);
  shell_register_command(&bbs_xfer_ul_command);
  if(board.dir_boost == 1u) {
    shell_register_command(&bbs_xfer_cd_command);
    shell_register_command(&bbs_xfer_md_command);
  }
}
