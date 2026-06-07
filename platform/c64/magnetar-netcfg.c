/*
 * Static Contiki network configuration for Magnetar BBS (TCP build).
 * Edit defaults below, then rebuild magbbs.prg.
 */

#include <string.h>

#include "contiki-net.h"
#include "magnetar-netcfg.h"

/* Same layout as cpu/6502/lib/config.c (STATIC_DRIVER: no driver filename). */
struct {
  uip_ipaddr_t hostaddr;
  uip_ipaddr_t netmask;
  uip_ipaddr_t draddr;
  uip_ipaddr_t resolvaddr;
  union {
    struct {
      uint16_t addr;
#ifndef STATIC_DRIVER
      char     name[12 + 1];
#endif
    }          ethernet;
    uint8_t    slip[5];
  };
} config;

void
magnetar_netcfg_init(void)
{
  /* Each octet is a separate byte: { a, b, c, d } not a.b.c.d */
  static const uint8_t host[] = { 192, 168, 0, 83 };
  static const uint8_t mask[] = { 255, 255, 255, 0 };
  static const uint8_t gw[]   = { 192, 168, 0, 1 };
  static const uint8_t dns[]  = { 192, 168, 0, 1 };

  memcpy(config.hostaddr.u8, host, 4);
  memcpy(config.netmask.u8, mask, 4);
  memcpy(config.draddr.u8, gw, 4);
  memcpy(config.resolvaddr.u8, dns, 4);
  config.ethernet.addr = 0xde08;

  uip_sethostaddr(&config.hostaddr);
  uip_setnetmask(&config.netmask);
  uip_setdraddr(&config.draddr);
}
