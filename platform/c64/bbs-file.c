/**
 * \file
 *         bbs-file.c - Contiki BBS file access functions
 * \author
 *         (c) 2009-2015 by Niels Haedecke <n.haedecke@unitybox.de>
 */

#include "bbs-shell.h"
#include "bbs-file.h"
#include "bbs-defs.h"
#include "bbs-wrap.h"
#include "bbs-telnetd.h"
#include "bbs-encodings.h"

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <em.h>

extern BBS_STATUS_REC bbs_status;
extern BBS_BOARD_REC board;

extern BBS_BUFFER buf;
//extern telnetd_buf buf;
//static telnetd_buf buf;
extern TELNETD_STATE s;

/*---------------------------------------------------------------------------*/
/*short bbs_filesize(char *prefix, char *filename, unsigned char device)
{
    struct cbm_dirent dirent;
    unsigned short fsize=0;
    char dir[12];

    sprintf(dir,"cd%s",prefix);

    cbm_open(1, device, 15, dir);
    cbm_close(1);

    if (cbm_opendir(1, device)==0) {
        while (!cbm_readdir(1, &dirent))
            if (strstr(dirent.name, filename)) 
               fsize=dirent.size;
        cbm_closedir(1);
    }
    cbm_open(1, device, 15, "cd//");
    cbm_close(1);

    return fsize*256; 
}*/

