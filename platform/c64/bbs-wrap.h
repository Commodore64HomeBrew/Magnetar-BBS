/**
 * \file
 *         Word-break rules + one backward scan for wrap (minimize .o / no fn-ptr on cc65).
 */
#ifndef BBS_WRAP_H
#define BBS_WRAP_H

#include "bbs-defs.h"

/* Telnet line editor: break words on PETSCII space and ISO tab (0x09). */
#define BWS_WORD_BREAK(c) \
	(((unsigned char)(c) == PETSCII_SPACE) || ((unsigned char)(c) == 0x09u))
/* Banner / seq. file wrap: only PETSCII space (no tab as break; tabs are not line input). */
#define BWS_SPACE_ONLY(c) ((unsigned char)(c) == PETSCII_SPACE)

/* bbs_find_break_back: TELNET = space+tab; BANNER = space only (must match forward scan) */
#define BWS_FIND_MODE_TELNET 0u
#define BWS_FIND_MODE_BANNER 1u

/* p must be non-null (call sites: telnet s.buf, banner buf.bufmem). */
unsigned short bws_find_break_back(
    const unsigned char *p,
    unsigned short preCol,
    unsigned short i,
    unsigned char space_only);

#endif /* BBS_WRAP_H */
