/*
 * si5351mcu - Si5351 library for Arduino, MCU tuned for size and click-less
 *
 * This is the packed simplest example.
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
 * Just put two frequencies out in clk0 and clk1 at different power levels
 *
 * Take into account your XTAL error, see Si.correction(###) below
 ***************************************************************************/

#include "si5351mcu.h"

// lib instantiation as "Si"
Si5351mcu Si;

// some variables
long F1 =   60000000;  //  60.0 MHz to CLK0
long F2 =   60500000;  //  60.5 MHz to CLK1

void setup() {
    // init the Si5351 lib
    Si.init();

    // For a different xtal (from the default of 27.00000 Mhz)
    // just pass it on the init procedure, just like this
    // Si.init(26570000);

    // set & apply my calculated correction factor
    Si.correction(-1250);

    // set max power to both outputs
    Si.setPower(0, SIOUT_8mA);
    Si.setPower(1, SIOUT_2mA);

    // Set frequencies
    Si.setFreq(0, F1);
    Si.setFreq(1, F2);

    // reset the PLLs
    Si.reset();
}


void loop() {
    // do nothing
}
