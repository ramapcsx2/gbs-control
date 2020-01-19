# Arduino Si5351 Library tuned for size and click noise free. #

This library is tuned for size on the Arduino platform, it will control CLK0, CLK1 and CLK2 outputs for the Si5351A (the version with just 3 clocks out, but you will not be able to use the three at once).

## Inspiration ##

This work is based on the previous work of these great people:

* [Etherkit/NT7S:](https://github.com/etherkit/Si5351Arduino) The mainstream full featured lib, with big code as well (based on Linux kernel code)
* [QRP Labs demo code from Hans Summers:](http://qrp-labs.com/synth/si5351ademo.html) The smallest and simple ones on the net.
* [DK7IH demo code:](https://radiotransmitter.wordpress.com/category/si5351a/) The first clickless noise code on the wild.
* [Jerry Gaffke](https://github.com/afarhan/ubitx.git) integer routines for the Raduino and ubitx

## Set your Goal and make an estrategy ##

There is a few routines in the Internet to manage the Si5351 chip, all of them has a few distinct feature set because they use different strategies (different goals) that make them unique in some way.

My goal is this:
* Keep it as small as possible (Smallest firmware footprint)
* Less phase and click noise possible (Playing with every trick possible)

The main purpose is to be used in Radio receiver projects, so this two mentioned goals are the golden rule.

Let's list some of goals archeivements and bonuses:

**Small firmware footprint:**

A basic sketch to set just only one clock out to a given frequency with a change in power and correcting the XTAL ppm error is only ~3.3 kBytes of firmware (~10% of an Arduino Uno)

The same settings with the [Si5351Arduino library (Etherkit)](https://github.com/etherkit/Si5351Arduino) will give you a bigger firmware space of ~10 kBytes or 31% of an Arduino Uno.

[Jerry Gaffke](https://github.com/afarhan/ubitx.git) embedded routines in the ubitx transceiver has the smallest footprint in the Arduino platform I have seen, but has worst phase noise and smallest frequency range.

**Phase noise to the minimum:**

We use every trick on the datasheet, OEM Application Notes or the Internet to minimize phase noise. (Even a few ones learned on the process)

For example the [Etherkit](https://github.com/etherkit/Si5351Arduino) library and [Jerry Gaffke](https://github.com/afarhan/ubitx.git) embedded routines uses some but not all the tricks to minimize phase noise (Etherkit one gives control over all features, Jerry Gaffke has small footprint and in the process he sacrifice phase noise and frequency range)

**Click noise free:**

If you play from the book (Datasheet and Application Notes) you will have a "click-like" noise burst every time you change the output frequency.

That's not a problem if you intend to use only fixed frequencies at the output, but if you plan to sweep or use it on any application that moves the frequency that click-like noise will haunt you. (like in a radio receiver or transceiver)

I have learned a few tricks from many sources in the Internet and after some local testing I have came across a scheme that make this lib virtually click-noise-less; see the "Click noise free" section below for details.

**Fast frequency changes:**

This was a side effect of the last trick to minimize the click noise, see the "Click noise free" section below for details.

Summary: other routines write all registers for every frequency change, I write half of them most of the time, speeding up the process.

**Two of three**

Yes, there is no such thing as free lunch, to get all the above features and the ones that follow I have to make some sacrifices, in this case spare one of the outputs. See "Two of three" section below.

## Features ##

This are so far the implemented features (Any particular wish? use the Issues tab for that):

* Custom XTAL passing on init (Default is 27.000 MHz (See _Si.init()_ )
* You can pass a correction to the xtal while running (See _Si.correction()_ )
* You have a fast way to power off all outputs of the Chip at once. (See _Si.off()_ )
* You can enable/disable any output at any time (See _Si.enable(clk) and Si.disable(clk)_ )
* By default all outputs are off after the Si.init() procedure. You has to enable them by hand.
* You can only have 2 of the 3 outputs running at any moment (See "Two of three" section below)
* Power control on each output independently (See _Si.setPower(clk, level)_ on the lib header)
* Initial power defaults to the lowest level (2mA) for all outputs.
* You don't need to include and configure the Wire (I2C) library, this lib do that for you already.
* Frequency limits are not hard coded on the lib, so you can stress your hardware to it's particular limit (_You can move usually from ~3kHz to ~225 MHz, far away from the 8kHz to 160 MHz limits from the datasheet_)
* You has a way to verify the status of a particular clock (_Enabled/Disabled by the Si.clkOn[clk] var_)
* From v0.5 and beyond we saved more than 1 kbyte of your precious firmware space due to the use of all integer math now (Worst induced error is below +/- 1 Hz)
* Overclock, yes, you can move the limits upward up to ~250MHz (see the "OVERCLOCK" section below)
* Improved the click noise algorithm to get even more click noise reduction (see Click noise free section below)
* Fast frequency changes as part of the improved click noise algorithm (see Click noise free section below)

## How to use the lib ##

Get the lib by cloning this git repository or get it by clicking the green "Download button" on the page.

Move it or extract it on your library directory

Include the lib in your code:


```
(... your code here ...)

// now include the library
#include "si5351mcu.h"

// lib instantiation as "Si"
Si5351mcu Si;

(... more of your code here ...)

```

Follow this sequence on you setup() procedure:

* Initialize the library with the default or optional Xtal Clock.
* Apply correction factor (if needed)
* Set some frequencies to the desired outputs.
* Enable the desired outputs

Here you have an example code ("Si" is the lib instance):

```
setup() {
    (... your code here ...)

    //////////////////////////////////
    //        Si5351 functions       /
    //////////////////////////////////

    // Init the library, in this case with the default 27.000 Mhz Xtal
    Si.init();

    // commented Init procedure for a not default 25.000 MHz xtal
    //Si.init(25000000L);

    // Optional, apply a pre-calculated correction factor
    Si.correction(-150);    // Xtal is low by 150 Hz

    // Enable the desired outputs with some frequencies
    Si.setFreq(0, 25000000);       // CLK0 output 25.000 MHz
    Si.setFreq(1, 145000000);      // CLK1 output 145.000 MHz

    // enable the outputs
    Si.enable(0);
    Si.enable(1);

    (... more of your code here ...)
}

```

If you need to apply/vary the correction factor **after** the setup process you will get a click noise on the next setFreq() to apply the changes.

Use it, you can enable, disable, change the power or frequency, see this code fragment with some examples:

```
loop() {
    (... your code here ...)

    // disable clk1
    Si.disable(1);

    // change the power of clk0 to 4mA
    Si.setPower(0, SIOUT_4mA);

    // apply a correction factor of 300 Hz (correction will be applied on the next Si.setFreq() call)
    Si.correction(300);

    // change the clk0 output frequency
    Si.setFreq(0, 7110000);

    // power of all outputs
    Si.off();

    (... more of your code here ...)
}
```


## OVERCLOCK ##

Yes, you can overclock the Si5351, the datasheet states that the VCO moves from 600 to 900 MHz and that gives us a usable range from ~3 kHz to 225 MHz.

But what if we can move the VCO frequency to a higher values?

The overclock feature does just that, use a higher top limit for the VCO on the calculations. In my test with two batch of the Si5351A I can get safely up to 1.000 GHz without trouble; in one batch the PLL unlocks around 1.1 GHz and in the other about 1.050 GHz; so I recommend not going beyond 1.000 GHz.

With a maximum VCO of 1.000 GHz and a lower division factor of 4 we have jumped from a 255 MHz to 250 MHz top frequency that can be generated with our cheap chip.

**Some "must include" WARNINGS:**

* The chip was not intended to go that high, so, use it with caution and test it on your hardware moving the overclock limit in steps of 10 MHz starting with 900 MHz and testing with every change until it does not work; then go down by 10 MHz to be in a safe zone.
* Moving the upper limit has its penalty on the lower level, your bottom frequency will move from the ~3 kHz to ~10 kHz range.
* The phase noise of the output if worst as you use a higher frequency, at a _**fixed**_ 250 MHz it's noticeable but no so bad for a TTL or CMOS clock application.
* The phase noise is specially bad if you make a sweep or move action beyond 180 MHz; the phase noise from the unlock state to next lock of the PLL is very noticeable in a spectrum analyzer, even on a cheap RTL-SDR one.
* I recommend to only use the outputs beyond 150 MHz as fixed value and don't move them if you cares about phase noise.

**How to do it?**

You need to declare a macro with the overclock value **BEFORE** the library include, just like this:

```
(... your code here ...)

// Using the overclock feature for the Si5351mcu library
#define SI_OVERCLOCK 1000000000L      // 1.0 GHz in Hz

// now include the library
#include "si5351mcu.h"

// lib instantiation as "Si"
Si5351mcu Si;

// now you can generate frequencies from ~10 kHz up to 250 MHz.

(... more of your code here ...)

```

## Click noise free ##

Click-like noise came from a few sources as per my testing:

* Turn off then on the CLKx outputs (Register 3. Output Enable Control)
* Power down then on the Msynths (CLKx_PDN bits for every Msynth)
* Reset the PLL (Register 177: PLL Reset)

We are concerned about click noise only when changing from one frequency to the other, so if we don't touch the output enable control or the power down msynth registers once activated; then we are set to avoid click from this two first sources.

The last one is tricky, in theory a PLL does not need to be reseted every time you change it's output frequency as it's a **P**hase **L**ocked **L**oop and it's a self correcting algorithm/hardware.

That last idea was put to test on a simple scheme: what if I set a fixed output divider Msynth and move the VCO for it's entire range without resetting it on any point?

If the "PLL reset" is in deed needed I will have some strange behavior at some point, right?

But practice confirmed my idea, I can set a output Msynth of 6 and move the VCO (PLL) for the entire range (600 to 900 MHz) and get a continuous and stable output from 100 to 150 MHz.

Then what is for the "PLL reset" (Register 177) in practice?

Some further test showed that this "reset" function is applied no to the PLL Msynth, but to the output Msynth and not in every case, yes, it has a bad name or a bad explained name.

After some test I find that you need the "PLL reset" (Register 177) trick only on some cases when you change the value of the output divider Msynth.

Implementing that in code was easy, an array to keep track of the actual output divider Msynth and only write it to the chip and reset "the PLL" when it's needed.

Hey! that leads to a I2C time reduction by half (most of the time) as a side effect!

Most of the time when you are making a sweep the output divider Msynth has a constant value and you only moves the VCO (PLL) Then I wrote just 8 bytes to the I2C bus (to control the VCO/PLL) instead of 16 (8 for the VCO/PLL & 8 more for the output divider Msynth) or 17 (16 + reset byte) most of the time, cutting time between writes to half making frequency changes 2x fast as before.

## Two of three ##

Yes, there is a tittle catch here with CLK1 and CLK2: both share PLL_B and as we use our own algorithm to calculate the frequencies and minimize phase noise you can only use one of them at a time.

Note: _In practice you can, but the other will move from the frequency you set, which is an unexpected behavior, so I made them mutually exclusive (CLK1 and CLK2)._

This are the valid combinations for independent clocks output.

* CLK0 and CLK1
* CLK0 and CLK2

Again: You can't use CLK1 and CLK2 at the same time, as soon as you set one of them the other will shut off. That's why you get two of three and one of them must be always CLK0.

## Author & contributors ##

The only author is Pavel Milanes, CO7WT, a cuban amateur radio operator; reachable at pavelmc@gmail.com, Until now I have no contributors or sponsors.

## Where to download the latest version? ##

Always download the latest version from the [github repository](https://github.com/pavelmc/Si5351mcu/)

See ChangeLog.md and Version files on this repository to know what are the latest changes and versions.

## If you like to give thanks... ##

No payment of whatsoever is required to use this code: this is [Free/Libre Software](https://en.wikipedia.org/wiki/Software_Libre), nevertheless donations are very welcomed.

I live in Cuba island and the Internet/Cell is very expensive here (USD $1.00/hour), you can donate anonymously internet time or cell phone air time to me via [Ding Topups](https://www.ding.com/) to keep me connected and developing for the homebrewers community.

If you like to do so, please go to Ding, select Cuba, select Cubacell (for phone top up) or Nauta (for Internet time)

* For phone topup use this number (My cell, feel free to call me if you like): +53 538-478-19
* For internet time use this user (Nauta service): co7wt@nauta.com.cu (that's not an email but an user account name)

Thanks!
