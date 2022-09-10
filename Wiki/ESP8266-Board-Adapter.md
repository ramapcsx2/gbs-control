---
sort: 6
---
# ESP8266 adapter

```warning
The adapter method places the ESP8266 very close to the TiVo5725 scaler chip, which exposes it to electromagnetic interference.   
As a result, the ESP8266 can have issues with its WiFi, resulting in limited access to the web interface.   
As such, I don't recommend using this adapter method.   
Instead, place the ESP8266 module right next to the GBS 8200 board, near the I2C connection headers.   
```

## Old Article

### Overview
This is a method of mating a GBS board with a common ESP8266 development board, using a socket to attach to the original MCU.   
It provides all the required signals for the ESP8266 to take over control of the scaler.   

<span class="anim-fade-in">

![](https://i.imgur.com/fEwLqRT.png)

</span>
   
### Grinding down stand-offs

<span class="anim-fade-in">

![](https://i.imgur.com/cbKwnoD.png)

</span>
   
### Inner socket surface has to be flat and even

<span class="anim-fade-in">

![](https://i.imgur.com/SFFIPYm.png)

</span>
   
### Signal Locations

<span class="anim-fade-in">

![](https://i.imgur.com/9DciLIe.png)

</span>
   
### Mating Adapter with ESP8266 Dev Board

<span class="anim-fade-in">

![](https://i.imgur.com/1JQfn3r.png)

</span>
