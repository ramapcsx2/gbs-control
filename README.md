# gbs-control

This project provides an alternative control software for Tvia Trueview5725 based video converter boards.
These boards are a cost efficient and offer powerful hardware, if used correctly.

Gbscontrol optimizes the Trueview5725 multimedia processor for the task of upscaling "240p" content.
It adds useful features, more resolution presets and offers a browser based user control panel via WiFi.
It eliminates common issues such as unnecessary deinterlacing, lag, bad color reproduction or synchronization loss.
 
Supported standards are NTSC / PAL, the EDTV and HD formats, as well as VGA from 192p to about 1600x1200.
Sources can be connected via RGB/HV (VGA), RGBS (game consoles, SCART) or Component Video (YUV).
Various variations are supported, such as the PlayStation 2 "VGA" over Component Video modes.

Gbscontrol is a continuation of previous work by dooklink, mybook4, Ian Stedman and others.

Head over to the wiki for the setup guide!
https://github.com/ramapcsx2/gbs-control/wiki/Build-the-Hardware

Development thread:  
https://shmups.system11.org/viewtopic.php?f=6&t=52172  

Previous work:  
https://github.com/dooklink/gbs-control  
https://github.com/mybook4/DigisparkSketches/tree/master/GBS_Control  
https://ianstedman.wordpress.com/  

This project uses the Arduino platform, targeting the popular Espressif ESP8266 microcontroller.
https://github.com/esp8266/Arduino
