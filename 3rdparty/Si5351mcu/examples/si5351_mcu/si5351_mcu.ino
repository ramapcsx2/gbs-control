/*
 * si5351mcu - Si5351 library for Arduino, MCU tuned for size and click-less
 *
 * This is the packed example.
 *
 * Copyright (C) 2017 Pavel Milanes <pavelmc@gmail.com>
 *
 * Many chunk of codes are derived-from/copied from other libs
 * all GNU GPL licenced:
 *  - Linux Kernel (www.kernel.org)
 *  - Hans Summers libs and demo code (qrp-labs.com)
 *  - Etherkit (NT7S) Si5351 libs on github
 *  - DK7IH example.
 *  - Jerry Gaffke integer routines for the bitx20 group
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/***************************************************************************
 * This example is meant to be monitored with an RTL-SDR receiver
 * but you can change the frequency and other vars to test with your hardware.
 *
 * Set your SDR software to monitor from 60 to 62 Mhz.
 *
 * This will set 60.0 Mhz in clock 0, put and alternating frequencies
 * at 60.5 and 61.0 Mhz on CLK1 and CLK2 to show they are mutually exclusive.
 *
 * Then make a sweep from 60 to 62 Mhz on CLK2, with an stop every 200Khz
 * and then a train of one second pulses will follow with varying power levels
 *
 * Take into account your XTAL error, see Si.correction(###) below
 *
 ***************************************************************************/

#include "si5351mcu.h"

// lib instantiation as "Si"
Si5351mcu Si;

// Stop every X Mhz for Y seconds to measure
#define EVERY        200000   // stop every 200khz
#define DANCE             3   // 3 seconds

// some variables
long freqStart =   60000000;  //  60.0 MHz
long freqStop  =   62000000;  //  62.0 MHz
long step      =      10000;  //  10.0 kHz
long freq      = freqStart;


void setup() {
    // init the Si5351 lib
    Si.init();

    // For a different xtal (from the default of 27.00000 Mhz)
    // just pass it on the init procedure, just like this
    // Si.init(26570000);

    // set & apply my calculated correction factor
    Si.correction(-1250);

    // pre-load some sweet spot freqs
    Si.setFreq(0, freqStart);
    Si.setFreq(1, freqStart);

    // reset the PLLs
    Si.reset();

    // put a tone in the start freq on CLK0
    Si.setFreq(0, freqStart);
    Si.enable(0);

    // make the dance on the two outputs
    Si.setFreq(1, freqStart +  500000);      // CLK1 output
    Si.enable(1);
    delay(3000);
    // Si.disable(1);   // no need to disable, enabling CLK2 disable this

    Si.setFreq(2, freqStart + 1000000);      // CLK2 output
    Si.enable(2);
    delay(3000);
    //Si.disable(2);   // no need to disable, enabling CLK1 disable this

    Si.setFreq(1, freqStart +  500000);      // CLK1 output
    Si.enable(1);
    delay(3000);
    //Si.disable(1);   // no need to disable, enabling CLK2 disable this

    Si.setFreq(2, freqStart + 1000000);      // CLK2 output
    Si.enable(2);
    delay(3000);
    Si.disable(2);   // this is the last in the dance, disable it

    // shut down CLK0
    Si.disable(0);

    // set CLK2 to the start freq
    Si.setFreq(2, freqStart);   // it's disabled by now
}


void loop() {
    // check for the stop to measure
    if ((freq % EVERY) == 0) {
        // it's time to flip-flop it

        for (byte i = 0; i < 4; i++) {
            // power off the clk2 output
            Si.disable(2);
            delay(500);
            // power mod, the lib define some macros for that:
            // SIOUT_2mA, SIOUT_4mA, SIOUT_6mA and SIOUT_8mA
            //
            // But they are incidentally matching the 0 to 3 count..
            // so I will use the cycle counter for that (i)
            //
            // moreover, setting the power on an output will enable it
            // so I will explicit omit the enable here
            Si.setPower(2, i);
            //Si.enable(2);

            delay(1000);
        }

        // reset the power to low
        Si.setPower(2, SIOUT_2mA);

        // set it for the new cycle
        freq += step;
    } else {
        // check if we are on the limits
        if (freq <= freqStop) {
            // no, set and increment
            Si.setFreq(2,freq);        // but it can be with CLK0 or CLK1 instead

            // set it for the new cycle
            freq += step;

            // a short delay to slow things a little.
            delay(50);
        } else {
            // we reached the limit, reset to start
            freq = freqStart;
        }
    }
}
