/**
 * \file
 *         bbs-post.c - post msg. to Contiki BBS message boards 
 * \author
 *         (c) 2009-2015 by Niels Haedecke <n.haedecke@unitybox.de>
 */


#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-post.h"
#include "bbs-file.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern BBS_BOARD_REC board;
extern BBS_CONFIG_REC bbs_config;
extern BBS_STATUS_REC bbs_status;
extern BBS_USER_REC bbs_user;
extern BBS_TIME_REC bbs_time;
extern BBS_USER_STATS bbs_usrstats;
extern BBS_SYSTEM_STATS bbs_sysstats;
//extern BBS_BUFFER bbs_buf;


void end_post(void){
	bbs_status.status=STATUS_LOCK;

	if (bbs_status.echo==2){bbs_status.echo=1;}

	//Turn on the screen again
	poke(0xd011, peek(0xd011) | 0x10);

	//Change border colour to red
	bordercolor(2);

	set_prompt();
}

PROCESS(bbs_post_process, "write");
SHELL_COMMAND(bbs_post_command, "w", "w : write a new message", &bbs_post_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_post_process, ev, data)
{

	//static short linecount=0;
	//static short disk_access=1;
	//static short bytes_used=2;
	struct shell_input *input;
	//static char post_buffer[BBS_MAX_MSGLINES*BBS_LINE_WIDTH];
	char post_buffer[BBS_BUFFER_SIZE];
	unsigned short num;



	ST_FILE file;

	PROCESS_BEGIN();
	
	//log_message("\x9a","posting msg...");

	//Change border colour to cyan
	bordercolor(3);

	//Blank the screen to speed things up
  	poke(0xd011, peek(0xd011) & 0xef);

	//process_exit(&bbs_read_process);
	//process_exit(&bbs_setboard_process);
	if (bbs_status.echo==2){bbs_status.echo=1;}
	
	shell_output_str(NULL,PETSCII_LOWER, PETSCII_WHITE);
	//shell_output_str(&bbs_post_command, "Subject: \r\n", "");
	
    shell_prompt("\r\nSubject:");

	

	bbs_status.status=STATUS_SUBJ;
	//bbs_status.msg_size=30;
	PROCESS_PAUSE();

	while(1) {

		PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
		input = data;

		if (bbs_status.status==STATUS_SUBJ){
			update_time();
			sprintf(post_buffer,"\x1c\n\rFrom: \x05%s\x1e\n\rDate: \x05%d:%d %d/%d/%d\x9e\n\rSubj: \05%s\n\r\n\r", bbs_user.user_name, bbs_time.hour, bbs_time.minute, bbs_time.day, bbs_time.month, bbs_time.year , input->data1);

			bbs_status.msg_size = strlen(post_buffer);


			shell_output_str(&bbs_post_command, "\r\n\r\nOn empty line:\r\n/a=abort /s=save\r\n", "");
			if (bbs_status.echo>0){
				shell_output_str(&bbs_post_command, "/r=raw toggle (ctrl chars, no word wrap)\r\n", "");
			}
			if ( bbs_status.width == BBS_22_COL) {
				shell_output_str(&bbs_post_command, BBS_STRING_EDITH22, "");
			}
			else {
				shell_output_str(&bbs_post_command, BBS_STRING_EDITH40, "");
			}

			bbs_status.status=STATUS_POST;
		}


		else if (! strcmp(input->data1, "/r\x0a\x0d") || ! strcmp(input->data1, "/r\x0d\x0a")) {
			if (bbs_status.echo==1){
				bbs_status.echo=2;
				shell_output_str(&bbs_post_command, "\r\nraw mode enabled\r\n", "");
			}
			else if (bbs_status.echo==2){
				bbs_status.echo=1;
				shell_output_str(&bbs_post_command, "\r\nraw mode disabled\r\n", "");
			}
		}


		else if (! strcmp(input->data1, "/a\x0a\x0d") || ! strcmp(input->data1, "/a\x0d\x0a")) {
			log_message("\x96","post abort!");
			//linecount=0;
			//disk_access=1;
			end_post();
			PROCESS_EXIT();
		}

		else if (! strcmp(input->data1,"/s\x0a\x0d") || ! strcmp(input->data1,"/s\x0d\x0a")){// || linecount >= BBS_MAX_MSGLINES) {

			//write post
			++bbs_config.msg_id[bbs_status.board_id];

			num = bbs_config.msg_id[bbs_status.board_id];

			
		    sprintf(file.szFileName, "%d-%d", bbs_status.board_id, num);

			log_message("", post_buffer);
			log_message("\x99write: ", file.szFileName);

		    sprintf(file.szFileName, "%s:%d-%d", file_path(file.szFileName,num), bbs_status.board_id, bbs_config.msg_id[bbs_status.board_id]);

			//Save the post to file:
			cbm_save (file.szFileName, board.subs_device, &post_buffer, bbs_status.msg_size);

			//Save the msg count struct to disk
			sprintf(file.szFileName, "@%s:%s", board.sys_prefix, BBS_CFG_FILE);
			cbm_save (file.szFileName, board.sys_device, &bbs_config, sizeof(bbs_config));

			//Clear the buffer:
			memset(post_buffer, 0, sizeof(post_buffer));

			//Increment the users msgs posted total:
			++bbs_usrstats.num_msgs;

			//Increment the dialy msgs posted total:
			++bbs_sysstats.daily_msgs[bbs_sysstats.day_ptr];

			//Clean things up:
			end_post();

			PROCESS_EXIT();
		}

		else {
			strcat(post_buffer, input->data1);
		}


	} 

	//bbs_setboard_init();
	//bbs_read_init();




	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
void
bbs_post_init(void)
{
  shell_register_command(&bbs_post_command);
}

