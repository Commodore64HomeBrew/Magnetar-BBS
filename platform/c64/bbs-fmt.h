#ifndef BBS_FMT_H_
#define BBS_FMT_H_

/* Tiny decimal/format helpers (no printf in bank builds). */

void bbs_u16_to_dec(unsigned short v, char *out);
void bbs_u8_to_dec(unsigned char v, char *out);
void bbs_fmt_msg_id(char *out, unsigned char bid, unsigned short num);
unsigned short bbs_parse_u16(const char *s);

#endif /* BBS_FMT_H_ */
