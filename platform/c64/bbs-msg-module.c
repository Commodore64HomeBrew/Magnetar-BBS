#include "bbs-modules.h"
#include "bbs-read.h"
#include "bbs-setboard.h"
#include "bbs-msg-bind.h"
#include "bbs-msg-extra.h"

static const bbs_module_ctx_t *g_ctx;

static unsigned char
bbs_msg_module_init(const bbs_module_ctx_t *ctx)
{
#ifdef BBS_MSG_MODULE
  if(bbs_msg_bind(ctx) == 0u) {
    return 0u;
  }
#endif
  if(ctx == 0 || ctx->msg_init == 0 || ctx->msg_deinit == 0) {
    return 0u;
  }
  g_ctx = ctx;
  if(ctx->msg_init() == 0u) {
    g_ctx = 0;
    return 0u;
  }
#ifdef BBS_MSG_MODULE
  bbs_setboard_init();
  bbs_read_init();
  if(bbs_msg_extra_bind(ctx) == 0u) {
    bbs_read_deinit();
    bbs_setboard_deinit();
    g_ctx = 0;
    return 0u;
  }
#endif
  return 1u;
}

static void
bbs_msg_module_deinit(void)
{
#ifdef BBS_MSG_MODULE
  bbs_read_deinit();
  bbs_setboard_deinit();
#endif
  if(g_ctx != 0 && g_ctx->msg_deinit != 0) {
    g_ctx->msg_deinit();
  }
  g_ctx = 0;
}

#pragma rodata-name (push, "HEADER")
const bbs_module_iface_t bbs_msg_module_iface = {
  { 'B', 'B', 'S', '1' },
  BBS_MODULE_ID_MSG,
  0,
  bbs_msg_module_init,
  0,
  0,
  0,
  0,
  0,
  bbs_msg_module_deinit
};
#pragma rodata-name (pop)
