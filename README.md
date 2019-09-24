# gbs-control

This project provides an alternative control software for Tvia Trueview5725 based upscalers / video converter boards.
These boards are a cost efficient and offer powerful hardware, if used correctly.

Gbscontrol provides a complete replacement of the original solution, offering many improvements:
- very low lag
- sharp and defined upscaling, comparing well to other -expensive- units
- no synchronization loss switching 240p/480i
- on demand motion adaptive deinterlacer that engages automatically and only when needed
- works with almost anything: 8 bit consoles, 16/32 bit consoles, 2000s consoles, home computers, etc
- little compromise, eventhough the hardware is very affordable (less than $30 typically)
- lots of useful features and image enhancements
- optional control interface via web browser, utilizing the ESP8266 WiFi capabilities
- good color reproduction with auto gain and auto offset for the tripple 8 bit @ 160MHz ADC
 
Supported standards are NTSC / PAL, the EDTV and HD formats, as well as VGA from 192p to 1600x1200 (earliest DOS, home computers, PC).
Sources can be connected via RGB/HV (VGA), RGBS (game consoles, SCART) or Component Video (YUV).
Various variations are supported, such as the PlayStation 2's VGA modes that run over Component cables.

Gbscontrol is a continuation of previous work by dooklink, mybook4, Ian Stedman and others.  
It uses the Arduino development platform, targeting the popular Espressif ESP8266 microcontroller.  
https://github.com/esp8266/Arduino

Head over to the wiki for the setup guide to learn how to build and use it.
https://github.com/ramapcsx2/gbs-control/wiki/Build-the-Hardware

Development threads:  
https://shmups.system11.org/viewtopic.php?f=6&t=52172   
https://circuit-board.de/forum/index.php/Thread/15601-GBS-8220-Custom-Firmware-in-Arbeit/   

Previous work:  
https://github.com/dooklink/gbs-control  
https://github.com/mybook4/DigisparkSketches/tree/master/GBS_Control  
https://ianstedman.wordpress.com/  
