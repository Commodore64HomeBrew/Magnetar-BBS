#!/bin/bash

make; mv contiki-bbs.c64 magbbs.prg
sudo chcodenet --reset; sleep 3; sudo chusb e magbbs.prg
