/**
 * \file bbs-setboard.c — select message boards (bank 3 overlay only).
 */

#ifndef BBS_BANK_BUILD
#error "bbs-setboard.c is linked only as bank overlay bbs-setboard-bank.o"
#endif

#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-setboard.h"
#include "bbs-bank-macros.h"
#include "bbs-fmt.h"
#include <string.h>

void bbs_sub_banner(void)
{
  unsigned char message[32];
  unsigned char file[12];

  bbs_fmt_sub_file((char *)file, bbs_status.board_id);
  bbs_banner(board.sys_prefix, file, bbs_status.encoding_suffix, board.sys_device, 0);
  bbs_fmt_petscii_name_ln((char *)message,
      (const char *)board.sub_names[bbs_status.board_id]);
  shell_output_str(NULL, (char *)message, "");
}


PROCESS(bbs_setboard_process, "board");
SHELL_COMMAND(bbs_setboard_command, "s", "s : select board", &bbs_setboard_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_setboard_process, ev, data)
{

  struct shell_input *input;
  const char *inline_arg;
  unsigned short num;
  unsigned char message[40];

  PROCESS_BEGIN();

  inline_arg = (const char *)data;
  if(inline_arg != NULL && inline_arg[0] != '\0') {
    num = bbs_parse_u16(inline_arg);
    if(num > 0u && num <= board.max_boards) {
      bbs_status.board_id = num;
      set_prompt();
      bbs_sub_banner();
      PROCESS_EXIT();
    }
  }

  bbs_banner(board.sys_prefix, BBS_BANNER_SUBS, bbs_status.encoding_suffix, board.sys_device, 0);
  
  shell_output_str(NULL,"\n\r","");

  for(num = 1; num <= BBS_MAX_BOARDS; num++) {
    bbs_fmt_board_list_line((char *)message, (unsigned char)num,
        (const char *)board.sub_names[num],
        (unsigned short)(bbs_config.msg_id[num] - bbs_usrstats.current_msg[num]));
    shell_output_str(NULL, "", (char *)message);
  }

  bbs_fmt_board_select_prompt((char *)message, board.max_boards);
  shell_prompt((char *)message);

  PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
  input = data;
  num = bbs_parse_u16(input->data1);

  if(num>0 && num <=board.max_boards){

    bbs_status.board_id = num;
    set_prompt();
    bbs_sub_banner();
  }

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
PROCESS(bbs_nextboard_process, "nextboard");
SHELL_COMMAND(bbs_nextboard_command, "+", "+ : next board", &bbs_nextboard_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_nextboard_process, ev, data)
{
  PROCESS_BEGIN();

  if(bbs_status.board_id < board.max_boards) {
    ++bbs_status.board_id;
    set_prompt();
    bbs_sub_banner();
  }

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
PROCESS(bbs_prevboard_process, "prevboard");
SHELL_COMMAND(bbs_prevboard_command, "-", "- : prev board", &bbs_prevboard_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_prevboard_process, ev, data)
{
  PROCESS_BEGIN();

  if(bbs_status.board_id > 1u) {
    --bbs_status.board_id;
    set_prompt();
    bbs_sub_banner();
  }

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
void
bbs_setboard_init(void)
{
  shell_register_command(&bbs_setboard_command);
  shell_register_command(&bbs_nextboard_command);
  shell_register_command(&bbs_prevboard_command);
}

void
bbs_setboard_deinit(void)
{
  shell_unregister_command(&bbs_setboard_command);
  shell_unregister_command(&bbs_nextboard_command);
  shell_unregister_command(&bbs_prevboard_command);
}
