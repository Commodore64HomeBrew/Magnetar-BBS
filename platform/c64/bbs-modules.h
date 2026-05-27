#ifndef BBS_MODULES_H_
#define BBS_MODULES_H_

#define BBS_MODULE_ID_MSG   1u
#define BBS_MODULE_ID_XFER  2u

struct shell_command;

typedef struct bbs_module_ctx {
  unsigned char (*msg_init)(void);
  void (*msg_deinit)(void);
  void *bbsm_board;
  void *bbsm_config;
  void *bbsm_status;
  void *bbsm_user;
  void *bbsm_time;
  void *bbsm_usrstats;
  void *bbsm_sysstats;
  int *bbsm_shell_event_input;
  void (*bbsm_shell_output_str)(struct shell_command *c, char *str1, char *str2);
  void (*bbsm_shell_prompt)(char *prompt);
  void (*bbsm_shell_register_command)(struct shell_command *c);
  void (*bbsm_shell_unregister_command)(struct shell_command *c);
  void (*bbsm_set_prompt)(void);
  void (*bbsm_update_time)(void);
  void (*bbsm_bbs_banner)(unsigned char filePrefix[10], unsigned char bannerFile[12],
      unsigned char fileSuffix[3], unsigned char device, unsigned char wordWrap);
  void (*bbsm_file_path)(const char *file, unsigned short num, char *out, unsigned char outsz);
  void (*bbsm_bbs_path_sys_at)(char *out, const char *suffix);
  void *(*bbsm_malloc)(unsigned size);
  void (*bbsm_free)(void *ptr);
#ifdef BBS_SERIAL_TRANSPORT
  void *bbsm_buffer;
  void (*bbsm_transport_poll)(void);
  int (*bbsm_buf_append)(const char *data, int len);
  unsigned long (*bbsm_clock_time)(void);
  void (*bbsm_serial_flush_outbound)(void);
#endif
} bbs_module_ctx_t;

/*
 * Runtime-loaded module ABI:
 * - signature must be "BBS1"
 * - module_id identifies command group
 * - init/deinit are called by core around swaps
 * - init receives a core-owned context/API table
 */
typedef struct bbs_module_iface {
  unsigned char signature[4];
  unsigned char module_id;
  char jmp_init;
  unsigned char (*init)(const bbs_module_ctx_t *ctx);
#ifdef BBS_SERIAL_TRANSPORT
  char jmp_set_op;
  void (*set_op)(const char *cmd);
  char jmp_feed;
  void (*feed)(unsigned char c);
#endif
  char jmp_deinit;
  void (*deinit)(void);
} bbs_module_iface_t;

#endif /* BBS_MODULES_H_ */
