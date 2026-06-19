#include "bbs-bank.h"
#include "bbs-bank-msg.h"
#include "bbs-read.h"
#include "bbs-setboard.h"
#include "bbs-bank-macros.h"

PROCESS(bbs_msg_nop_process, "");
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bbs_msg_nop_process, ev, data)
{
  PROCESS_BEGIN();
  (void)ev;
  (void)data;
  PROCESS_END();
}

void
bbs_msg_set_op(const char *cmd)
{
  unsigned short num;
  unsigned char c;
  unsigned char acted;

  if(cmd == NULL || cmd[0] == '\0') {
    return;
  }
  c = (unsigned char)cmd[0];
  acted = 0u;
  switch(c) {
  case 'r':
    num = bbs_usrstats.current_msg[bbs_status.board_id] - 1u;
    if(num > 0u) {
      --bbs_usrstats.current_msg[bbs_status.board_id];
      read_msg(num);
      acted = 1u;
    }
    break;
  case '\r':
  case '\n':
    num = (unsigned short)(bbs_usrstats.current_msg[bbs_status.board_id] + 1u);
    if(num <= bbs_config.msg_id[bbs_status.board_id]) {
      ++bbs_usrstats.current_msg[bbs_status.board_id];
      read_msg(num);
      acted = 1u;
    } else {
      set_prompt();
      acted = 1u;
    }
    break;
  case '+':
    if(bbs_status.board_id < board.max_boards) {
      ++bbs_status.board_id;
      set_prompt();
      bbs_sub_banner();
      acted = 1u;
    }
    break;
  case '-':
    if(bbs_status.board_id > 1u) {
      --bbs_status.board_id;
      set_prompt();
      bbs_sub_banner();
      acted = 1u;
    }
    break;
  default:
    break;
  }
  if(acted != 0u) {
    shell_prompt(bbs_status.prompt);
  }
}

unsigned char
bbs_msg_bank_init(void)
{
  bbs_read_init();
  bbs_setboard_init();
  return 1u;
}

void
bbs_msg_bank_deinit(void)
{
  bbs_read_deinit();
  bbs_setboard_deinit();
}
