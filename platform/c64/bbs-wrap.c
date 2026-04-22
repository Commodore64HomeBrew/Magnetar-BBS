/**
 * \file bbs-wrap.c — single backward scan, no function pointer (smaller on 6502).
 */
#include "bbs-wrap.h"

unsigned short
bws_find_break_back(
    const unsigned char *p,
    unsigned short preCol,
    unsigned short i,
    unsigned char space_only)
{
	unsigned short j;
	unsigned char ch;

	if(p == 0) {
		return preCol;
	}
	if(i <= preCol) {
		return preCol;
	}
	j = i;
	while(j > preCol) {
		ch = p[j];
		if(space_only != 0u) {
			if(BWS_SPACE_ONLY(ch)) {
				break;
			}
		} else {
			if(BWS_WORD_BREAK(ch)) {
				break;
			}
		}
		--j;
	}
	return j;
}
