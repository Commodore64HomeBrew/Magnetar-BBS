/* bbs-post.c — compose/save board messages */

#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-post.h"
#include "bbs-file.h"
#if defined(BBS_MSG_MODULE)
#include "bbs-msg-bind.h"
#elif defined(BBS_POST_MODULE)
#include "bbs-post-bind.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(BBS_MSG_MODULE) && !defined(BBS_POST_MODULE)
extern BBS_BOARD_REC board;
extern BBS_CONFIG_REC bbs_config;
extern BBS_STATUS_REC bbs_status;
extern BBS_USER_REC bbs_user;
extern BBS_TIME_REC bbs_time;
extern BBS_USER_STATS bbs_usrstats;
extern BBS_SYSTEM_STATS bbs_sysstats;
#endif

#if defined(BBS_MSG_MODULE) || defined(BBS_POST_MODULE)
#define log_message(a, b) do { (void)(a); (void)(b); } while(0)
#endif

static char *post_buffer;
static char msg_name[12];
static char file_name[40];
static unsigned short post_cur;
static unsigned short post_room;
static unsigned short post_chunk;

static void *
bbs_post_alloc(unsigned size)
{
#if defined(BBS_MSG_MODULE) || defined(BBS_POST_MODULE)
  return bbsm_malloc_p(size);
#else
  return malloc(size);
#endif
}

static void
bbs_post_free(void *ptr)
{
#if defined(BBS_MSG_MODULE) || defined(BBS_POST_MODULE)
  bbsm_free_p(ptr);
#else
  free(ptr);
#endif
}

static void
post_echo_fix(void)
{
  if(bbs_status.echo == 2) {
    bbs_status.echo = 1;
  }
}

void
end_post(void)
{
  if(post_buffer != NULL) {
    bbs_post_free(post_buffer);
    post_buffer = NULL;
  }
  bbs_status.status = STATUS_LOCK;
  post_echo_fix();
  poke(0xd011, peek(0xd011) | 0x10);
  bordercolor(2);
  set_prompt();
}

static int
post_cmd_eq(const char *in, const char *a, const char *b)
{
  return strcmp(in, a) == 0 || strcmp(in, b) == 0;
}

static void
post_commit(char *post_buffer, char *msg_name, char *file_name)
{
  unsigned char b = bbs_status.board_id;
  unsigned short id = ++bbs_config.msg_id[b];

  sprintf(msg_name, "%d-%d", b, id);
  file_path(msg_name, id, file_name, 40);
  sprintf(file_name, "%s:%d-%d", file_name, b, id);
#ifndef BBS_MSG_MODULE
  log_message("", post_buffer);
  log_message("\x99write: ", file_name);
#endif
  cbm_save(file_name, board.subs_device, post_buffer, bbs_status.msg_size);
  bbs_path_sys_at(file_name, BBS_CFG_FILE);
  cbm_save(file_name, board.sys_device, &bbs_config, sizeof(bbs_config));
  ++bbs_usrstats.num_msgs;
  ++bbs_sysstats.daily_msgs[bbs_sysstats.day_ptr];
  end_post();
}

PROCESS(bbs_post_process, "write");
SHELL_COMMAND(bbs_post_command, "w", "w : write msg", &bbs_post_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_post_process, ev, data)
{
	struct shell_input *input;

	PROCESS_BEGIN();

        post_buffer = (char *)bbs_post_alloc((unsigned)BBS_POST_BUFFER_SIZE);
        if(post_buffer == NULL) {
          //shell_output_str(NULL, "\r\npost buffer unavailable\r\n", "");
          bbs_status.status = STATUS_LOCK;
          set_prompt();
          PROCESS_EXIT();
        }

	bordercolor(3);
	poke(0xd011, peek(0xd011) & 0xef);
	post_echo_fix();

	shell_output_str(NULL, PETSCII_LOWER, PETSCII_WHITE);
	shell_prompt("\r\nSubject:");

	bbs_status.status = STATUS_SUBJ;
	bbs_status.msg_size = 0u;
	post_buffer[0] = '\0';
#if defined(BBS_SERIAL_TRANSPORT) && !defined(BBS_MSG_MODULE)
	PROCESS_PAUSE();
#endif

	while(1) {

		PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
		input = data;

		if(bbs_status.status == STATUS_SUBJ) {
			int nw;

			update_time();
			nw = snprintf(post_buffer, BBS_POST_BUFFER_SIZE,
			    "\x1c\n\rFrom: \x05%s\x1e\n\rDate: \x05%d:%d %d/%d/%d\x9e\n\rSubj: \05%s\n\r\n\r",
			    (const char *)bbs_user.user_name,
			    (int)bbs_time.hour, (int)bbs_time.minute,
			    (int)bbs_time.day, (int)bbs_time.month, (int)bbs_time.year,
			    input->data1);
			if(nw < 0) nw = 0;
			if(nw >= BBS_POST_BUFFER_SIZE) {
				log_message("\x96", "subj cutoff");
				nw = BBS_POST_BUFFER_SIZE - 1;
			}

			bbs_status.msg_size = (unsigned short)nw;

			shell_output_str(&bbs_post_command, "\r\n\r\nOn empty line:\r\n/a=abort /s=save\r\n", "");
			if(bbs_status.echo > 0) {
				shell_output_str(&bbs_post_command, "/r=raw toggle (ctrl chars, no word wrap)\r\n", "");
			}
			shell_output_str(&bbs_post_command,
			    bbs_status.width == BBS_22_COL ? BBS_STRING_EDITH22 : BBS_STRING_EDITH40, "");

			bbs_status.status = STATUS_POST;
		}

		else if(post_cmd_eq(input->data1, "/r\x0a\x0d", "/r\x0d\x0a")) {
			if(bbs_status.echo == 1 || bbs_status.echo == 2) {
				bbs_status.echo = (bbs_status.echo == 1) ? 2 : 1;
				shell_output_str(&bbs_post_command,
				    bbs_status.echo == 2 ? "\r\nraw mode enabled\r\n" : "\r\nraw mode disabled\r\n", "");
			}
		} else if(post_cmd_eq(input->data1, "/a\x0a\x0d", "/a\x0d\x0a")) {
			log_message("\x96", "post abort!");
			end_post();
			PROCESS_EXIT();
		}

		else if(post_cmd_eq(input->data1, "/s\x0a\x0d", "/s\x0d\x0a")) {
			post_commit(post_buffer, msg_name, file_name);
			PROCESS_EXIT();
		}

		else {
			post_cur = bbs_status.msg_size;
			if(post_cur >= (unsigned short)(BBS_POST_BUFFER_SIZE - 1u)) {
				//log_message("\x96", "post buffer full");
				continue;
			}
			post_room = (unsigned short)(BBS_POST_BUFFER_SIZE - 1u - post_cur);
			post_chunk = (unsigned short)input->len1;
			if(post_chunk > post_room) {
				post_chunk = post_room;
				//log_message("\x96", "post line truncated");
			}
			memcpy(post_buffer + post_cur, input->data1, post_chunk);
			post_buffer[post_cur + post_chunk] = '\0';
			bbs_status.msg_size = (unsigned short)(post_cur + post_chunk);
		}

	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
void
bbs_post_init(void)
{
  shell_register_command(&bbs_post_command);
}

void
bbs_post_deinit(void)
{
  shell_unregister_command(&bbs_post_command);
}

