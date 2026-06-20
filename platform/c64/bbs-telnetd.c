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
#ifndef BBS_SERIAL_TRANSPORT
#include "contiki-net.h"
#else
#include <serial.h>
#include <time.h> /* CLK_TCK: jiffies/sec, same basis as clock_seconds() in settime */
#endif
#include "bbs-encodings.h"
#include "bbs-resident.h"
#include "bbs-shell.h"
#include "bbs-bank.h"
#include "bbs-defs.h"
#include "bbs-wrap.h"
#include "bbs-telnetd.h"
extern unsigned short bbs_locked;

PROCESS(telnetd_process, "Telnet server");

/* AUTOSTART is in contiki-bbs.c (.co, -DAUTOSTART_ENABLE); this .c is plain .o. */

#ifdef TELNETD_CONF_REJECT
extern char telnetd_reject_text[];
#endif

#define TELNET_IAC   255
#define TELNET_WILL  251
#define TELNET_WONT  252
#define TELNET_DO    253
#define TELNET_DONT  254

static unsigned char col_num;
static unsigned char sd_c[MAX_STREAM_SPEED];
static unsigned int sd_len;
/* Ignore trailing CR/LF from the speed-selection line when stream starts. */
static unsigned char telnetd_stream_rx_grace;

static unsigned char col1_cell(unsigned char c)
{
  return (unsigned char)((c > 0x1Fu && c < 0x80u) || c > 0x9Fu);
}

static void col1_bump(unsigned char c)
{
  if(c == ISO_cr || c == ISO_nl) {
    col_num = 0;
  } else if(BBS_PETSCII_ATTR0(c)) {
  } else if(c == 0x09u) {
    if(col_num < bbs_status.width) {
      ++col_num;
    }
  } else if(col1_cell(c)) {
    ++col_num;
  }
}

#define TELNETD_LAST_SPACE_NONE 255u

/* PETSCII soft-wrap: bp[row]=buf index of row start (POP column from buf, no tail array). */
#define TELWRAP_MAX_ROWS TELNETD_CONF_NUMLINES
static unsigned char telwrap_bp[TELWRAP_MAX_ROWS];
static unsigned char telwrap_tr;

#define TELWRAP_LINE_RESET() do { \
	telwrap_tr = 0; \
	telwrap_bp[0] = 0u; \
} while(0)

/* After CR-ended line, absorb one following LF (Telnet NVT / CRLF bridges). */
static unsigned char telnetd_ignore_lf_after_cr;

static struct timer silence_timer;

TELNETD_STATE s;

BBS_BUFFER buf = {
  (unsigned char *)BBS_BUFFER_SCR_BASE,
  0u,
  0u,
  BBS_BUFFER_SIZE
};

static struct process *bbs_stream_eof_process;

void
bbs_stream_set_eof_process(struct process *p)
{
  bbs_stream_eof_process = p;
}

/* Stream hits EOF while movie PT waits in PROCESS_YIELD_UNTIL; POLL resumes it */
static void
bbs_notice_stream_eof(void)
{
  bbs_status.status = STATUS_LOCK;
  if(bbs_stream_eof_process != NULL) {
    process_poll(bbs_stream_eof_process);
  }
}

void
telnetd_discard_pending_rx(void)
{
  s.bufptr = 0;
  s.last_space_at = (unsigned char)TELNETD_LAST_SPACE_NONE;
  telnetd_ignore_lf_after_cr = 0u;
  col_num = 0;
  TELWRAP_LINE_RESET();
}

static void telnetd_feed(const unsigned char *ptr, unsigned int len);

#ifndef BBS_SERIAL_TRANSPORT
static unsigned int
telnetd_movie_read_req(void)
{
  unsigned int req = (unsigned int)bbs_status.speed;

  if(req < 1u) {
    req = 1u;
  }
  if(req > (unsigned int)MAX_STREAM_SPEED) {
    req = (unsigned int)MAX_STREAM_SPEED;
  }
  return req;
}

/* uIP can notify this back-end for multiple TCP connections.
   This BBS shell is single-session; keep the active/primary uip_conn here
   so a secondary connection can't stop/steal the global shell state. */
static struct uip_conn *primary_conn;
/* 1 until first inbound segment is consumed (probe reject check). */
static unsigned char telnetd_tcp_firstrx;
/* 1 while a ring slice is in flight (wait for uip_acked before sending from head again). */
static unsigned char telnetd_tcp_tx_pending;
/* 1: unacked TCP payload came from STATUS_STREAM (sd_c), not the shell ring. */
static unsigned char telnetd_tcp_stream_unacked;
#ifndef BBS_SERIAL_TRANSPORT
static unsigned char transport_flush_depth;
#endif
/* strlen(telnetd_reject_text) after optional PETSCII→ASCII in process init. */
static unsigned int telnetd_reject_len;
#else
/* 1 until first inbound byte opens session (like TCP: no shell until peer talks). */
static unsigned char serial_waiting_peer;
static unsigned char serial_banner_depth;
static void telnetd_serial_on_connect(void);
static void telnetd_serial_disconnect(void);
static unsigned int telnetd_serial_write(const void *vd, unsigned int len);
static void telnetd_serial_tx(void);
#endif

/*---------------------------------------------------------------------------*/
static void
telnetd_rescan_last_space(void)
{
	unsigned short wj;

	if(s.bufptr == 0u) {
		s.last_space_at = (unsigned char)TELNETD_LAST_SPACE_NONE;
		return;
	}
	wj = bws_find_break_back(s.buf, 0u, (unsigned short)(s.bufptr - 1u),
	    BWS_FIND_MODE_TELNET);
	if(wj < (unsigned short)s.bufptr &&
	    BWS_WORD_BREAK(s.buf[(int)wj]) != 0u) {
		s.last_space_at = (unsigned char)wj;
	} else {
		s.last_space_at = (unsigned char)TELNETD_LAST_SPACE_NONE;
	}
}

