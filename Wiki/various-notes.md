---
sort: 12
---

# Various Notes

## ESP8266 WiFi connection   

Wow, the ESP8266 WiFi connection issues are weird.
Stabilizing the power supply, using short wires, fiddling with delay() and yield(), nothing brought better signal stability.

But while doing some testing, I switched my source from NTSC to PAL.   
That instantly brought the signal back and pings came through regularly.

### Long story short  
The 5725 horizontal PLL (pixel clock recovery from HSync) can lock into a frequency that disturbs the WiFi signal!   
It's in such a narrow range that changing the PLL divider by a few ticks fixes it.

#### Quick Fix
* Change the router WiFi channel (switching to channel 6 or 11 helps in my case)   

#### Better Fix
* Modify the PCB antenna to increase signal reception (Example: https://www.instructables.com/id/Enhanced-NRF24L01/)
* Move the ESP8266 board away from the GBS scaler chip (heatsink) as much as possible   
