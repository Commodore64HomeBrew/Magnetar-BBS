#include "bbs-modules.h"

static unsigned char
bbs_xfer_module_init(const bbs_module_ctx_t *ctx)
{
  (void)ctx;
  return 0u;
}

static void
bbs_xfer_module_deinit(void)
{
}

#pragma rodata-name (push, "HEADER")
const bbs_module_iface_t bbs_xfer_module_iface = {
  { 'B', 'B', 'S', '1' },
  BBS_MODULE_ID_XFER,
  0,
  bbs_xfer_module_init,
  0,
  bbs_xfer_module_deinit
};
#pragma rodata-name (pop)
