/*
 * Serial transport entrypoint for Magnetar BBS.
 * No uIP/TCP stack is brought in for this build.
 */

#ifndef BBS_SERIAL_TRANSPORT
#error "magnetar-serial.c is for BBS_SERIAL_TRANSPORT builds only; use contiki-bbs.c for TCP/uIP."
#endif

#include "contiki.h"
#include <serial.h>
#include "bbs-telnetd.h"

PROCINIT(&etimer_process);

/* Kept for config compatibility even though serial mode never sends busy text. */
char telnetd_reject_text[] = "centronian bbs is busy, please try again later.";

/* SwiftLink serial defaults (same style as contiki serconfig output).
 * First serial split used SER_BAUD_38400; 9600 matches common modem/bridge lines. */
const struct ser_params magnetar_serial_params = {
  SER_BAUD_38400,
  SER_BITS_8,
  SER_STOP_1,
  SER_PAR_NONE,
  SER_HS_HW
};

AUTOSTART_PROCESSES(&telnetd_process);

void
main(void)
{
  process_init();
  procinit_init();
  autostart_start(autostart_processes);

  while(1) {
    process_run();
    etimer_request_poll();
  }
}
