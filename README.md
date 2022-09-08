<div class="d-table col-12">
  <div class="col-2 d-table-cell v-align-middle">
    <img class="width-full avatar" src="https://github.com/ramapcsx2/gbs-control/blob/master/public/assets/icons/icon-1024.png" alt="github" />
  </div>
  <div class="col-10 d-table-cell v-align-middle pl-4">
    <h1 class="text-normal lh-condensed">GBS Control</h1>
    <p class="h4 color-fg-muted text-normal mb-2">an alternative firmware for Tvia Trueview5725 based upscalers / video converter boards.</p>
    <a class="color-fg-muted text-small" href="./Wiki/README.md">Read the documentation</a>
  </div>
</div>


 
## Features:
- very low lag
- sharp and defined upscaling, comparing well to other -expensive- units
- no synchronization loss switching 240p/480i (output runs independent from input, sync to display never drops)
- on demand motion adaptive deinterlacer that engages automatically and only when needed
- works with almost anything: 8 bit consoles, 16/32 bit consoles, 2000s consoles, home computers, etc
- little compromise, eventhough the hardware is very affordable (less than $30 typically)
- lots of useful features and image enhancements
- optional control interface via web browser, utilizing the ESP8266 WiFi capabilities
- good color reproduction with auto gain and auto offset for the tripple 8 bit @ 160MHz ADC
- optional bypass capability to, for example, transcode Component to RGB/HV in high quality
 
Supported standards are NTSC / PAL, the EDTV and HD formats, as well as VGA from 192p to 1600x1200 (earliest DOS, home computers, PC).
Sources can be connected via RGB/HV (VGA), RGBS (game consoles, SCART) or Component Video (YUV).
Various variations are supported, such as the PlayStation 2's VGA modes that run over Component cables.

Gbscontrol is a continuation of previous work by dooklink, mybook4, Ian Stedman and others.  

Bob from RetroRGB did an [overview video on the project](https://www.youtube.com/watch?v=fmfR0XI5czI). This is a highly recommended watch!   


Head over to the [wiki](./Wiki/README.md) for the setup guide to learn how to build and use it!  
[Build the hardware](./Wiki/Build-the-Hardware.md)

Development threads:  
https://shmups.system11.org/viewtopic.php?f=6&t=52172   
https://circuit-board.de/forum/index.php/Thread/15601-GBS-8220-Custom-Firmware-in-Arbeit/   

If you like gbscontrol, you can now <a class="bmc-button" target="_blank" href="https://www.buymeacoffee.com/ramapcsx2"><img src="https://cdn.buymeacoffee.com/buttons/bmc-new-btn-logo.svg" alt="Buy me a coffee">buy me a coffee</a>.   
Cheers! :)   
