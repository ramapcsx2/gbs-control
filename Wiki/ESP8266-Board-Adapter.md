---
sort: 6
---
# ESP8266 adapter

# NOTE
The adapter method places the ESP8266 very close to the TiVo5725 scaler chip, which exposes it to electromagnetic interference.   
As a result, the ESP8266 can have issues with its WiFi, resulting in limited access to the web interface.   
As such, I don't recommend using this adapter method.   
Instead, place the ESP8266 module right next to the GBS 8200 board, near the I2C connection headers.   

### Old Article

### Overview
This is a method of mating a GBS board with a common ESP8266 development board, using a socket to attach to the original MCU.   
It provides all the required signals for the ESP8266 to take over control of the scaler.   

![](https://i.imgur.com/fEwLqRT.png)
   
### Grinding down stand-offs
![](https://i.imgur.com/cbKwnoD.png)
   
### Inner socket surface has to be flat and even
![](https://i.imgur.com/SFFIPYm.png)
   
### Signal Locations
![](https://i.imgur.com/9DciLIe.png)
   
### Mating Adapter with ESP8266 Dev Board
![](https://i.imgur.com/1JQfn3r.png)