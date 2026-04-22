/**
 * \file
 *         Shared line-wrap helpers (word-break scan). Used from bbs-file / bbs-telnetd.
 */
#ifndef BBS_WRAP_H
#define BBS_WRAP_H

#include "bbs-defs.h"

/* Space or ISO TAB — matches telnet line editor. */
unsigned char bws_word_break(unsigned char c);

/* PETSCII space only — matches bbs_banner file wrap when not using tabs in buffer. */
unsigned char bws_is_space(unsigned char c);

/**
 * Find break index when wrapping from a forward scan that reached column == width
 * at position \p i. Walks \p j down from \p i while \p j > preCol and
 * !is_break(p[j]); returns the stopping \p j in [preCol, i].
 *
 * If is_break(p[j]) is true, a word break was found at \p j. If not and j == preCol,
 * there is no break in (preCol, i] (long word / hard wrap — caller policy).
 */
unsigned short bws_find_break_back(
    const unsigned char *p,
    unsigned short preCol,
    unsigned short i,
    unsigned char (*is_break)(unsigned char));

#endif /* BBS_WRAP_H */
