ifndef CONTIKI
CONTIKI = ../../../contiki
endif

#
# --- Default (omit BBS_SERIAL, or BBS_SERIAL=0): contiki-bbs.c64 -----------------
# Contiki uIP/TCP on the C64; CS8900A (RR-Net / TFE) via STATIC_DRIVER=_cs8900a.
#
# --- BBS_SERIAL=1 only: magnetar-serial.c64 ---------------------------------------
# No Contiki TCP/uIP on the C64. SwiftLink raw serial; host runs telnet/TCP bridge.
#
ifeq ($(strip $(BBS_SERIAL)),1)
CONTIKI_PROJECT := magnetar-serial
SKIP_CONTIKI_MAIN := 1
CONTIKI_WITH_IPV4 := 0
CONTIKI_WITH_IPV6 := 0
CONTIKI_WITH_RIME := 0
CONTIKI_WITH_RPL := 0
BBS_HEADLESS := 1
CONTIKI_BBS_MINIMAL := 1
DEFINES := WITH_LOGGING,TELNETD_CONF_REJECT,BBS_SERIAL_TRANSPORT,STATIC_DRIVER=c64_swlink_ser
CFLAGS += -DCONNECTIONS=1 -DUIP_CONF_MAX_LISTENPORTS=1 -DUIP_CONF_UDP=0 -DUIP_CONF_ACTIVE_OPEN=0 -DMTU_SIZE=128 -DUIP_CONF_TCP_MSS=64
else
CONTIKI_PROJECT := contiki-bbs
CONTIKI_NET_MINIMAL := 1
BBS_HEADLESS := 1
CONTIKI_BBS_MINIMAL := 1
endif

all: $(CONTIKI_PROJECT)

TARGET = c64

PROJECT_SOURCEFILES = bbs-wrap.c bbs-shell.c bbs-file.c bbs-read.c bbs-post.c bbs-setboard.c bbs-telnetd.c bbs-encodings.c

ifneq ($(strip $(BBS_SERIAL)),1)
DEFINES = WITH_LOGGING,TELNETD_CONF_REJECT,UIP_CONF_LOGGING=0,MAGNETAR_BBS,MAGNETAR_STATIC_NETCFG,STATIC_DRIVER=cs8900a
CFLAGS += -DCONNECTIONS=2 -DUIP_CONF_MAX_LISTENPORTS=1 -DUIP_CONF_UDP=0 -DUIP_CONF_ACTIVE_OPEN=0 -DMTU_SIZE=606 -DUIP_CONF_TCP_MSS=536 -DUIP_CONF_ARPTAB_SIZE=2 -DUIP_CONF_ICMP=0 -DUIP_CONF_STATISTICS=0
PROJECT_SOURCEFILES += magnetar-netcfg.c
CONTIKI_WITH_IPV4  = 1
CONTIKI_WITH_IPV6  = 0
CONTIKI_WITH_RIME  = 0
CONTIKI_WITH_RPL   = 0
endif

CFLAGS += --static-locals

# contiki-main.c includes magnetar-netcfg.h when MAGNETAR_STATIC_NETCFG is set.
ifneq ($(strip $(BBS_SERIAL)),1)
CFLAGS += -I.
endif

include $(CONTIKI)/Makefile.include

ifeq ($(strip $(BBS_HEADLESS)),1)
CONTIKI_HEADLESS_FILTER := \
	ctk.c ctk-conio.c ctk-filedialog.c ctk-textentry-checkbox.c \
	ctk-textentry-cmdline.c ctk-textentry-multiline.c ctk-mouse.c \
	petsciiconv.c cfs-posix-dir.c exec.c \
	pfs.S pfs-dir.c pfs-dir-asm.S pfs_remove.S pfs_seek.S pfs_write.S \
	slip.c slip_arch.c lan91c96.S w5100.S
ifeq ($(findstring STATIC_DRIVER,$(DEFINES)),)
CONTIKI_HEADLESS_FILTER += cs8900a.S
endif
CONTIKI_SOURCEFILES := $(filter-out $(CONTIKI_HEADLESS_FILTER),$(CONTIKI_SOURCEFILES))
ifneq ($(findstring STATIC_DRIVER,$(DEFINES)),)
# STATIC_DRIVER: link cs8900a into the main binary (no cs8900a.eth on disk).
CONTIKI_SOURCEFILES += cs8900a.S
endif
CONTIKI_OBJECTFILES := ${addprefix $(OBJECTDIR)/,${call oname,$(CONTIKI_SOURCEFILES)}}
endif

ifeq ($(strip $(BBS_SERIAL)),1)
CONTIKI_SOURCEFILES := $(filter-out config.c,$(CONTIKI_SOURCEFILES))
CONTIKI_OBJECTFILES := ${addprefix $(OBJECTDIR)/,${call oname,$(CONTIKI_SOURCEFILES)}}
cs8900a.eth lan91c96.eth w5100.eth:
	@:
else
CONTIKI_SOURCEFILES := $(filter-out config.c,$(CONTIKI_SOURCEFILES))
CONTIKI_OBJECTFILES := ${addprefix $(OBJECTDIR)/,${call oname,$(CONTIKI_SOURCEFILES)}}
CC65_LIB = $(patsubst %/target,%/lib,$(shell $(CC) --print-target-path))/$(TARGET).lib
%.eth: $(OBJECTDIR)/%.o
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -t module -m $@.map $< $(CC65_LIB)
endif
