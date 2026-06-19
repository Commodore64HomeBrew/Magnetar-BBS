; Packed JMP stubs for bank overlays. Segment RESAPI links at $A0E4.
.include "bbs-api.inc"

.import _shell_output_str, _shell_prompt
.import _shell_register_command, _shell_unregister_command
.import _buf_append, _buf_putc_raw
.import _bbs_banner, _log_message, _file_path
.import _bbs_transport_poll, _bbs_transport_flush_outbound
.import _bbs_transport_buf_reset, _bbs_transport_buf_discard
.import _bbs_scr_layout_output, _bbs_scr_layout_xfer, _bbs_stream_begin
.import _set_prompt, _update_time, _bbs_path_sys_at
.import _clock_time, _bbs_transport_poll_send
.import _bbs_serial_flush_outbound

.segment "RESAPI"

        .org $A0E4

        jmp _shell_output_str
        jmp _shell_prompt
        jmp _shell_register_command
        jmp _shell_unregister_command
        jmp _buf_append
        jmp _buf_putc_raw
        jmp _bbs_banner
        jmp _log_message
        jmp _file_path
        jmp _bbs_transport_poll
        jmp _bbs_transport_flush_outbound
        jmp _bbs_transport_buf_reset
        jmp _bbs_transport_buf_discard
        jmp _bbs_scr_layout_output
        jmp _bbs_scr_layout_xfer
        jmp _bbs_stream_begin
        jmp _set_prompt
        jmp _update_time
        jmp _bbs_path_sys_at
        jmp _clock_time
        jmp _bbs_transport_poll_send
        jmp _bbs_serial_flush_outbound
