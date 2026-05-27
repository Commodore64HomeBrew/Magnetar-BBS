/**
 * \file
 *         bbs-post.h - post msg. to Contiki BBS message boards - header file
 * \author
 *         (c) 2009-2015 by Niels Haedecke <n.haedecke@unitybox.de>
 */

#ifndef __BBS_POST_H__
#define __BBS_POST_H__

#include "sys/log.h"
#include "bbs-shell.h"
#include "bbs-defs.h"

void bbs_post_init(void);
void bbs_post_deinit(void);

#endif /* __BBS_POST_H__ */
