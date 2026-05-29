#ifndef BBS_BANK_H_
#define BBS_BANK_H_

#include "bbs-defs.h"

/*
 * Magic Desk compatible bank image at $B000 (8K, BBK1..BBK4 header).
 * SD2IEC: load .bin from drive 8 root into RAM at $B000; no $DE00 cart map yet.
 * Future cart: enable $DE00 (see BBS_BANK_HW_REG) before exec on hardware.
 */

#define BBS_BANK_BASE       0xB000u
#define BBS_BANK_SIZE       0x2000u
#define BBS_BANK_HW_REG     0xDE00u
#define BBS_BANK_HW_DISABLE 0x80u

#define BBS_BANK_ID_XFER    1u
#define BBS_BANK_ID_POST    2u
#define BBS_BANK_ID_MSG     3u
#define BBS_BANK_ID_UI      4u

struct shell_command;
struct shell_input;
struct process;

typedef struct bbs_shared_s {
  unsigned char sig0;
  unsigned char sig1;
  unsigned char active_bank;
  unsigned char pad;
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
 * Mailbox word at start of RESIDENT segment (c64-bbs-core.cfg @ $A986).
 * Not $0C00: that address lies inside the linked MAIN image on this build.
 */
#define BBS_SHARED_MAILBOX  0xA986u

extern bbs_shared_t bbs_shared_data;

#ifdef BBS_BANK_BUILD
#define BBS_SHARED    (*(bbs_shared_t **)(BBS_SHARED_MAILBOX))
#else
#define BBS_SHARED    (&bbs_shared_data)
#endif

typedef struct bbs_bank_hdr_s {
  char sig[4];
  unsigned char (*init)(void);
  void (*deinit)(void);
  void (*set_op)(const char *cmd);
  void (*feed)(unsigned char c);
  void (*run_sys_stats)(void);
} bbs_bank_hdr_t;

#define BBS_BANK_HDR  ((bbs_bank_hdr_t *)BBS_BANK_BASE)

void bbs_shared_publish(void);
void bbs_shared_sync_back(void);
void bbs_bank_hw_enable_for_exec(void);
void bbs_bank_hw_disable_exec(void);
unsigned char bbs_bank_load(unsigned char bank_id);
void bbs_bank_unload(void);
unsigned char bbs_bank_active(void);
unsigned char bbs_bank_id_active(void);
void bbs_bank_set_op(const char *cmd);
void bbs_bank_feed(unsigned char c);

#endif /* BBS_BANK_H_ */
