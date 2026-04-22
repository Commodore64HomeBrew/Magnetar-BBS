/*
 * Copyright (c) 2003-2018, Adam Dunkels, Kevin Casteels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the Contiki desktop OS.
 *
 *
 */

#include <string.h>

#include "sys/timer.h"
#include "sys/cc.h"
#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "bbs-encodings.h"
#include "bbs-shell.h"
#include "bbs-defs.h"
#include "bbs-wrap.h"
#include "bbs-telnetd.h"

extern BBS_BOARD_REC board;
extern BBS_STATUS_REC bbs_status;
extern BBS_USER_REC bbs_user;
extern BBS_USER_STATS bbs_usrstats;
extern BBS_SYSTEM_STATS bbs_sysstats;

PROCESS(telnetd_process, "Telnet server");

AUTOSTART_PROCESSES(&telnetd_process);

#ifdef TELNETD_CONF_REJECT
extern char telnetd_reject_text[];
#else
static char telnetd_reject_text[] =
            "Too many connections, please try again later.";
#endif


/*
#define STATE_NORMAL 0
#define STATE_IAC    1
#define STATE_WILL   2
#define STATE_WONT   3
#define STATE_DO     4
#define STATE_DONT   5
#define STATE_CLOSE  6
*/

#define TELNET_IAC   255
#define TELNET_WILL  251
#define TELNET_WONT  252
#define TELNET_DO    253
#define TELNET_DONT  254
/*
#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
*/
uint8_t cr=0x0d;
uint8_t dl=0x14;
uint8_t col_num=0;
unsigned char sd_c[MAX_STREAM_SPEED];
unsigned int sd_len;

/* Echo mode 1: one terminal column for wrap/DEL sync (issue 3). */
#define TELNETD_COL1_CELL(c) \
	(((unsigned char)(c) > 0x1Fu && (unsigned char)(c) < 0x80u) \
	 || (unsigned char)(c) > 0x9Fu)
#define TELNETD_COL1_BUMP_AFTER_ECHO(c) do { \
	uint8_t col1c = (c); \
	if(col1c == ISO_cr || col1c == ISO_nl) { \
		col_num = 0; \
	} else if(col1c == 0x09u) { \
		if(col_num < bbs_status.width) { ++col_num; } \
	} else if(TELNETD_COL1_CELL(col1c)) { \
		++col_num; \
	} \
} while(0)

#define TELNETD_LAST_SPACE_NONE 255u

/* Issue 5: word-wrap breaks: PETSCII/ASCII 0x20 and ISO tab (0x09) from telnet. */
#define TELNETD_WORD_BREAK(c) \
	((unsigned char)(c) == PETSCII_SPACE || (unsigned char)(c) == 0x09u)

//static struct telnetd_buf buf;
static struct timer silence_timer;

TELNETD_STATE s;

BBS_BUFFER buf;

//static uint8_t connected;

/*---------------------------------------------------------------------------*/
static void
telnetd_rescan_last_space(void)
{
	unsigned char j;

	s.last_space_at = (unsigned char)TELNETD_LAST_SPACE_NONE;
	if(s.bufptr == 0) {
		return;
	}
	for(j = s.bufptr; j > 0u; ) {
		--j;
		if(bws_word_break(s.buf[(int)j]) != 0u) {
			s.last_space_at = j;
			break;
		}
	}
}


/*---------------------------------------------------------------------------*/
static void
buf_init(void)
{
  buf.head = 0;
  buf.used = 0;
  buf.size = BBS_BUFFER_SIZE;
}

/*---------------------------------------------------------------------------*/
void
buf_compact(void)
{
  if(buf.head == 0) {
    return;
  }
  if(buf.used > 0) {
    memmove(buf.bufmem, &buf.bufmem[buf.head], buf.used);
  }
  buf.head = 0;
}

/*---------------------------------------------------------------------------*/
static void
buf_ack_sent(unsigned int n)
{
  if(n > buf.used) {
    n = buf.used;
  }
  buf.head = (buf.head + n) % buf.size;
  buf.used -= n;
}

