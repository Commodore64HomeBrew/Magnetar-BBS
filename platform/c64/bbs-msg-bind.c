#include "bbs-msg-bind.h"

#ifdef BBS_MSG_MODULE
BBS_BOARD_REC *bbsm_board_p;
BBS_CONFIG_REC *bbsm_config_p;
BBS_STATUS_REC *bbsm_status_p;
BBS_USER_REC *bbsm_user_p;
BBS_TIME_REC *bbsm_time_p;
BBS_USER_STATS *bbsm_usrstats_p;
BBS_SYSTEM_STATS *bbsm_sysstats_p;
int *bbsm_shell_event_input_p;
void (*bbsm_shell_output_str_p)(struct shell_command *c, char *str1, char *str2);
void (*bbsm_shell_prompt_p)(char *prompt);
void (*bbsm_shell_register_command_p)(struct shell_command *c);
void (*bbsm_shell_unregister_command_p)(struct shell_command *c);
void (*bbsm_set_prompt_p)(void);
void (*bbsm_update_time_p)(void);
void (*bbsm_bbs_banner_p)(unsigned char filePrefix[10], unsigned char bannerFile[12],
    unsigned char fileSuffix[3], unsigned char device, unsigned char wordWrap);
void (*bbsm_file_path_p)(const char *file, unsigned short num, char *out, unsigned char outsz);
void (*bbsm_bbs_path_sys_at_p)(char *out, const char *suffix);
void *(*bbsm_malloc_p)(unsigned size);
void (*bbsm_free_p)(void *ptr);

unsigned char
bbs_msg_bind(const bbs_module_ctx_t *ctx)
{
  if(ctx == NULL || ctx->bbsm_board == NULL || ctx->bbsm_config == NULL ||
      ctx->bbsm_status == NULL || ctx->bbsm_user == NULL || ctx->bbsm_time == NULL ||
      ctx->bbsm_usrstats == NULL || ctx->bbsm_sysstats == NULL ||
      ctx->bbsm_shell_event_input == NULL || ctx->bbsm_shell_output_str == NULL ||
      ctx->bbsm_shell_prompt == NULL || ctx->bbsm_shell_register_command == NULL ||
      ctx->bbsm_shell_unregister_command == NULL || ctx->bbsm_set_prompt == NULL ||
      ctx->bbsm_update_time == NULL || ctx->bbsm_bbs_banner == NULL ||
      ctx->bbsm_file_path == NULL || ctx->bbsm_bbs_path_sys_at == NULL ||
      ctx->bbsm_malloc == NULL || ctx->bbsm_free == NULL) {
    return 0u;
  }
  bbsm_board_p = (BBS_BOARD_REC *)ctx->bbsm_board;
  bbsm_config_p = (BBS_CONFIG_REC *)ctx->bbsm_config;
  bbsm_status_p = (BBS_STATUS_REC *)ctx->bbsm_status;
  bbsm_user_p = (BBS_USER_REC *)ctx->bbsm_user;
  bbsm_time_p = (BBS_TIME_REC *)ctx->bbsm_time;
  bbsm_usrstats_p = (BBS_USER_STATS *)ctx->bbsm_usrstats;
  bbsm_sysstats_p = (BBS_SYSTEM_STATS *)ctx->bbsm_sysstats;
  bbsm_shell_event_input_p = ctx->bbsm_shell_event_input;
  bbsm_shell_output_str_p = ctx->bbsm_shell_output_str;
  bbsm_shell_prompt_p = ctx->bbsm_shell_prompt;
  bbsm_shell_register_command_p = ctx->bbsm_shell_register_command;
  bbsm_shell_unregister_command_p = ctx->bbsm_shell_unregister_command;
  bbsm_set_prompt_p = ctx->bbsm_set_prompt;
  bbsm_update_time_p = ctx->bbsm_update_time;
  bbsm_bbs_banner_p = ctx->bbsm_bbs_banner;
  bbsm_file_path_p = ctx->bbsm_file_path;
  bbsm_bbs_path_sys_at_p = ctx->bbsm_bbs_path_sys_at;
  bbsm_malloc_p = ctx->bbsm_malloc;
  bbsm_free_p = ctx->bbsm_free;
  return 1u;
}
#endif
