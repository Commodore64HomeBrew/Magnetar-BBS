/**
 * \file bbs-wrap.c — word-break and backward wrap scan (see bbs-wrap.h)
 */

#include "bbs-wrap.h"

unsigned char
bws_word_break(unsigned char c)
{
	if(c == PETSCII_SPACE) {
		return 1u;
	}
	if(c == 0x09u) {
		return 1u;
	}
	return 0u;
}

unsigned char
bws_is_space(unsigned char c)
{
	return (c == PETSCII_SPACE) ? 1u : 0u;
}

unsigned short
bws_find_break_back(
    const unsigned char *p,
    unsigned short preCol,
    unsigned short i,
    unsigned char (*is_break)(unsigned char))
{
	unsigned short j;

	if(p == 0) {
		return preCol;
	}
	if(i <= preCol) {
		return preCol;
	}
	j = i;
	while(j > preCol && !is_break(p[j])) {
		--j;
	}
	return j;
}
