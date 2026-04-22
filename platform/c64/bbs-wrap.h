/**
 * \file
 *         Word-break rules + one backward scan for wrap (minimize .o / no fn-ptr on cc65).
 */
#ifndef BBS_WRAP_H
#define BBS_WRAP_H

#include "bbs-defs.h"

/* Inlined: no out-of-line calls on hot tests (saves over bws_word_break()) */
#define BWS_WORD_BREAK(c) \
	(((unsigned char)(c) == PETSCII_SPACE) || ((unsigned char)(c) == 0x09u))
#define BWS_SPACE_ONLY(c) ((unsigned char)(c) == PETSCII_SPACE)

/* bbs_find_break_back: space_only=1 → PETSCII space; 0 → space + tab (telnet) */
#define BWS_FIND_MODE_TELNET 0u
#define BWS_FIND_MODE_BANNER 1u

unsigned short bws_find_break_back(
    const unsigned char *p,
    unsigned short preCol,
    unsigned short i,
    unsigned char space_only);

#endif /* BBS_WRAP_H */
