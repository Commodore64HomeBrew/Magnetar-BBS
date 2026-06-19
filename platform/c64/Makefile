ifndef CONTIKI
CONTIKI = ../../../contiki
endif

#
# --- Default (omit BBS_SERIAL, or BBS_SERIAL=0): contiki-bbs.c64 -----------------
# Contiki uIP/TCP on the C64; CS8900A (RR-Net / TFE) via STATIC_DRIVER=_cs8900a.
#
# --- BBS_SERIAL=1: magnetar-serial.c64 (SwiftLink, 38400) -------------------------
# --- BBS_SERIAL=2: magnetar-serial.c64 (userport UP2400, 2400) --------------------
# No Contiki TCP/uIP on the C64. Host runs telnet/TCP bridge. XMODEM is serial-only.
#
BBS_SERIAL_FLAVOR := $(strip $(BBS_SERIAL))
ifneq ($(filter 1 2,$(BBS_SERIAL_FLAVOR)),)
CONTIKI_PROJECT := magnetar-serial
SKIP_CONTIKI_MAIN := 1
CONTIKI_WITH_IPV4 := 0
CONTIKI_WITH_IPV6 := 0
CONTIKI_WITH_RIME := 0
CONTIKI_WITH_RPL := 0
BBS_HEADLESS := 1
CONTIKI_BBS_MINIMAL := 1
ifeq ($(BBS_SERIAL_FLAVOR),2)
DEFINES := WITH_LOGGING,WITH_PFS,MAGNETAR_BBS,TELNETD_CONF_REJECT,BBS_SERIAL_TRANSPORT,BBS_SERIAL_UP2400,STATIC_DRIVER=c64_up2400_ser
else
DEFINES := WITH_LOGGING,WITH_PFS,MAGNETAR_BBS,TELNETD_CONF_REJECT,BBS_SERIAL_TRANSPORT,STATIC_DRIVER=c64_swlink_ser
endif
CFLAGS += -DCONNECTIONS=1 -DUIP_CONF_MAX_LISTENPORTS=1 -DUIP_CONF_UDP=0 -DUIP_CONF_ACTIVE_OPEN=0 -DMTU_SIZE=128 -DUIP_CONF_TCP_MSS=64
PROJECT_SOURCEFILES = bbs-wrap.c bbs-shell.c bbs-file.c bbs-shared-reserve.S bbs-core-api.S bbs-resident.c bbs-bank.c bbs-telnetd.c bbs-encodings.c
magnetar-serial: banks
ifeq ($(BBS_SERIAL_FLAVOR),2)
PROJECT_SOURCEFILES += vendor/up2400/c64-up2400.S
endif
else
CONTIKI_PROJECT := contiki-bbs
contiki-bbs: banks
.PHONY: banks
CONTIKI_NET_MINIMAL := 1
BBS_HEADLESS := 1
CONTIKI_BBS_MINIMAL := 1
PROJECT_SOURCEFILES = bbs-wrap.c bbs-shell.c bbs-file.c bbs-shared-reserve.S bbs-core-api.S bbs-resident.c bbs-bank.c bbs-telnetd.c bbs-encodings.c
endif

all: $(CONTIKI_PROJECT) banks memcheck

# Banks: bankN.prg on host; loader opens //x:bankN (no extension).
banks: bank1.prg bank2.prg bank3.prg bank4.prg bank5.prg

.PHONY: memcheck
memcheck: $(CONTIKI_PROJECT).c64 banks
	@echo "  MEMCHECK  shared struct"
	@$(CC) -c $(CFLAGS) -o $(OBJECTDIR)/bbs-shared-size.o bbs-shared-size.c
	@echo "  MEMCHECK  core"
	@python3 $(CURDIR)/check-memmap.py core contiki-c64.map $(CONTIKI_PROJECT).c64 0
	@set -e; for n in 1 2 3 4 5; do \
	  echo "  MEMCHECK  bank$$n"; \
	  python3 $(CURDIR)/check-memmap.py bank bank$$n.prg.map bank$$n.prg $$n; \
	done

$(OBJECTDIR):
	@mkdir -p "$@"

TARGET = c64
BANK_CFG = c64-bbs-bank.cfg
BANK_CC65_LIB = $(OBJECTDIR)/c64-bank.lib

