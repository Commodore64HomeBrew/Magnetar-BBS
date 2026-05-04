ifndef CONTIKI
CONTIKI = ../../../contiki
endif

CONTIKI_PROJECT = contiki-bbs

all: $(CONTIKI_PROJECT)

TARGET = c64

#PROJECTDIRS = ../.. $(CONTIKI)/apps/telnetd

# bbs-wrap.c: word-break + backward scan (shared by bbs-file / bbs-telnetd in a later step)
PROJECT_SOURCEFILES = bbs-wrap.c bbs-shell.c bbs-file.c bbs-read.c bbs-post.c bbs-setboard.c bbs-telnetd.c bbs-encodings.c

# Ship: no WITH_LOGGING (faster uIP, smaller; see 6502def.h LOG/UIP logging).
# WITH_BOOST keeps TCP split; do not add WITH_GUI / WITH_DNS / WITH_MOUSE for headless BBS.
#DEFINES = WITH_BOOST,TELNETD_CONF_REJECT,TELNETD_CONF_MAX_IDLE_TIME=0
DEFINES = WITH_LOGGING,WITH_BOOST,TELNETD_CONF_REJECT

#
# RR-Net / ethernet (default):  make clean all
#
# SwiftLink SLIP — Contiki statically links SwiftLink as c64_swlink_ser when SLIP=1:
# builds use cc65's serial driver described at https://www.cc65.org/doc/c64-5.html#ss5.5 .
# Requires a SLIP-capable peer (not direct Ethernet telnet).
# MSS 1500 overflows BSS once SLIP is enabled; shrink uIP packet buffer + TCP slots:
ifdef SLIP
DEFINES := $(DEFINES),MTU_SIZE=576,CONNECTIONS=1
endif

# IPv4-only BBS: pin off other stacks so Makefile.include never pulls v6/mesh by mistake.
CONTIKI_WITH_IPV4  = 1
CONTIKI_WITH_IPV6  = 0
CONTIKI_WITH_RIME  = 0
CONTIKI_WITH_RPL   = 0

CFLAGS += --static-locals

include $(CONTIKI)/Makefile.include
