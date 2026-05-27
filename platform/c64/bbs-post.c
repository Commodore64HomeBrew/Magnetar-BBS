/* bbs-post.c — compose/save board messages */

#include "contiki.h"
#include "bbs-shell.h"
#include "bbs-post.h"
#include "bbs-file.h"
#include <stdio.h>
#include <string.h>

extern BBS_BOARD_REC board;
extern BBS_CONFIG_REC bbs_config;
extern BBS_STATUS_REC bbs_status;
extern BBS_USER_REC bbs_user;
extern BBS_TIME_REC bbs_time;
extern BBS_USER_STATS bbs_usrstats;
extern BBS_SYSTEM_STATS bbs_sysstats;

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
  log_message("", post_buffer);
  log_message("\x99write: ", file_name);
  cbm_save(file_name, board.subs_device, post_buffer, bbs_status.msg_size);
  bbs_path_sys_at(file_name, BBS_CFG_FILE);
  cbm_save(file_name, board.sys_device, &bbs_config, sizeof(bbs_config));
  ++bbs_usrstats.num_msgs;
  ++bbs_sysstats.daily_msgs[bbs_sysstats.day_ptr];
  end_post();
}

PROCESS(bbs_post_process, "write");
SHELL_COMMAND(bbs_post_command, "w", "w : write a new message", &bbs_post_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_post_process, ev, data)
{
	struct shell_input *input;
	char post_buffer[BBS_POST_BUFFER_SIZE];
	char msg_name[12];
	char file_name[40];

	PROCESS_BEGIN();

	bordercolor(3);
	poke(0xd011, peek(0xd011) & 0xef);
	post_echo_fix();

	shell_output_str(NULL, PETSCII_LOWER, PETSCII_WHITE);
	shell_prompt("\r\nSubject:");

	bbs_status.status = STATUS_SUBJ;
	bbs_status.msg_size = 0u;
	post_buffer[0] = '\0';
#ifdef BBS_SERIAL_TRANSPORT
	PROCESS_PAUSE();
#endif

	while(1) {

		PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input);
		input = data;

		if(bbs_status.status == STATUS_SUBJ) {
			int nw;

			update_time();
			nw = snprintf(post_buffer, sizeof(post_buffer),
			    "\x1c\n\rFrom: \x05%s\x1e\n\rDate: \x05%d:%d %d/%d/%d\x9e\n\rSubj: \05%s\n\r\n\r",
			    (const char *)bbs_user.user_name,
			    (int)bbs_time.hour, (int)bbs_time.minute,
			    (int)bbs_time.day, (int)bbs_time.month, (int)bbs_time.year,
			    input->data1);
			if(nw < 0) nw = 0;
			if(nw >= (int)sizeof(post_buffer)) {
				log_message("\x96", "post subject truncated");
				nw = (int)sizeof(post_buffer) - 1;
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
			unsigned short cur;
			unsigned short room;
			unsigned short chunk;

			cur = bbs_status.msg_size;
			if(cur >= (unsigned short)(BBS_POST_BUFFER_SIZE - 1u)) {
				log_message("\x96", "post buffer full");
				continue;
			}
			room = (unsigned short)(BBS_POST_BUFFER_SIZE - 1u - cur);
			chunk = (unsigned short)input->len1;
			if(chunk > room) {
				chunk = room;
				log_message("\x96", "post line truncated");
			}
			memcpy(post_buffer + cur, input->data1, chunk);
			post_buffer[cur + chunk] = '\0';
			bbs_status.msg_size = (unsigned short)(cur + chunk);
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