ifeq ($(filter 1 2,$(BBS_SERIAL_FLAVOR)),)
# WITH_PFS: contiki.cfg and //x/ BBS paths via cfs_open (not cbm_* during boot).
DEFINES = WITH_LOGGING,WITH_PFS,TELNETD_CONF_REJECT,UIP_CONF_LOGGING=0,MAGNETAR_BBS,MAGNETAR_STATIC_NETCFG,STATIC_DRIVER=cs8900a
CFLAGS += -DCONNECTIONS=2 -DUIP_CONF_MAX_LISTENPORTS=1 -DUIP_CONF_UDP=0 -DUIP_CONF_ACTIVE_OPEN=0 -DMTU_SIZE=256 -DUIP_CONF_TCP_MSS=180 -DUIP_CONF_ARPTAB_SIZE=2 -DUIP_CONF_ICMP=0 -DUIP_CONF_STATISTICS=0
PROJECT_SOURCEFILES += magnetar-netcfg.c
CONTIKI_WITH_IPV4  = 1
CONTIKI_WITH_IPV6  = 0
CONTIKI_WITH_RIME  = 0
CONTIKI_WITH_RPL   = 0
endif

CFLAGS += --static-locals

# Keep core BSS below $B000; bank overlays load at $B000-$BFFF.
CORE_CFG = c64-bbs-core.cfg

include $(CONTIKI)/Makefile.include

# After Contiki's cpu/6502/Makefile.6502 resets LDFLAGS (-t c64).
# Use our core cfg instead of -t; ld65 rejects -C and -t together.
LDFLAGS := $(filter-out -t $(TARGET) -D __STACKSIZE__=0x200,$(LDFLAGS)) -C $(CORE_CFG)

$(OBJECTDIR)/bbs-shell.o: bbs-shell.c
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -Os -o $@ $<

$(OBJECTDIR)/bbs-telnetd.o: bbs-telnetd.c
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -Os -o $@ $<

ifeq ($(strip $(BBS_HEADLESS)),1)
CONTIKI_HEADLESS_FILTER := \
	ctk.c ctk-conio.c ctk-filedialog.c ctk-textentry-checkbox.c \
	ctk-textentry-cmdline.c ctk-textentry-multiline.c ctk-mouse.c \
	petsciiconv.c cfs-posix-dir.c exec.c \
	slip.c slip_arch.c lan91c96.S w5100.S
ifeq ($(findstring STATIC_DRIVER,$(DEFINES)),)
CONTIKI_HEADLESS_FILTER += cs8900a.S
endif
CONTIKI_SOURCEFILES := $(filter-out $(CONTIKI_HEADLESS_FILTER),$(CONTIKI_SOURCEFILES))
ifneq ($(findstring STATIC_DRIVER=cs8900a,$(DEFINES)),)
# Ethernet STATIC_DRIVER: link cs8900a into the main binary (no cs8900a.eth on disk).
CONTIKI_SOURCEFILES += cs8900a.S
endif
CONTIKI_OBJECTFILES := ${addprefix $(OBJECTDIR)/,${call oname,$(CONTIKI_SOURCEFILES)}}
endif

# CONTIKI_BBS_MINIMAL omits PFS from platform/c64/Makefile.c64; add it for disk config.
ifneq ($(findstring WITH_PFS,$(DEFINES)),)
CONTIKI_SOURCEFILES += pfs.S pfs-dir.c pfs-dir-asm.S pfs_remove.S pfs_seek.S pfs_write.S
CONTIKI_OBJECTFILES := ${addprefix $(OBJECTDIR)/,${call oname,$(CONTIKI_SOURCEFILES)}}
endif

ifneq ($(filter 1 2,$(BBS_SERIAL_FLAVOR)),)
CONTIKI_SOURCEFILES := $(filter-out config.c,$(CONTIKI_SOURCEFILES))
CONTIKI_OBJECTFILES := ${addprefix $(OBJECTDIR)/,${call oname,$(CONTIKI_SOURCEFILES)}}
cs8900a.eth lan91c96.eth w5100.eth:
	@:
else
CONTIKI_SOURCEFILES := $(filter-out config.c,$(CONTIKI_SOURCEFILES))
CONTIKI_OBJECTFILES := ${addprefix $(OBJECTDIR)/,${call oname,$(CONTIKI_SOURCEFILES)}}
%.eth: $(OBJECTDIR)/%.o
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -t module -m $@.map $< $(CC65_LIB)
endif

CC65_LIB = $(patsubst %/target,%/lib,$(shell $(CC) --print-target-path))/$(TARGET).lib

$(OBJECTDIR)/vendor/up2400/c64-up2400.o: vendor/up2400/c64-up2400.S | $(OBJECTDIR)
	@mkdir -p $(dir $@)
	@echo "  AS        $<"
	@$(AS) -t c64 -D DYN_DRV=0 -I vendor/up2400 -o $@ $<

$(BANK_CC65_LIB): | $(OBJECTDIR)
	@cp "$(CC65_LIB)" "$@"
	@ar65 d "$@" crt0.o callmain.o condes.o zerobss.o copydata.o
	@ar65 d "$@" vsprintf.o vsnprintf.o printf.o sprintf.o snprintf.o

