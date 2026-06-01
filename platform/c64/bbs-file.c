/**
 * \file
 *         bbs-file.c - Contiki BBS file access functions
 * \author
 *         (c) 2009-2015 by Niels Haedecke <n.haedecke@unitybox.de>
 */

#include "contiki.h"

#pragma bss-name("LOWBSS")

#include "bbs-resident.h"
#include "bbs-shell.h"
#include "bbs-file.h"
#include "bbs-defs.h"
#include "bbs-wrap.h"
#include "bbs-telnetd.h"
#include "bbs-encodings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <em.h>

extern BBS_BUFFER buf;

/*---------------------------------------------------------------------------*/
/* read_layout: 1 = STATUS_READ (read at ptr, reserve 2 for CRLF append); 0 = payload at ptr+2, CR+NL at ptr..ptr+1. */
static void
banner_load_file_payload(unsigned short ptr, unsigned char read_layout)
{
  unsigned char *dst;
  unsigned int room;
  int n;

  if(read_layout != 0u) {
    unsigned int pos;
    unsigned int contig;

    pos = ((unsigned int)buf.head + (unsigned int)buf.used) % buf.size;
    room = buf_free_bytes();
    if(room > 2u) {
      room -= 2u;
    } else {
      room = 0u;
    }
    contig = buf.size - pos;
    if(room > contig) {
      room = contig;
    }
    if(room > 0u) {
      dst = &buf.bufmem[pos];
      n = cbm_read(10, dst, (unsigned short)room);
      if(n < 0) {
        n = 0;
      }
      if(bbs_status.encoding == 1) {
        petscii_to_ascii((char *)dst, (unsigned int)n);
      }
      buf.used += (unsigned int)n;
      if(buf.used > buf.size) {
        buf.used = buf.size;
      }
    }
    if(buf_free_bytes() >= 2u) {
      (void)buf_putc_raw(ISO_cr);
      (void)buf_putc_raw(ISO_nl);
    }
  } else {
    if((unsigned int)ptr + 2u >= buf.size) {
      n = 0;
    } else {
      room = buf.size - (unsigned int)ptr - 2u;
      dst = &buf.bufmem[ptr + 2];
      n = cbm_read(10, dst, (unsigned short)room);
    }
    if(n < 0) {
      n = 0;
    }
    buf.used = (unsigned int)ptr + 2u + (unsigned int)n;
  }
}

