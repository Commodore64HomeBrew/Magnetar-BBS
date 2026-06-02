#ifndef BBS_BANK_H_
#define BBS_BANK_H_

#include "bbs-defs.h"

/*
 * Bank overlays at $B000 (8 KiB, BBK1..BBK4 header in each bbs-bankN.bin).
 * Core loads banks via bbs_bank_load(); command code is not cc65 -t module.
 * SD2IEC: read .bin into RAM at $B000; future cart: $DE00 (BBS_BANK_HW_REG).
 */

#define BBS_BANK_BASE       0xB000u
#define BBS_BANK_SIZE       0x2000u
#define BBS_BANK_HW_REG     0xDE00u
#define BBS_BANK_HW_DISABLE 0x80u

#define BBS_BANK_ID_XFER    1u
#define BBS_BANK_ID_POST    2u
#define BBS_BANK_ID_MSG     3u
#define BBS_BANK_ID_UI      4u
#define BBS_BANK_ID_XMODEM  5u

#define BBS_XFER_CWD_LEN    24u

struct shell_command;
struct shell_input;
struct process;

typedef struct bbs_shared_s {
  unsigned char sig0;
  unsigned char sig1;
  unsigned char active_bank;
  char xfer_cwd[BBS_XFER_CWD_LEN];
  BBS_BOARD_REC s_board;
  BBS_CONFIG_REC s_config;
  BBS_STATUS_REC s_status;
  BBS_USER_REC s_user;
  BBS_USER_STATS s_usrstats;
  BBS_SYSTEM_STATS s_sysstats;
  BBS_TIME_REC s_time;
  BBS_BUFFER *s_buf;
  int *s_shell_ev;
  void (*shell_output_str)(struct shell_command *c, char *str1, char *str2);
  void (*shell_prompt)(char *prompt);
  void (*shell_register_command)(struct shell_command *c);
  void (*shell_unregister_command)(struct shell_command *c);
  void (*transport_poll)(void);
  void (*transport_poll_send)(void);
  void (*transport_flush_outbound)(void);
  void (*transport_buf_reset)(void);
  void (*transport_buf_discard)(void);
  void (*scr_layout_output)(void);
  void (*scr_layout_xfer)(void);
  void (*stream_begin)(void);
  int (*buf_append)(const char *data, int len);
  int (*buf_putc_raw)(unsigned char c);
  unsigned long (*clock_time)(void);
  void (*serial_flush_outbound)(void);
  void (*set_prompt)(void);
  void (*update_time)(void);
  void (*log_message)(const char *a, const char *b);
  void (*file_path)(const char *file, unsigned short num, char *out, unsigned char outsz);
  void (*bbs_banner)(unsigned char *filePrefix, unsigned char *szBannerFile,
      unsigned char *fileSuffix, unsigned char device, unsigned char wordWrap);
  void (*bbs_path_sys_at)(char *out, const char *suffix);
} bbs_shared_t;

/*
 * Fixed resident block (SHARED in c64-bbs-core.cfg, bbs-shared-reserve.S).
 * Core and banks both use BBS_SHARED; no pointer mailbox or double indirection.
 * ABI: do not change BBS_SHARED_BASE without rebuilding all bank .bin files.
 */
#define BBS_SHARED_BASE     0xA280u

extern bbs_shared_t bbs_shared_data;
#define BBS_SHARED          ((bbs_shared_t *)BBS_SHARED_BASE)

typedef struct bbs_bank_hdr_s {
  char sig[4];
  unsigned char (*init)(void);
  void (*deinit)(void);
  void (*set_op)(const char *cmd);
  void (*feed)(unsigned char c);
  void (*run_sys_stats)(void);
} bbs_bank_hdr_t;

#define BBS_BANK_HDR  ((bbs_bank_hdr_t *)BBS_BANK_BASE)

void bbs_shared_init(void);
void bbs_bank_hw_enable_for_exec(void);
void bbs_bank_hw_disable_exec(void);
unsigned char bbs_bank_load(unsigned char bank_id);
void bbs_bank_unload(void);
/* Clear loaded-bank state without calling overlay deinit (safe during logout). */
void bbs_bank_forget(void);
unsigned char bbs_bank_active(void);
unsigned char bbs_bank_id_active(void);
void bbs_bank_set_op(const char *cmd);
void bbs_bank_feed(unsigned char c);

#endif /* BBS_BANK_H_ */
