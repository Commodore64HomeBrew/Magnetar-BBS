#ifndef BBS_BANK_H_
#define BBS_BANK_H_

#include "bbs-defs.h"

/*
 * Bank overlays at $B000 (8 KiB). Core loads bankN from the sys device
 * root (same volume as magbbs; not under //x/). Files are bankN.prg on disk;
 * open paths omit the hidden .prg type.
 * Banks call core through fixed RESAPI stubs at $A210 (bbs-api.h).
 *
 * Policy:
 * - After password: load bank 4 (stats) if needed; run login chart; keep stats loaded at prompt.
 * - At prompt: bank switch only when a routed command needs another bank (bbs_bank_load).
 * - On disconnect: leave the active bank overlay in place (no unload/deinit here).
 * - Next login: bbs_bank_load() unloads any stale bank before loading stats if needed.
 * - bbs_bank_load() skips disk/init when the requested bank is already active.
 */

#define BBS_BANK_HDR_SIZE       0x0015u

#define BBS_BANK_BASE       0xB000u
#define BBS_BANK_LINK_TOP   0xD000u
#define BBS_BANK_LINK_STACK 0x0200u
#define BBS_BANK_RAM_SIZE   (BBS_BANK_LINK_TOP - BBS_BANK_LINK_STACK - BBS_BANK_BASE)
#define BBS_BANK_SIZE       0x2000u
#define BBS_BANK_HW_REG     0xDE00u
#define BBS_BANK_HW_DISABLE 0x80u

#define BBS_BANK_ID_XFER    1u
#define BBS_BANK_ID_POST    2u
#define BBS_BANK_ID_MSG     3u
#define BBS_BANK_ID_STATS   4u
#define BBS_BANK_ID_XMODEM  5u

#define BBS_XFER_CWD_LEN    24u

/* Fixed bank entry points (must match c64-bbs-bank.cfg BANKENT segments). */
#define BBS_BANK_SIG_OFF        0x0000u
#define BBS_BANK_ABI_VER_OFF    0x0004u
#define BBS_BANK_INIT_OFF       0x0006u
#define BBS_BANK_DEINIT_OFF     0x0009u
#define BBS_BANK_SET_OP_OFF     0x000Cu
#define BBS_BANK_FEED_OFF       0x000Fu
#define BBS_BANK_RUN_STATS_OFF  0x0012u

#define BBS_BANK_INIT_ADDR      (BBS_BANK_BASE + BBS_BANK_INIT_OFF)
#define BBS_BANK_DEINIT_ADDR    (BBS_BANK_BASE + BBS_BANK_DEINIT_OFF)
#define BBS_BANK_SET_OP_ADDR    (BBS_BANK_BASE + BBS_BANK_SET_OP_OFF)
#define BBS_BANK_FEED_ADDR      (BBS_BANK_BASE + BBS_BANK_FEED_OFF)
#define BBS_BANK_RUN_STATS_ADDR (BBS_BANK_BASE + BBS_BANK_RUN_STATS_OFF)

struct shell_command;
struct shell_input;
struct process;

typedef struct bbs_shared_s {
  unsigned char sig0;
  unsigned char sig1;
  unsigned char abi_major;
  unsigned char abi_minor;
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
} bbs_shared_t;

#define BBS_SHARED_BASE     0xA280u
#define BBS_SHARED_SIZE     0x0347u

extern bbs_shared_t bbs_shared_data;
#define BBS_SHARED          ((bbs_shared_t *)BBS_SHARED_BASE)

void bbs_api_init(void);
unsigned char bbs_bank_load(unsigned char bank_id);
void bbs_bank_unload(void);
void bbs_bank_forget(void);
unsigned char bbs_bank_ensure_stats(void);
unsigned char bbs_bank_active(void);
unsigned char bbs_bank_id_active(void);
void bbs_bank_set_op(const char *cmd);
void bbs_bank_feed(unsigned char c);
void bbs_bank_run_sys_stats(void);

#endif /* BBS_BANK_H_ */
