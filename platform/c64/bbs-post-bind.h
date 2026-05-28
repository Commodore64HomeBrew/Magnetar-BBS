#ifndef BBS_POST_BIND_H_
#define BBS_POST_BIND_H_

#include "bbs-shell.h"
#include "bbs-modules.h"

#ifdef BBS_POST_MODULE
extern BBS_BOARD_REC *bbsp_board_p;
extern BBS_CONFIG_REC *bbsp_config_p;
extern BBS_STATUS_REC *bbsp_status_p;
extern BBS_USER_REC *bbsp_user_p;
extern BBS_TIME_REC *bbsp_time_p;
extern BBS_USER_STATS *bbsp_usrstats_p;
extern BBS_SYSTEM_STATS *bbsp_sysstats_p;
extern int *bbsp_shell_event_input_p;
extern void (*bbsp_shell_output_str_p)(struct shell_command *c, char *str1, char *str2);
extern void (*bbsp_shell_prompt_p)(char *prompt);
extern void (*bbsp_shell_register_command_p)(struct shell_command *c);
extern void (*bbsp_shell_unregister_command_p)(struct shell_command *c);
extern void (*bbsp_set_prompt_p)(void);
extern void (*bbsp_update_time_p)(void);
extern void (*bbsp_file_path_p)(const char *file, unsigned short num, char *out, unsigned char outsz);
extern void (*bbsp_bbs_path_sys_at_p)(char *out, const char *suffix);
extern void *(*bbsp_malloc_p)(unsigned size);
extern void (*bbsp_free_p)(void *ptr);

unsigned char bbs_post_bind(const bbs_module_ctx_t *ctx);

#define board (*bbsp_board_p)
#define bbs_config (*bbsp_config_p)
#define bbs_status (*bbsp_status_p)
#define bbs_user (*bbsp_user_p)
#define bbs_time (*bbsp_time_p)
#define bbs_usrstats (*bbsp_usrstats_p)
#define bbs_sysstats (*bbsp_sysstats_p)
#define shell_event_input (*bbsp_shell_event_input_p)
#define shell_output_str bbsp_shell_output_str_p
#define shell_prompt bbsp_shell_prompt_p
#define shell_register_command bbsp_shell_register_command_p
#define shell_unregister_command bbsp_shell_unregister_command_p
#define set_prompt bbsp_set_prompt_p
#define update_time bbsp_update_time_p
#define file_path bbsp_file_path_p
#define bbs_path_sys_at bbsp_bbs_path_sys_at_p
#define bbsm_malloc_p bbsp_malloc_p
#define bbsm_free_p bbsp_free_p
#endif

#endif /* BBS_POST_BIND_H_ */