/*---------------------------------------------------------------------------*/
static unsigned char
telwrap_ttl_trimmed(unsigned short rs, unsigned short ex, unsigned char wmax)
{
	unsigned short bx, j;
	unsigned char sim, ch;

	bx = ex;
	while(bx > rs && BWS_WORD_BREAK((unsigned char)s.buf[(int)(bx - 1u)]) != 0u) {
		--bx;
	}
	sim = 0;
	for(j = rs; j < bx; ++j) {
		ch = (unsigned char)s.buf[(int)j];
		if(ch == ISO_cr || ch == ISO_nl) {
			sim = 0;
			continue;
		}
		if(ch == PETSCII_UP || ch == PETSCII_DOWN || ch == PETSCII_LEFT ||
		    ch == PETSCII_RIGHT || ch == PETSCII_CLRSCN || ch == PETSCII_HOME) {
			continue;
		}
		if(BBS_PETSCII_ATTR0(ch)) {
			continue;
		}
		if(ch == 0x09u) {
			if(sim < wmax) {
				++sim;
			}
		} else if(col1_cell(ch)) {
			++sim;
		}
	}
	if(sim > wmax && wmax != 0u) {
		sim = wmax - 1u;
	}
	return sim;
}

/*---------------------------------------------------------------------------*/
static void
telwrap_echo_dls(unsigned char hi, unsigned char lo)
{
	unsigned char k;

	for(k = hi; k > lo; --k) {
		(void)buf_putc_raw(PETSCII_DEL);
	}
}

/*---------------------------------------------------------------------------*/
static void
telwrap_pop_paint(void)
{
	unsigned char nsteps, w;
	unsigned short rs;

	w = (unsigned char)bbs_status.width;
	rs = (unsigned short)telwrap_bp[(int)telwrap_tr];
	col_num = telwrap_ttl_trimmed(rs, (unsigned short)s.bufptr, w);
	if(col_num < w && s.bufptr > (unsigned char)rs &&
	    BWS_WORD_BREAK((unsigned char)s.buf[(int)s.bufptr - 1u]) != 0u)
		++col_num;
	if(bbs_status.encoding != 0u) {
		return;
	}
	(void)buf_putc_raw(PETSCII_UP);
	nsteps = col_num;
	while(nsteps != 0u) {
		(void)buf_putc_raw(PETSCII_RIGHT);
		--nsteps;
	}
}

/*---------------------------------------------------------------------------*/
static void
telwrap_fixup_delete(unsigned char del_idx)
{
	unsigned char k;

	for(k = telwrap_tr; k != 0u; --k) {
		if(telwrap_bp[k] > del_idx) {
			--telwrap_bp[k];
		}
	}
}

/*---------------------------------------------------------------------------*/
static void
telwrap_commit_row_finish(unsigned char next_first_idx)
{
	if(telwrap_tr + 1u < TELWRAP_MAX_ROWS) {
		++telwrap_tr;
		telwrap_bp[telwrap_tr] = next_first_idx;
	}
}


/*---------------------------------------------------------------------------*/
void
bbs_scr_layout_output(void)
{
  buf.bufmem = (unsigned char *)BBS_BUFFER_SCR_BASE;
  buf.size = BBS_BUFFER_SIZE;
  buf.head = 0u;
  buf.used = 0u;
}

void
bbs_scr_layout_xfer(void)
{
  buf.bufmem = (unsigned char *)BBS_XFER_SCR_TX_BASE;
  buf.size = BBS_XFER_SCR_TX_SIZE;
  buf.head = 0u;
  buf.used = 0u;
}

static void
buf_scr_guard(void)
{
  if(bbs_status.status == STATUS_XFER) {
    if(buf.bufmem != (unsigned char *)BBS_XFER_SCR_TX_BASE ||
        buf.size != BBS_XFER_SCR_TX_SIZE) {
      bbs_scr_layout_xfer();
    }
  } else {
    if(buf.bufmem != (unsigned char *)BBS_BUFFER_SCR_BASE ||
        buf.size != BBS_BUFFER_SIZE) {
      bbs_scr_layout_output();
    }
  }
}

static void
buf_ring_memset_safe(unsigned char fill)
{
  unsigned char den;

  /* Ring is VIC screen RAM; blank DEN while clearing so the CPU isn't fighting the beam. */
  den = *(volatile unsigned char *)0xD011;
  *(volatile unsigned char *)0xD011 = (unsigned char)(den & (unsigned char)~0x10u);
  memset(buf.bufmem, fill, buf.size);
  *(volatile unsigned char *)0xD011 = den;
}

/*
 * Outbound ring uses screen RAM ($0400+). Policy:
 * - bbs_transport_buf_discard(): pointer reset only; safe mid-session (bank load,
 *   login banner, read prep). New output overwrites stale bytes in the ring window.
 * - bbs_transport_buf_reset(): also memset's bufmem with DEN off; use at connect
 *   init, login home screen, and login reject/disconnect when a full ring wipe is required.
 */
void
bbs_transport_buf_discard(void)
{
  buf_scr_guard();
  buf.head = 0u;
  buf.used = 0u;
  s.numsent = 0;
#ifndef BBS_SERIAL_TRANSPORT
  telnetd_tcp_tx_pending = 0u;
  telnetd_tcp_stream_unacked = 0u;
#endif
  TELWRAP_LINE_RESET();
}

