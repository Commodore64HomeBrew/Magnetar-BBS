#include "bbs-post-bind.h"

#ifdef BBS_POST_MODULE
BBS_BOARD_REC *bbsp_board_p;
BBS_CONFIG_REC *bbsp_config_p;
BBS_STATUS_REC *bbsp_status_p;
BBS_USER_REC *bbsp_user_p;
BBS_TIME_REC *bbsp_time_p;
BBS_USER_STATS *bbsp_usrstats_p;
BBS_SYSTEM_STATS *bbsp_sysstats_p;
int *bbsp_shell_event_input_p;
void (*bbsp_shell_output_str_p)(struct shell_command *c, char *str1, char *str2);
void (*bbsp_shell_prompt_p)(char *prompt);
void (*bbsp_shell_register_command_p)(struct shell_command *c);
void (*bbsp_shell_unregister_command_p)(struct shell_command *c);
void (*bbsp_set_prompt_p)(void);
void (*bbsp_update_time_p)(void);
void (*bbsp_file_path_p)(const char *file, unsigned short num, char *out, unsigned char outsz);
void (*bbsp_bbs_path_sys_at_p)(char *out, const char *suffix);
void *(*bbsp_malloc_p)(unsigned size);
void (*bbsp_free_p)(void *ptr);

unsigned char
bbs_post_bind(const bbs_module_ctx_t *ctx)
{
  if(ctx == NULL || ctx->bbsm_board == NULL || ctx->bbsm_config == NULL ||
      ctx->bbsm_status == NULL || ctx->bbsm_user == NULL || ctx->bbsm_time == NULL ||
      ctx->bbsm_usrstats == NULL || ctx->bbsm_sysstats == NULL ||
      ctx->bbsm_shell_event_input == NULL || ctx->bbsm_shell_output_str == NULL ||
      ctx->bbsm_shell_prompt == NULL || ctx->bbsm_shell_register_command == NULL ||
      ctx->bbsm_shell_unregister_command == NULL || ctx->bbsm_set_prompt == NULL ||
      ctx->bbsm_update_time == NULL || ctx->bbsm_file_path == NULL ||
      ctx->bbsm_bbs_path_sys_at == NULL || ctx->bbsm_malloc == NULL ||
      ctx->bbsm_free == NULL) {
    return 0u;
  }

  bbsp_board_p = (BBS_BOARD_REC *)ctx->bbsm_board;
  bbsp_config_p = (BBS_CONFIG_REC *)ctx->bbsm_config;
  bbsp_status_p = (BBS_STATUS_REC *)ctx->bbsm_status;
  bbsp_user_p = (BBS_USER_REC *)ctx->bbsm_user;
  bbsp_time_p = (BBS_TIME_REC *)ctx->bbsm_time;
  bbsp_usrstats_p = (BBS_USER_STATS *)ctx->bbsm_usrstats;
  bbsp_sysstats_p = (BBS_SYSTEM_STATS *)ctx->bbsm_sysstats;
  bbsp_shell_event_input_p = ctx->bbsm_shell_event_input;
  bbsp_shell_output_str_p = ctx->bbsm_shell_output_str;
  bbsp_shell_prompt_p = ctx->bbsm_shell_prompt;
  bbsp_shell_register_command_p = ctx->bbsm_shell_register_command;
  bbsp_shell_unregister_command_p = ctx->bbsm_shell_unregister_command;
  bbsp_set_prompt_p = ctx->bbsm_set_prompt;
  bbsp_update_time_p = ctx->bbsm_update_time;
  bbsp_file_path_p = ctx->bbsm_file_path;
  bbsp_bbs_path_sys_at_p = ctx->bbsm_bbs_path_sys_at;
  bbsp_malloc_p = ctx->bbsm_malloc;
  bbsp_free_p = ctx->bbsm_free;
  return 1u;
}
#endif

