#ifndef BBS_XMODEM_H_
#define BBS_XMODEM_H_

#include <stdint.h>
#include "bbs-defs.h"

#define BBS_XMODEM_RBUF  ((unsigned char *)(BBS_XMODEM_RBUF_BASE))

/* Returns 1 if a byte is ready in bbs_xmodem_inbyte (poll ~3s in C) */
unsigned char bbs_xmodem_poll(void);
extern unsigned char bbs_xmodem_inbyte;

void bbs_xmodem_putc(unsigned char c);

/* Fill BBS_XMODEM_RBUF[2..129]; A=1 last short block, A=0 full 128 */
unsigned char bbs_xmodem_read_block(void);

void bbs_xmodem_write_block(void);

/* A=0 ok, 0xfe abort, 0xff error */
unsigned char bbs_xmodem_send(void);
unsigned char bbs_xmodem_recv(void);

void bbs_xmodem_io_begin(void);
void bbs_xmodem_io_end(void);

#endif /* BBS_XMODEM_H_ */
