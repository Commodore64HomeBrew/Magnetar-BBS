#ifndef BBS_BANK_MACROS_H_
#define BBS_BANK_MACROS_H_

#include "bbs-bank.h"

#define board              (BBS_SHARED->s_board)
#define bbs_config         (BBS_SHARED->s_config)
#define bbs_status         (BBS_SHARED->s_status)
#define bbs_user           (BBS_SHARED->s_user)
#define bbs_usrstats       (BBS_SHARED->s_usrstats)
#define bbs_sysstats       (BBS_SHARED->s_sysstats)
#define bbs_time           (BBS_SHARED->s_time)
#define buf                (*BBS_SHARED->s_buf)
#define shell_event_input  (*(BBS_SHARED->s_shell_ev))
#define shell_output_str   BBS_SHARED->shell_output_str
#define shell_prompt       BBS_SHARED->shell_prompt
#define shell_register_command BBS_SHARED->shell_register_command
#define shell_unregister_command BBS_SHARED->shell_unregister_command
#define bbs_transport_poll BBS_SHARED->transport_poll
#define bbs_transport_poll_send BBS_SHARED->transport_poll_send
#define bbs_transport_flush_outbound BBS_SHARED->transport_flush_outbound
#define bbs_stream_begin     BBS_SHARED->stream_begin
#define bbs_transport_buf_reset BBS_SHARED->transport_buf_reset
#define bbs_transport_buf_discard BBS_SHARED->transport_buf_discard
#define bbs_scr_layout_output BBS_SHARED->scr_layout_output
#define bbs_scr_layout_xfer   BBS_SHARED->scr_layout_xfer
#define buf_append         BBS_SHARED->buf_append
#define buf_putc_raw(c)    (BBS_SHARED->buf_putc_raw((unsigned char)(c)))
#define clock_time()       (BBS_SHARED->clock_time())
#define set_prompt()       (BBS_SHARED->set_prompt())
#define update_time()      (BBS_SHARED->update_time())
#define log_message(a, b)  (BBS_SHARED->log_message((a), (b)))
#define file_path(a, b, c, d) (BBS_SHARED->file_path((a), (b), (c), (d)))
#define bbs_banner(a, b, c, d, e) (BBS_SHARED->bbs_banner((a), (b), (c), (d), (e)))
#define bbs_path_sys_at(o, s) (BBS_SHARED->bbs_path_sys_at((o), (s)))
#define xfer_cwd             (BBS_SHARED->xfer_cwd)

#ifdef BBS_SERIAL_TRANSPORT
#define bbs_serial_flush_outbound BBS_SHARED->serial_flush_outbound
#endif

#endif /* BBS_BANK_MACROS_H_ */