/*---------------------------------------------------------------------------*/
unsigned int
buf_free_bytes(void)
{
  if(buf.used >= buf.size) {
    return 0;
  }
  return (unsigned int)(buf.size - buf.used);
}

/*---------------------------------------------------------------------------*/
int
buf_putc_raw(unsigned char c)
{
  unsigned int pos;

  if(buf.used >= buf.size) {
    return -1;
  }
  pos = (buf.head + buf.used) % buf.size;
  buf.bufmem[pos] = c;
  ++buf.used;
  return 0;
}

/*---------------------------------------------------------------------------*/
int
buf_append(const char *data, int len)
{
  int copylen;
  unsigned int room;
  unsigned int dst0;
  unsigned int contig;
  unsigned int first_part;

  if(buf.used > buf.size) {
    buf.used = buf.size;
  }
  room = buf_free_bytes();
  if(room == 0 || len <= 0) {
    return 0;
  }

  copylen = MIN(len, (int)room);
  dst0 = (buf.head + buf.used) % buf.size;
  contig = buf.size - dst0;

  if((unsigned int)copylen <= contig) {
    memcpy(&buf.bufmem[dst0], data, (unsigned int)copylen);
    if(bbs_status.encoding == 1) {
      petscii_to_ascii((char *)&buf.bufmem[dst0], (unsigned int)copylen);
    }
  } else {
    first_part = contig;
    memcpy(&buf.bufmem[dst0], data, first_part);
    if(bbs_status.encoding == 1) {
      petscii_to_ascii((char *)&buf.bufmem[dst0], first_part);
    }
    memcpy(buf.bufmem, data + first_part, (unsigned int)copylen - first_part);
    if(bbs_status.encoding == 1) {
      petscii_to_ascii((char *)buf.bufmem, (unsigned int)copylen - first_part);
    }
  }
  buf.used += (unsigned int)copylen;

  return copylen;
}
/*---------------------------------------------------------------------------*/
/*static void
buf_copyto(char *to, int len)
{
  memcpy(to, &buf.bufmem[0], len);
}*/
/*---------------------------------------------------------------------------*/
void
telnetd_quit(void)
{
  shell_quit();

  process_exit(&telnetd_process);
  LOADER_UNLOAD();
}
/*---------------------------------------------------------------------------*/
void
shell_prompt(char *str)
{
  buf_append(str, (int)strlen(str));
}
/*---------------------------------------------------------------------------*/
/*void
shell_default_output(char *str1, int len1,char *str2, int len2)
{
  static const char crnl[2] = {ISO_cr, ISO_nl};
  
  //if(bbs_status.encoding==1){
    //petscii_to_ascii(&str1[0], len1);
  	//petscii_to_ascii(&str2[0], len2);
  //}

  if(len1 > 0 && str1[len1 - 1] == '\n') {
    --len1;
  }
  if(len2 > 0 && str2[len2 - 1] == '\n') {
    --len2;
  }

  //PRINTF("shell_default_output: %.*s %.*s\n", len1, str1, len2, str2);
  

  buf_append(str1, len1);
  buf_append(str2, len2);
  buf_append(crnl, sizeof(crnl));
}*/
/*---------------------------------------------------------------------------*/
/*void
shell_exit(void)
{
  //log_message("\x9e", "shell exit");
  s.state = STATE_CLOSE;
}*/
/*---------------------------------------------------------------------------*/
/*void stream_data(void){

    sd_len = cbm_read(10, &sd_c, bbs_status.speed);
    if(sd_len>0){
      uip_send(&sd_c,bbs_status.speed);
      s.numsent = bbs_status.speed;
    }
    else{
      bbs_status.status = STATUS_LOCK;
      //s.numsent = 0;
      //cbm_close(10);
      //Change boarder back to red
      //bordercolor(2);
      //Turn on the screen again
      //poke(0xd011, peek(0xd011) | 0x10);
    }
}*/
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(telnetd_process, ev, data)
{
  PROCESS_BEGIN();
  
  shell_init();

  if(bbs_status.encoding==1){petscii_to_ascii(telnetd_reject_text, strlen(telnetd_reject_text));}

  tcp_listen(UIP_HTONS(board.telnet_port));

  while(1) {
    PROCESS_WAIT_EVENT();

    if(ev == tcpip_event) {
      telnetd_appcall(data);
    } else if(ev == PROCESS_EVENT_EXIT) {
      telnetd_quit();
    }
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/*static void
acked(void)
{
  buf_pop(s.numsent);
}*/
/*---------------------------------------------------------------------------*/
/*static void
senddata(void)
{
  int len;
  len = MIN((int)buf.used, uip_mss());
  memcpy(uip_appdata, &buf.bufmem[buf.head], (unsigned int)len);
  uip_send(uip_appdata, (unsigned int)len);
  s.numsent = (unsigned int)len;
}*/
/*---------------------------------------------------------------------------*/
static void
get_char(uint8_t c)
{

	short i,n;
	//PRINTF("telnetd: get_char '%c' %d %d\n", c, c, s.bufptr);

	if(c == 0) {
		return;
	}


	/* Issue 6: echo 1 and 2 share the same line editor (wrap, DEL, column).
	   Mode 2 used to only ++col_num per byte and never wrapped at width. */
	if(bbs_status.echo==1 || bbs_status.echo==2){

		if (c == PETSCII_DEL){
			if(s.bufptr>0){
				unsigned char del_at;

				del_at = s.bufptr - 1u;
				--s.bufptr;
				s.buf[(int)s.bufptr] = 0;
				if(s.last_space_at == del_at) {
					telnetd_rescan_last_space();
				}

        (void)buf_putc_raw(c);

        if(col_num>0){--col_num;}
			}
			return;	
		}

		if(c==PETSCII_UP || c==PETSCII_DOWN || c==PETSCII_LEFT || c==PETSCII_RIGHT || c==PETSCII_CLRSCN || c==PETSCII_HOME){
			return;
		}

    //if (c > 0x1F && c < 0x80) || (c > 0x9F)


		if(col_num>=bbs_status.width){

			if(c == ISO_cr || c == ISO_nl) {
				/* Issue 2: at right margin these were skipped by the inner
				   wrap branch, so nothing was echoed while buffer logic still
				   ran — desynced display from line state. */
				(void)buf_putc_raw(c);
				col_num = 0;
			} else {

				if(TELNETD_WORD_BREAK(c))
      			{
					//jump to next line
          (void)buf_putc_raw(c);
          (void)buf_putc_raw(cr);

					col_num=0;
				}
				else
      			{
					/* Last committed char is at bufptr-1; c is not stored yet.
					   bufptr was used as erase start (issue 1): extra DEL and
					   s.buf[bufptr] was wrong index. Guard bufptr<1: no underread. */
					if((int)s.bufptr < 1) {
						(void)buf_putc_raw(cr);
						col_num = 0;
						(void)buf_putc_raw(c);
						TELNETD_COL1_BUMP_AFTER_ECHO(c);
					} else {
					/* Issue 4: use last_space_at to avoid O(n) backward scan. */
					if(s.last_space_at != (unsigned char)TELNETD_LAST_SPACE_NONE
					    && s.last_space_at < s.bufptr
					    && TELNETD_WORD_BREAK(s.buf[(int)s.last_space_at])) {
						unsigned char k;

						for(k = s.bufptr - 1u; k > s.last_space_at; --k) {
							(void)buf_putc_raw(dl);
						}
						i = (int)s.last_space_at + 1;
					} else {
					unsigned short wj, kd;

					wj = bws_find_break_back(
					    s.buf, 0u, (unsigned short)(s.bufptr - 1u), bws_word_break);
					for(kd = (unsigned short)(s.bufptr - 1u); kd > wj; --kd) {
						(void)buf_putc_raw(dl);
					}
					if(bws_word_break(s.buf[(int)wj]) != 0u) {
						i = (short)wj + 1;
					} else {
						i = 0;
					}
					}
					/* Issue 7: with no word break in the buffer, the while stops
					   at i==0 and never issues dl for s.buf[0] (i>0 is false).
					   One extra del removes that last char from the old row. */
					if(i == 0 && (int)s.bufptr > 0) {
						(void)buf_putc_raw(dl);
					}

					(void)buf_putc_raw(cr);
					for(n = i; n < (int)s.bufptr; ++n) {
						(void)buf_putc_raw((unsigned char)s.buf[n]);
					}
					col_num = (int)s.bufptr - i;
					(void)buf_putc_raw(c);
					if(c == ISO_cr || c == ISO_nl) {
						col_num = 0;
					} else if(TELNETD_COL1_CELL((unsigned char)c) || c == 0x09u) {
						++col_num;
					}
					}
				}
			}
		}
		else{	
      (void)buf_putc_raw(c);
      TELNETD_COL1_BUMP_AFTER_ECHO(c);
		}
	}

	if(c != ISO_nl){
		s.buf[(int)s.bufptr] = c;
		++s.bufptr;
		if(TELNETD_WORD_BREAK(c)) {
			s.last_space_at = s.bufptr - 1u;
		}
	}

	//if(s.bufptr == sizeof(s.buf) || c == ISO_cr || c == ISO_nl) {
	if(s.bufptr == TELNETD_CONF_LINELEN || c == ISO_cr) {

		if(bbs_status.status!=STATUS_POST){
		  if((c == ISO_cr) && s.bufptr>1){
			--s.bufptr;
			col_num = 0;
		  }
		}
		else if(c == ISO_cr){
			s.buf[(int)s.bufptr] = ISO_nl;
			++s.bufptr;
		}

		bbs_status.msg_size += s.bufptr;
		s.buf[(int)s.bufptr] = 0;
		if(bbs_status.encoding==1){ascii_to_petscii(s.buf, TELNETD_CONF_LINELEN);}
		//if(bbs_status.encoding==2){atascii_to_petscii(s.buf, TELNETD_CONF_LINELEN);}
		//PRINTF("telnetd: get_char '%.*s'\n", s.bufptr, s.buf);
		shell_input(s.buf, s.bufptr);
		s.bufptr = 0;
		s.last_space_at = (unsigned char)TELNETD_LAST_SPACE_NONE;

	}

}
/*---------------------------------------------------------------------------*/
static void
sendopt(uint8_t option, uint8_t value)
{
  char line[4];
  line[0] = (char)TELNET_IAC;
  line[1] = option;
  line[2] = value;
  line[3] = 0;
  if(bbs_status.encoding==1){ascii_to_petscii(line, 4);}
  buf_append(line, 4);
}
/*---------------------------------------------------------------------------*/
static void
newdata(void)
{
  uint16_t len;
  uint8_t c;
  uint8_t *ptr;
    
  len = uip_datalen();
  //PRINTF("newdata len %d '%.*s'\n", len, len, (char *)uip_appdata);

  ptr = uip_appdata;
  //while(len > 0 && s.bufptr < sizeof(s.buf)) {
  while(len > 0 && s.bufptr < TELNETD_CONF_LINELEN) {

    c = *ptr;
    //PRINTF("newdata char '%c' %d %d state %d\n", c, c, len, s.state);
    ++ptr;
    --len;
    switch(s.state) {
    case STATE_IAC:
      if(c == TELNET_IAC) {
		get_char(c);
		s.state = STATE_NORMAL;
      } else {
			switch(c) {
			case TELNET_WILL:
			  s.state = STATE_WILL;
			  break;
			case TELNET_WONT:
			  s.state = STATE_WONT;
			  break;
			case TELNET_DO:
			  s.state = STATE_DO;
			  break;
			case TELNET_DONT:
			  s.state = STATE_DONT;
			  break;
			default:
			  s.state = STATE_NORMAL;
			  break;
			}
      }
      break;
    case STATE_WILL:
      /* Reply with a DONT */
      sendopt(TELNET_DONT, c);
      s.state = STATE_NORMAL;
      break;
      
    case STATE_WONT:
      /* Reply with a DONT */
      sendopt(TELNET_DONT, c);
      s.state = STATE_NORMAL;
      break;
    case STATE_DO:
      /* Reply with a WONT */
      sendopt(TELNET_WONT, c);
      s.state = STATE_NORMAL;
      break;
    case STATE_DONT:
      /* Reply with a WONT */
      sendopt(TELNET_WONT, c);
      s.state = STATE_NORMAL;
      break;
    case STATE_NORMAL:
      if(c == TELNET_IAC) {
	s.state = STATE_IAC;
      } else {
	get_char(c);
      }
      break;
    }
  }
}
/*---------------------------------------------------------------------------*/
void
telnetd_appcall(void *ts)
{
  if(uip_connected()) {
    if(!s.connected) {
      buf_init();
      s.bufptr = 0;
      s.last_space_at = (unsigned char)TELNETD_LAST_SPACE_NONE;
      s.state = STATE_NORMAL;
      s.connected = 1;
      shell_start();
      timer_set(&silence_timer, BBS_IDLE_TIMEOUT);
      ts = (char *)0;
    } else {
      uip_send(telnetd_reject_text, strlen(telnetd_reject_text));
      ts = (char *)1;
    }
    tcp_markconn(uip_conn, ts);
  }

  if(!ts) {
    if(s.state == STATE_CLOSE) {
      s.state = STATE_NORMAL;
      uip_close();
      return;
    }
    if(uip_closed() ||
        uip_aborted() ||
        uip_timedout()) {
      log_message("\x9e", "telnetd stop");
      update_time();

  	  if(bbs_status.login==1){
    		save_stats();
    		bbs_status.login=0;
  	  }
      shell_stop();
    }
    if(uip_acked()) {
      timer_set(&silence_timer, BBS_IDLE_TIMEOUT);
      buf_ack_sent((unsigned int)s.numsent);
    }
    if(uip_newdata()) {
      timer_set(&silence_timer, BBS_IDLE_TIMEOUT);
      newdata();
    }
    if(uip_rexmit() ||
        uip_newdata() ||
        uip_acked() ||
        uip_connected() ||
        uip_poll()) {
      if(bbs_status.status == STATUS_STREAM){

	//File streaming code
	sd_len = cbm_read(10, &sd_c, bbs_status.speed);
	if(sd_len>0){
		uip_send(&sd_c,bbs_status.speed);
		s.numsent = bbs_status.speed;
	}
	else{
		bbs_status.status = STATUS_LOCK;
		//uip_send(&cr,1);
	}
      }
      else{
        /* Ring: send one contiguous run from head (up to MSS, up to wrap). */
        if(buf.used == 0) {
          sd_len = 0;
        } else {
          unsigned int mss;
          unsigned int first;

          mss = (unsigned int)uip_mss();
          first = buf.size - buf.head;
          sd_len = buf.used;
          if(sd_len > mss) {
            sd_len = mss;
          }
          if(sd_len > first) {
            sd_len = first;
          }
          memcpy(uip_appdata, &buf.bufmem[buf.head], sd_len);
        }
        uip_send(uip_appdata, sd_len);
        s.numsent = sd_len;
      }
      if(s.numsent > 0) {
        timer_set(&silence_timer, BBS_IDLE_TIMEOUT);
      }
    }
    if(uip_poll()) {
      if(timer_expired(&silence_timer)) {
        uip_close();
        tcp_markconn(uip_conn, NULL);
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
/*void
telnetd_init(void)
{
  process_start(&telnetd_process, NULL);
}*/
/*---------------------------------------------------------------------------*/
