/**
 * \file
 *         bbs-file.h - Contiki BBS file access functions - header file
 * \author
 *         (c) 2009-2015 by Niels Haedecke <n.haedecke@unitybox.de>
 */

#ifndef FILE_H
#define FILE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cbm.h>
#include <conio.h>
#include <em.h>

#define MAX_FILENAME 16
#define FILE_MODE_WRITE  0
#define FILE_MODE_APPEND 1

#define PAGE_SIZE       128                     /* Size in words */
#define BUF_SIZE        (PAGE_SIZE + PAGE_SIZE/2)

typedef struct
{
	char szFileName[MAX_FILENAME];
	unsigned char ucDeviceNo;
} ST_FILE;

void bbs_banner(unsigned char filePrefix[10], unsigned char szBannerFile[12], unsigned char fileSuffix[3], unsigned char device, unsigned char wordWrap);

/* Writes directory prefix for message num into out (NUL-terminated). outsz must be >= BBS_FILE_PATH_BUFLEN. */
void file_path(const char *file, unsigned short num, char *out, unsigned char outsz);

/* board.sys_prefix + ':' + suffix, or '@' + same (save-style paths). */
void bbs_path_sys_colon(char *out, const char *suffix);
void bbs_path_sys_at(char *out, const char *suffix);

#endif
