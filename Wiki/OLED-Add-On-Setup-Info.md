### NOTE: This OLED Add-On is completely optional to GBS-Control. If you choose not to install it, your GBS-Control board will still operate normally.

This addon is not meant to replace the Web UI and was/is just a means to help select the more common settings. This is still a work-in-progress to add more features in the future. 

## Installation:
- Materials Needed:
   * 1x I2C 0.96" OLED Screen
   * 1x Rotary Encoder (EC11)
   * 2x 0.47uF Capacitors (50V, Electrolytic)

- Schematic:

NOTE: This is just a general/draft "how-to" schematic.

![schematic_test1](https://i.imgur.com/X3MbPzX.png)

## General Usage

After the boot logo splash screen, you'll be presented with a simple menu system.

![mainMenu](https://i.imgur.com/sI7LR1n.jpg)

By rotating left or right on the encoder, the indicator will move up or down. Pressing the encoder will select the option.
  (If the indicator is acting odd, you may have the CLK and DATA pins of the encoder reversed)

Each submenu will have a _BACK_ option which upon selecting will go back to the main menu, for example:

![resolutionBackEx](https://i.imgur.com/jPDNq2N.jpg)

The only menu that this is slightly different is the _Current Settings_ menu. Within this menu you will be able see various specs that are currently being utilized. While in this menu press the encoder once without rotation and this will bring you back to the main menu. 

![currentsettingsExample](https://i.imgur.com/wPpbZNC.jpg)

## Additional Information
### OLED:
- Communication method is very important for this! Please, try to get a hold of an **I2C** not a **SPI** screen.
- At the moment, this was designed (font/text alignments) for the 0.96" variant of screens. Any other size will produce "interesting" results.
- Color configurations will be found in various listings/datasheets for screens (BLUE,BLUE/YELLOW,WHITE). While any color configuration will work, it's recommended to stick to a single color option to achieve a cleaner look. Case in point shown below.

![dcbootExample](https://i.imgur.com/cBahzGe.jpg)

### Rotary Encoders:
- This type of encoder has many applications and benefits to its design. However, one major downside is that it tends to be a _noisy_ device. Along with the ISR and filter the goal is to not have many wrong inputs occur while rotation or selection.
- Any general EC11 model of encoder will work. Pretty much "EC11 Rotary Encoder" on Amazon/Ebay will yield the correct results. It's a fairly inexpensive and common part
- Breakout board versions of this part should work, but isn't entirely recommended. Mainly coming down to cost and the fact that there could be many variations of said breakout boards out there. Meaning, potentially differing resistor/capacitor values for the filtering circuit. This *could* lead to disruption of the ISR. Tests were done to ensure the capacitors/resistors worked well together with the ISR. If you choose to use a breakout then omit the capacitors and pull-up resistors.
