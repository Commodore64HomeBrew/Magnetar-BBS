#include "bbs-modules.h"

#ifdef BBS_POST_MODULE
#include "bbs-post-bind.h"
#include "bbs-defs.h"
#include <string.h>
#include <stdio.h>
#endif

#ifdef BBS_POST_MODULE
static char *post_buffer;
static unsigned short post_cur;
static unsigned short post_room;
static unsigned short post_chunk;

static void post_echo_fix(void)
{
  if(bbs_status.echo == 2) {
    bbs_status.echo = 1;
  }
}

static void end_post(void)
{
  if(post_buffer != NULL) {
    bbsm_free_p(post_buffer);
    post_buffer = NULL;
  }
  bbs_status.status = STATUS_LOCK;
  post_echo_fix();
  poke(0xd011, peek(0xd011) | 0x10);
  bordercolor(2);
  set_prompt();
}

static int post_cmd_eq(const char *in, const char *a, const char *b)
{
  return strcmp(in, a) == 0 || strcmp(in, b) == 0;
}

static void post_commit(void)
{
  unsigned char b = bbs_status.board_id;
  unsigned short id = ++bbs_config.msg_id[b];
  char msg_name[12];
  char file_name[40];

  sprintf(msg_name, "%d-%d", b, id);
  file_path(msg_name, id, file_name, 40);
  sprintf(file_name, "%s:%d-%d", file_name, b, id);

  cbm_save(file_name, board.subs_device, post_buffer, bbs_status.msg_size);
  bbs_path_sys_at(file_name, BBS_CFG_FILE);
  cbm_save(file_name, board.sys_device, &bbs_config, sizeof(bbs_config));

  ++bbs_usrstats.num_msgs;
  ++bbs_sysstats.daily_msgs[bbs_sysstats.day_ptr];
  end_post();
}

unsigned char bbs_post_begin(void)
{
  post_buffer = (char *)bbsm_malloc_p((unsigned)BBS_POST_BUFFER_SIZE);
  if(post_buffer == NULL) {
    bbs_status.status = STATUS_LOCK;
    set_prompt();
    return 0u;
  }

  bordercolor(3);
  poke(0xd011, peek(0xd011) & 0xef);
  post_echo_fix();
  bbs_status.status = STATUS_SUBJ;
  bbs_status.msg_size = 0u;
  post_buffer[0] = '\0';
  shell_prompt("\n\rcmds: /s save, /c cancel\n\rsubj:");
  return 1u;
}

void bbs_post_cancel(void)
{
  end_post();
}

void bbs_post_on_input(const struct shell_input *input)
{
  int nw;

  if(input == NULL || input->data1 == NULL) {
    return;
  }

  if(bbs_status.status == STATUS_SUBJ) {
    if(post_cmd_eq(input->data1, "/c\x0a\x0d", "/c\x0d\x0a")) {
      end_post();
      return;
    }

    update_time();
    nw = snprintf(post_buffer, BBS_POST_BUFFER_SIZE,
        "\x1c\n\rFrom: \x05%s\x1e\n\rDate: \x05%d:%d %d/%d/%d\x9e\n\rSubj: \05%s\n\r\n\r",
        (const char *)bbs_user.user_name,
        (int)bbs_time.hour, (int)bbs_time.minute,
        (int)bbs_time.day, (int)bbs_time.month, (int)bbs_time.year,
        input->data1);
    if(nw < 0) nw = 0;
    if(nw >= BBS_POST_BUFFER_SIZE) {
      nw = BBS_POST_BUFFER_SIZE - 1;
    }
    bbs_status.msg_size = (unsigned short)nw;
    bbs_status.status = STATUS_POST;
    shell_prompt("\n\rmsg (end with /s):");
    return;
  }

  if(bbs_status.status == STATUS_POST) {
    if(post_cmd_eq(input->data1, "/c\x0a\x0d", "/c\x0d\x0a")) {
      end_post();
      return;
    }
    if(post_cmd_eq(input->data1, "/s\x0a\x0d", "/s\x0d\x0a")) {
      post_commit();
      return;
    }

    post_cur = bbs_status.msg_size;
    if(post_cur >= (unsigned short)(BBS_POST_BUFFER_SIZE - 1u)) {
      return;
    }
    post_room = (unsigned short)(BBS_POST_BUFFER_SIZE - 1u - post_cur);
    post_chunk = (unsigned short)input->len1;
    if(post_chunk > post_room) {
      post_chunk = post_room;
    }
    memcpy(post_buffer + post_cur, input->data1, post_chunk);
    post_buffer[post_cur + post_chunk] = '\0';
    bbs_status.msg_size = (unsigned short)(post_cur + post_chunk);
  }
}
#endif

static unsigned char
bbs_post_module_init(const bbs_module_ctx_t *ctx)
{
#ifdef BBS_POST_MODULE
  if(bbs_post_bind(ctx) == 0u) {
    return 0u;
  }
  /* Provide post handlers to core-side w process. */
  if(ctx == NULL || ctx->bbsm_post_set_handlers == NULL) {
    return 0u;
  }
  ctx->bbsm_post_set_handlers(bbs_post_begin, bbs_post_on_input, bbs_post_cancel);
  return 1u;
#else
  (void)ctx;
  return 0u;
#endif
}

static void
bbs_post_module_deinit(void)
{
#ifdef BBS_POST_MODULE
  /* Core owns the command and the process; nothing to unregister here. */
#endif
}

static void
bbs_post_module_set_op(const char *cmd)
{
  (void)cmd;
}

static void
bbs_post_module_feed(unsigned char c)
{
  (void)c;
}

#pragma rodata-name (push, "HEADER")
const bbs_module_iface_t bbs_post_module_iface = {
  { 'B', 'B', 'S', '1' },
  BBS_MODULE_ID_POST,
  0,
  bbs_post_module_init,
  0,
  bbs_post_module_set_op,
  0,
  bbs_post_module_feed,
  0,
  bbs_post_module_deinit
};
#pragma rodata-name (pop)

