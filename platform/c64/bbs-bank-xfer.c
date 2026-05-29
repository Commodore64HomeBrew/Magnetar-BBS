#include "bbs-bank.h"
#include "bbs-transfer.h"

#pragma rodata-name("BANKHDR")

const bbs_bank_hdr_t bank_hdr = {
  { 'B', 'B', 'K', '1' },
  bbs_xfer_init,
  bbs_xfer_deinit,
  bbs_xfer_set_op,
  bbs_xfer_feed
};

#pragma rodata-name("CODE")
