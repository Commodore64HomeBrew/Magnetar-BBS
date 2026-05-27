#include "bbs-modules.h"

static const bbs_module_ctx_t *g_ctx;

static unsigned char
bbs_xfer_module_init(const bbs_module_ctx_t *ctx)
{
  if(ctx == 0 || ctx->xfer_init == 0 || ctx->xfer_deinit == 0) {
    return 0u;
  }
  g_ctx = ctx;
  return ctx->xfer_init();
}

static void
bbs_xfer_module_deinit(void)
{
  if(g_ctx != 0 && g_ctx->xfer_deinit != 0) {
    g_ctx->xfer_deinit();
  }
  g_ctx = 0;
}

#ifdef BBS_SERIAL_TRANSPORT
static void
bbs_xfer_module_set_op(const char *cmd)
{
  if(g_ctx != 0 && g_ctx->xfer_set_op != 0) {
    g_ctx->xfer_set_op(cmd);
  }
}
#endif

#pragma rodata-name (push, "HEADER")
const bbs_module_iface_t bbs_xfer_module_iface = {
  { 'B', 'B', 'S', '1' },
  BBS_MODULE_ID_XFER,
  0,
  bbs_xfer_module_init,
#ifdef BBS_SERIAL_TRANSPORT
  0,
  bbs_xfer_module_set_op,
#endif
  0,
  bbs_xfer_module_deinit
};
#pragma rodata-name (pop)
