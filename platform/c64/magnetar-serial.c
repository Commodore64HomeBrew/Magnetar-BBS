/*
 * Serial transport entrypoint for Magnetar BBS.
 *
 * Makefile serial builds (BBS_SERIAL=1 or 2) pass STATIC_DRIVER so the
 * chosen cc65 serial driver is linked. slip_arch.c is not part of this build,
 * so we call ser_install(STATIC_DRIVER) here before any ser_open() in
 * bbs-telnetd.c.
 *
 * BBS_SERIAL=1  SwiftLink cartridge (c64_swlink_ser, 38400 HW handshake)
 * BBS_SERIAL=2  Userport UP2400 driver (c64_up2400_ser, 2400 no handshake)
 */

#ifndef BBS_SERIAL_TRANSPORT
#error "magnetar-serial.c is for BBS_SERIAL_TRANSPORT builds only; use contiki-bbs.c for TCP/uIP."
#endif

#ifndef STATIC_DRIVER
#error "Magnetar serial requires STATIC_DRIVER. Set in platform/c64/Makefile for BBS_SERIAL=1 or 2."
#endif

#include "contiki.h"
#include <serial.h>
#include "bbs-telnetd.h"
#include "lib/error.h"
#include "sys/log.h"

#ifdef BBS_SERIAL_UP2400
extern void c64_up2400_ser[];
#else
#include <c64.h>
#endif

PROCINIT(&etimer_process);

/* Kept for config compatibility even though serial mode never sends busy text. */
char telnetd_reject_text[] = "centronian bbs is busy, please try again later.";

#ifdef BBS_SERIAL_UP2400
/* Userport UP2400 (c64_up2400_ser): 2400 baud, software handshake only. */
const struct ser_params magnetar_serial_params = {
  SER_BAUD_2400,
  SER_BITS_8,
  SER_STOP_1,
  SER_PAR_NONE,
  SER_HS_NONE
};
#else
/* SwiftLink (c64_swlink_ser): match modem line speed. */
const struct ser_params magnetar_serial_params = {
  SER_BAUD_38400,
  SER_BITS_8,
  SER_STOP_1,
  SER_PAR_NONE,
  SER_HS_HW
};
#endif

AUTOSTART_PROCESSES(&telnetd_process);

static void
magnetar_serial_install_driver(void)
{
  unsigned char err;

#ifdef BBS_SERIAL_UP2400
  err = ser_install(c64_up2400_ser);
#else
  err = ser_install(c64_swlink_ser);
#endif
  if(err == SER_ERR_OK || err == SER_ERR_INSTALLED) {
    return;
  }
  err = (unsigned char)(err + (unsigned char)'0');
#ifdef BBS_SERIAL_UP2400
  log_message("UP2400 ser_install err ", (char *)&err);
#else
  log_message("SwiftLink ser_install err ", (char *)&err);
#endif
  error_exit();
}

void
main(void)
{
  process_init();
  magnetar_serial_install_driver();
  procinit_init();
  autostart_start(autostart_processes);

  while(1) {
    process_run();
    etimer_request_poll();
  }
}
