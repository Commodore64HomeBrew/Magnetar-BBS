/*
 * Copyright (c) 2003, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
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
 * This file is part of the Contiki desktop environment
 *
 *
 */
#ifndef TELNETD_H_
#define TELNETD_H_

#include "contiki.h"
#include "bbs-defs.h"

/* Telnet/serial line state (transport layer only; not used by shell). */
typedef struct {
  unsigned char buf[TELNETD_CONF_LINELEN + 1];
  unsigned char bufptr;
  unsigned char last_space_at; /* last word-break in buf (space/tab); 255=none */
  unsigned char connected;
  unsigned long numsent;
  unsigned char state;
} TELNETD_STATE;

PROCESS_NAME(telnetd_process);

/* Session/transport hooks (both builds): shell must not touch TELNETD_STATE directly. */
void bbs_transport_session_close(void);
void bbs_transport_busy_reject(void);
void bbs_transport_stream_clear_sent(void);
void bbs_transport_buf_reset(void);
void bbs_transport_buf_discard(void);
/* Screen RAM layout: full 1 KiB telnet ring (default). */
void bbs_scr_layout_output(void);
/* Screen RAM layout: RX/TX/XMODEM partition (call only for upload/download). */
void bbs_scr_layout_xfer(void);
/* Set/clear process to poll on stream EOF (movie stream in core). */
void bbs_stream_set_eof_process(struct process *p);
void telnetd_discard_pending_rx(void);
void bbs_stream_begin(void);

#ifndef BBS_SERIAL_TRANSPORT
void telnetd_appcall(void *data);
void telnetd_quit(void);
#endif
/* TCP: schedule uip poll; serial: no-op. */
void telnetd_kick_disconnect(void);
void telnetd_kick_stream(void);
void bbs_transport_poll(void);
/* Drive outbound TCP/serial without process_run (safe during shell command). */
void bbs_transport_poll_send(void);
/* Drain outbound ring before XMODEM/movie so the user sees the prompt first. */
void bbs_transport_flush_outbound(void);
void bbs_serial_flush_outbound(void);
#ifdef BBS_SERIAL_TRANSPORT
/* Push outbound ring toward UART so bulk cbm_read into buf isn't capped prematurely. */
void bbs_serial_drain_wire(void);
/* Nesting count: serial poll discards RX while >0 unless STATUS_STREAM (movies). */
void bbs_serial_banner_begin(void);
void bbs_serial_banner_end(void);
#endif

int buf_append(const char *data, int len);

unsigned int buf_free_bytes(void);
/* Append one raw byte to the telnet output queue; returns 0 on success, -1 if full. */
int buf_putc_raw(unsigned char c);
/* Move pending data to start of bufmem so [0..used) is linear (optional before bulk writes). */
void buf_compact(void);

#endif /* TELNETD_H_ */
