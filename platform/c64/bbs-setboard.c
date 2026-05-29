/**
 * \file
 *         bbs-setboard.c - select Contiki BBS message boards 
 *
 * \author
 *         (c) 2009-2015 by Niels Haedecke <n.haedecke@unitybox.de>
 */


#include "contiki.h"
#include "bbs-shell.h"

#include "bbs-setboard.h"
#ifdef BBS_MSG_MODULE
#include "bbs-msg-bind.h"
#endif
#ifdef BBS_BANK_BUILD
#include "bbs-bank-macros.h"
#include "bbs-fmt.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(BBS_MSG_MODULE) && !defined(BBS_BANK_BUILD)
extern BBS_BOARD_REC board;
extern BBS_CONFIG_REC bbs_config;
extern BBS_STATUS_REC bbs_status;
extern BBS_USER_STATS bbs_usrstats;
#endif


void bbs_sub_banner(void)
{
  unsigned char message[32];
  unsigned char file[12];

  sprintf(file, "%s%d",BBS_PREFIX_SUB,bbs_status.board_id);
  bbs_banner(board.sys_prefix, file, bbs_status.encoding_suffix, board.sys_device,0);
  sprintf(message, "\x05%s\n\r", board.sub_names[bbs_status.board_id]);
  shell_output_str(NULL, message, "");
}


PROCESS(bbs_setboard_process, "board");
SHELL_COMMAND(bbs_setboard_command, "s", "s : select board", &bbs_setboard_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_setboard_process, ev, data)
{

  struct shell_input *input;
  //char szBuff[BBS_LINE_WIDTH];
  unsigned short num;
  unsigned char message[40];
  //ST_FILE file;

  PROCESS_BEGIN();

  bbs_banner(board.sys_prefix, BBS_BANNER_SUBS, bbs_status.encoding_suffix, board.sys_device, 0);
  
  shell_output_str(NULL,"\n\r","");

  for (num=1; num<=BBS_MAX_BOARDS;num++){
    sprintf(message, "\x05%d:\x9e%s\x1c-\x05%d", num, board.sub_names[num], bbs_config.msg_id[num] - bbs_usrstats.current_msg[num]);
    shell_output_str(NULL,"", message);
  }


  //shell_output_str(NULL,"", PETSCII_WHITE);
  sprintf(message, "\x05\n\rselect board (1-%d):", board.max_boards);
  shell_prompt(message);

  PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
  input = data;
#ifdef BBS_BANK_BUILD
  num = bbs_parse_u16(input->data1);
#else
  num = atoi(input->data1);
#endif

  if(num>0 && num <=board.max_boards){

    bbs_status.board_id = num;
    set_prompt();
    bbs_sub_banner();
  }


  //PROCESS_EXIT();
   
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/


PROCESS(bbs_nextboard_process, "next");
SHELL_COMMAND(bbs_nextboard_command, "+", "+ : next board", &bbs_nextboard_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_nextboard_process, ev, data)
{
  PROCESS_BEGIN();
#if defined(BBS_SERIAL_TRANSPORT) && !defined(BBS_MSG_MODULE)
  PROCESS_PAUSE();
#endif

  if(bbs_status.board_id < board.max_boards){

    ++bbs_status.board_id;

    set_prompt();
    bbs_sub_banner();
  }

  //PROCESS_EXIT();
   
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

PROCESS(bbs_prevboard_process, "previous");
SHELL_COMMAND(bbs_prevboard_command, "-", "- : last board", &bbs_prevboard_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_prevboard_process, ev, data)
{
  PROCESS_BEGIN();
#if defined(BBS_SERIAL_TRANSPORT) && !defined(BBS_MSG_MODULE)
  PROCESS_PAUSE();
#endif

  if(bbs_status.board_id > 1){

    --bbs_status.board_id;

	set_prompt();
    bbs_sub_banner();
  }

  //PROCESS_EXIT();
   
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
