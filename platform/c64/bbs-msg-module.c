#include "bbs-modules.h"

static const bbs_module_ctx_t *g_ctx;

static unsigned char
bbs_msg_module_init(const bbs_module_ctx_t *ctx)
{
  if(ctx == 0 || ctx->msg_init == 0 || ctx->msg_deinit == 0) {
    return 0u;
  }
  g_ctx = ctx;
  return ctx->msg_init();
}

static void
bbs_msg_module_deinit(void)
{
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
#ifdef BBS_SERIAL_TRANSPORT
  0,
  0,
#endif
  0,
  bbs_msg_module_deinit
};
#pragma rodata-name (pop)
