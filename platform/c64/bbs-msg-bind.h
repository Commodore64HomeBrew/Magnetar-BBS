#ifndef BBS_MSG_BIND_H_
#define BBS_MSG_BIND_H_

#include "bbs-shell.h"
#include "bbs-modules.h"

#ifdef BBS_MSG_MODULE
extern BBS_BOARD_REC *bbsm_board_p;
extern BBS_CONFIG_REC *bbsm_config_p;
extern BBS_STATUS_REC *bbsm_status_p;
extern BBS_USER_REC *bbsm_user_p;
extern BBS_TIME_REC *bbsm_time_p;
extern BBS_USER_STATS *bbsm_usrstats_p;
extern BBS_SYSTEM_STATS *bbsm_sysstats_p;
extern int *bbsm_shell_event_input_p;
extern void (*bbsm_shell_output_str_p)(struct shell_command *c, char *str1, char *str2);
extern void (*bbsm_shell_prompt_p)(char *prompt);
extern void (*bbsm_shell_register_command_p)(struct shell_command *c);
extern void (*bbsm_shell_unregister_command_p)(struct shell_command *c);
extern void (*bbsm_set_prompt_p)(void);
extern void (*bbsm_update_time_p)(void);
extern void (*bbsm_bbs_banner_p)(unsigned char filePrefix[10], unsigned char bannerFile[12],
    unsigned char fileSuffix[3], unsigned char device, unsigned char wordWrap);
extern void (*bbsm_file_path_p)(const char *file, unsigned short num, char *out, unsigned char outsz);
extern void (*bbsm_bbs_path_sys_at_p)(char *out, const char *suffix);
extern void *(*bbsm_malloc_p)(unsigned size);
extern void (*bbsm_free_p)(void *ptr);
extern BBS_BUFFER *bbsm_buffer_p;
extern int (*bbsm_buf_append_p)(const char *data, int len);
extern int (*bbsm_buf_putc_raw_p)(unsigned char c);
extern void (*bbsm_transport_stream_clear_sent_p)(void);
extern void (*bbsm_stream_set_eof_process_p)(struct process *p);

unsigned char bbs_msg_bind(const bbs_module_ctx_t *ctx);

#define board (*bbsm_board_p)
#define bbs_config (*bbsm_config_p)
#define bbs_status (*bbsm_status_p)
#define bbs_user (*bbsm_user_p)
#define bbs_time (*bbsm_time_p)
#define bbs_usrstats (*bbsm_usrstats_p)
#define bbs_sysstats (*bbsm_sysstats_p)
#define shell_event_input (*bbsm_shell_event_input_p)
#define shell_output_str bbsm_shell_output_str_p
#define shell_prompt bbsm_shell_prompt_p
#define shell_register_command bbsm_shell_register_command_p
#define shell_unregister_command bbsm_shell_unregister_command_p
#define set_prompt bbsm_set_prompt_p
#define update_time bbsm_update_time_p
#define bbs_banner bbsm_bbs_banner_p
#define file_path bbsm_file_path_p
#define bbs_path_sys_at bbsm_bbs_path_sys_at_p
#define buf (*bbsm_buffer_p)
#define buf_append bbsm_buf_append_p
#define buf_putc_raw bbsm_buf_putc_raw_p
#define bbs_transport_stream_clear_sent bbsm_transport_stream_clear_sent_p
#define bbs_stream_set_eof_process bbsm_stream_set_eof_process_p
#endif

#endif /* BBS_MSG_BIND_H_ */
