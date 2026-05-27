#ifndef BBS_MODULES_H_
#define BBS_MODULES_H_

#define BBS_MODULE_ID_MSG   1u
#define BBS_MODULE_ID_XFER  2u

struct shell_command;

typedef struct bbs_module_ctx {
  unsigned char (*msg_init)(void);
  void (*msg_deinit)(void);
  unsigned char (*xfer_init)(void);
  void (*xfer_deinit)(void);
#ifdef BBS_SERIAL_TRANSPORT
  void (*xfer_set_op)(const char *cmd);
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
#endif
  char jmp_deinit;
  void (*deinit)(void);
} bbs_module_iface_t;

#endif /* BBS_MODULES_H_ */
