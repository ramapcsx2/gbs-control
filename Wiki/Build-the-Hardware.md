---
sort: 2
---

# Intro   
Gbs-control is a replacement firmware for GBS8200 upscaler boards.   
It runs on the popular ESP8266 microcontroller and uses the Arduino plattform.    
   
GBS8200 upscalers can be bought on Ebay, at prices around $20.   
Ebay also has ESP8266 microcontroller boards for about $4.   
The ones called "Wemos D1" or "NodeMCU" work well and are recommended.   

# Basic Install
You need the GBS upscaler, an ESP8266 board, a bit of cabling and a jumper or wire link for disabling the onboard processor, so that the ESP8266 can take over.   
Power for the ESP8266 can be provided by, for example:
- using the power supply that powers the GBS > into the ESP8266 boards "Vin" (recommended, white connector next to power input)
- using the GBS regulated Vcc (3.3V) > into the ESP8266 boards "3.3V" input (not recommended, but photo below shows this)

Connect the ESP8266 board ground to a convenient ground point on the GBS.   

Connect the two I2C bus wires (GBS side: SDA and SCL) to the ESP8266 pins of the same name.   
ESP8266 boards do not have standardized pin names, but they follow some naming rules.
- most boards label SDA and SCL directly
- if they are not named (Wemos D1 Mini and many others): SCL is "D1", SDA is "D2"
- if unsure, see if you can find your ESP8266 board [here](https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/)
- GBS side SDA goes to ESP8266 side SDA, same for SCL
![](https://i.imgur.com/TvSAQuX.png)
- Use a jumper to bridge the 2 pins below the first programming port (see picture above)
# Connect DebugPin
To enable automatic image position and timing adjustment, the ESP8266 needs to measure some timings.
Carefully solder a wire from the pictured DebugPin to: 
- Wemos D1 / NodeMCU pin "D6"
- some boards label this pin "D12" or "MISO"
   
# Software setup
Next, the ESP8266 needs to be programmed. Head over to the [software setup](https://github.com/ramapcsx2/gbs-control/wiki/Software-Setup) page.

# Troubleshooting
### No Picture
- Are SDA / SCL reversed? It's safe to reverse them and try again.
- The debug pin does not have to be connected for gbscontrol to work at a basic level.
- Forgot to install the jumper that disables the GBS original controller?
- ~100 Ohm resistor to ground on Sync-in is installed?
- when using a sync stripper: Is the LM1881 source voltage `5V`?   
<span class="anim-fade-in">
```tip
### To Debug Issues
The Arduino IDE serial monitor shows debug information at `115200` baud.   
If your ESP8266 is connected to a computer via USB, you can access this serial monitor to find out more about the issue.   
In the Arduino IDE, you need to select an ESP8266 board that matches your hardware (if unsure, select "LOLIN(WEMOS) D1 R2 & mini").
```
</span>