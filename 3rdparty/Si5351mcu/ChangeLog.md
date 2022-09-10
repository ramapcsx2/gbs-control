# Si5351mcu Changelog File #

## v0.6.1 (Augus 1, 2018) ##

* Documentation update and completion to match code features and tricks

## v0.6 (February 10, 2018) ##

* Feature: you can now OVERCLOCK the Si5351 to get up to 250 MHz from it, see the overclock subject on the README.md
* Documentation improvements and re-arrangement.

## v0.5 (February 7, 2018) ##

* Feature: All integer math now, induced error must be at worst +/- 2 Hz.
* Feature: Clock status via clkOn[clk] public var.
* Bug Fix: Output divider low limit safe guard in place (it make some trouble under some circumstances)
* New super simple example.

## v0.4 (August 2, 2017) ##

* Bug Fix: Triaged a strange level problem with freqs above ~112 Mhz, the signal level start dropping around 112MHz and ~150 MHz suddenly go beyond limits (high) to slowly drop again up to the end. Fact: the lib needs a reset() on EVERY frequency change above VCO/8 (~112MHz). Remember that datasheet specs are 8KHz to 160MHz, but we are pushing it up to ~225 Mhz (max_vco/4)
* Code refractory on some points to match correct behavior, as previous code has little bugs introduced by bad documentation from Silicon Labs (+1 for the chip; -5 for the docs errors). See Bitx20 mail-lits archives for June-August 2017 for more info and the debate.

## v0.3 (June 14, 2017) ##

* Feature: the lib now handle the include and start of the I2C (Wire) library internally via the init procedures
* Added a new generic init() procedure to handle the default parameters
* The init() function is required from now on (MANDATORY)
* Fixed the way we handled the base xtal and the correction factor

## v0.2rc (April 23, 2017) ##

* Added power level support for each output independently, watch out!: setting the power level will enable the output.
* Set default power to the lowest one (2mA) from the maximun possible (8mA).
* Fixed the need for a reset after each correction, it does it now automatically
* Added a init function to pass a custom xtal
* Modified the example to show no need for a init unless you use a different xtal
* Improved the keywords.txt file to improve edition on the Arduino IDE
* Included a "features" section on the README.md

## v0.1rc (April 20, 2017) ##

* Added enable(), disable() and off() functions.
* Added support for handling all the three outputs of the Si5351A, (CLK1 & CLK2 are mutually-exclusive)
* Updated the example with the new functions.
* Improved library logic by reusing and optimizing functions.
* Improved the documentation and comments (lib code, README and example)
* The compiled code is slightly smaller now (~1% on an ATMega328p)
* Added Changelog and version files.
* Extensive tests made to validate every function.

## Initial Release, v0.0 (April 9, 2017) ##

* Basic functionality.
