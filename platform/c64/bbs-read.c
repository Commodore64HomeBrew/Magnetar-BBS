/**
 * \file bbs-read.c — read messages (bank 3 overlay only).
 */

#ifndef BBS_BANK_BUILD
#error "bbs-read.c is linked only as bank overlay bbs-read-bank.o"
#endif

#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-read.h"
#include "bbs-file.h"
#include "bbs-bank-macros.h"
#include "bbs-bank-msg.h"
#include "bbs-fmt.h"

int
read_msg(unsigned short num)
{
    char sub_num_prefix[BBS_FILE_PATH_BUFLEN];
    char file[12];

    bbs_transport_buf_discard();
    shell_output_str(NULL, PETSCII_LOWER, PETSCII_WHITE);

    bbs_fmt_msg_id(file, bbs_status.board_id, num);
    set_prompt();
    bbs_status.status = STATUS_READ;

    file_path(file, num, sub_num_prefix, sizeof(sub_num_prefix));
    bbs_banner((unsigned char *)sub_num_prefix, file, "", board.subs_device,
        bbs_status.wrap);

    bbs_status.status = STATUS_LOCK;

    return 0;
}

/*---------------------------------------------------------------------------*/
PROCESS(bbs_read_process, "read");
SHELL_COMMAND(bbs_read_command, "#", "# : select msg", &bbs_read_process);
SHELL_COMMAND(bbs_prevmsg_command, "r", "", &bbs_msg_nop_process);
SHELL_COMMAND(bbs_nextmsg1_command, "\x0d", "", &bbs_msg_nop_process);
SHELL_COMMAND(bbs_nextmsg2_command, "\x0a", "", &bbs_msg_nop_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_read_process, ev, data)
{

  struct shell_input *input;
  const char *inline_arg;
  char message[40];
  unsigned short num;

  PROCESS_BEGIN();

  shell_output_str(NULL, PETSCII_LOWER, PETSCII_WHITE);

  inline_arg = (const char *)data;
  if(inline_arg != NULL && inline_arg[0] != '\0') {
    num = bbs_parse_u16(inline_arg);
    if(num > 0u && num <= bbs_config.msg_id[bbs_status.board_id]) {
      bbs_usrstats.current_msg[bbs_status.board_id] = num;
      read_msg(num);
      PROCESS_EXIT();
    }
  }

  bbs_fmt_read_select_prompt(message, bbs_config.msg_id[bbs_status.board_id]);
  shell_prompt(message);

  PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
  input = data;
  num = bbs_parse_u16(input->data1);

  if(num > 0u && num <= bbs_config.msg_id[bbs_status.board_id]) {
    bbs_usrstats.current_msg[bbs_status.board_id] = num;
    read_msg(num);
  }

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
void
bbs_read_init(void)
{
  shell_register_command(&bbs_read_command);
  shell_register_command(&bbs_prevmsg_command);
  shell_register_command(&bbs_nextmsg1_command);
  shell_register_command(&bbs_nextmsg2_command);
}

void
bbs_read_deinit(void)
{
  shell_unregister_command(&bbs_read_command);
  shell_unregister_command(&bbs_prevmsg_command);
  shell_unregister_command(&bbs_nextmsg1_command);
  shell_unregister_command(&bbs_nextmsg2_command);
}