void bbs_banner(unsigned char filePrefix[20], unsigned char szBannerFile[12], unsigned char fileSuffix[3], unsigned char device, unsigned char wordWrap)//, unsigned char encodeToggle) 
{
  unsigned short i=0, j=0;
  unsigned short line=0;
  unsigned short col, preCol;
  unsigned short width;
  unsigned char file[BBS_FILE_PATH_BUFLEN + 20];
  unsigned short ptr;
  unsigned char read_layout;
  unsigned char c;

  read_layout = (unsigned char)(bbs_status.status == STATUS_READ ? 1u : 0u);

  /* Blank screen for banners only; message read uses same RAM as the ring. */
  if(read_layout == 0u) {
    poke(0xd011, peek(0xd011) & 0xef);
  }

  sprintf(file, "%s%s",szBannerFile, fileSuffix);
  log_message("\x9fread: ", file);

  //log_message("[debug] ", file);
#ifdef BBS_SERIAL_TRANSPORT
  bbs_serial_banner_begin();
#endif
  buf_compact();
#ifdef BBS_SERIAL_TRANSPORT
  /* Single cbm_read is capped by free ring space; push pending TX out first. */
  bbs_serial_drain_wire();
#endif
  ptr = (unsigned short)(((unsigned int)buf.head + (unsigned int)buf.used) %
      buf.size);

  sprintf(file, "%s:%s%s", filePrefix, szBannerFile, fileSuffix);

  cbm_open(10, device, 10, file);

  banner_load_file_payload(ptr, read_layout);

  /* Banner layout reserves CR+NL before payload; read layout adds CRLF via load. */
  if(read_layout == 0u && (unsigned int)ptr + 1u < buf.size) {
    buf.bufmem[ptr] = ISO_cr;
    buf.bufmem[ptr + 1] = ISO_nl;
  }
  if(buf.used > buf.size) {
    buf.used = buf.size;
  }

  
  cbm_close(10);


  if(wordWrap == 1u && read_layout == 0u) {
    int last_spc;

    width = bbs_status.width;
    col = 0;
    preCol = 0;
    last_spc = -1;
    if(ptr < (unsigned short)buf.used) {
    for(i = ptr;
        i < (unsigned short)buf.used && i < (unsigned short)buf.size;
        i++) {

      c = buf.bufmem[i];

      if (col == width) {
        /* Hint from forward pass (last word-break on this row) avoids O(n) backward walk. */
        if(last_spc >= (int)preCol
		    && last_spc <= (int)i
		    && BWS_SPACE_ONLY(buf.bufmem[last_spc]) != 0u) {
          j = (unsigned short)last_spc;
        } else {
          j = bws_find_break_back(
              buf.bufmem, preCol, i, BWS_FIND_MODE_BANNER);
        }
        if(bbs_status.encoding==1) {
          buf.bufmem[j] = ISO_nl;
        } else {
          buf.bufmem[j] = ISO_cr;
        }
        preCol = j;
        col = (unsigned short)(i - j);
        ++line;
        last_spc = -1;
      } else if (c == ISO_cr || c == ISO_nl) {
      	col=0;
        ++line;
        last_spc = -1;
      } else if(BBS_PETSCII_ATTR0(c) != 0u) {
        /* no column advance; last_spc unchanged */
      } else if(c==PETSCII_UP || c==PETSCII_DOWN || c==PETSCII_LEFT || c==PETSCII_RIGHT || c==PETSCII_CLRSCN || c==PETSCII_HOME) {

        if(c==PETSCII_LEFT) {
          if(col>0) {
            --col;
          }
          last_spc = -1;
        } else if(c==PETSCII_RIGHT) {
          ++col;
          last_spc = -1;
        } else if(c==PETSCII_UP) {
          if(line>0) {
            --line;
          }
          col=0;
          last_spc = -1;
        } else if(c==PETSCII_DOWN) {
          ++line;
          col=0;
          last_spc = -1;
        } else if(c==PETSCII_HOME || c==PETSCII_CLRSCN) {
          col=0;
          line=0;
          last_spc = -1;
        }
      } else {
        ++col;
        if(BWS_SPACE_ONLY(c) != 0u) {
          last_spc = (int)i;
        }
      }
    }
    }
  }

  if(read_layout == 0u) {
    poke(0xd011, peek(0xd011) | 0x10);
  }
#ifdef BBS_SERIAL_TRANSPORT
  bbs_serial_flush_outbound();
  bbs_serial_banner_end();
#endif
}

/*---------------------------------------------------------------------------*/
void
bbs_path_sys_colon(char *out, const char *suffix)
{
  sprintf(out, "%s:%s", board.sys_prefix, suffix);
}

void
bbs_path_sys_at(char *out, const char *suffix)
{
  sprintf(out, "@%s:%s", board.sys_prefix, suffix);
}

/*---------------------------------------------------------------------------*/
void
file_path(const char *file, unsigned short num, char *out, unsigned char outsz)
{
  const char *pfx;
  unsigned char bid;

  if(out == NULL || outsz < 2) {
    return;
  }

  pfx = board.subs_prefix;
  bid = bbs_status.board_id;

  if(board.dir_boost != 1) {
    sprintf(out, "%s%d/", pfx, (int)bid);
    return;
  }

  if(num < 10u) {
    sprintf(out, "%s%d/0/0/0/%c/", pfx, (int)bid, (int)file[2]);
  } else if(num < 100u) {
    sprintf(out, "%s%d/0/0/%c/%c/", pfx, (int)bid,
        (int)file[2], (int)file[3]);
  } else if(num < 1000u) {
    sprintf(out, "%s%d/0/%c/%c/%c/", pfx, (int)bid,
        (int)file[2], (int)file[3], (int)file[4]);
  } else {
    sprintf(out, "%s%d/%c/%c/%c/%c/", pfx, (int)bid,
        (int)file[2], (int)file[3], (int)file[4], (int)file[5]);
  }
}
