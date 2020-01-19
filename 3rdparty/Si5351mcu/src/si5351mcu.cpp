/*
 * si5351mcu - Si5351 library for Arduino MCU tuned for size and click-less
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

#include "Arduino.h"
#include "si5351mcu.h"

// wire library loading, if not defined
#ifndef WIRE_H
    #include "Wire.h"
#endif

/*****************************************************************************
 * This is the default init procedure, it set the Si5351 with this params:
 * XTAL 27.000 Mhz
 *****************************************************************************/
 void Si5351mcu::init() {
    // init with the default freq
    init(int_xtal);
}


/*****************************************************************************
 * This is the custom init procedure, it's used to pass a custom xtal
 * and has the duty of init the I2C protocol handshake
 *****************************************************************************/
 void Si5351mcu::init(uint32_t nxtal) {
    // set the new base xtal freq
    base_xtal = int_xtal = nxtal;

    // start I2C (wire) procedures
    Wire.begin();

    // power off all the outputs
    off();
}


/*****************************************************************************
 * This function set the freq of the corresponding clock.
 *
 * In my tests my Si5351 can work between 7,8 Khz and ~225 Mhz [~250 MHz with
 * overclocking] as usual YMMV
 *
 * Click noise:
 * - The lib has a reset programmed [aka: click noise] every time it needs to
 *   change the output divider of a particular MSynth, if you move in big steps
 *   this can lead to an increased rate of click noise per tunning step.
 * - If you move at a pace of a few Hz each time the output divider will
 *   change at a low rate, hence less click noise per tunning step.
 * - The output divider moves [change] faster at high frequencies, so at HF the
 *   clikc noise is at the real minimum possible.
 *
 * [See the README.md file for other details]
 ****************************************************************************/
void Si5351mcu::setFreq(uint8_t clk, unsigned long freq) {
    uint8_t a, R = 1, shifts = 0;
    uint32_t b, c, f, fvco, outdivider;
    uint32_t MSx_P1, MSNx_P1, MSNx_P2, MSNx_P3;

    // Overclock option
    #ifdef SI_OVERCLOCK
        // user a overclock setting for the VCO, max value in my hardware
        // was 1.05 to 1.1 GHz, as usual YMMV [See README.md for details]
        outdivider = SI_OVERCLOCK / freq;
    #else
        // normal VCO from the datasheet and AN
        // With 900 MHz beeing the maximum internal PLL-Frequency
        outdivider = 900000000 / freq;
    #endif

    // use additional Output divider ("R")
    while (outdivider > 900) {
        R = R * 2;
        outdivider = outdivider / 2;
    }

    // finds the even divider which delivers the intended Frequency
    if (outdivider % 2) outdivider--;

    // Calculate the PLL-Frequency (given the even divider)
    fvco = outdivider * R * freq;

    // Convert the Output Divider to the bit-setting required in register 44
    switch (R) {
        case 1:   R = 0; break;
        case 2:   R = 16; break;
        case 4:   R = 32; break;
        case 8:   R = 48; break;
        case 16:  R = 64; break;
        case 32:  R = 80; break;
        case 64:  R = 96; break;
        case 128: R = 112; break;
    }

    // we have now the integer part of the output msynth
    // the b & c is fixed below
    MSx_P1 = 128 * outdivider - 512;

    // calc the a/b/c for the PLL Msynth
    /***************************************************************************
    * We will use integer only on the b/c relation, and will >> 5 (/32) both
    * to fit it on the 1048 k limit of C and keep the relation
    * the most accurate possible, this works fine with xtals from
    * 24 to 28 Mhz.
    *
    * This will give errors of about +/- 2 Hz maximum
    * as per my test and simulations in the worst case, well below the
    * XTAl ppm error...
    *
    * This will free more than 1K of the final eeprom
    *
    ****************************************************************************/
    a = fvco / int_xtal;
    b = (fvco % int_xtal) >> 5;     // Integer par of the fraction
                                    // scaled to match "c" limits
    c = int_xtal >> 5;              // "c" scaled to match it's limits
                                    // in the register

    // f is (128*b)/c to mimic the Floor(128*(b/c)) from the datasheet
    f = (128 * b) / c;

    // build the registers to write
    MSNx_P1 = 128 * a + f - 512;
    MSNx_P2 = 128 * b - f * c;
    MSNx_P3 = c;

    // PLLs and CLK# registers are allocated with a shift, we handle that with
    // the shifts var to make code smaller
    if (clk > 0 ) shifts = 8;

    // plls, A & B registers separated by 8 bytes
    i2cWrite(26 + shifts, (MSNx_P3 & 65280) >> 8);   // Bits [15:8] of MSNx_P3 in register 26
    i2cWrite(27 + shifts, MSNx_P3 & 255);
    i2cWrite(28 + shifts, (MSNx_P1 & 196608) >> 16);
    i2cWrite(29 + shifts, (MSNx_P1 & 65280) >> 8);   // Bits [15:8]  of MSNx_P1 in register 29
    i2cWrite(30 + shifts, MSNx_P1 & 255);            // Bits [7:0]  of MSNx_P1 in register 30
    i2cWrite(31 + shifts, ((MSNx_P3 & 983040) >> 12) | ((MSNx_P2 & 983040) >> 16)); // Parts of MSNx_P3 and MSNx_P1
    i2cWrite(32 + shifts, (MSNx_P2 & 65280) >> 8);   // Bits [15:8]  of MSNx_P2 in register 32
    i2cWrite(33 + shifts, MSNx_P2 & 255);            // Bits [7:0]  of MSNx_P2 in register 33

    // Write the output divider msynth only if we need to, in this way we can
    // speed up the frequency changes almost by half the time most of the time
    // and the main goal is to avoid the nasty click noise on freq change
    if (omsynth[clk] != outdivider) {
        // CLK# registers are exactly 8 * clk# bytes shifted from a base register.
        shifts = clk * 8;

        // multisynths
        i2cWrite(42 + shifts, 0);                        // Bits [15:8] of MS0_P3 (always 0) in register 42
        i2cWrite(43 + shifts, 1);                        // Bits [7:0]  of MS0_P3 (always 1) in register 43
        // See datasheet, special trick when R=4
        if (outdivider == 4) {
            i2cWrite(44 + shifts, 12 | R);
            i2cWrite(45 + shifts, 0);            // Bits [15:8] of MSx_P1 must be 0
            i2cWrite(46 + shifts, 0);            // Bits [7:0] of MSx_P1 must be 0
        } else {
            i2cWrite(44 + shifts, ((MSx_P1 & 196608) >> 16) | R);  // Bits [17:16] of MSx_P1 in bits [1:0] and R in [7:4]
            i2cWrite(45 + shifts, (MSx_P1 & 65280) >> 8);    // Bits [15:8]  of MSx_P1 in register 45
            i2cWrite(46 + shifts, MSx_P1 & 255);             // Bits [7:0]  of MSx_P1 in register 46
        }
        i2cWrite(47 + shifts, 0);                        // Bits [19:16] of MS0_P2 and MS0_P3 are always 0
        i2cWrite(48 + shifts, 0);                        // Bits [15:8]  of MS0_P2 are always 0
        i2cWrite(49 + shifts, 0);                        // Bits [7:0]   of MS0_P2 are always 0

        // must reset the so called "PLL", in fact the output msynth
        reset();

        // keep track of the change
        omsynth[clk] = (uint16_t)outdivider;
    }
}


