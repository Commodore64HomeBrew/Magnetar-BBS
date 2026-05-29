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
DEFINES := WITH_LOGGING,TELNETD_CONF_REJECT,BBS_SERIAL_TRANSPORT,BBS_SERIAL_UP2400,STATIC_DRIVER=c64_up2400_ser
else
DEFINES := WITH_LOGGING,TELNETD_CONF_REJECT,BBS_SERIAL_TRANSPORT,STATIC_DRIVER=c64_swlink_ser
endif
CFLAGS += -DCONNECTIONS=1 -DUIP_CONF_MAX_LISTENPORTS=1 -DUIP_CONF_UDP=0 -DUIP_CONF_ACTIVE_OPEN=0 -DMTU_SIZE=128 -DUIP_CONF_TCP_MSS=64
PROJECT_SOURCEFILES = bbs-wrap.c bbs-shell.c bbs-file.c bbs-read.c bbs-post.c bbs-setboard.c bbs-msg-extra.c bbs-fmt.c bbs-telnetd.c bbs-encodings.c
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
PROJECT_SOURCEFILES = bbs-wrap.c bbs-shell.c bbs-file.c bbs-msg-extra.c bbs-fmt.c bbs-resident.c bbs-bank.c bbs-telnetd.c bbs-encodings.c
endif

all: $(CONTIKI_PROJECT)

ifneq ($(filter 1 2,$(BBS_SERIAL_FLAVOR)),)
banks:
	@:
else
banks: bbs-bank1.bin bbs-bank2.bin bbs-bank3.bin
endif

$(OBJECTDIR):
	@mkdir -p "$@"

TARGET = c64
BANK_CFG = c64-bbs-bank.cfg
BANK_CC65_LIB = $(OBJECTDIR)/c64-bank.lib

ifeq ($(filter 1 2,$(BBS_SERIAL_FLAVOR)),)
# WITH_PFS: contiki.cfg and //x/ BBS paths via cfs_open (not cbm_* during boot).
DEFINES = WITH_LOGGING,WITH_PFS,TELNETD_CONF_REJECT,UIP_CONF_LOGGING=0,MAGNETAR_BBS,STATIC_DRIVER=cs8900a
CFLAGS += -DCONNECTIONS=2 -DUIP_CONF_MAX_LISTENPORTS=1 -DUIP_CONF_UDP=0 -DUIP_CONF_ACTIVE_OPEN=0 -DMTU_SIZE=606 -DUIP_CONF_TCP_MSS=536 -DUIP_CONF_ARPTAB_SIZE=2 -DUIP_CONF_ICMP=0 -DUIP_CONF_STATISTICS=0
CONTIKI_WITH_IPV4  = 1
CONTIKI_WITH_IPV6  = 0
CONTIKI_WITH_RIME  = 0
CONTIKI_WITH_RPL   = 0
endif

CFLAGS += --static-locals

include $(CONTIKI)/Makefile.include

$(OBJECTDIR)/bbs-shell.o: bbs-shell.c
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -o $@ $<

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
ifeq ($(filter 1 2,$(BBS_SERIAL_FLAVOR)),)
ifneq ($(findstring WITH_PFS,$(DEFINES)),)
CONTIKI_SOURCEFILES += pfs.S pfs-dir.c pfs-dir-asm.S pfs_remove.S pfs_seek.S pfs_write.S
CONTIKI_OBJECTFILES := ${addprefix $(OBJECTDIR)/,${call oname,$(CONTIKI_SOURCEFILES)}}
endif
endif

ifneq ($(filter 1 2,$(BBS_SERIAL_FLAVOR)),)
CONTIKI_SOURCEFILES := $(filter-out config.c,$(CONTIKI_SOURCEFILES))
CONTIKI_OBJECTFILES := ${addprefix $(OBJECTDIR)/,${call oname,$(CONTIKI_SOURCEFILES)}}
cs8900a.eth lan91c96.eth w5100.eth:
	@:
else
%.eth: $(OBJECTDIR)/%.o
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -t module -m $@.map $< $(CC65_LIB)
endif

CC65_LIB = $(patsubst %/target,%/lib,$(shell $(CC) --print-target-path))/$(TARGET).lib

$(OBJECTDIR)/vendor/up2400/c64-up2400.o: vendor/up2400/c64-up2400.S | $(OBJECTDIR)
	@mkdir -p $(dir $@)
	@echo "  AS        $<"
	@$(AS) -t c64 -D DYN_DRV=0 -I vendor/up2400 -o $@ $<

ifneq ($(filter 1 2,$(BBS_SERIAL_FLAVOR)),)
else

$(BANK_CC65_LIB): | $(OBJECTDIR)
	@cp "$(CC65_LIB)" "$@"
	@ar65 d "$@" crt0.o callmain.o condes.o zerobss.o copydata.o
	@ar65 d "$@" vsprintf.o vsnprintf.o printf.o sprintf.o snprintf.o

$(OBJECTDIR)/bbs-transfer-bank.o: bbs-transfer.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -Os -o $@ $<

$(OBJECTDIR)/bbs-bank-xfer.o: bbs-bank-xfer.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -o $@ $<

$(OBJECTDIR)/bbs-xmodem-bank.o: bbs-xmodem.S | $(OBJECTDIR)
	@echo "  AS        $<"
	@$(AS) $(ASFLAGS) -o $@ $<

bbs-bank1.prg: $(OBJECTDIR)/bbs-bank-xfer.o $(OBJECTDIR)/bbs-transfer-bank.o $(OBJECTDIR)/bbs-xmodem-bank.o $(BANK_CC65_LIB)
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -C $(BANK_CFG) -m $@.map $(filter-out $(BANK_CC65_LIB),$^) $(BANK_CC65_LIB)

bbs-bank1.bin: bbs-bank1.prg
bbs-bank2.bin: bbs-bank2.prg
bbs-bank3.bin: bbs-bank3.prg

%.bin: %.prg
	@echo "  BIN      " $@
	@dd if=$< of=$@ bs=1 skip=2 2>/dev/null


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

$(OBJECTDIR)/bbs-bank-post.o: bbs-bank-post.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -o $@ $<

$(OBJECTDIR)/bbs-bank-msg.o: bbs-bank-msg.c | $(OBJECTDIR)
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_BANK_BUILD -o $@ $<

bbs-bank2.prg: $(OBJECTDIR)/bbs-bank-post.o $(OBJECTDIR)/bbs-post-bank.o $(CC65_LIB)
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -C $(BANK_CFG) -m $@.map $^

bbs-bank3.prg: $(OBJECTDIR)/bbs-bank-msg.o $(OBJECTDIR)/bbs-read-bank.o $(OBJECTDIR)/bbs-setboard-bank.o $(OBJECTDIR)/bbs-fmt-bank.o $(CC65_LIB)
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -C $(BANK_CFG) -m $@.map $^

endif