void
bbs_transport_buf_reset(void)
{
  unsigned char st;

  buf_scr_guard();
  st = bbs_status.status;
  if(st != STATUS_READ && st != STATUS_DIRLIST &&
      st != STATUS_STREAM && st != STATUS_XFER) {
    buf_ring_memset_safe(0x20);
  }
  buf.head = 0u;
  buf.used = 0u;
  s.numsent = 0;
#ifndef BBS_SERIAL_TRANSPORT
  telnetd_tcp_tx_pending = 0u;
  telnetd_tcp_stream_unacked = 0u;
#endif
  TELWRAP_LINE_RESET();
}

static void
buf_init(void)
{
  bbs_scr_layout_output();
  bbs_transport_buf_reset();
}

/*---------------------------------------------------------------------------*/
void
buf_compact(void)
{
  if(buf.used == 0u) {
    buf.head = 0u;
    return;
  }
  if(buf.head == 0u) {
    return;
  }
  memmove(buf.bufmem, &buf.bufmem[buf.head], buf.used);
  buf.head = 0u;
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
  if(buf.used == 0u) {
    buf.head = 0u;
    if(bbs_status.status != STATUS_STREAM &&
        bbs_status.status != STATUS_XFER &&
        bbs_status.status != STATUS_READ &&
        bbs_status.status != STATUS_DIRLIST) {
      log_message("\x93", "");
    }
  }
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

  buf_scr_guard();
  if(buf.used >= buf.size) {
    return -1;
  }
  pos = (buf.head + buf.used) % buf.size;
  buf.bufmem[pos] = c;
  ++buf.used;
  return 0;
}

/*---------------------------------------------------------------------------*/
/* Copy n bytes from src into ring at dst_off; PETSCII→ASCII in place if needed. */
static void
buf_ring_write_conv(unsigned int dst_off, const void *src, unsigned int n)
{
  memcpy(&buf.bufmem[dst_off], src, n);
  if(bbs_status.encoding == 1) {
    petscii_to_ascii((char *)&buf.bufmem[dst_off], n);
  }
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

  buf_scr_guard();
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
    buf_ring_write_conv(dst0, data, (unsigned int)copylen);
  } else {
    first_part = contig;
    buf_ring_write_conv(dst0, data, first_part);
    buf_ring_write_conv(0u, data + first_part,
        (unsigned int)copylen - first_part);
  }
  buf.used += (unsigned int)copylen;

  return copylen;
}
/*---------------------------------------------------------------------------*/
#ifndef BBS_SERIAL_TRANSPORT
void
telnetd_quit(void)
{
  shell_quit();

  process_exit(&telnetd_process);
  LOADER_UNLOAD();
}
#endif /* !BBS_SERIAL_TRANSPORT */
/*---------------------------------------------------------------------------*/
/* After bbs_unlock (STATE_CLOSE), schedule a tcp poll so uip_close() runs soon
 * instead of waiting for the next inbound packet / timer. Must not allocate. */
#ifndef BBS_SERIAL_TRANSPORT
void
telnetd_kick_disconnect(void)
{
  if(primary_conn != NULL) {
    tcpip_poll_tcp(primary_conn);
  }
}

void
telnetd_kick_stream(void)
{
  if(primary_conn != NULL) {
    bbs_transport_poll_send();
  }
}
#else /* BBS_SERIAL_TRANSPORT */
void
telnetd_kick_disconnect(void)
{
}

void
telnetd_kick_stream(void)
{
}
#endif /* BBS_SERIAL_TRANSPORT */

void
bbs_stream_begin(void)
{
  telnetd_discard_pending_rx();
  telnetd_stream_rx_grace = 4u;
  bbs_status.status = STATUS_STREAM;
#ifndef BBS_SERIAL_TRANSPORT
  telnetd_tcp_tx_pending = 0u;
  telnetd_tcp_stream_unacked = 0u;
#endif
  telnetd_kick_stream();
}

void
bbs_transport_session_close(void)
{
  s.state = STATE_CLOSE;
  telnetd_kick_disconnect();
}

static unsigned char
telnetd_outbound_quiescent(void)
{
  if(buf.used != 0u) {
    return 0u;
  }
#ifndef BBS_SERIAL_TRANSPORT
  if(telnetd_tcp_tx_pending != 0u || telnetd_tcp_stream_unacked != 0u) {
    return 0u;
  }
#endif
  return 1u;
}

static void
telnetd_finish_session_if_closed(void)
{
  if(s.state != STATE_CLOSE || telnetd_outbound_quiescent() == 0u) {
    return;
  }
  if(bbs_locked != 0u) {
    shell_stop();
  }
  s.state = STATE_NORMAL;
}

void
bbs_transport_busy_reject(void)
{
  s.state = STATE_CLOSE;
}

#ifdef BBS_SERIAL_TRANSPORT
/* Serial: disk -> outbound ring at $0400 (same path as f362414). */
static void
telnetd_stream_fill_ring(void)
{
  unsigned int room;
  unsigned int nreq;
  int nread;

  if(bbs_status.status != STATUS_STREAM) {
    return;
  }
  room = buf_free_bytes();
  if(room == 0u) {
    return;
  }
  nreq = (unsigned int)bbs_status.speed;
  if(nreq > room) {
    nreq = room;
  }
  if(nreq > (unsigned int)MAX_STREAM_SPEED) {
    nreq = (unsigned int)MAX_STREAM_SPEED;
  }
  nread = cbm_read(BBS_MEDIA_CHANNEL, sd_c, (int)nreq);
  if(nread <= 0) {
    bbs_notice_stream_eof();
    return;
  }
  buf_append((const char *)sd_c, nread);
}

static void telnetd_serial_poll_io(void);
#endif /* BBS_SERIAL_TRANSPORT */

void
bbs_transport_stream_clear_sent(void)
{
  s.numsent = 0u;
#ifndef BBS_SERIAL_TRANSPORT
  telnetd_tcp_tx_pending = 0u;
  telnetd_tcp_stream_unacked = 0u;
#endif
}

