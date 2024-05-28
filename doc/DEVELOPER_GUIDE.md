## Datasheets

[Google drive directory](https://drive.google.com/drive/folders/0B9y2RH4Lb3MZLUYtWGFOMUFVdlE?resourcekey=0-NRfMZNuFYhL2txb3TlAXrg)

## Connections

There are four things required for this to work:

1. Composite video port connected to Green RCA Luma input of the GBS board
2. I2C SDA, SCL & GND connected between the Pi and Port 5 of the GBS board (Do not use port 6!!)
3. A usb keyboard, wireless is fine
4. P8 jumper shorted on GBS board

Here is a pinout diagram for the rev2 and later boards: http://pi.gadgetoid.com/pinout/i2c

## Component Video Sync Levels

As you may know RGB signals are normally 0.7v pk-pk. A component luma signal (or SOG) have an added 0.3v sync level underneath, making the signal 1.0v pk-pk total. The GBS board has a resistor that sets the maximum output range of the video DAC. This is set to create 0.7v pk-pk signals only, which explains the issues with component thus far. Version 0.3 of gbs-control has a sync level setting to adjust this to attempt to get the best compromise between sync stablility and brightness levels for a standard GBS board set for a 0.7v signal. A value of 194 should achieve 0.3v sync levels, but with reduced video range.

However, there is a hardware mod to get correct 1.0v pk-pk output. The formula in the programming guide is:

`Vout = (1.25 * 2046 * Rout) / (846 * Riref)`

Where Vout is the max voltage level before double termination, that is 0.7v * 2 to get 1.4v. Rout equals the output resistance, 75 ohms. And Riref is the setting resistor, normally 150 ohms on the PCB. This give a value 0f 1.48v pk-pk max for the normal GBS boards, which is slightly above the expected 1.4v pk-pk. If the total series value of the Riref resistance can be changed to around 111 ohms, then the output voltage would be 2.0v pk-pk, or 1.0v pk-pk when terminated. I have fitted a switch to one of my boards with a 430 ohm (100 + 330) resistance that can be switch in parallel with the PCB 150 ohm resistor. This gives the option of 150 ohm for 0.7v signals, or 111 ohm for 1.0v signals. So those that want better component output can try this. Use the default sync level of 146 in the menu for 0.3v sync with this mod. The Iref resistor is shown below, it is connected to the sixth pin along, and then to ground.

## Original Firmware Problems
The original firmware had many issues for use with low res gamming content, particularly for us PAL 50Hz users. I have seen and read about lots of these issues before I started work on my custom settings. The most obvious is the deinterlacing applied to progressive 15Khz sources. I have been able to get around this issue with an odd glitch. I have not been able to get 240p input working without using the deinterlacer, but It can be tricked into doing an exact line doubling. And by disabling the vertical scaling engine and setting the output resolution to exactly double the input the results are very sharp.

The most frustrating for myself, was the 50Hz performance. I knew this board was hit and miss with 50Hz, but fagins YouTube video review of the SLG-In-A-Box shows that the unit can work with a 50Hz Megadrive. But that's when my trouble started. I bought a GBS board locally from a PC & Arcade shop. The first board I bought worked with 50Hz input, but had insane amounts of noise in the image. Here are some images from that board with Sonic spinball on the Megadrive.

50Hz Noise:
![image](/doc/img/669cd5363629833.jpg)

50Hz Noise, with just sync signal:
![Image](/doc/img/4e6bb9363629898.jpg)

And yet here it is in 60Hz:
![Image](/doc/img/e2dd64363629965.jpg)


------
More to read:
- [The self introduction thread](https://shmups.system11.org/viewtopic.php?f=1&t=3568&start=1050).
- Original topic at [shmups.system11.org](https://shmups.system11.org/viewtopic.php?f=6&t=52172)
