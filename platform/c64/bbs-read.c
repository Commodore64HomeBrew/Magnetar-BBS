/**
 * \file
 *         bbs-read.c - read msg. from Contiki BBS message boards
 * \author
 *         (c) 2099-2015 by Niels Haedecke <n.haedecke@unitybox.de>
 */


#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-read.h"
#include "bbs-file.h"
#include "bbs-telnetd.h"
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



int read_msg(unsigned short num)
{
    char sub_num_prefix[BBS_FILE_PATH_BUFLEN];
    char file[12];

    shell_output_str(NULL,PETSCII_LOWER, PETSCII_WHITE);

#ifdef BBS_BANK_BUILD
    bbs_fmt_msg_id(file, bbs_status.board_id, num);
#else
    sprintf(file, "%d-%d", bbs_status.board_id, num);
#endif
    
    set_prompt();
    bbs_status.status=STATUS_READ;

    file_path(file, num, sub_num_prefix, sizeof(sub_num_prefix));
    bbs_banner((unsigned char *)sub_num_prefix, file, "", board.subs_device, bbs_status.wrap);

    bbs_status.status=STATUS_LOCK;

    return 0;
}



/*---------------------------------------------------------------------------*/
PROCESS(bbs_read_process, "read");
SHELL_COMMAND(bbs_read_command, "#", "# : select msg", &bbs_read_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_read_process, ev, data)
{

  struct shell_input *input;
  const char *inline_arg;
  char message[40];
  unsigned short num;

  PROCESS_BEGIN();

  shell_output_str(NULL,PETSCII_LOWER, PETSCII_WHITE);

  inline_arg = (const char *)data;
  if(inline_arg != NULL && inline_arg[0] != '\0') {
#ifdef BBS_BANK_BUILD
    num = bbs_parse_u16(inline_arg);
#else
    num = (unsigned short)atoi(inline_arg);
#endif
    if(num > 0u && num <= bbs_config.msg_id[bbs_status.board_id]) {
      bbs_usrstats.current_msg[bbs_status.board_id] = num;
      read_msg(num);
      PROCESS_EXIT();
    }
  }

#ifdef BBS_BANK_BUILD
  bbs_fmt_read_select_prompt(message, bbs_config.msg_id[bbs_status.board_id]);
#else
  sprintf(message, "\n\rselect msg (1-%d): ", bbs_config.msg_id[bbs_status.board_id]);
#endif
  shell_prompt(message);


  PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
  input = data;
#ifdef BBS_BANK_BUILD
  num = bbs_parse_u16(input->data1);
#else
  num = (unsigned short)atoi(input->data1);
#endif

  if(num > 0u && num <= bbs_config.msg_id[bbs_status.board_id]) {
    bbs_usrstats.current_msg[bbs_status.board_id] = num;
    read_msg(num);
  }

  //PROCESS_EXIT();

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/

PROCESS(bbs_nextmsg_process, "nextmsg");
SHELL_COMMAND(bbs_nextmsg1_command, "\x0d", "", &bbs_nextmsg_process);
SHELL_COMMAND(bbs_nextmsg2_command, "\x0a", "ret : next msg", &bbs_nextmsg_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_nextmsg_process, ev, data)
{
  unsigned short num;

  PROCESS_BEGIN();
#if defined(BBS_SERIAL_TRANSPORT) && !defined(BBS_MSG_MODULE)
  PROCESS_PAUSE();
#endif

  num = bbs_usrstats.current_msg[bbs_status.board_id]+1;

  if(num <= bbs_config.msg_id[bbs_status.board_id]){
    ++bbs_usrstats.current_msg[bbs_status.board_id];
    
    read_msg(num);
  }

  //PROCESS_EXIT();
   
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
PROCESS(bbs_prevmsg_process, "prevmsg");
SHELL_COMMAND(bbs_prevmsg_command, "r", "r : last msg", &bbs_prevmsg_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_prevmsg_process, ev, data)
{
  unsigned short num;

  PROCESS_BEGIN();
#if defined(BBS_SERIAL_TRANSPORT) && !defined(BBS_MSG_MODULE)
  PROCESS_PAUSE();
#endif

  num = bbs_usrstats.current_msg[bbs_status.board_id]-1;

  if(num>0){
    --bbs_usrstats.current_msg[bbs_status.board_id];
    
    read_msg(num);
  }

  //PROCESS_EXIT();
   
  PROCESS_END();

}



/*---------------------------------------------------------------------------*/
void
bbs_read_init(void)
{
  shell_register_command(&bbs_read_command);
  shell_register_command(&bbs_nextmsg1_command);
  shell_register_command(&bbs_nextmsg2_command);
  shell_register_command(&bbs_prevmsg_command);

}

void
bbs_read_deinit(void)
{
  shell_unregister_command(&bbs_read_command);
  shell_unregister_command(&bbs_nextmsg1_command);
  shell_unregister_command(&bbs_nextmsg2_command);
  shell_unregister_command(&bbs_prevmsg_command);
}

