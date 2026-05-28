#include "bbs-modules.h"
#ifdef BBS_XFER_MODULE
#include "bbs-transfer.h"
#endif

static unsigned char
bbs_xfer_module_init(const bbs_module_ctx_t *ctx)
{
#ifdef BBS_XFER_MODULE
  if(bbs_xfer_bind(ctx) == 0u) {
    return 0u;
  }
  return bbs_xfer_init();
#else
  (void)ctx;
  return 0u;
#endif
}

static void
bbs_xfer_module_deinit(void)
{
#ifdef BBS_XFER_MODULE
  bbs_xfer_deinit();
#endif
}

#ifdef BBS_XFER_MODULE
static void
bbs_xfer_module_set_op(const char *cmd)
{
  bbs_xfer_set_op(cmd);
}

static void
bbs_xfer_module_feed(unsigned char c)
{
  bbs_xfer_feed(c);
}
#endif

#pragma rodata-name (push, "HEADER")
const bbs_module_iface_t bbs_xfer_module_iface = {
  { 'B', 'B', 'S', '1' },
  BBS_MODULE_ID_XFER,
  0,
  bbs_xfer_module_init,
#ifdef BBS_XFER_MODULE
  0,
  bbs_xfer_module_set_op,
  0,
  bbs_xfer_module_feed,
#else
  0,
  0,
  0,
  0,
#endif
  0,
  bbs_xfer_module_deinit
};
#pragma rodata-name (pop)
