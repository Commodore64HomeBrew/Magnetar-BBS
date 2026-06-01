#include "bbs-fmt.h"
#include <stddef.h>

#ifndef BBS_BANK_BUILD
#pragma bss-name("LOWBSS")
#endif

#ifdef BBS_BANK_BUILD
#include "bbs-defs.h"
#endif

void
bbs_u16_to_dec(unsigned short v, char *out)
{
  static const unsigned short pow10[5] = { 10000u, 1000u, 100u, 10u, 1u };
  unsigned char i;
  unsigned char started;

  if(out == NULL) {
    return;
  }

  started = 0u;
  for(i = 0u; i < 5u; ++i) {
    unsigned char digit = 0u;
    unsigned short p = pow10[i];

    while(v >= p) {
      v = (unsigned short)(v - p);
      ++digit;
    }
    if(digit != 0u || started != 0u || i == 4u) {
      *out++ = (char)('0' + digit);
      started = 1u;
    }
  }
  *out = '\0';
}

void
bbs_u8_to_dec(unsigned char v, char *out)
{
  bbs_u16_to_dec((unsigned short)v, out);
}

void
bbs_fmt_msg_id(char *out, unsigned char bid, unsigned short num)
{
  char *p;

  if(out == NULL) {
    return;
  }
  p = out;
  bbs_u8_to_dec(bid, p);
  while(*p != '\0') {
    ++p;
  }
  *p++ = '-';
  bbs_u16_to_dec(num, p);
}

unsigned short
bbs_parse_u16(const char *s)
{
  unsigned short v = 0u;

  if(s == NULL) {
    return 0u;
  }
  while(*s >= '0' && *s <= '9') {
    v = (unsigned short)(v * 10u + (unsigned short)(*s - '0'));
    ++s;
  }
  return v;
}

#ifdef BBS_BANK_BUILD

static char *
fmt_end(char *out)
{
  while(*out != '\0') {
    ++out;
  }
  return out;
}

static char *
fmt_strcpy(char *dst, const char *src)
{
  while(*src != '\0') {
    *dst++ = *src++;
  }
  *dst = '\0';
  return dst;
}

void
bbs_fmt_sub_file(char *out, unsigned char bid)
{
  char *p;

  if(out == NULL) {
    return;
  }
  p = out;
  fmt_strcpy(p, BBS_PREFIX_SUB);
  bbs_u8_to_dec(bid, fmt_end(out));
}

void
bbs_fmt_petscii_name_ln(char *out, const char *name)
{
  char *p;

  if(out == NULL || name == NULL) {
    return;
  }
  p = out;
  *p++ = '\x05';
  while(*name != '\0') {
    *p++ = *name++;
  }
  *p++ = '\n';
  *p++ = '\r';
  *p = '\0';
}

void
bbs_fmt_board_list_line(char *out, unsigned char num, const char *name,
    unsigned short unread)
{
  char *p;

  if(out == NULL || name == NULL) {
    return;
  }
  p = out;
  *p++ = '\x05';
  bbs_u8_to_dec(num, p);
  p = fmt_end(out);
  *p++ = ':';
  *p++ = '\x9e';
  while(*name != '\0') {
    *p++ = *name++;
  }
  *p++ = '\x1c';
  *p++ = '-';
  *p++ = '\x05';
  bbs_u16_to_dec(unread, p);
}

void
bbs_fmt_board_select_prompt(char *out, unsigned char max_boards)
{
  char *p;

  if(out == NULL) {
    return;
  }
  p = out;
  *p++ = '\x05';
  *p++ = '\n';
  *p++ = '\r';
  fmt_strcpy(p, "select board (1-");
  p = fmt_end(out);
  bbs_u8_to_dec(max_boards, p);
  p = fmt_end(out);
  *p++ = ':';
  *p = '\0';
}

void
bbs_fmt_read_select_prompt(char *out, unsigned short max_msg)
{
  char *p;

  if(out == NULL) {
    return;
  }
  p = out;
  *p++ = '\n';
  *p++ = '\r';
  fmt_strcpy(p, "select msg (1-");
  p = fmt_end(out);
  bbs_u16_to_dec(max_msg, p);
  p = fmt_end(out);
  *p++ = ':';
  *p++ = ' ';
  *p = '\0';
}

#endif /* BBS_BANK_BUILD */
