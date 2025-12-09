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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern BBS_BOARD_REC board;
extern BBS_CONFIG_REC bbs_config;
extern BBS_STATUS_REC bbs_status;
extern BBS_USER_STATS bbs_usrstats;



int read_msg(unsigned short num)
{
    //short level_1, level_2;
    char sub_num_prefix[20];
    ST_FILE file;

    shell_output_str(NULL,PETSCII_LOWER, PETSCII_WHITE);

    sprintf(file.szFileName, "%d-%d", bbs_status.board_id, num);
    
    set_prompt();
    bbs_status.status=STATUS_READ;

    strcpy(sub_num_prefix, file_path(file.szFileName, num));
    bbs_banner(sub_num_prefix, file.szFileName, "", board.subs_device, bbs_status.wrap);


    bbs_status.status=STATUS_LOCK;

    return 0;
}



/*---------------------------------------------------------------------------*/
PROCESS(bbs_read_process, "read");
SHELL_COMMAND(bbs_read_command, "#", "# : select message #", &bbs_read_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_read_process, ev, data)
{

  struct shell_input *input;
  char message[40];
  unsigned short num;

  PROCESS_BEGIN();

  shell_output_str(NULL,PETSCII_LOWER, PETSCII_WHITE);

  sprintf(message, "\n\rselect msg (1-%d): ", bbs_config.msg_id[bbs_status.board_id]);
  shell_prompt(message);


  PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
  input = data;
  num = atoi(input->data1);
  bbs_usrstats.current_msg[bbs_status.board_id] = num;

  if(num>0 && num <= bbs_config.msg_id[bbs_status.board_id]){
    read_msg(num);
  }

  //PROCESS_EXIT();

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/

PROCESS(bbs_nextmsg_process, "nextmsg");
SHELL_COMMAND(bbs_nextmsg1_command, "\x0d", "", &bbs_nextmsg_process);
SHELL_COMMAND(bbs_nextmsg2_command, "\x0a", "ret : read next message", &bbs_nextmsg_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_nextmsg_process, ev, data)
{
  unsigned short num;

  PROCESS_BEGIN();

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
SHELL_COMMAND(bbs_prevmsg_command, "r", "r : read last message", &bbs_prevmsg_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_prevmsg_process, ev, data)
{
  unsigned short num;

  PROCESS_BEGIN();

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