void
bbs_transport_poll_send(void)
{
#ifdef BBS_SERIAL_TRANSPORT
  bbs_serial_drain_wire();
#else
  unsigned char n;

  if(primary_conn == NULL) {
    return;
  }
  for(n = 0u; n < 32u; ++n) {
    if(buf.used == 0u && telnetd_tcp_tx_pending == 0u &&
        telnetd_tcp_stream_unacked == 0u) {
      break;
    }
    tcpip_poll_tcp(primary_conn);
  }
#endif
}

void
bbs_transport_poll(void)
{
#ifdef BBS_SERIAL_TRANSPORT
  telnetd_serial_poll_io();
#else
  bbs_transport_poll_send();
  process_run();
#endif
}

void
bbs_transport_flush_outbound(void)
{
#ifdef BBS_SERIAL_TRANSPORT
  bbs_serial_flush_outbound();
#else
  clock_time_t t0;
  unsigned char rounds;

  if(primary_conn == NULL) {
    return;
  }
  if(transport_flush_depth != 0u) {
    bbs_transport_poll_send();
    return;
  }
  transport_flush_depth = 1u;
  t0 = clock_time();
  rounds = 0u;
  while((buf.used != 0u || telnetd_tcp_tx_pending != 0u ||
         telnetd_tcp_stream_unacked != 0u) &&
        rounds < 128u &&
        (clock_time_t)(clock_time() - t0) < (clock_time_t)(CLOCK_SECOND * 2u)) {
    if(primary_conn != NULL) {
      tcpip_poll_tcp(primary_conn);
    }
    process_run();
    ++rounds;
  }
  transport_flush_depth = 0u;
#endif
}
/*---------------------------------------------------------------------------*/
void
shell_prompt(char *str)
{
  buf_append(str, (int)strlen(str));
}
/*---------------------------------------------------------------------------*/
#ifdef BBS_SERIAL_TRANSPORT
extern const struct ser_params magnetar_serial_params;

/* KERNAL 24-bit jiffy ($A2/$A1/$A0), advanced by CIA #1 IRQ — same tick family as settime/clock(). */
static unsigned long
telnetd_serial_jiffy24(void)
{
  unsigned char j0, j1, j2;

  for(;;) {
    j0 = *(volatile unsigned char *)0x00A2;
    j1 = *(volatile unsigned char *)0x00A1;
    j2 = *(volatile unsigned char *)0x00A0;
    if(*(volatile unsigned char *)0x00A2 == j0) {
      return (unsigned long)j0 | ((unsigned long)j1 << 8) | ((unsigned long)j2 << 16);
    }
  }
}

/* ~1s: drain modem connect garbage; CLK_TCK jiffies matches PAL/NTSC (see clock.c). */
static void
telnetd_serial_warmup_one_second(void)
{
  unsigned long t0;
  unsigned char c;

  t0 = telnetd_serial_jiffy24();
  for(;;) {
    while(ser_get((char *)&c) == SER_ERR_OK) {
      (void)c;
    }
    if((telnetd_serial_jiffy24() - t0) >= (unsigned long)CLK_TCK) {
      break;
    }
  }
}

static unsigned char telnetd_serial_hw_open;

static unsigned char
telnetd_serial_hw_ensure_open(void)
{
  if(telnetd_serial_hw_open != 0u) {
    return 1u;
  }
  if(ser_open(&magnetar_serial_params) == SER_ERR_OK) {
#ifdef BBS_SERIAL_UP2400
    /* UP2400 driver: enable userport RS-232 hooks (see c64-up2400.s IOCTL). */
    (void)ser_ioctl(1, NULL);
#endif
    telnetd_serial_hw_open = 1u;
    return 1u;
  }
  return 0u;
}

static void
telnetd_serial_hw_close_keep_offline(void)
{
  (void)ser_close();
  telnetd_serial_hw_open = 0u;
}

/* 1 = no carrier / line down (SER_STATUS_DCD means NOT DCD). */
static unsigned char
telnetd_serial_modem_offline_explicit(void)
{
  unsigned char st;
  unsigned char err;

  err = ser_status(&st);
  if(err != SER_ERR_OK) {
    /* Mid-session status failure usually means port closed after remote hang-up. */
    return (s.connected != 0u) ? 1u : 0u;
  }
  return (((st & SER_STATUS_DCD) != 0u) ? 1u : 0u);
}

