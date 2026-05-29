/**
 * \file
 *         shell.c - Contiki BBS core shell based on the Contiki OS shell. 
 *
 *         Contiki OS Shell Copyright (c) 2008, Swedish Institute of Computer Science.
 *         All rights reserved.
 *
 * \author
 *         Contiki BBS shell modifications (c) 2009-2015 by Niels Haedecke <n.haedecke@unitybox.de>
 */

#include "contiki.h"
#include "contiki-lib.h"
#ifndef BBS_SERIAL_TRANSPORT
#include "bbs-resident.h"
#endif
#include "bbs-shell.h"
#include "bbs-encodings.h"
#include "bbs-msg-extra.h"
#ifndef BBS_SERIAL_TRANSPORT
#include "bbs-bank.h"
#else
#include "bbs-setboard.h"
#include "bbs-read.h"
#include "bbs-post.h"
#endif
#include "bbs-file.h"
#include "bbs-telnetd.h"
//#include <em.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

LIST(commands);

/* ROM: days per month (not modified). */
static const unsigned char month_days[12] =
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

int shell_event_input;
static struct process *front_process;
static unsigned long clock_offset;
static unsigned long last_time=0;

#ifdef BBS_SERIAL_TRANSPORT
BBS_BOARD_REC board;
BBS_CONFIG_REC bbs_config;
BBS_STATUS_REC bbs_status;
BBS_USER_REC bbs_user;
BBS_USER_STATS bbs_usrstats;
BBS_SYSTEM_STATS bbs_sysstats;
BBS_TIME_REC bbs_time;
#endif
extern BBS_BUFFER buf;

unsigned short bbs_locked=0;
unsigned short set_step=0;

#ifndef BBS_SERIAL_TRANSPORT
static unsigned char bbs_xfer_prepare_command(const char *cmd);
static unsigned char bbs_command_bank_id(const char *cmd, int len);
static unsigned char bbs_bank_route_command(const char *cmd, int len);
#endif

/*---------------------------------------------------------------------------*/
PROCESS(shell_process, "Shell");
PROCESS(shell_server_process, "Shell server");
PROCESS(bbs_timer_process, "timer");

PROCESS(bbs_login_process, "login");
SHELL_COMMAND(bbs_login_command, "login", "login  : login proc", &bbs_login_process);

#ifdef BBS_SERIAL_TRANSPORT
/* post_synch from telnet flush would re-enter a child PT (e.g. prevmsg/read); defer with process_post. */
static char shell_serial_input_line[TELNETD_CONF_LINELEN + 1];
static struct shell_input shell_serial_input_holder;
#endif

/*---------------------------------------------------------------------------*/
PROCESS(version_process, "version");
SHELL_COMMAND(version_command, "v", "v : version", &version_process);

PROCESS(help_command_process, "help");
SHELL_COMMAND(help_command, "?", "? : help", &help_command_process);

PROCESS(shell_exit_process, "exit");
SHELL_COMMAND(quit_command, "q", "q : quit", &shell_exit_process);

PROCESS(settime_process, "settime");
SHELL_COMMAND(settime_command, "t", "t : time", &settime_process);

PROCESS(sys_stats_process, "sysstats");
SHELL_COMMAND(sys_stats_command, "x", "x : bbs stats", &sys_stats_process);

PROCESS(usr_stats_process, "usrstats");
SHELL_COMMAND(usr_stats_command, "y", "y : your stats", &usr_stats_process);

PROCESS(info_process, "info");
SHELL_COMMAND(info_command, "i", "i : bbs info", &info_process);

PROCESS(movie_process, "movies");
SHELL_COMMAND(movie_command, "m", "m : movies", &movie_process);

/*---------------------------------------------------------------------------*/
void bbs_defaults(void)
{
  bbs_status.encoding=1;
  bbs_status.echo=1;
  bbs_status.wrap=0;
  bbs_status.width=BBS_40_COL;
  bbs_status.status=STATUS_UNLOCK;
  bbs_status.login=0;
  bbs_status.board_id=1;
  bbs_status.prompt[0] = '\0';

}

static int
login_token_eq(const char *in, char key)
{
  char c = in[0];

  if(c == 0 || in[1] != 0) {
    return 0;
  }
  if(key >= 'a' && key <= 'z' && c >= 'A' && c <= 'Z') {
    c += 32;
  }
  return c == key;
}
/*---------------------------------------------------------------------------*/
void set_prompt(void)
{
	unsigned short next_msg;
	unsigned char pet;

	next_msg = bbs_usrstats.current_msg[bbs_status.board_id] + 1u;
	pet = (bbs_status.encoding == 0);

	if(next_msg > bbs_config.msg_id[bbs_status.board_id]) {
		if(pet) {
			sprintf(bbs_status.prompt, "\r\n\x12\x9fsub:\x05%d\x1cmsgs:\x05%d\x92\x9f>\x05 ", bbs_status.board_id, bbs_config.msg_id[bbs_status.board_id]);
		} else {
			sprintf(bbs_status.prompt, "\r\nsub:%d msgs:%d> ", bbs_status.board_id, bbs_config.msg_id[bbs_status.board_id]);
		}
	} else {
		if(pet) {
			sprintf(bbs_status.prompt, "\r\n\x12\x9fsub:\x05%d\x1eret=\x05%d\x1c/\x05%d\x92\x9f>\x05 ", bbs_status.board_id, next_msg, bbs_config.msg_id[bbs_status.board_id]);
		} else {
			sprintf(bbs_status.prompt, "\r\nsub:%d ret=%d / %d> ", bbs_status.board_id, next_msg, bbs_config.msg_id[bbs_status.board_id]);
		}
	}
}

