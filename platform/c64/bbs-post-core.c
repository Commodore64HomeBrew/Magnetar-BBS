#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-defs.h"

PROCESS(bbs_post_core_process, "write");
SHELL_COMMAND(bbs_post_core_command, "w", "w : write msg", &bbs_post_core_process);

PROCESS_THREAD(bbs_post_core_process, ev, data)
{
  const struct shell_input *in;

  PROCESS_BEGIN();

  if(bbs_post_begin_h == NULL || bbs_post_on_input_h == NULL) {
    shell_output_str(NULL, "\r\npost module unavailable\r\n", "");
    PROCESS_EXIT();
  }
  if(bbs_post_begin_h() == 0u) {
    shell_output_str(NULL, "\r\npost failed\r\n", "");
    PROCESS_EXIT();
  }

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
    in = (const struct shell_input *)data;
    if(in != NULL) {
      bbs_post_on_input_h(in);
    }
    if(bbs_status.status == STATUS_LOCK) {
      break;
    }
  }

  PROCESS_END();
}

void
bbs_post_core_init(void)
{
  shell_register_command(&bbs_post_core_command);
}

void
bbs_post_core_deinit(void)
{
  shell_unregister_command(&bbs_post_core_command);
}

