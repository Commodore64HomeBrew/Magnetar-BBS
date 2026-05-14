/*
 * Serial transport entrypoint for Magnetar BBS.
 *
 * Makefile (BBS_SERIAL=1) must pass STATIC_DRIVER=c64_swlink_ser so the
 * SwiftLink driver is linked. slip_arch.c is not part of this build, so we
 * must call ser_install(STATIC_DRIVER) here — same as slip_arch_init() does
 * for SLIP — before any ser_open() in bbs-telnetd.c.
 */

#ifndef BBS_SERIAL_TRANSPORT
#error "magnetar-serial.c is for BBS_SERIAL_TRANSPORT builds only; use contiki-bbs.c for TCP/uIP."
#endif

#ifndef STATIC_DRIVER
#error "Magnetar serial requires STATIC_DRIVER (c64_swlink_ser). Set in platform/c64/Makefile DEFINES for BBS_SERIAL=1."
#endif

#include "contiki.h"
#include <serial.h>
#include "bbs-telnetd.h"
#include "lib/error.h"
#include "sys/log.h"

PROCINIT(&etimer_process);

/* Kept for config compatibility even though serial mode never sends busy text. */
char telnetd_reject_text[] = "centronian bbs is busy, please try again later.";

/* SwiftLink (c64_swlink_ser): match modem line speed. */
const struct ser_params magnetar_serial_params = {
  SER_BAUD_38400,
  SER_BITS_8,
  SER_STOP_1,
  SER_PAR_NONE,
  SER_HS_HW
};

AUTOSTART_PROCESSES(&telnetd_process);

static void
magnetar_serial_install_swiftlink(void)
{
  unsigned char err;

  err = ser_install(STATIC_DRIVER);
  if(err == SER_ERR_OK || err == SER_ERR_INSTALLED) {
    return;
  }
  err = (unsigned char)(err + (unsigned char)'0');
  log_message("SwiftLink ser_install err ", (char *)&err);
  error_exit();
}

void
main(void)
{
  process_init();
  magnetar_serial_install_swiftlink();
  procinit_init();
  autostart_start(autostart_processes);

  while(1) {
    process_run();
    etimer_request_poll();
  }
}