static void
bbs_sub_banner_core(void)
{
  unsigned char message[32];
  unsigned char file[12];

  sprintf((char *)file, "%s%d", BBS_PREFIX_SUB, bbs_status.board_id);
  bbs_banner(board.sys_prefix, file, bbs_status.encoding_suffix, board.sys_device, 0);
  sprintf((char *)message, "\x05%s\n\r", board.sub_names[bbs_status.board_id]);
  shell_output_str(NULL, (char *)message, "");
}
/*---------------------------------------------------------------------------*/
static void bbs_init(void) 
{
  unsigned short fsize;
  unsigned long set_time;
  unsigned char file[25];
  unsigned char i;

  cbm_open(4, 4, 7, "");

	sprintf(board.board_name, "\n\rCENTRONIAN BBS\n\r");
	board.telnet_port = 6400;
	board.max_boards = 8;

	board.subs_device = 8;
	sprintf(board.subs_prefix, "//s/");

	board.sys_device = 8;
	sprintf(board.sys_prefix, "//x/");

	board.user_device = 8;
	sprintf(board.user_prefix, "//u/u/");

	board.userstats_device = 8;
	sprintf(board.userstats_prefix, "//u/s/");

	board.media_device = 8;
	sprintf(board.media_prefix, "//m/");

	board.transfer_device = 8;
	sprintf(board.transfer_prefix, "//t/");
	
	/* read BBS base configuration */

	sprintf(board.sub_names[0], "blackhole");
	sprintf(board.sub_names[1], "the lounge     ");
	sprintf(board.sub_names[2], "science & tech ");
	sprintf(board.sub_names[3], "la musique     ");
	sprintf(board.sub_names[4], "hardware corner");
	sprintf(board.sub_names[5], "games & warez  ");
	sprintf(board.sub_names[6], "vic64 news     ");
	sprintf(board.sub_names[7], "great outdoors ");
	sprintf(board.sub_names[8], "member intros  ");

	board.dir_boost=1;

  bbs_time.minute=0;
  bbs_time.hour=0;
  bbs_time.day=1;
  bbs_time.month=8;
  bbs_time.year=1982;

  set_time = (unsigned long)bbs_time.minute*60 + (unsigned long)bbs_time.hour*3600;

  clock_offset =  set_time - clock_seconds();

  bbs_path_sys_colon((char *)file, BBS_CFG_FILE);
  cbm_open(10, board.sys_device, 10, file);
  cbm_read(10, &bbs_config, 2);
  fsize = cbm_read(10, &bbs_config, sizeof(bbs_config));
  cbm_close(10);

  if(fsize > 0) {
    log_message("\x99", "config loaded");
  } else {
    log_message("\x96", "config not found, using defaults");
    for(i = 0; i <= board.max_boards; ++i) {
      bbs_config.msg_id[i] = 0;
    }
  }

  bbs_path_sys_colon((char *)file, BBS_STATS_FILE);
  cbm_open(10, board.sys_device, 10, file);
  cbm_read(10, &bbs_sysstats, 2);
  fsize = cbm_read(10, &bbs_sysstats, sizeof(bbs_sysstats));
  cbm_close(10);

  if(fsize > 0) {
    log_message("\x99", "stats loaded");
  } else {
    log_message("\x96", "stats not found, using defaults");
    bbs_sysstats.caller_ptr = 0;
    bbs_sysstats.total_calls = 0;
    bbs_sysstats.total_msgs = 0;
    bbs_sysstats.day_ptr = 0;
  }

  bbs_defaults();
}
/*---------------------------------------------------------------------------*/
/* moved to bbs-msg-extra.c (msg module) */

