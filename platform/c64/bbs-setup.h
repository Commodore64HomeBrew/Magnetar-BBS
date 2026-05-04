/**
 * \file
 *         bbs-setup.h - main program of the Contiki BBS setup program - header file
 * \author
 *         (c) 2009-2015 by Niels Haedecke <n.haedecke@unitybox.de>
 */

#ifndef __BBS_SETUP_H__
#define __BBS_SETUP_H__
#include <conio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cbm.h>

//#include "bbs-file.h"
#include "bbs-defs.h"


extern BBS_BOARD_REC board;
extern BBS_CONFIG_REC bbs_config;
extern BBS_USER_REC bbs_user;
extern BBS_USER_STATS bbs_usrstats;
extern BBS_SYSTEM_STATS bbs_sysstats;


typedef struct
{
	char szFileName[16];
	unsigned char ucDeviceNo;
} ST_FILE;

typedef struct {
   unsigned int   srvip[4];
   unsigned int   netmask[4];
   unsigned int   gateway[4];
   unsigned int   nameserv[4];
   unsigned int   mem;
   unsigned char  driver[15];
} CTK_CFG_REC; 

void mainMenu(void);
void clearScreen(void);
int nibbleIP(unsigned char *src, unsigned int *addr);
int networkSetup(unsigned short drive);
void bbs_setup();

#endif /* __BBS_SETUP_H__ */
