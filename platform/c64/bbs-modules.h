#ifndef BBS_MODULES_H_
#define BBS_MODULES_H_

#define BBS_MODULE_ID_MSG   1u
#define BBS_MODULE_ID_XFER  2u

struct shell_command;

typedef struct bbs_module_ctx {
  void *board;
  void *status;
  void *buffer;
  int shell_event_input;
  void (*shell_output_str)(struct shell_command *c, char *str1, char *str2);
  void (*shell_prompt)(char *prompt);
  void (*shell_register_command)(struct shell_command *c);
  void (*shell_unregister_command)(struct shell_command *c);
  void (*transport_poll)(void);
  void (*serial_flush_outbound)(void);
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
  char jmp_deinit;
  void (*deinit)(void);
} bbs_module_iface_t;

#endif /* BBS_MODULES_H_ */