/*---------------------------------------------------------------------------*/
void bbs_banner(unsigned char filePrefix[20], unsigned char szBannerFile[12], unsigned char fileSuffix[3], unsigned char device, unsigned char wordWrap)//, unsigned char encodeToggle) 
{
  //unsigned char *file_buffer;
  //char file_buffer[BBS_BUFFER_SIZE];

  unsigned short i=0, j=0;
  unsigned short line=0;
  unsigned short col, preCol;
  unsigned short width;
  unsigned char file[25];
  unsigned short ptr;
  unsigned char c;

  //Blank the screen to speed things up
  poke(0xd011, peek(0xd011) & 0xef);

  sprintf(file, "%s%s",szBannerFile, fileSuffix);
  log_message("\x9fread: ", file);

  //log_message("[debug] ", file);
  buf_compact();
  ptr = (unsigned short)buf.used;

  sprintf(file, "%s:%s%s",filePrefix, szBannerFile, fileSuffix);

  cbm_open(10, device, 10, file);

  if(bbs_status.status == STATUS_READ) {
    unsigned int room;
    int n;

    /* Leave space for trailing CR+NL after file data. */
    room = buf_free_bytes();
    if(room > 2u) {
      room -= 2u;
    } else {
      room = 0;
    }
    n = cbm_read(10, &buf.bufmem[ptr], (unsigned short)room);
    if(n < 0) {
      n = 0;
    }
    buf.used = (unsigned int)ptr + (unsigned int)n;

    if(bbs_status.encoding == 1) {
      petscii_to_ascii((char *)&buf.bufmem[ptr],
		       (unsigned int)(buf.used - (unsigned int)ptr));
    }

    if(buf_free_bytes() >= 2u) {
      (void)buf_putc_raw(ISO_cr);
      (void)buf_putc_raw(ISO_nl);
    }
  } else {
    unsigned int room;
    int n;

    /* File payload starts at ptr+2; ptr..ptr+1 reserved for CR+NL below. */
    if(ptr + 2u >= buf.size) {
      n = 0;
    } else {
      room = buf.size - (unsigned int)ptr - 2u;
      n = cbm_read(10, &buf.bufmem[ptr + 2], (unsigned short)room);
    }
    if(n < 0) {
      n = 0;
    }
    buf.used = (unsigned int)ptr + 2u + (unsigned int)n;
  }

  if((unsigned int)ptr + 1u < buf.size) {
    buf.bufmem[ptr] = ISO_cr;
    buf.bufmem[ptr + 1] = ISO_nl;
  }
  if(buf.used > buf.size) {
    buf.used = buf.size;
  }

  
  cbm_close(10);


  if (wordWrap==1) {
    int last_spc;

    width = bbs_status.width;
    col = 0;
    preCol = 0;
    last_spc = -1;
    for(i = ptr; i < (unsigned short)buf.used; i++) {

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
      } else if (c==0x05 || c==0x1c || c==0x1e || c==0x1f|| c==0x81 || c==0x90 || c==0x95 || c==0x96 || c==0x97 || c==0x98 || c==0x99 || c==0x9a || c==0x9b || c==0x9c || c==0x9e || c==0x9f){
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

  //Turn on the screen again
  poke(0xd011, peek(0xd011) | 0x10);
}

/*---------------------------------------------------------------------------*/
void
file_path(const char *file, unsigned short num, char *out, unsigned char outsz)
{
  if(out == NULL || outsz < 2) {
    return;
  }

  if(board.dir_boost == 1) {

    if(num < 10) {
      sprintf(out, "%s%d/0/0/0/%c/", board.subs_prefix, bbs_status.board_id, file[2]);
    } else if(num < 100) {
      sprintf(out, "%s%d/0/0/%c/%c/", board.subs_prefix, bbs_status.board_id,
              file[2], file[3]);
    } else if(num < 1000) {
      sprintf(out, "%s%d/0/%c/%c/%c/", board.subs_prefix, bbs_status.board_id,
              file[2], file[3], file[4]);
    } else {
      /* num >= 1000 (including >= 10000): deepest boost layout. */
      sprintf(out, "%s%d/%c/%c/%c/%c/", board.subs_prefix, bbs_status.board_id,
              file[2], file[3], file[4], file[5]);
    }
  } else {
    sprintf(out, "%s%d/", board.subs_prefix, bbs_status.board_id);
  }
}




void stream_file(){

  bordercolor(7);

  //Blank the screen to speed things up
  poke(0xd011, peek(0xd011) & 0xef);

  cbm_open(10, 8, 10, "//m/:terror");

  bbs_status.status = STATUS_STREAM;

}




//uip_send(&c,1);


//m/terror.prg
//m/tmnt.prg

/*---------------------------------------------------------------------------*/
/*PROCESS_THREAD(bbs_read_file, ev, data)
{
  struct shell_input *input;
  int return_code;

  PROCESS_BEGIN();

  while(1) {

    PROCESS_WAIT_EVENT_UNTIL(ev == shell_event_input || ev == PROCESS_EVENT_TIMER);

    if (ev == PROCESS_EVENT_TIMER) {
       shell_stop();
       log_message("\x9a","event timer");
    }
    if (ev == shell_event_input) {
      input = data;
      switch (bbs_status.status) {

          case STATUS_CONFUSR: {

            if(! strcmp(input->data1, "y") || ! strcmp(input->data1, "Y")){

              shell_output_str(NULL, "\r\nhit return to continue", "");
              bbs_status.status=STATUS_STATS;

            }
            else{
              shell_prompt("\n\rhandle: ");
              bbs_status.status=STATUS_HANDLE;
            }
            break;
          }
          case STATUS_STATS: {
            if(strlen(input->data1)>0) {
              bbs_login();
            }
            break;
          }

       }
    }
  }

  PROCESS_END();
}*/

/*---------------------------------------------------------------------------*/
/*
void em_load(unsigned char filePrefix[10], unsigned char szBannerFile[12], unsigned char fileSuffix[3], unsigned char device, unsigned short file_num) 
{
  unsigned char *buffer;
  unsigned short fsize=0;
  unsigned short i=0, siRet=0, len=0; 
  int *page,n;
  unsigned char file[15];
  unsigned I;
  unsigned PageCount;




  sprintf(file, "%s%s",szBannerFile,fileSuffix);

  log_message("[bbs] em loaded: ", file);

  fsize=bbs_filesize(filePrefix, file, device);

  buffer = (char*) malloc(fsize);

  memset(buffer, 0, fsize);
  siRet = cbm_open(10, device, 10, file);

  if (! siRet) {
     len = cbm_read(10, buffer, fsize);
     cbm_close(10);

  }

  shell_output_str(NULL, "\n\r", buffer);
  

  //+++++++++++++++++++++++++++++++++++++++++++++++++++

	PageCount = em_pagecount ();
    // Fill all pages 
  n=0;
	I=0;
    //for (I = 0; I < PageCount; ++I) {
	bbs_em.file[0][0] = I;	
	while (n<fsize && I < PageCount){
		++I;
    	// Set the next page:
		page = em_use (I);


        // Copy the buffer to em one page at a time:

	    for (i = 0; i < PAGE_SIZE; ++i, ++page, ++n) {
	        *page = buffer[n];
	    }
	    //Now commit the page to extended memory: 
        em_commit ();
    }

	bbs_em.file[0][1] = I;

	if (buffer != NULL)
	 free(buffer);
}
*/
/*---------------------------------------------------------------------------*/
/*
void em_out(unsigned short file_num)
{

  unsigned short i,n;
  unsigned I;
  unsigned PageCount;
  register const unsigned* em_buf;
  unsigned char *buffer;

  buffer = (char*) malloc((bbs_em.file[file_num][1] - bbs_em.file[file_num][0])*PAGE_SIZE);

  PageCount = em_pagecount ();

  if(bbs_em.file[file_num][1] <= PageCount){
    n=0;
    // Check all pages
    for (I = bbs_em.file[file_num][0]; I <= bbs_em.file[file_num][1]; ++I) {

        em_buf = em_map(I);

        //buf_append(&buf, em_buf, PageCount);

        for (i = 0; i < PAGE_SIZE; ++i, ++em_buf, ++n) {
          buffer[n] = *em_buf;
        }

        // Get the buffer and compare it
        //cmp (I, em_map (I), PAGE_SIZE, I);

    }
  }
  shell_output_str(NULL, "\n\r", buffer);
}

*/
/*---------------------------------------------------------------------------*/

/*int ssWriteSEQFile(ST_FILE *pstFile, short ssMode, void *pvBuffer, unsigned int uiBuffSize)
{
  int siRet=0;
  char szTmp[15];
  
  strcpy(szTmp,"@:");
  strcat(szTmp, pstFile->szFileName);  
  //(ssMode != 0) ? strcat(szTmp, ",s,a") : strcat(szTmp, ",s,w");
  strcat(szTmp, ",s,w");
  siRet = cbm_open(10, pstFile->ucDeviceNo, 10, szTmp);
  log_message("[bbs] *szTmp* ", szTmp);
  
  if (! siRet)
  {
     if (pvBuffer != NULL) {
        log_message("[bbs] *write* ", pvBuffer);     
        cbm_write(10, pvBuffer, uiBuffSize);   
     }
  } else {
    cbm_close(10);
    return siRet;
  }

  cbm_close(10);
    
  return siRet;
}*/
/*
int ssReadSEQFile(ST_FILE *pstFile, void *pvBuffer, unsigned int uiBuffSize)
{
  int siRet=0;
  char szTmp[15];
 
  memset(pvBuffer, 0, uiBuffSize);
 
  strcpy(szTmp, pstFile->szFileName);
  strcat(szTmp, ",s,r");
   
  siRet = cbm_open(10, pstFile->ucDeviceNo, 10, szTmp);
 
  if (! siRet)
  {
     cbm_read(10, pvBuffer, uiBuffSize);
  } else {
    cbm_close(10);
    return siRet;
  }

  cbm_close(10);

  return siRet;    
}
*/
/*int ssStreamSEQFile(ST_FILE *pstFile, void *pvBuffer, unsigned int uiBuffSize)
{
  int i;
  int siRet=0;
  char szTmp[15];
  char in[1];

  memset(pvBuffer, 0, uiBuffSize);
 
  strcpy(szTmp, pstFile->szFileName);
  strcat(szTmp, ",s,r");
   
  siRet = cbm_open(10, pstFile->ucDeviceNo, 10, szTmp);
 
  if (! siRet)
  {
    for(i=0;i<uiBuffSize;i++){
      cbm_read(10, in, 1);
      shell_output_str(NULL, in, "");
      //buf_append(&buf, in, 1);
    }  
  } else {
    cbm_close(10);
    return siRet;
  }

  cbm_close(10);

  return siRet;    
}*/


/*---------------------------------------------------------------------------*/
/*int siDriveStatus(ST_FILE *pstFile)
{
   unsigned char ucBuff[128];
   unsigned char msg[40];
   unsigned char e, t, s;


   if (cbm_open(1, pstFile->ucDeviceNo, 15, "") == 0) {
   
      if ( cbm_read(1, ucBuff, sizeof(ucBuff)) < 0) {
         return -1;
      }
      cbm_close(1);
   }

   if (sscanf(ucBuff, "%hhu, %[^,], %hhu, %hhu", &e, msg, &t, &s) != 4) {
      printf("\nparse error\n");
      puts(ucBuff);
      return -1;
   }

   //printf("\n%hhu,%s,%hhu,%hhu\n", (int) e, msg, (int) t, (int) s);

   return (int) e;
}*/
/*---------------------------------------------------------------------------*/
/*int siFileExists(ST_FILE *pstFile)
{
   unsigned char ucBuff[128];
   unsigned char szTmp[15];
   unsigned char msg[40];
   unsigned char e, t, s;
   int siRet=0;

   strcpy(szTmp,"@:");
   strcat(szTmp, pstFile->szFileName);  
   strcat(szTmp, ",p,r");

   cbm_open( 15, pstFile->ucDeviceNo, 15, NULL);
   cbm_open( 2, pstFile->ucDeviceNo,  3, pstFile->szFileName);    

   if ( cbm_read(15, ucBuff, sizeof(ucBuff)) < 0) {
      return -1;
   }

   cbm_close(15);

   if (sscanf(ucBuff, "%hhu, %[^,], %hhu, %hhu", &e, msg, &t, &s) != 4) {
      puts("parse error");
      puts(ucBuff);
      return -1;
   }

   cbm_close(2);
   cbm_close(15);

   return (int) e;
}
*/
/*unsigned char ucCheckDeviceNo(unsigned char *ucDeviceNo)
{
   if (*ucDeviceNo < 8 || *ucDeviceNo > 11)
      return 8;
   else 
   	  return *ucDeviceNo;
}*/
