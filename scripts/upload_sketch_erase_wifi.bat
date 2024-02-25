@echo off
cmd /k esptool.exe --chip esp8266 --baud "921600" "" erase_region "0x3FC000" 0x4000 --before default_reset --after hard_reset write_flash 0x0 gbs-control.ino.bin
