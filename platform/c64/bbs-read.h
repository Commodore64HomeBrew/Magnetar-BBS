/**
 * \file
 *         bbs-read.h - read msg. from Contiki BBS message boards - header file
 * \author
 *         (c) 2009-2015 by Niels Haedecke <n.haedecke@unitybox.de>
 */

#ifndef __BBS_READ_H__
#define __BBS_READ_H__

#include "bbs-shell.h"
#include "sys/log.h"
#include "bbs-defs.h"
#include "bbs-file.h"

int read_msg(unsigned short num);
void bbs_read_init(void);
void bbs_read_deinit(void);

#endif /* __BBS_READ_H__ */
