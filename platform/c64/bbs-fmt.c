#include "bbs-fmt.h"
#include <stddef.h>

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
