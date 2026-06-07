#!/bin/bash
make clean
make all; mv contiki-bbs.c64 magbbs.prg
mv bbs-bank1.bin bbs-bank1.bin.prg
mv bbs-bank2.bin bbs-bank2.bin.prg
mv bbs-bank3.bin bbs-bank3.bin.prg
mv bbs-bank4.bin bbs-bank4.bin.prg
mv bbs-bank5.bin bbs-bank5.bin.prg
mv bbs-bank6.bin bbs-bank6.bin.prg
sudo chcodenet --reset; sleep 3; sudo chusb e magbbs.prg