static void
bbs_record_last_caller(void)
{
  if(++bbs_sysstats.caller_ptr >= BBS_STATS_USRS) {
    bbs_sysstats.caller_ptr = 0;
  }
  strcpy(bbs_sysstats.last_callers[bbs_sysstats.caller_ptr], bbs_user.user_name);
}
/*---------------------------------------------------------------------------*/
/* moved to bbs-msg-extra.c (msg module) */
//---------------------------------------------------------------------------
void save_stats(void)
{
	unsigned char file[25];
	char message[80];

	//Save system stats:
	bbs_path_sys_at((char *)file, BBS_STATS_FILE);
	cbm_save (file, board.sys_device, &bbs_sysstats, sizeof(bbs_sysstats));

	//Save user stats:
	sprintf(file, "@%s:s-%s", board.userstats_prefix, bbs_user.user_name);
	cbm_save (file, board.userstats_device, &bbs_usrstats, sizeof(bbs_usrstats));

  	//log_message("\x96stats file saved for: ", bbs_user.user_name);
	sprintf(message,"%d:%d %d/%d/%d - %s - %d,%d - %d,%d,%d,%d,%d,%d,%d,%d\n\r", bbs_time.hour ,bbs_time.minute, bbs_time.day, bbs_time.month, bbs_time.year, bbs_user.user_name, bbs_status.encoding, bbs_status.width, bbs_config.msg_id[1],bbs_config.msg_id[2],bbs_config.msg_id[3],bbs_config.msg_id[4],bbs_config.msg_id[5],bbs_config.msg_id[6],bbs_config.msg_id[7],bbs_config.msg_id[8]);
	cbm_write(4, message, strlen(message));
}
/*---------------------------------------------------------------------------*/
void bbs_splash(unsigned short mode) 
{
  if (mode==BBS_MODE_CONSOLE)
    log_message("\x05",BBS_COPYRIGHT_STRING);
  else
    shell_output_str(&version_command,"",BBS_COPYRIGHT_STRING);
}
/*---------------------------------------------------------------------------*/
void bbs_lock(void)
{

  log_message("\x1c","connect");

  //Change border colour to red
  bordercolor(2);
  //Blank the screen to speed things up
  //poke(0xd011, peek(0xd011) & 0xef);
  if(bbs_status.login) {
    save_stats();
    //bbs_status.login=0;
  }
  bbs_locked=1;
  bbs_defaults();
  process_start(&bbs_timer_process, NULL);
}
/*---------------------------------------------------------------------------*/
void bbs_unlock(void)
{
  //char message[20];
  log_message("\x1e","disconnect");

  //Change border colour to black
  bordercolor(0);
  //Turn on the screen again
  poke(0xd011, peek(0xd011) | 0x10);

  //Clean up any open files
  /* Do not clear stream sent count mid-logout: ACK path still drains buf.
     Clearing it retriggers the same outbound bytes → duplicate/garbled art. */
  cbm_close(10);



  bbs_status.status=STATUS_UNLOCK;
  bbs_locked=0;
  process_exit(&bbs_timer_process);
  //shell_exit();
  bbs_transport_session_close();
}
/*---------------------------------------------------------------------------*/
int bbs_get_user(char *data)
{
	unsigned short fsize=0;
	unsigned short siRet=0;
	unsigned char file[25];

	strcpy(bbs_user.user_name, data);

	sprintf(file, "%s:u-%s", board.user_prefix, bbs_user.user_name);

	siRet = cbm_open(10, board.user_device, 10, file);
	cbm_read(10, &bbs_user, 2);
	fsize = cbm_read(10, &bbs_user, sizeof(bbs_user));
	cbm_close(10);

	if (fsize > 0) {
		log_message("\x99login: ", bbs_user.user_name);
		return 1;
	}
	else{
		log_message("\x96user not found: ", bbs_user.user_name);
		return 2;
	}
}

/*---------------------------------------------------------------------------*/
int bbs_new_user(char *data)
{
	//unsigned char i;
	//unsigned char file[25];

	strcpy(bbs_user.user_pwd, data);
	bbs_user.access_req = 1;


	return 1;
}



/*---------------------------------------------------------------------------*/

int bbs_save_user()
{
  unsigned char i;
  unsigned char file[25];

  sprintf(file, "%s:u-%s",board.user_prefix, bbs_user.user_name);

  cbm_save (file, board.user_device, &bbs_user, sizeof(bbs_user));

  bbs_usrstats.num_calls=0;
  bbs_usrstats.num_msgs=0;

  for (i=0; i<=board.max_boards; i++) {
    bbs_usrstats.current_msg[i]=bbs_config.msg_id[i]-20;
  }

  return 1;
}


void bbs_login()
{

	unsigned short siRet=0;
	unsigned char file[25];
	unsigned char message[80];
	unsigned char k;
	unsigned short total_msgs=0, user_msgs=0, unread_msgs=0;

 	//**********************************************************************
	process_exit(&bbs_timer_process);
	bbs_status.status=STATUS_LOCK;

	sprintf(file, "%s:s-%s", board.userstats_prefix, bbs_user.user_name);

	siRet = cbm_open(10, board.userstats_device, 10, file);
	if (! siRet) {
		//log_message("[debug] stats file: ", file);
		cbm_read(10, &bbs_usrstats, 2);
		cbm_read(10, &bbs_usrstats, sizeof(bbs_usrstats));
		cbm_close(10);
	}
	else{
		log_message("\x96load error: ", file);
		bbs_usrstats.num_calls=0;
		bbs_usrstats.num_msgs=0;
	}

	//Set the login flag to 1:
	bbs_status.login=1;

	//Increment the users calls total:
	++bbs_usrstats.num_calls;

	//Increment the dialy calls total:
	++bbs_sysstats.daily_calls[bbs_sysstats.day_ptr];


	//Display the Centronian logo and system stats:
	bbs_banner(board.sys_prefix, BBS_BANNER_LOGO, bbs_status.encoding_suffix, board.sys_device, 0);

	//**********************************************************************

	for(k = 1; k <= BBS_MAX_BOARDS; ++k) {
		total_msgs += bbs_config.msg_id[k];
		user_msgs += bbs_usrstats.current_msg[k];
	}
	unread_msgs = total_msgs - user_msgs;

	sprintf(message,"\r\n\x9eunread msgs:\x05 %hu", unread_msgs);
	shell_output_str(NULL, message, "");


	shell_output_str(NULL, "\r\n\x05? \x9fto list commands", "");
	shell_output_str(NULL, "\x05s \x9eselect msg board\r\n", "");

#ifndef BBS_SERIAL_TRANSPORT
	if(bbs_bank_load(BBS_BANK_ID_MSG) == 0u) {
		shell_output_str(NULL, "\r\n\x96message bank unavailable\r\n", "");
	}
#endif

	//Display the sub banner:
	bbs_sub_banner_core();
	set_prompt();
	shell_prompt(bbs_status.prompt);
	process_start(&bbs_timer_process, NULL);
  front_process=&shell_process;
}


