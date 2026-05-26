/*
 * Static Contiki network configuration for Magnetar BBS (TCP build).
 * Replaces config_read("contiki.cfg") when MAGNETAR_STATIC_NETCFG is defined.
 */

#include <stdlib.h>
#include <string.h>

#include "contiki-net.h"
#include "sys/log.h"
#include "lib/config.h"

#if LOG_CONF_ENABLED
static char *
ipaddrtoa(uip_ipaddr_t *ipaddr, char *buffer)
{
  char *ptr = buffer;
  uint8_t i;

  for(i = 0; i < 4; ++i) {
    *ptr = '.';
    utoa(ipaddr->u8[i], ++ptr, 10);
    ptr += strlen(ptr);
  }

  return buffer + 1;
}
#endif /* LOG_CONF_ENABLED */

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
  /* Defaults match contiki/tools/c64/sample.cfg (edit for your LAN). */
  static const uint8_t host[] = { 192, 168, 0, 111 };
  static const uint8_t mask[] = { 255, 255, 255, 0 };
  static const uint8_t gw[]   = { 192, 168, 0, 1 };
  static const uint8_t dns[]  = { 192, 168, 0, 1 };

  memcpy(config.hostaddr.u8, host, 4);
  memcpy(config.netmask.u8, mask, 4);
  memcpy(config.draddr.u8, gw, 4);
  memcpy(config.resolvaddr.u8, dns, 4);
  config.ethernet.addr = 0xde08;

#if LOG_CONF_ENABLED
  log_message("IP Address:  ", ipaddrtoa(&config.hostaddr, uip_buf));
  log_message("Subnet Mask: ", ipaddrtoa(&config.netmask, uip_buf));
  log_message("Def. Router: ", ipaddrtoa(&config.draddr, uip_buf));
#ifdef STATIC_DRIVER
#define _stringize(arg) #arg
#define  stringize(arg) _stringize(arg)
  log_message("Eth. Driver: ", stringize(STATIC_DRIVER));
#undef  stringize
#undef _stringize
#endif /* STATIC_DRIVER */
  log_message("Driver Port: $", utoa(config.ethernet.addr, uip_buf, 16));
#endif /* LOG_CONF_ENABLED */

  uip_sethostaddr(&config.hostaddr);
  uip_setnetmask(&config.netmask);
  uip_setdraddr(&config.draddr);
}