/*****************************************************************************
 * Reset of the PLLs and multisynths output enable
 *
 * This must be called to soft reset the PLLs and cycle the output of the
 * multisynths: this is the "click" noise source in the RF spectrum.
 *
 * So it must be avoided at all costs, so this lib just call it at the
 * initialization of the PLLs and when a correction is applied
 *
 * If you are concerned with accuracy you can implement a reset every
 * other Mhz to be sure it get exactly on spot.
 ****************************************************************************/
void Si5351mcu::reset(void) {
    // This soft-resets PLL A & B (32 + 128) in just one step
    i2cWrite(177, 0xA0);
}


/*****************************************************************************
 * Function to disable all outputs
 *
 * The PLL are kept running, just the m-synths are powered off.
 *
 * This allows to keep the chip warm and exactly on freq the next time you
 * enable an output.
 ****************************************************************************/
void Si5351mcu::off() {
    // This disable all the CLK outputs
    for (byte i=0; i<3; i++) disable(i);
}


/*****************************************************************************
 * Function to set the correction in Hz over the Si5351 XTAL.
 *
 * This will call a reset of the PLLs and multi-synths so it will produce a
 * click every time it's called
 ****************************************************************************/
void Si5351mcu::correction(int32_t diff) {
    // apply some corrections to the xtal
    int_xtal = base_xtal + diff;

    // reset the PLLs to apply the correction
    reset();
}


/*****************************************************************************
 * This function enables the selected output
 *
 * Beware: ZERO is clock output enabled
 *****************************************************************************/
void Si5351mcu::enable(uint8_t clk) {
    // var to handle the mask of the registers value
    uint8_t m = SICLK0_R;
    if (clk > 0) m = SICLK12_R;

    // write the register value
    i2cWrite(16 + clk, m + clkpower[clk]);

    // 1 & 2 are mutually exclusive
    if (clk == 1) disable(2);
    if (clk == 2) disable(1);

    // update the status of the clk
    clkOn[clk] = 1;
}


/*****************************************************************************
 * This function disables the selected output
 *
 * Beware: ONE is clock output disabled
 * *****************************************************************************/
void Si5351mcu::disable(uint8_t clk) {
    // send
    i2cWrite(16 + clk, 128);

    // update the status of the clk
    clkOn[clk] = 0;
}


/****************************************************************************
 * Set the power output for each output independently
 ***************************************************************************/
void Si5351mcu::setPower(byte clk, byte power) {
    // set the power to the correct var
    clkpower[clk] = power;

    // now enable the output to get it applied
    enable(clk);
}


/****************************************************************************
 * Private function to send the register data to the Si5351, arduino way.
 ***************************************************************************/
void Si5351mcu::i2cWrite(byte regist, byte value){
    Wire.beginTransmission(SIADDR);
    Wire.write(regist);
    Wire.write(value);
    Wire.endTransmission();
}