static void
telnetd_serial_on_connect(void)
{
  (void)telnetd_serial_hw_ensure_open();
  buf_init();
  s.bufptr = 0;
  s.last_space_at = (unsigned char)TELNETD_LAST_SPACE_NONE;
  s.state = STATE_NORMAL;
  s.connected = 1;
  timer_set(&silence_timer, BBS_IDLE_TIMEOUT);
  telnetd_serial_warmup_one_second();
  shell_start();
  timer_set(&silence_timer, BBS_IDLE_TIMEOUT);
}
/*---------------------------------------------------------------------------*/
static void
telnetd_serial_disconnect(void)
{
  telnetd_serial_hw_close_keep_offline();
  log_message("\x9e", "telnetd stop");
  update_time();

  if(bbs_status.login == 1) {
    save_stats();
    bbs_status.login = 0;
  }
  /* shell_stop() -> bbs_unlock() shows "bbs disconnect". Skip if logout already ran. */
  if(bbs_locked != 0u) {
    shell_stop();
  }
  s.connected = 0u;
  serial_waiting_peer = 1u;
}
/*---------------------------------------------------------------------------*/
static unsigned int
telnetd_serial_write(const void *vd, unsigned int len)
{
  const unsigned char *q = (const unsigned char *)vd;
  unsigned int written;

  written = 0;
  while(len != 0u) {
    /* Never spin forever on CTS / TX full. Write what we can and retry next tick. */
    if(ser_put((char)*q) == SER_ERR_OVERFLOW) {
      break;
    }
    ++q;
    --len;
    ++written;
  }
  return written;
}
/*---------------------------------------------------------------------------*/
static void
telnetd_serial_tx(void)
{
  if(bbs_status.status == STATUS_READ ||
      bbs_status.status == STATUS_DIRLIST) {
    return;
  }

  telnetd_stream_fill_ring();

  if(buf.used == 0u) {
    sd_len = 0;
  } else {
    unsigned int chunk;
    unsigned int first;

    chunk = (unsigned int)TELNETD_SERIAL_TX_CHUNK;
    first = buf.size - buf.head;
    sd_len = buf.used;
    if(sd_len > chunk) {
      sd_len = chunk;
    }
    if(sd_len > first) {
      sd_len = first;
    }
    sd_len = telnetd_serial_write(&buf.bufmem[buf.head], sd_len);
  }
  s.numsent = sd_len;
  if(s.numsent > 0u) {
    buf_ack_sent((unsigned int)s.numsent);
  }
}
/*---------------------------------------------------------------------------*/
void
bbs_serial_drain_wire(void)
{
  unsigned int n;

  if(s.connected == 0u) {
    return;
  }
  for(n = 0u; n < 48u && buf_free_bytes() < 900u; ++n) {
    telnetd_serial_tx();
  }
}
/*---------------------------------------------------------------------------*/
void
bbs_serial_banner_begin(void)
{
  if(serial_banner_depth < 255u) {
    ++serial_banner_depth;
  }
}
/*---------------------------------------------------------------------------*/
void
bbs_serial_banner_end(void)
{
  /* Hard reset to avoid stale depth blocking RX forever. */
  serial_banner_depth = 0u;
}
/*---------------------------------------------------------------------------*/
static void
telnetd_serial_poll_io(void)
{
  unsigned char c;
  unsigned char sg;

  /* Pre-session: assert DTR only when modem reports carrier; else stay closed (DTR low). */
  if(serial_waiting_peer != 0u && s.connected == 0u) {
    if(telnetd_serial_hw_ensure_open() == 0u) {
      return;
    }
    if(telnetd_serial_modem_offline_explicit() != 0u) {
      telnetd_serial_hw_close_keep_offline();
      return;
    }
  } else if(s.connected != 0u) {
    if(telnetd_serial_hw_ensure_open() == 0u) {
      telnetd_serial_disconnect();
      return;
    }
    if(telnetd_serial_modem_offline_explicit() != 0u) {
      telnetd_serial_disconnect();
      return;
    }
  }

  sg = SER_ERR_NO_DATA;
  while((sg = ser_get((char *)&c)) != SER_ERR_NO_DATA) {
    if(sg != SER_ERR_OK) {
      break;
    }
    /* While a banner file is in the outbound path, drop RX (same as TCP: no input
     * during bulk send). Movies use STATUS_STREAM — keep accepting Return to stop. */
    if(serial_banner_depth > 0u && buf.used != 0u
       && bbs_status.status != STATUS_STREAM
       && bbs_status.status != STATUS_XFER) {
      continue;
    }
    if(serial_waiting_peer != 0u) {
      /* First RX byte opens the session (pre-52041c9). Raw bridges often send no
       * leading CR/LF or Telnet IAC; requiring those left the shell idle while the
       * modem still local-echoed keystrokes. */
      serial_waiting_peer = 0u;
      telnetd_serial_on_connect();
      /* Byte that opened the session is modem noise; warmup already drained wire. */
      continue;
    }
    if(s.connected != 0u) {
      timer_set(&silence_timer, BBS_IDLE_TIMEOUT);
      telnetd_feed(&c, 1u);
    }
    if(s.state == STATE_CLOSE) {
      break;
    }
  }

  if(sg != SER_ERR_OK && sg != SER_ERR_NO_DATA && s.connected != 0u) {
    telnetd_serial_disconnect();
    return;
  }

  if(s.connected != 0u && telnetd_serial_modem_offline_explicit() != 0u) {
    telnetd_serial_disconnect();
    return;
  }

  if(s.connected == 0u) {
    if(s.state == STATE_CLOSE) {
      /* Stale CLOSE after disconnect path cleared connected (idle/timer). */
      s.state = STATE_NORMAL;
    }
    return;
  }

  {
    unsigned int u;

    for(u = 0u; u < 24u; ++u) {
      if(buf.used == 0u && bbs_status.status != STATUS_STREAM) {
        break;
      }
      telnetd_serial_tx();
    }
  }

  if(s.state == STATE_CLOSE && telnetd_outbound_quiescent() != 0u) {
    telnetd_finish_session_if_closed();
    telnetd_serial_disconnect();
    return;
  }

  if(timer_expired(&silence_timer)) {
    if(s.state != STATE_CLOSE || telnetd_outbound_quiescent() != 0u) {
      telnetd_serial_disconnect();
    }
  }
}
/*---------------------------------------------------------------------------*/
void
bbs_serial_flush_outbound(void)
{
  unsigned int stall;
  unsigned int prev_used;
  unsigned long t0;

  if(s.connected == 0u) {
    return;
  }
  /* TX-only flush: avoid nested RX/shell re-entry while waiting for wire drain. */
  t0 = telnetd_serial_jiffy24();
  stall = 0u;
  while(buf.used != 0u && s.connected != 0u) {
    if(stall >= 4096u ||
        (telnetd_serial_jiffy24() - t0) >= (unsigned long)(CLK_TCK * 3u)) {
      break;
    }
    prev_used = buf.used;
    telnetd_serial_tx();
    if(buf.used < prev_used) {
      stall = 0u;
    } else {
      ++stall;
    }
  }
}
#else /* !BBS_SERIAL_TRANSPORT */
void
bbs_serial_flush_outbound(void)
{
  bbs_transport_flush_outbound();
}
#endif /* BBS_SERIAL_TRANSPORT */
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(telnetd_process, ev, data)
{
  PROCESS_BEGIN();

  shell_init();

  if(bbs_status.encoding == 1) {
    petscii_to_ascii(telnetd_reject_text, strlen(telnetd_reject_text));
  }

#ifndef BBS_SERIAL_TRANSPORT
  telnetd_reject_len = (unsigned int)strlen(telnetd_reject_text);
  tcp_listen(UIP_HTONS(board.telnet_port));

  while(1) {
    PROCESS_WAIT_EVENT();

    if(ev == tcpip_event) {
      telnetd_appcall(data);
    } else if(ev == PROCESS_EVENT_EXIT) {
      telnetd_quit();
    }
  }
#else
  serial_waiting_peer = 1u;

  while(1) {
    telnetd_serial_poll_io();
    PROCESS_PAUSE();
  }
#endif

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
static void
get_char(uint8_t c)
{
	unsigned char i, n;
	//PRINTF("telnetd: get_char '%c' %d %d\n", c, c, s.bufptr);

	if(c == 0) {
		return;
	}

	if(c == ISO_nl && telnetd_ignore_lf_after_cr != 0u) {
		telnetd_ignore_lf_after_cr = 0u;
		return;
	}
	if(c != ISO_nl && c != ISO_cr) {
		telnetd_ignore_lf_after_cr = 0u;
	}

	if(bbs_status.status == STATUS_STREAM) {
	  if(c == ISO_cr || c == ISO_nl) {
	    if(telnetd_stream_rx_grace > 0u) {
	      if(c == ISO_cr) {
	        telnetd_ignore_lf_after_cr = 1u;
	      }
	      --telnetd_stream_rx_grace;
	      return;
	    }
	    if(c == ISO_cr) {
	      telnetd_ignore_lf_after_cr = 1u;
	    }
	    bbs_status.status = STATUS_LOCK;
	    if(bbs_stream_eof_process != NULL) {
	      process_poll(bbs_stream_eof_process);
	    }
	    telnetd_discard_pending_rx();
	  }
	  return;
	}

	if(bbs_status.echo == 1u || bbs_status.echo == 2u) {

		if(c == PETSCII_DEL){
			if(s.bufptr > 0u) {
				unsigned char del_at;

				del_at = s.bufptr - 1u;
				--s.bufptr;
				s.buf[(int)s.bufptr] = 0;
				if(bbs_status.echo == 1u) {
					telwrap_fixup_delete(del_at);
				}
				if(s.last_space_at == del_at) {
					telnetd_rescan_last_space();
				}

				(void)buf_putc_raw(c);

				if(bbs_status.echo == 1u && telwrap_tr > 0u &&
				    (unsigned int)s.bufptr ==
				    (unsigned int)telwrap_bp[(int)telwrap_tr]) {
					--telwrap_tr;
					telwrap_pop_paint();
				} else if(col_num > 0u) {
					--col_num;
				}
			}
			return;
		}

		if(c==PETSCII_UP || c==PETSCII_DOWN || c==PETSCII_LEFT || c==PETSCII_RIGHT || c==PETSCII_CLRSCN || c==PETSCII_HOME){
			return;
		}

		if(bbs_status.echo == 2u) {
			if(col_num >= (unsigned char)bbs_status.width && c != ISO_cr &&
			    c != ISO_nl && !BBS_PETSCII_ATTR0((unsigned char)c) &&
			    (col1_cell((unsigned char)c) || (unsigned char)c == 0x09u)) {
				(void)buf_putc_raw(ISO_cr);
				col_num = 0;
			}
			(void)buf_putc_raw(c);
			col1_bump(c);
		} else if(col_num>=bbs_status.width){

			if(c == ISO_cr || c == ISO_nl) {
				(void)buf_putc_raw(c);
				col_num = 0;
			} else {

				if(BWS_WORD_BREAK(c)) {
					telwrap_commit_row_finish((unsigned char)s.bufptr);
					(void)buf_putc_raw(c);
					(void)buf_putc_raw(ISO_cr);
					col_num = 0;
				} else {

					if((int)s.bufptr < 1) {
						telwrap_commit_row_finish((unsigned char)s.bufptr);
						(void)buf_putc_raw(ISO_cr);
						col_num = 0;
						(void)buf_putc_raw(c);
						col1_bump(c);
					} else {

					if(s.last_space_at != (unsigned char)TELNETD_LAST_SPACE_NONE
					    && s.last_space_at < s.bufptr
					    && BWS_WORD_BREAK(s.buf[(int)s.last_space_at])) {
						telwrap_echo_dls(s.bufptr - 1u, s.last_space_at);
						i = s.last_space_at + 1u;
					} else {
					unsigned short wj;

					wj = bws_find_break_back(
					    s.buf, 0u, (unsigned short)(s.bufptr - 1u), BWS_FIND_MODE_TELNET);
					telwrap_echo_dls(s.bufptr - 1u, (unsigned char)wj);
					if(BWS_WORD_BREAK(s.buf[(int)wj]) != 0u) {
						i = (unsigned char)wj + 1u;
					} else {
						i = 0u;
					}
					}
					if(i == 0u && s.bufptr > 0u) {
						(void)buf_putc_raw(PETSCII_DEL);
					}

					telwrap_commit_row_finish(i);

					(void)buf_putc_raw(ISO_cr);
					for(n = i; n < s.bufptr; ++n) {
						(void)buf_putc_raw((unsigned char)s.buf[(int)n]);
					}
					col_num = (unsigned char)(s.bufptr - i);
					(void)buf_putc_raw(c);
					if(c == ISO_cr || c == ISO_nl) {
						col_num = 0;
					} else if(col1_cell((unsigned char)c) || c == 0x09u) {
						++col_num;
					}
					}
				}
			}
		}
		else{	
      (void)buf_putc_raw(c);
      col1_bump(c);
		}
	}

	if(c != ISO_nl){
		s.buf[(int)s.bufptr] = c;
		++s.bufptr;
		if(BWS_WORD_BREAK(c)) {
			s.last_space_at = s.bufptr - 1u;
		}
	}

	//if(s.bufptr == sizeof(s.buf) || c == ISO_cr || c == ISO_nl) {
	if(s.bufptr == TELNETD_CONF_LINELEN || c == ISO_cr || c == ISO_nl) {

		if(c == ISO_cr) {
			telnetd_ignore_lf_after_cr = 1u;
		}

		if(bbs_status.status!=STATUS_POST){
		  if((c == ISO_cr) && s.bufptr>1){
			--s.bufptr;
			col_num = 0;
		  }
		}
		else if(c == ISO_cr || c == ISO_nl) {
			s.buf[(int)s.bufptr] = ISO_nl;
			++s.bufptr;
		}

		s.buf[(int)s.bufptr] = 0;
		if(bbs_status.encoding==1){ascii_to_petscii(s.buf, TELNETD_CONF_LINELEN);}
		//if(bbs_status.encoding==2){atascii_to_petscii(s.buf, TELNETD_CONF_LINELEN);}
		//PRINTF("telnetd: get_char '%.*s'\n", s.bufptr, s.buf);
		TELWRAP_LINE_RESET();
		if(s.bufptr > 0u) {
		  shell_input(s.buf, s.bufptr);
		} else if(bbs_status.status == STATUS_LOCK) {
		  set_prompt();
		  shell_prompt(bbs_status.prompt);
		}
		s.bufptr = 0;
		s.last_space_at = (unsigned char)TELNETD_LAST_SPACE_NONE;
		col_num = 0;

	}

}
/*---------------------------------------------------------------------------*/
static void
sendopt(uint8_t option, uint8_t value)
{
  char line[3];

  line[0] = (char)TELNET_IAC;
  line[1] = option;
  line[2] = value;
  if(bbs_status.encoding == 1) {
    ascii_to_petscii(line, 3);
  }
  buf_append(line, 3);
}
/*---------------------------------------------------------------------------*/
static void
telnetd_feed(const unsigned char *ptr, unsigned int len)
{
  unsigned char c;

  if(bbs_status.status == STATUS_XFER) {
    while(len > 0u) {
      bbs_bank_feed(*ptr++);
      --len;
    }
    return;
  }

  /* Cheap hard stop: if this chunk cannot fit in the current input line,
     close instead of partially parsing browser/HTTP garbage. */
  if(len > (unsigned int)(TELNETD_CONF_LINELEN - s.bufptr)) {
    s.state = STATE_CLOSE;
    return;
  }

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
    case STATE_WONT:
      sendopt(TELNET_DONT, c);
      s.state = STATE_NORMAL;
      break;
    case STATE_DO:
    case STATE_DONT:
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

  if(len > 0) {
    s.state = STATE_CLOSE;
  }
}

#ifndef BBS_SERIAL_TRANSPORT
static unsigned char
reject_non_telnet(const unsigned char *p, unsigned int len)
{
  static const unsigned char rejtbl[] = {
    2, 22, 3,
    4, 'S', 'S', 'H', '-',
    4, 'G', 'E', 'T', ' ',
    4, 'P', 'U', 'T', ' ',
    5, 'P', 'O', 'S', 'T', ' ',
    5, 'H', 'E', 'A', 'D', ' ',
    5, 'P', 'R', 'I', ' ', '*',
    0
  };
  const unsigned char *t;
  unsigned char n;
  unsigned char i;

  for(t = rejtbl; (n = *t++) != 0u; ) {
    if(len >= (unsigned int)n) {
      for(i = 0u; i < n; ++i) {
        if(p[i] != t[i]) {
          goto nomatch;
        }
      }
      return 1u;
    }
nomatch:
    t += n;
  }
  return 0u;
}

static void
telnetd_tcp_conn_uclose(void)
{
  uip_close();
  tcp_markconn(uip_conn, NULL);
}

static unsigned char
newdata(void)
{
  unsigned int len;
  const unsigned char *p;

  len = (unsigned int)uip_datalen();
  p = (const unsigned char *)uip_appdata;

  if(len > 0u && uip_conn == primary_conn && s.connected != 0u) {
    if(telnetd_tcp_firstrx != 0u) {
      telnetd_tcp_firstrx = 0u;
      if(reject_non_telnet(p, len) != 0u) {
        if(bbs_locked != 0u) {
          shell_stop();
        }
        s.connected = 0;
        primary_conn = NULL;
        telnetd_tcp_conn_uclose();
        return 1;
      }
    }
  }

  telnetd_feed(p, len);
  return 0;
}
/*---------------------------------------------------------------------------*/
void
telnetd_appcall(void *ts)
{
  /* Secondary connection protection (single-session BBS):
     ignore anything not coming from the primary uip_conn. */
  if(s.connected && primary_conn != NULL && uip_conn != primary_conn && ts == (void *)0) {
    if(uip_connected()) {
      uip_send(telnetd_reject_text, telnetd_reject_len);
      tcp_markconn(uip_conn, (char *)1);
    } else {
      /* Stale tcp after primary moved to another uip_conn (e.g. new login while
       * old socket not closed). uip_connected() is only true during SYN
       * handshake; idle sessions must be closed explicitly. */
      telnetd_tcp_conn_uclose();
    }
    return;
  }

  /* Busy/reject conn: marked (char*)1 after uip_send. Close on ACK, but if peer
     never ACKs idle-only stacks stall here — (char*)2 after one poll, then force close. */
  if(ts == (char *)1) {
    if(uip_acked()) {
      telnetd_tcp_conn_uclose();
    } else if(uip_poll()) {
      tcp_markconn(uip_conn, (char *)2);
    }
    return;
  }
  if(ts == (char *)2) {
    if(uip_acked()) {
      telnetd_tcp_conn_uclose();
    } else if(uip_poll()) {
      telnetd_tcp_conn_uclose();
    }
    return;
  }

  if(uip_connected()) {
    /* Recover wedge: teardown must only null primary_conn when THAT conn closed;
       secondaries clearing primary left s.connected=1 → perpetual busy. */
    if(s.connected != 0u && primary_conn == NULL) {
      s.connected = 0u;
    }
    if(!s.connected) {
      buf_init();
      s.bufptr = 0;
      s.last_space_at = (unsigned char)TELNETD_LAST_SPACE_NONE;
      s.state = STATE_NORMAL;
      s.connected = 1;
      primary_conn = uip_conn;
      timer_set(&silence_timer, BBS_IDLE_TIMEOUT);
      telnetd_tcp_firstrx = 1u;
      shell_start();
      ts = (char *)0;
    } else {
      uip_send(telnetd_reject_text, telnetd_reject_len);
      /* Busy text only: mark non-NULL and close on next poll,
         so the reject text can actually get transmitted. */
      tcp_markconn(uip_conn, (char *)1);
      return;
    }
    tcp_markconn(uip_conn, ts);
  }

  if(!ts) {
    /* Logoff banner queued in ring; FIN only after outbound ring drains. */
    if(s.state == STATE_CLOSE && telnetd_outbound_quiescent() != 0u) {
      telnetd_finish_session_if_closed();
      uip_close();
      return;
    }
    if(uip_closed() ||
        uip_aborted() ||
        uip_timedout()) {
      /* Only the primary uip_conn owns shell/BBS state. A secondary that finished
       * busy/reject has ts NULL here too — must not clear primary_conn. */
      if(primary_conn != NULL && uip_conn == primary_conn) {
        if(bbs_locked != 0u) {
          log_message("\x9e", "telnetd stop");
        }
        update_time();

        if(bbs_status.login == 1) {
          save_stats();
          bbs_status.login = 0;
        }
        if(bbs_locked != 0u) {
          shell_stop();
        }
        primary_conn = NULL;
        s.connected = 0u;
      }
    }
    if(uip_acked()) {
      if(telnetd_tcp_stream_unacked != 0u) {
        telnetd_tcp_stream_unacked = 0u;
      } else if(telnetd_tcp_tx_pending != 0u) {
        buf_ack_sent((unsigned int)s.numsent);
        telnetd_tcp_tx_pending = 0u;
      }
    }
    if(uip_newdata()) {
      timer_set(&silence_timer, BBS_IDLE_TIMEOUT);
      if(newdata()) {
        return;
      }
    }
    if(uip_rexmit() ||
        uip_newdata() ||
        uip_acked() ||
        uip_connected() ||
        uip_poll()) {
      sd_len = 0;
      if(bbs_status.status == STATUS_STREAM) {
        if(uip_rexmit() != 0) {
          if(s.numsent > 0u) {
            telnetd_tcp_stream_unacked = 1u;
            memcpy(uip_appdata, sd_c, (int)s.numsent);
            uip_send(uip_appdata, (int)s.numsent);
          }
        } else if(telnetd_tcp_stream_unacked == 0u) {
          int sdr;
          int req;

          req = (int)telnetd_movie_read_req();
          sdr = cbm_read(BBS_MEDIA_CHANNEL, sd_c, req);
          if(sdr > 0) {
            sd_len = (unsigned int)sdr;
            telnetd_tcp_stream_unacked = 1u;
            memcpy(uip_appdata, sd_c, sdr);
            uip_send(uip_appdata, sdr);
            s.numsent = sd_len;
          } else {
            bbs_notice_stream_eof();
          }
        }
      } else {
        if(telnetd_tcp_stream_unacked != 0u || buf.used == 0) {
          sd_len = 0;
        } else if(uip_rexmit() != 0 && telnetd_tcp_tx_pending != 0u &&
            s.numsent > 0u) {
          unsigned int first;

          first = buf.size - buf.head;
          sd_len = (unsigned int)s.numsent;
          if(sd_len > first) {
            sd_len = first;
          }
          memcpy(uip_appdata, &buf.bufmem[buf.head], sd_len);
        } else if(telnetd_tcp_tx_pending == 0u && buf.used != 0u &&
            bbs_status.status != STATUS_READ &&
            bbs_status.status != STATUS_DIRLIST) {
          unsigned int mss;
          unsigned int first;

          mss = (unsigned int)uip_mss();
          first = buf.size - buf.head;
          sd_len = buf.used;
          if(sd_len > mss) {
            sd_len = mss;
          }
          if(sd_len > (unsigned int)TELNETD_STREAM_CHUNK) {
            sd_len = (unsigned int)TELNETD_STREAM_CHUNK;
          }
          if(sd_len > first) {
            sd_len = first;
          }
          memcpy(uip_appdata, &buf.bufmem[buf.head], sd_len);
        }
        if(sd_len > 0u) {
          uip_send(uip_appdata, (int)sd_len);
          s.numsent = sd_len;
          telnetd_tcp_tx_pending = 1u;
        }
      }
    }
    if(uip_poll()) {
      if(timer_expired(&silence_timer)) {
        if(s.state != STATE_CLOSE || buf.used == 0u) {
          telnetd_tcp_conn_uclose();
        }
      }
    }
  }
}
#endif /* !BBS_SERIAL_TRANSPORT */
/*---------------------------------------------------------------------------*/
