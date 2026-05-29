#ifndef BBS_FMT_H_
#define BBS_FMT_H_

/* Tiny decimal/format helpers (no printf in bank builds). */

void bbs_u16_to_dec(unsigned short v, char *out);
void bbs_u8_to_dec(unsigned char v, char *out);
void bbs_fmt_msg_id(char *out, unsigned char bid, unsigned short num);
unsigned short bbs_parse_u16(const char *s);

#ifdef BBS_BANK_BUILD
void bbs_fmt_sub_file(char *out, unsigned char bid);
void bbs_fmt_petscii_name_ln(char *out, const char *name);
void bbs_fmt_board_list_line(char *out, unsigned char num, const char *name,
    unsigned short unread);
void bbs_fmt_board_select_prompt(char *out, unsigned char max_boards);
void bbs_fmt_read_select_prompt(char *out, unsigned short max_msg);
#endif

#endif /* BBS_FMT_H_ */
