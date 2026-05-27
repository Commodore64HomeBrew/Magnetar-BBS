/**
 * \file
 *         bbs-setboard.h - select Contiki BBS message boards - header file
 *
 * \author
 *         (c) 2009-2015 by Niels Haedecke <n.haedecke@unitybox.de>
 */

#ifndef __BBS_SETBOARD_H__
#define __BBS_SETBOARD_H__

#include "bbs-shell.h"
#include "sys/log.h"
#include "bbs-defs.h"

void bbs_setboard_init(void);
void bbs_setboard_deinit(void);
void bbs_sub_banner(void);

#endif /* __BBS_SETBOARD_H__ */
