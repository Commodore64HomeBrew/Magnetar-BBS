# Build an 8K C64 autostart cartridge that loads MAGBBS from device 8.
#
# Outputs:
#   magbbs-boot.bin  raw 8K ROM image ($8000-$9FFF)
#   magbbs-boot.crt  VICE CRT V2.0 wrapper

TARGET   := magbbs-boot
AS       := ca65
LD       := ld65
CFG      := c64-bootcart.cfg
SRC      := bootcart.S

.PHONY: all clean

all: $(TARGET).bin $(TARGET).crt

$(TARGET).o: $(SRC)
	$(AS) -t c64 -o $@ $<

$(TARGET).bin: $(TARGET).o $(CFG)
	$(LD) -C $(CFG) -o $@ $< -m $(TARGET).map

$(TARGET).crt: $(TARGET).bin bin2crt.py
	python3 bin2crt.py $< $@

clean:
	rm -f $(TARGET).o $(TARGET).bin $(TARGET).crt $(TARGET).map
