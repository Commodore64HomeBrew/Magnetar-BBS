#ifndef BBS_API_H_
#define BBS_API_H_

#include "bbs-bank.h"

/* Fixed RESAPI addresses — must match bbs-api.inc / bbs-core-api.S */
#define BBS_API_BASE              0xA0E4u

#define BBS_API_SHELL_OUTPUT_STR  ((void (*)(struct shell_command *, char *, char *))0xA0E4u)
#define BBS_API_SHELL_PROMPT      ((void (*)(char *))0xA0E7u)
#define BBS_API_SHELL_REG_CMD     ((void (*)(struct shell_command *))0xA0EAu)
#define BBS_API_SHELL_UNREG_CMD   ((void (*)(struct shell_command *))0xA0EDu)
#define BBS_API_BUF_APPEND        ((int (*)(const char *, int))0xA0F0u)
#define BBS_API_BUF_PUTC_RAW      ((int (*)(unsigned char))0xA0F3u)
#define BBS_API_BBS_BANNER        ((void (*)(unsigned char *, unsigned char *, unsigned char *, unsigned char, unsigned char))0xA0F6u)
#define BBS_API_LOG_MESSAGE       ((void (*)(const char *, const char *))0xA0F9u)
#define BBS_API_FILE_PATH         ((void (*)(const char *, unsigned short, char *, unsigned char))0xA0FCu)
#define BBS_API_TRANSPORT_POLL    ((void (*)(void))0xA0FFu)
#define BBS_API_TRANSPORT_FLUSH   ((void (*)(void))0xA102u)
#define BBS_API_BUF_RESET         ((void (*)(void))0xA105u)
#define BBS_API_BUF_DISCARD       ((void (*)(void))0xA108u)
#define BBS_API_SCR_LAYOUT_OUT    ((void (*)(void))0xA10Bu)
#define BBS_API_SCR_LAYOUT_XFER   ((void (*)(void))0xA10Eu)
#define BBS_API_STREAM_BEGIN      ((void (*)(void))0xA111u)
#define BBS_API_SET_PROMPT        ((void (*)(void))0xA114u)
#define BBS_API_UPDATE_TIME       ((void (*)(void))0xA117u)
#define BBS_API_PATH_SYS_AT       ((void (*)(char *, const char *))0xA11Au)
#define BBS_API_CLOCK_TIME        ((unsigned long (*)(void))0xA11Du)
#define BBS_API_POLL_SEND         ((void (*)(void))0xA120u)
#define BBS_API_SERIAL_FLUSH      ((void (*)(void))0xA123u)

#define BBS_ABI_MAJOR             1u
#define BBS_ABI_MINOR             0u

#ifdef BBS_BANK_BUILD

#define shell_output_str(c, a, b)  BBS_API_SHELL_OUTPUT_STR((c), (a), (b))
#define shell_prompt(p)            BBS_API_SHELL_PROMPT((p))
#define shell_register_command(c)  BBS_API_SHELL_REG_CMD((c))
#define shell_unregister_command(c) BBS_API_SHELL_UNREG_CMD((c))
#define buf_append(d, n)           BBS_API_BUF_APPEND((d), (n))
#define buf_putc_raw(c)            BBS_API_BUF_PUTC_RAW((unsigned char)(c))
#define bbs_banner(a, b, c, d, e)  BBS_API_BBS_BANNER((a), (b), (c), (d), (e))
#define log_message(a, b)          BBS_API_LOG_MESSAGE((a), (b))
#define file_path(a, b, c, d)      BBS_API_FILE_PATH((a), (b), (c), (d))
#define bbs_transport_poll()         BBS_API_TRANSPORT_POLL()
#define bbs_transport_flush_outbound() BBS_API_TRANSPORT_FLUSH()
#define bbs_transport_buf_reset()  BBS_API_BUF_RESET()
#define bbs_transport_buf_discard() BBS_API_BUF_DISCARD()
#define bbs_scr_layout_output()    BBS_API_SCR_LAYOUT_OUT()
#define bbs_scr_layout_xfer()      BBS_API_SCR_LAYOUT_XFER()
#define bbs_stream_begin()         BBS_API_STREAM_BEGIN()
#define set_prompt()               BBS_API_SET_PROMPT()
#define update_time()              BBS_API_UPDATE_TIME()
#define bbs_path_sys_at(o, s)      BBS_API_PATH_SYS_AT((o), (s))
#define clock_time()               BBS_API_CLOCK_TIME()
#define bbs_transport_poll_send()  BBS_API_POLL_SEND()
#define bbs_serial_flush_outbound() BBS_API_SERIAL_FLUSH()

#endif /* BBS_BANK_BUILD */

#endif /* BBS_API_H_ */