static void
login_stats_continue(void)
{
  bbs_record_last_caller();
  bbs_msg_system_stats();
  shell_output_str(NULL, "\r\nhit return to continue", "");
  bbs_status.status = STATUS_STATS;
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_login_process, ev, data)
{
  struct shell_input *input;
  int return_code;

  PROCESS_BEGIN();

  while(1) {

    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input || ev == PROCESS_EVENT_TIMER);

    if (ev == PROCESS_EVENT_TIMER) {
       //bbs_unlock();
       shell_stop();
       log_message("\x9a","event timer");
    }
    if (ev == shell_event_input) {
      input = data;
      switch (bbs_status.status) {

          case STATUS_UNLOCK: {
            if(input->len1 <= 0 ||
               (unsigned char)input->data1[0] < (unsigned char)' ') {
              break;
            }


            if(login_token_eq(input->data1, '8')) {
              bbs_status.encoding=0;
              bbs_status.wrap=1;
              bbs_status.width=BBS_80_COL;
              strcpy(bbs_status.encoding_suffix, BBS_PET80_SUFFIX);
            }

            else if(login_token_eq(input->data1, '4')) {
              bbs_status.encoding=0;
              bbs_status.wrap=1;
              strcpy(bbs_status.encoding_suffix, BBS_PET40_SUFFIX);
            }
            else if(login_token_eq(input->data1, '2')) {
              bbs_status.encoding=0;
              bbs_status.wrap=1;
              bbs_status.width=BBS_22_COL;
              strcpy(bbs_status.encoding_suffix, BBS_PET22_SUFFIX);
            }


            else if(login_token_eq(input->data1, 'l')) {
              bbs_status.echo=0;
              bbs_status.width=BBS_80_COL;
              strcpy(bbs_status.encoding_suffix, BBS_ASCII_SUFFIX);
            }
            else if(login_token_eq(input->data1, 'e')) {
              bbs_status.encoding=1;
              strcpy(bbs_status.encoding_suffix, BBS_ASCII_SUFFIX);
            }

            else if(login_token_eq(input->data1, 't')) {
              bbs_status.encoding=2;
              bbs_status.echo=1;
              strcpy(bbs_status.encoding_suffix, BBS_ASCII_SUFFIX);
            }


            else{
              shell_prompt(BBS_ENCODING_STRING);
              break;
            }
            bbs_banner(board.sys_prefix, BBS_BANNER_LOGIN, bbs_status.encoding_suffix, board.sys_device,0);
            shell_output_str(NULL, "\r\nnew users enter a new handle.", "");
            shell_prompt("\n\rhandle: ");
            bbs_status.status=STATUS_HANDLE;
            break;
          }

          case STATUS_HANDLE: {
            if((int)strlen(input->data1)>12){
              shell_output_str(NULL, "\r\nhandle can't be longer than 12 chars.", "");
              shell_prompt("\n\rhandle: ");
              bbs_status.status=STATUS_HANDLE;
              break;
            }
    			 
      			return_code = bbs_get_user(input->data1);
            //return_code=1;
      			if ( return_code == 1 ) {
      			    shell_prompt("\n\rpassword: ");
      			    bbs_status.status=STATUS_PASSWD;
      			}
      			else if ( return_code == 2 ) {
                shell_output_str(NULL,"\n\rnew user.\n\rplease enter a password.\n\r" , "");
      			    shell_prompt("\n\rpassword: ");
      			    bbs_status.status=STATUS_NEWUSR;
      			}
      			else {
      			  shell_output_str(&bbs_login_command, "login failed.", "");
      			  shell_output_str(NULL, BBS_HELP_STRING, "");

      			  //bbs_unlock();
              	  shell_stop();
      			  log_message("\x96", "login failed");
      			}
      			break;
          }

          case STATUS_PASSWD: {
            if(! strcmp(input->data1, bbs_user.user_pwd)) {

				login_stats_continue();

            } else {
              shell_output_str(NULL, "wrong password.", "");
              shell_output_str(NULL, BBS_HELP_STRING, "");
              //bbs_unlock();
              shell_stop();
              log_message("\x96", "wrong password");
            }
            break;
          }
          case STATUS_NEWUSR: {
           	  bbs_new_user(input->data1);
              
              shell_output_str(NULL,"\n\r\n\rhandle:   " , bbs_user.user_name);
              shell_output_str(NULL,"\n\rpassword: " , bbs_user.user_pwd);

              bbs_status.status=STATUS_CONFUSR;
              shell_prompt("\n\r\n\rcorrect (y/n): ");
          	break;
          }
          case STATUS_CONFUSR: {

            if(login_token_eq(input->data1, 'y')) {
              bbs_save_user();

				login_stats_continue();

              //bbs_login();
            }
            else{
              shell_prompt("\n\rhandle: ");
              bbs_status.status=STATUS_HANDLE;
            }
            break;
          }
          case STATUS_STATS: {
            bbs_login();
            break;
          }
          case STATUS_LOCK:
            break;

          default:
            break;

       }
    }
  } /* end ... while */

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
static void
command_kill(struct shell_command *c)
{
  if(c != NULL) {
    process_exit(c->process);
  }
}
/*---------------------------------------------------------------------------*/
static void
killall(void)
{
  struct shell_command *c;
  for(c = list_head(commands);
      c != NULL;
      c = c->next) {
    if(process_is_running(c->process)) {
      command_kill(c);
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_timer_process, ev, data)
{
  static struct etimer bbs_session_timer;

  PROCESS_BEGIN();
  etimer_set(&bbs_session_timer,
      bbs_status.status > STATUS_HANDLE ? BBS_SESSION_TIMEOUT : BBS_LOGIN_TIMEOUT);

  while (1) {

     PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input ||
                              etimer_expired(&bbs_session_timer));

     if(ev == shell_event_input) {
       etimer_set(&bbs_session_timer,
           bbs_status.status > STATUS_HANDLE ? BBS_SESSION_TIMEOUT : BBS_LOGIN_TIMEOUT);
     } else if(etimer_expired(&bbs_session_timer)) {
        if(bbs_status.status > STATUS_HANDLE) {
          process_post(PROCESS_BROADCAST, PROCESS_EVENT_TIMER, NULL);
        } else {
          process_post(&bbs_login_process, PROCESS_EVENT_TIMER, NULL);
        }

        shell_output_str(NULL, "timeout", "");
        etimer_set(&bbs_session_timer,
            bbs_status.status > STATUS_HANDLE ? BBS_SESSION_TIMEOUT : BBS_LOGIN_TIMEOUT);
     }
  }

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(version_process, ev, data)
{
  PROCESS_BEGIN();
#ifdef BBS_SERIAL_TRANSPORT
  PROCESS_PAUSE();
#endif

    bbs_splash(BBS_MODE_SHELL);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sys_stats_process, ev, data)
{
  PROCESS_BEGIN();
  bbs_msg_system_stats();
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(usr_stats_process, ev, data)
{
  PROCESS_BEGIN();
  bbs_msg_user_stats();
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(info_process, ev, data)
{
  PROCESS_BEGIN();
  bbs_msg_info();
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(movie_process, ev, data)
{
  unsigned short num;
  struct shell_input *input;
  unsigned char file[12];

  PROCESS_BEGIN();

  shell_output_str(NULL, "\x93\x8e", "");
  PROCESS_PAUSE();

  bbs_banner(board.sys_prefix, BBS_BANNER_MOVIE, bbs_status.encoding_suffix, board.sys_device, 0);
  shell_prompt("\x05\n\rselect movie (1-20):");

  PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
  input = data;
  num = (unsigned short)atoi(input->data1);
  sprintf((char *)file, "%s:%d", board.media_prefix, (int)num);

  if(num > 0u && num <= 21u) {
    shell_prompt("\x05\n\rselect speed (1-10) (default 1):");
    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
    input = data;
    num = (unsigned short)atoi(input->data1);

    if(num > 0u && num <= (unsigned short)MAX_STREAM_SPEED) {
      bbs_status.speed = (unsigned char)num;
    } else {
      bbs_status.speed = 1u;
    }

    bordercolor(7);
    poke(0xd011, peek(0xd011) & 0xef);
    shell_output_str(NULL, "\x93", "\x8e");
    PROCESS_PAUSE();

    cbm_open(10, board.media_device, 10, (char *)file);
    bbs_stream_set_eof_process(&movie_process);
    bbs_status.status = STATUS_STREAM;

    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input || bbs_status.status == STATUS_LOCK);
    if(ev == shell_event_input) {
      bbs_status.status = STATUS_LOCK;
      PROCESS_PAUSE();
    }

    bbs_transport_stream_clear_sent();
    cbm_close(10);
    bbs_stream_set_eof_process(NULL);
    bordercolor(2);
    poke(0xd011, peek(0xd011) | 0x10);
  }

  PROCESS_PAUSE();
  set_prompt();
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(help_command_process, ev, data)
{
  PROCESS_BEGIN();
  bbs_banner(board.sys_prefix, BBS_BANNER_MENU, bbs_status.encoding_suffix, board.sys_device, 0);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(shell_exit_process, ev, data)
{

  unsigned char file[25];
  unsigned char prefix[20];
  unsigned char enc0;

  PROCESS_BEGIN();
	enc0 = (bbs_status.encoding == 0);
	if(enc0) {
		shell_output_str(NULL, "\x8e", "");
	}

	if(enc0 && bbs_status.width > 22) {
		sprintf(prefix,"%sq/4/", board.sys_prefix);

		srand(clock_seconds());

		sprintf(file,"%d", ((rand() % 64)+1));

		bbs_banner(prefix, file, "", board.sys_device,0);

	}
	else{
		bbs_banner(board.sys_prefix, (unsigned char *)BBS_BANNER_LOGOUT,
		    bbs_status.encoding_suffix, board.sys_device, 0);

	}
	
	log_message("\x05logout: ", bbs_user.user_name);

	PROCESS_PAUSE();

	shell_stop();

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(settime_process, ev, data)
{
  struct shell_input *input;
  unsigned long set_time;
  unsigned short num;
  char message[40];
  PROCESS_BEGIN();
#ifdef BBS_SERIAL_TRANSPORT
  PROCESS_PAUSE();
#endif

  update_time();
  sprintf(message,"%d:%d %d/%d/%d\n\r", bbs_time.hour ,bbs_time.minute, bbs_time.day,  bbs_time.month, bbs_time.year);
  shell_output_str(NULL, "\n\rcurrent time: ", message);

  if (! strcmp(bbs_user.user_name, "alterus")) {

    shell_prompt("yr:");
    set_step=1;
	last_time=0;
    while(1) {

      PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);

      if (ev == shell_event_input) {
        input = data;
        num = atoi(input->data1);

        if(set_step==1) {
          bbs_time.year=num;
          set_step=2;
          shell_prompt("mon:");
        }
        else if (set_step==2) {
          bbs_time.month=num;
          set_step=3;
          shell_prompt("day:");
        }
        else if (set_step==3) {
          bbs_time.day=num;
          set_step=4;
          shell_prompt("hr:");
        }
        else if (set_step==4) {
          bbs_time.hour=num;
          set_step=5;
          shell_prompt("min:");
        }
        else if (set_step==5) {
          bbs_time.minute=num;
          set_step=0;
          sprintf(message,"%d:%d %d/%d/%d\n\r", bbs_time.hour ,bbs_time.minute, bbs_time.day,  bbs_time.month, bbs_time.year);
          shell_output_str(NULL, "\n\t\rnew time: ", message);
          //log_message("\x9enew time: ", message);

          set_time = (unsigned long)bbs_time.minute*60 + (unsigned long)bbs_time.hour*3600;
          clock_offset =  set_time - clock_seconds();
          update_time();
          break;
        }
      }
    }
  }
  //PROCESS_EXIT();
  PROCESS_END();

}



#ifndef BBS_SERIAL_TRANSPORT
static unsigned char
bbs_command_bank_id(const char *cmd, int len)
{
  if(len == 1) {
    switch(cmd[0]) {
    case '$':
    case 'u':
    case 'd':
      return BBS_BANK_ID_XFER;
    case 'w':
      return BBS_BANK_ID_POST;
    case '#':
    case 'r':
    case 's':
    case '+':
    case '-':
      return BBS_BANK_ID_MSG;
    case '\r':
    case '\n':
      return BBS_BANK_ID_MSG;
    default:
      break;
    }
  }
  if(len == 2 && cmd[1] == 'd' && (cmd[0] == 'c' || cmd[0] == 'm')) {
    return BBS_BANK_ID_XFER;
  }
  return 0u;
}

static const char *
bbs_bank_unavailable_msg(unsigned char bank_id)
{
  switch(bank_id) {
  case BBS_BANK_ID_XFER:
    return "\n\rtransfer bank unavailable\n\r";
  case BBS_BANK_ID_POST:
    return "\n\rpost bank unavailable\n\r";
  case BBS_BANK_ID_MSG:
    return "\n\rmessage bank unavailable\n\r";
  default:
    return "\n\rbank unavailable\n\r";
  }
}

static unsigned char
bbs_bank_route_command(const char *cmd, int len)
{
  unsigned char bank_id;

  bank_id = bbs_command_bank_id(cmd, len);
  if(bank_id == 0u) {
    return 1u;
  }

  if(bank_id == BBS_BANK_ID_XFER && bbs_status.status == STATUS_XFER) {
    shell_output_str(NULL, "\n\rtransfer active\n\r", "");
    return 0u;
  }

  if(front_process != &shell_process) {
    shell_output_str(NULL, "\n\rfinish current command first\n\r", "");
    return 0u;
  }

  if(bbs_bank_active() == 0u || bbs_bank_id_active() != bank_id) {
    if(bbs_bank_load(bank_id) == 0u) {
      shell_output_str(NULL, (char *)bbs_bank_unavailable_msg(bank_id), "");
      return 0u;
    }
  }
  return 1u;
}
#endif /* !BBS_SERIAL_TRANSPORT */

#ifndef BBS_SERIAL_TRANSPORT
static unsigned char
bbs_xfer_prepare_command(const char *cmd)
{
  if(bbs_bank_active() != 0u) {
    bbs_bank_set_op(cmd);
    return 1u;
  }
  (void)cmd;
  return 0u;
}
#endif

/*---------------------------------------------------------------------------*/
static struct shell_command *
start_command(char *commandline, struct shell_command *child)
{
  //char *next;
  char *args;
  int command_len;
  struct shell_command *c;

  /* Shave off any leading spaces. */
  while(*commandline == ' ') {
    commandline++;
  }

  /* Find the next command in a pipeline and start it. */
  /*next = find_pipe(commandline);
  if(next != NULL) {
    *next = 0;
    child = start_command(next + 1, child);
  }*/

  /* Separate the command arguments, and remove braces. */
  //replace_braces(commandline);
  args = strchr(commandline, ' ');
  if(args != NULL) {
    args++;
  }

  /* Shave off any trailing spaces. */
  command_len = (int)strlen(commandline);
  while(command_len > 0 && commandline[command_len - 1] == ' ') {
    commandline[command_len - 1] = 0;
    command_len--;
  }
  
  if(args == NULL) {
    command_len = (int)strlen(commandline);
    args = &commandline[command_len];
  } else {
    command_len = (int)(args - commandline - 1);
  }

#ifndef BBS_SERIAL_TRANSPORT
  if(bbs_bank_route_command(commandline, command_len) == 0u) {
    command_kill(child);
    return NULL;
  }
#endif

  
  /* Go through list of commands to find a match for the first word in
     the command line. */
  for(c = list_head(commands);
      c != NULL &&
	!(strncmp(c->command, commandline, command_len) == 0 &&
	  c->command[command_len] == 0);
      c = c->next);
  
  if(c == NULL) {
    shell_output_str(NULL, commandline, ": cmd not found (try '?')");
    command_kill(child);
    c = NULL;
  }/* else if(process_is_running(c->process) || child == c) {
    shell_output_str(NULL, commandline, ": command already running");
    c->child = NULL;
    c = NULL;
  }*/ else {
    c->child = child;
    /*    printf("shell: start_command starting '%s'\n", c->process->name);*/
    /* Start a new process for the command. */
#ifndef BBS_SERIAL_TRANSPORT
    if(bbs_command_bank_id(commandline, command_len) == BBS_BANK_ID_XFER) {
      if(bbs_xfer_prepare_command(commandline) == 0u) {
        command_kill(child);
        return NULL;
      }
      process_start(c->process, NULL);
    } else
#endif
    {
      process_start(c->process, args);
    }
  }
  
  return c;
}
/*---------------------------------------------------------------------------*/
int
shell_start_command(char *commandline, int commandline_len,
		    struct shell_command *child,
		    struct process **started_process)
{
  struct shell_command *c;
  int background = 0;

  if(commandline_len == 0) {
    if(started_process != NULL) {
      *started_process = NULL;
    }
    return SHELL_NOTHING;
  }

  if(commandline[commandline_len - 1] == '&') {
    commandline[commandline_len - 1] = 0;
    background = 1;
    commandline_len--;
  }

  c = start_command(commandline, child);

  /* Return a pointer to the started process, so that the caller can
     wait for the process to complete. */
  if(c != NULL && started_process != NULL) {
    *started_process = c->process;
    if(background) {
      return SHELL_BACKGROUND;
    } else {
      return SHELL_FOREGROUND;
    }
  }
  return SHELL_NOTHING;
}
/*---------------------------------------------------------------------------*/
static void
input_to_child_command(struct shell_command *c,
		       char *data1, int len1,
		       const char *data2, int len2)
{
  struct shell_input input;
  if(process_is_running(c->process)) {
    input.data1 = data1;
    input.len1 = len1;
    input.data2 = data2;
    input.len2 = len2;
    process_post_synch(c->process, shell_event_input, &input);
  }
}
/*---------------------------------------------------------------------------*/
void
shell_input(char *commandline, int commandline_len)
{
  struct shell_input input;
#ifdef BBS_SERIAL_TRANSPORT
  int postres;
#endif

  if(bbs_status.status == STATUS_XFER) {
    return;
  }

  /*  printf("shell_input front_process '%s'\n", front_process->name);*/

  //log_message("cmd",commandline);
  //if(commandline[0] == '~' &&
  //   commandline[1] == 'K') {
    /*    process_start(&shell_killall_process, commandline);*/
  //  if(front_process != &shell_process) {
  //    process_exit(front_process);
  //  }
  //} else {
    if(!process_is_running(front_process)) {
      front_process = &shell_process;
    }
    if(process_is_running(front_process)) {
      input.data1 = commandline;
      input.len1 = commandline_len;
      input.data2 = "";
      input.len2 = 0;
#ifdef BBS_SERIAL_TRANSPORT
      {
	int clen;

	clen = commandline_len;
	if(clen > TELNETD_CONF_LINELEN) {
	  clen = TELNETD_CONF_LINELEN;
	}
	memcpy(shell_serial_input_line, commandline, (unsigned int)clen);
	shell_serial_input_line[clen] = '\0';
	shell_serial_input_holder.data1 = shell_serial_input_line;
	shell_serial_input_holder.len1 = clen;
	shell_serial_input_holder.data2 = "";
	shell_serial_input_holder.len2 = 0;
	postres = process_post(front_process, shell_event_input,
		     &shell_serial_input_holder);
	if(postres != PROCESS_ERR_OK) {
	  front_process = &shell_process;
	  shell_serial_input_holder.data1 = shell_serial_input_line;
	  shell_serial_input_holder.len1 = clen;
	  shell_serial_input_holder.data2 = "";
	  shell_serial_input_holder.len2 = 0;
	  (void)process_post(front_process, shell_event_input,
			&shell_serial_input_holder);
	}
      }
#else
      process_post_synch(front_process, shell_event_input, &input);
#endif
    }
    if(process_is_running(&bbs_timer_process)) {
      process_post(&bbs_timer_process, shell_event_input, NULL);
    }
  //}
}

void
bbs_bank_xfer_feed(unsigned char c)
{
  bbs_bank_feed(c);
}
/*---------------------------------------------------------------------------*/
void
shell_output_str(struct shell_command *c, char *text1, char *text2)
{

	static const char crnl[2] = {ISO_cr, ISO_nl};
	//unsigned int len1,len2;

	unsigned int len1 = (int)strlen(text1);
	unsigned int len2 = (int)strlen(text2);

	if(c != NULL && c->child != NULL) {
		input_to_child_command(c->child, text1, len1, text2, len2);
	} 
	else {

	  if(len1 > 0 && text1[len1 - 1] == '\n') {
	    --len1;
	  }
	  if(len2 > 0 && text2[len2 - 1] == '\n') {
	    --len2;
	  }  

	  buf_append(text1, len1);
	  buf_append(text2, len2);
	  buf_append(crnl, 2);
	}
}
/*---------------------------------------------------------------------------*/
void
shell_register_command(struct shell_command *c)
{
  struct shell_command *i, *p;

  p = NULL;
  for(i = list_head(commands);
      i != NULL &&
	strcmp(i->command, c->command) < 0;
      i = i->next) {
    p = i;
  }
  if(p == NULL) {
    list_push(commands, c);
  } else if(i == NULL) {
    list_add(commands, c);
  } else {
    list_insert(commands, p, c);
  }
}
/*---------------------------------------------------------------------------*/
void
shell_unregister_command(struct shell_command *c)
{
  if(c == NULL) {
    return;
  }
  list_remove(commands, c);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(shell_process, ev, data)
{
  static struct process *started_process;
  struct shell_input *input;
  int ret;

  PROCESS_BEGIN();

  while(1) {
  
    PROCESS_WAIT_EVENT();

    if (ev == shell_event_input)
    {
      input = data;
      ret = shell_start_command(input->data1, input->len1, NULL,
				&started_process);
      if(started_process != NULL && ret == SHELL_FOREGROUND && process_is_running(started_process)) {
        front_process = started_process;
      } else {
	front_process = &shell_process;
      }
    }

    if (ev == PROCESS_EVENT_TIMER){
      log_message("\x9a", "timer event2");
      shell_stop();
      //bbs_unlock();
    }
    if(bbs_status.status>STATUS_HANDLE && front_process == &shell_process) {
      shell_prompt(bbs_status.prompt);
    }
  
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(shell_server_process, ev, data)
{
  struct process *p;
  struct shell_command *c;
  PROCESS_BEGIN();

  while(1) {
    PROCESS_WAIT_EVENT();
    if(ev == PROCESS_EVENT_EXITED) {
      p = data;
      if(p == front_process) {
        front_process = &shell_process;
      }
      for(c = list_head(commands);
	  c != NULL && c->process != p;
	  c = c->next);
      while(c != NULL) {
	if(c->child != NULL && c->child->process != NULL) {
	  input_to_child_command(c->child, "", 0, "", 0);
	}
	c = c->child;
      }
    }
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
void
shell_init(void)
{
  /* register BBS processes */
  list_init(commands);
  shell_register_command(&help_command);
  shell_register_command(&version_command);
  shell_register_command(&settime_command);
  shell_register_command(&quit_command);
  shell_register_command(&sys_stats_command);
  shell_register_command(&usr_stats_command);
  shell_register_command(&info_command);
  shell_register_command(&movie_command);
#ifndef BBS_SERIAL_TRANSPORT
  bbs_shared_publish();
#else
  bbs_read_init();
  bbs_setboard_init();
  bbs_post_init();
#endif

  /* local console eye candy */
  clrscr();
  bordercolor(0);
  bgcolor(0);
  textcolor(5);
  bbs_splash(BBS_MODE_CONSOLE);

  bbs_init();

  shell_event_input = process_alloc_event();

  process_start(&bbs_login_process, NULL);
  process_start(&shell_process, NULL);
  process_start(&shell_server_process, NULL);
  front_process = &bbs_login_process;

  bbs_status.status=STATUS_UNLOCK;
}
/*---------------------------------------------------------------------------*/
void
magnetar_bbs_after_autostart(void)
{
  unsigned char i;

  /* Drain autostart CONTINUE events before main loop (shell_init already started processes). */
  for(i = 0; i < 16u; ++i) {
    if(process_run() == 0 && process_nevents() == 0) {
      break;
    }
    etimer_request_poll();
  }
}
/*---------------------------------------------------------------------------*/
static int
bbs_try_lock_for_session(void)
{
  if(bbs_locked == 1) {
    bbs_transport_busy_reject();
    log_message("\x96", "busy");
    return 0;
  }
  bbs_lock();
  return 1;
}
/*---------------------------------------------------------------------------*/
void
shell_preconnect_banner(void)
{
  shell_output_str(NULL, PETSCII_LOWER, board.board_name);
  shell_prompt(BBS_ENCODING_STRING);
}
/*---------------------------------------------------------------------------*/
void
shell_start_after_probe(void)
{
  if(!bbs_try_lock_for_session()) {
    return;
  }
  front_process = &bbs_login_process;
}
/*---------------------------------------------------------------------------*/
void
shell_start(void)
{
  if(!bbs_try_lock_for_session()) {
    return;
  }
  shell_preconnect_banner();
  front_process = &bbs_login_process;
}
/*---------------------------------------------------------------------------*/
void
shell_stop(void)
{
  //log_message("\x9e", "shell stop");
#ifndef BBS_SERIAL_TRANSPORT
  bbs_bank_unload();
#endif
  bbs_unlock();
  killall();
}
/*---------------------------------------------------------------------------*/
void
shell_quit(void)
{
  //log_message("\x9e", "shell quit");
  process_exit(&bbs_login_process);
  process_exit(&bbs_timer_process);
  process_exit(&shell_process);
  process_exit(&shell_server_process);
  shell_stop();
}
/*---------------------------------------------------------------------------*/

void
update_time(void) {
  unsigned long now_sec;
  char message[40];

  now_sec = clock_seconds() + clock_offset;

  if (now_sec >  86400){
    now_sec = now_sec - 86400;
  }

  bbs_time.hour = now_sec/3600;
  bbs_time.minute = now_sec/60 - bbs_time.hour*60; 


  if (last_time > now_sec) {

	//Increment the stats day pointer:
	++bbs_sysstats.day_ptr;
	if(bbs_sysstats.day_ptr>=BBS_STATS_DAYS){
		bbs_sysstats.day_ptr=0;
	}
	bbs_sysstats.daily_msgs[bbs_sysstats.day_ptr]=0;

    if (bbs_time.day==month_days[bbs_time.month-1]){

	  //Future code to handle leap years:
      //if(bbs_status.month==2 && bbs_status.day==28 && (bbs_status.year % 4) == 0)
      
      bbs_time.day=1;

      if(bbs_time.month==12){
        bbs_time.month=1;
        ++bbs_time.year;
      }
      else{
        ++bbs_time.month;
      }
    }
    else{
      ++bbs_time.day;
    }
  }

  last_time = now_sec;
	
  //gotoxy(25,0);
  sprintf(message,"%d:%d %d/%d/%d\n\r", bbs_time.hour ,bbs_time.minute, bbs_time.day,  bbs_time.month, bbs_time.year);
  log_message("\x9e", message);

}








/** @} */
