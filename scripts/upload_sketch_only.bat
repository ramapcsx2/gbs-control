@echo off
cmd /k esptool.exe --chip esp8266 --baud "921600" ""  --before default_reset --after hard_reset write_flash 0x0 gbs-control.ino.bin
