ifndef CONTIKI
CONTIKI = ../../../contiki
endif

#
# --- Default (omit BBS_SERIAL, or BBS_SERIAL=0): contiki-bbs.c64 -----------------
# Contiki uIP/TCP on the C64; RR-Net / TFE / etc. (Ethernet drivers on disk, contiki.cfg).
#
# --- BBS_SERIAL=1 only: magnetar-serial.c64 ---------------------------------------
# No Contiki TCP/uIP on the C64. SwiftLink raw serial; host runs telnet/TCP bridge.
# Outbound/post sizes use bbs-defs.h defaults (same as RR-Net: BBS_BUFFER_SIZE 1267,
# BBS_POST_BUFFER_SIZE 1500); do not override BBS_BUFFER_SIZE here or banners/prompts
# truncate when the ring fills.
#
ifeq ($(strip $(BBS_SERIAL)),1)
CONTIKI_PROJECT := magnetar-serial
CONTIKI_WITH_IPV4 := 0
CONTIKI_WITH_IPV6 := 0
CONTIKI_WITH_RIME := 0
CONTIKI_WITH_RPL := 0
BBS_HEADLESS := 1
DEFINES := TELNETD_CONF_REJECT,BBS_SERIAL_TRANSPORT,STATIC_DRIVER=c64_swlink_ser
else
# Default program name; links full IPv4 + uIP + tcpip (RR-Net style deployment).
CONTIKI_PROJECT := contiki-bbs
endif

all: $(CONTIKI_PROJECT)

TARGET = c64

#PROJECTDIRS = ../.. $(CONTIKI)/apps/telnetd

# bbs-wrap.c: word-break + backward scan (shared by bbs-file / bbs-telnetd in a later step)
PROJECT_SOURCEFILES = bbs-wrap.c bbs-shell.c bbs-file.c bbs-read.c bbs-post.c bbs-setboard.c bbs-telnetd.c bbs-encodings.c

ifneq ($(strip $(BBS_SERIAL)),1)
# UIP_CONF_LOGGING=0: silence uIP UIP_LOG() (e.g. "neither tcp nor icmp" on IGMP etc.);
# keeps LOG_CONF_ENABLED / log_message from WITH_LOGGING without logscr per stray IP.
DEFINES = WITH_LOGGING,WITH_BOOST,TELNETD_CONF_REJECT,UIP_CONF_LOGGING=0
CONTIKI_WITH_IPV4  = 1
CONTIKI_WITH_IPV6  = 0
CONTIKI_WITH_RIME  = 0
CONTIKI_WITH_RPL   = 0
endif

CFLAGS += --static-locals

include $(CONTIKI)/Makefile.include

# Serial-only link: drop uIP/TCP/Ethernet sources from the library set (after MODULES).
ifeq ($(strip $(BBS_SERIAL)),1)
CONTIKI_SOURCEFILES := $(filter-out contiki-main.c config.c slip_arch.c uip_arch.c ethernet-drv.c ethernet.c cs8900a.S lan91c96.S w5100.S slip.c,$(CONTIKI_SOURCEFILES))
endif
