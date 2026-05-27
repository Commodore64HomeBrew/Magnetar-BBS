ifndef CONTIKI
CONTIKI = ../../../contiki
endif

#
# --- Default (omit BBS_SERIAL, or BBS_SERIAL=0): contiki-bbs.c64 -----------------
# Contiki uIP/TCP on the C64; CS8900A (RR-Net / TFE) via STATIC_DRIVER=_cs8900a.
#
# --- BBS_SERIAL=1 only: magnetar-serial.c64 ---------------------------------------
# No Contiki TCP/uIP on the C64. SwiftLink raw serial; host runs telnet/TCP bridge.
# File transfer (XMODEM) is serial-only.
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
PROJECT_SOURCEFILES = bbs-wrap.c bbs-shell.c bbs-file.c bbs-telnetd.c bbs-encodings.c
else
CONTIKI_PROJECT := contiki-bbs
CONTIKI_NET_MINIMAL := 1
BBS_HEADLESS := 1
CONTIKI_BBS_MINIMAL := 1
PROJECT_SOURCEFILES = bbs-wrap.c bbs-shell.c bbs-file.c bbs-telnetd.c bbs-encodings.c
endif

all: $(CONTIKI_PROJECT)

modules: bbs-msg.mod bbs-xfer.mod

$(OBJECTDIR):
	@mkdir -p "$@"

MODULE_FLAVOR := $(if $(filter 1,$(strip $(BBS_SERIAL))),serial,tcp)

TARGET = c64
MODULE_FLAVOR_FILE := obj_$(TARGET)/.modules-flavor

.PHONY: check-module-flavor
check-module-flavor: | $(OBJECTDIR)
	@if [ -f "$(MODULE_FLAVOR_FILE)" ] && [ "$$(cat "$(MODULE_FLAVOR_FILE)")" != "$(MODULE_FLAVOR)" ]; then \
		echo "error: module flavor mismatch (found $$(cat "$(MODULE_FLAVOR_FILE)"), need $(MODULE_FLAVOR)). run 'make clean' first."; \
		exit 1; \
	fi
	@echo "$(MODULE_FLAVOR)" > "$(MODULE_FLAVOR_FILE)"

ifneq ($(strip $(BBS_SERIAL)),1)
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
ifneq ($(findstring STATIC_DRIVER,$(DEFINES)),)
# STATIC_DRIVER: link cs8900a into the main binary (no cs8900a.eth on disk).
CONTIKI_SOURCEFILES += cs8900a.S
endif
CONTIKI_OBJECTFILES := ${addprefix $(OBJECTDIR)/,${call oname,$(CONTIKI_SOURCEFILES)}}
endif

# CONTIKI_BBS_MINIMAL omits PFS from platform/c64/Makefile.c64; add it for disk config.
ifneq ($(strip $(BBS_SERIAL)),1)
ifneq ($(findstring WITH_PFS,$(DEFINES)),)
CONTIKI_SOURCEFILES += pfs.S pfs-dir.c pfs-dir-asm.S pfs_remove.S pfs_seek.S pfs_write.S
CONTIKI_OBJECTFILES := ${addprefix $(OBJECTDIR)/,${call oname,$(CONTIKI_SOURCEFILES)}}
endif
endif

ifeq ($(strip $(BBS_SERIAL)),1)
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
MODULE_CC65_LIB = $(OBJECTDIR)/c64-module.lib

$(OBJECTDIR)/bbs-msg-module.o: bbs-msg-module.c | $(OBJECTDIR) check-module-flavor
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_MSG_MODULE -o $@ $<

$(OBJECTDIR)/bbs-msg-bind.o: bbs-msg-bind.c | $(OBJECTDIR) check-module-flavor
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_MSG_MODULE -o $@ $<

$(OBJECTDIR)/bbs-read-module.o: bbs-read.c | $(OBJECTDIR) check-module-flavor
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_MSG_MODULE -o $@ $<

$(OBJECTDIR)/bbs-post-module.o: bbs-post.c | $(OBJECTDIR) check-module-flavor
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_MSG_MODULE -o $@ $<

$(OBJECTDIR)/bbs-setboard-module.o: bbs-setboard.c | $(OBJECTDIR) check-module-flavor
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_MSG_MODULE -o $@ $<

$(OBJECTDIR)/bbs-module-stubs.o: bbs-module-stubs.S | $(OBJECTDIR) check-module-flavor
	@echo "  AS        $<"
	@$(AS) $(ASFLAGS) -o $@ $<

$(MODULE_CC65_LIB): | $(OBJECTDIR) check-module-flavor
	@cp "$(CC65_LIB)" "$@"
	@ar65 d "$@" crt0.o callmain.o condes.o zerobss.o copydata.o

ifeq ($(strip $(BBS_SERIAL)),1)
$(OBJECTDIR)/bbs-xfer-module.o: bbs-xfer-module.c | $(OBJECTDIR) check-module-flavor
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_XFER_MODULE -o $@ $<

$(OBJECTDIR)/bbs-transfer-module.o: bbs-transfer.c | $(OBJECTDIR) check-module-flavor
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -DBBS_XFER_MODULE -o $@ $<

$(OBJECTDIR)/bbs-xmodem-module.o: bbs-xmodem.S | $(OBJECTDIR) check-module-flavor
	@echo "  AS        $<"
	@$(AS) $(ASFLAGS) -o $@ $<
else
$(OBJECTDIR)/bbs-xfer-module.o: bbs-xfer-module.c | $(OBJECTDIR) check-module-flavor
	@echo "  CC        $<"
	@$(CC) -c $(filter-out --static-locals,$(CFLAGS)) -o $@ $<
endif

bbs-msg.mod: $(OBJECTDIR)/bbs-msg-module.o $(OBJECTDIR)/bbs-msg-bind.o $(OBJECTDIR)/bbs-read-module.o $(OBJECTDIR)/bbs-post-module.o $(OBJECTDIR)/bbs-setboard-module.o $(OBJECTDIR)/bbs-module-stubs.o $(MODULE_CC65_LIB)
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -t module -m $@.map $(filter-out $(MODULE_CC65_LIB),$^) $(MODULE_CC65_LIB)

ifeq ($(strip $(BBS_SERIAL)),1)
bbs-xfer.mod: $(OBJECTDIR)/bbs-xfer-module.o $(OBJECTDIR)/bbs-transfer-module.o $(OBJECTDIR)/bbs-xmodem-module.o $(OBJECTDIR)/bbs-module-stubs.o $(MODULE_CC65_LIB)
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -t module -m $@.map $(filter-out $(MODULE_CC65_LIB),$^) $(MODULE_CC65_LIB)
else
bbs-xfer.mod: $(OBJECTDIR)/bbs-xfer-module.o
	@echo "  LD       " $@
	$(Q)$(LD) -o $@ -t module -m $@.map $< $(CC65_LIB)
endif