$(OBJECTDIR)/bbs-transfer-bank.o: bbs-transfer.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -Os -o $@ $<

$(OBJECTDIR)/bbs-xfer-xmodem-bank.o: bbs-xfer-xmodem.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -Os -o $@ $<

bank5.prg: $(OBJECTDIR)/bbs-bank-hdr-xmodem.o $(OBJECTDIR)/bbs-xfer-xmodem-bank.o $(OBJECTDIR)/bbs-xmodem-bank.o $(BANK_CC65_LIB)
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -C $(BANK_CFG) -m $@.map $(filter-out $(BANK_CC65_LIB),$^) $(BANK_CC65_LIB)

$(OBJECTDIR)/bbs-xmodem-bank.o: bbs-xmodem.S | $(OBJECTDIR)
	@echo "  AS        $<"
	@$(AS) $(ASFLAGS) -I. -o $@ $<

$(OBJECTDIR)/bbs-bank-hdr-msg.o: bbs-bank-hdr-msg.S | $(OBJECTDIR)
	@echo "  AS        $<"
	@$(AS) $(ASFLAGS) -I. -o $@ $<

$(OBJECTDIR)/bbs-core-api.o: bbs-core-api.S | $(OBJECTDIR)
	@echo "  AS        $<"
	@$(AS) $(ASFLAGS) -I. -o $@ $<

$(OBJECTDIR)/bbs-bank-hdr-xfer.o: bbs-bank-hdr-xfer.S | $(OBJECTDIR)
	@echo "  AS        $<"
	@$(AS) $(ASFLAGS) -I. -o $@ $<

$(OBJECTDIR)/bbs-bank-hdr-post.o: bbs-bank-hdr-post.S | $(OBJECTDIR)
	@echo "  AS        $<"
	@$(AS) $(ASFLAGS) -I. -o $@ $<

$(OBJECTDIR)/bbs-bank-hdr-xmodem.o: bbs-bank-hdr-xmodem.S | $(OBJECTDIR)
	@echo "  AS        $<"
	@$(AS) $(ASFLAGS) -I. -o $@ $<

$(OBJECTDIR)/bbs-bank-hdr-stats.o: bbs-bank-hdr-stats.S | $(OBJECTDIR)
	@echo "  AS        $<"
	@$(AS) $(ASFLAGS) -I. -o $@ $<

$(OBJECTDIR)/bbs-bank-stats.o: bbs-bank-stats.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -Os -o $@ $<

bank1.prg: $(OBJECTDIR)/bbs-bank-hdr-xfer.o $(OBJECTDIR)/bbs-transfer-bank.o $(BANK_CC65_LIB)
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -C $(BANK_CFG) -m $@.map $(filter-out $(BANK_CC65_LIB),$^) $(BANK_CC65_LIB)

$(OBJECTDIR)/bbs-post-bank.o: bbs-post.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -Os -o $@ $<

$(OBJECTDIR)/bbs-read-bank.o: bbs-read.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -Os -o $@ $<

$(OBJECTDIR)/bbs-setboard-bank.o: bbs-setboard.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -Os -o $@ $<

$(OBJECTDIR)/bbs-fmt-bank.o: bbs-fmt.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -Os -o $@ $<

$(OBJECTDIR)/bbs-bank-msg.o: bbs-bank-msg.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -o $@ $<

$(OBJECTDIR)/bbs-msg-extra-bank.o: bbs-msg-extra.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -Os -o $@ $<

$(OBJECTDIR)/bbs-sys-stats-bank.o: bbs-sys-stats.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -Os -o $@ $<

bank2.prg: $(OBJECTDIR)/bbs-bank-hdr-post.o $(OBJECTDIR)/bbs-post-bank.o $(CC65_LIB)
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -C $(BANK_CFG) -m $@.map $^

bank3.prg: $(OBJECTDIR)/bbs-bank-hdr-msg.o $(OBJECTDIR)/bbs-bank-msg.o $(OBJECTDIR)/bbs-read-bank.o $(OBJECTDIR)/bbs-setboard-bank.o $(OBJECTDIR)/bbs-fmt-bank.o $(CC65_LIB)
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -C $(BANK_CFG) -m $@.map $^

bank4.prg: $(OBJECTDIR)/bbs-bank-hdr-stats.o $(OBJECTDIR)/bbs-bank-stats.o $(OBJECTDIR)/bbs-sys-stats-bank.o $(OBJECTDIR)/bbs-msg-extra-bank.o $(OBJECTDIR)/bbs-fmt-bank.o $(BANK_CC65_LIB)
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -C $(BANK_CFG) -m $@.map $(filter-out $(BANK_CC65_LIB),$^) $(BANK_CC65_LIB)

