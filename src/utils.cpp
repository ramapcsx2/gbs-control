/*
#####################################################################################
# File: utils.cpp                                                                  #
# File Created: Thursday, 2nd May 2024 5:37:47 pm                                   #
# Author:                                                                           #
# Last Modified: Thursday, 20th June 2024 6:41:56 pm                      #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/
#include "utils.h"

// GBS segment for direct access
static uint8_t lastSegment = 0xFF;

/**
 * @brief a simple getter to prevent parameters from spreading in code
 *
 * @return true if it is Passthrough / Bypass mode
 * @return false
 */
bool utilsIsPassThroughMode() {
    return (uopt.resolutionID == OutputHdBypass || uopt.resolutionID == OutputRGBHVBypass);
}

/**
 * @brief a simple getter to prevent parameters from spreading in code
 *
 * @return true if it is NOT a Passthrough / Bypass mode
 * @return false
 */
bool utilsNotPassThroughMode() {
    return (uopt.resolutionID != OutputHdBypass && uopt.resolutionID != OutputRGBHVBypass);
}

/**
 * @brief a simple getter to prevent parameters from
 *          spreading in code (15kHz/Downscale mode)
 *
 * @return true
 * @return false
 */
bool utilsIsDownscaleMode() {
    return (uopt.resolutionID == Output15kHz || uopt.resolutionID == Output15kHz50);
}

/**
 * @brief a simple getter to prevent parameters from spreading in code
 *
 * @return true
 * @return false
 */
bool utilsNotDownscaleMode() {
    return (uopt.resolutionID != Output15kHz && uopt.resolutionID != Output15kHz50);
}

/**
 * @brief
 *
 */
void latchPLLAD()
{
    GBS::PLLAD_LAT::write(0);
    delayMicroseconds(128);
    GBS::PLLAD_LAT::write(1);
}

/**
 * @brief
 *
 */
void resetPLLAD()
{
    GBS::PLLAD_VCORST::write(1);
    GBS::PLLAD_PDZ::write(1); // in case it was off
    latchPLLAD();
    GBS::PLLAD_VCORST::write(0);
    delay(1);
    latchPLLAD();
    rto.clampPositionIsSet = 0; // test, but should be good
    rto.continousStableCounter = 1;
}

/**
 * @brief
 *
 */
void resetPLL()
{
    GBS::PLL_VCORST::write(1);
    delay(1);
    GBS::PLL_VCORST::write(0);
    delay(1);
    rto.clampPositionIsSet = 0; // test, but should be good
    rto.continousStableCounter = 1;
}

/**
 * @brief
 *
 */
void ResetSDRAM()
{
    // GBS::SDRAM_RESET_CONTROL::write(0x87); // enable "Software Control SDRAM Idle Period" and "SDRAM_START_INITIAL_CYCLE"
    // GBS::SDRAM_RESET_SIGNAL::write(1);
    // GBS::SDRAM_RESET_SIGNAL::write(0);
    // GBS::SDRAM_START_INITIAL_CYCLE::write(0);
    GBS::SDRAM_RESET_CONTROL::write(0x02);
    GBS::SDRAM_RESET_SIGNAL::write(1);
    GBS::SDRAM_RESET_SIGNAL::write(0);
    GBS::SDRAM_RESET_CONTROL::write(0x82);
}

/**
 * @brief Set the Cs Vs Start object
 *
 * @param start
 */
void setCsVsStart(uint16_t start)
{
    GBS::SP_SDCS_VSST_REG_H::write(start >> 8);
    GBS::SP_SDCS_VSST_REG_L::write(start & 0xff);
}

/**
 * @brief Set the Cs Vs Stop object
 *
 * @param stop
 */
void setCsVsStop(uint16_t stop)
{
    GBS::SP_SDCS_VSSP_REG_H::write(stop >> 8);
    GBS::SP_SDCS_VSSP_REG_L::write(stop & 0xff);
}

/**
 * @brief
 *
 */
void freezeVideo()
{
    // GBS::IF_VB_ST::write(GBS::IF_VB_SP::read());
    uopt.freezeCapture = true;
    GBS::CAPTURE_ENABLE::write(0);
}

/**
 * @brief
 *
 */
void unfreezeVideo()
{
    // GBS::IF_VB_ST::write(GBS::IF_VB_SP::read() - 2);
    uopt.freezeCapture = false;
    GBS::CAPTURE_ENABLE::write(1);
}

/**
 * @brief soft reset cycle
 *          This restarts all chip units, which is sometimes required
 *          when important config bits are changed.
 */
void resetDigital()
{
    bool keepBypassActive = 0;
    if (GBS::SFTRST_HDBYPS_RSTZ::read() == 1) { // if HDBypass enabled
        keepBypassActive = 1;
    }

    // GBS::RESET_CONTROL_0x47::write(0x00);
    GBS::RESET_CONTROL_0x47::write(0x17); // new, keep 0,1,2,4 on (DEC,MODE,SYNC,INT) //MODE okay?

    // if (rto.outModeHdBypass) { // if currently in bypass
    if (uopt.resolutionID == OutputHdBypass) { // if currently in bypass
        GBS::RESET_CONTROL_0x46::write(0x00);
        GBS::RESET_CONTROL_0x47::write(0x1F);
        return; // 0x46 stays all 0
    }

    GBS::RESET_CONTROL_0x46::write(0x41); // keep VDS (6) + IF (0) enabled, reset rest
    if (keepBypassActive == 1) {          // if HDBypass enabled
        GBS::RESET_CONTROL_0x47::write(0x1F);
    }
    // else {
    //   GBS::RESET_CONTROL_0x47::write(0x17);
    // }
    GBS::RESET_CONTROL_0x46::write(0x7f);
}

/**
 * @brief
 *
 */
void resetDebugPort()
{
    GBS::PAD_BOUT_EN::write(1); // output to pad enabled
    GBS::IF_TEST_EN::write(1);
    GBS::IF_TEST_SEL::write(3);    // IF vertical period signal
    GBS::TEST_BUS_SEL::write(0xa); // test bus to SP
    GBS::TEST_BUS_EN::write(1);
    GBS::TEST_BUS_SP_SEL::write(0x0f); // SP test signal select (vsync in, after SOG separation)
    GBS::MEM_FF_TOP_FF_SEL::write(1);  // g0g13/14 shows FIFO stats (capture, rff, wff, etc)
    // SP test bus enable bit is included in TEST_BUS_SP_SEL
    GBS::VDS_TEST_EN::write(1); // VDS test enable
}

/**
 * @brief Returns videoMode ID, idntified by TV chip
 *
 * @return uint8_t
 */
uint8_t getVideoMode()
{
    uint8_t detectedMode = 0;

    if (rto.videoStandardInput >= 14) { // check RGBHV first // not mode 13 here, else mode 13 can't reliably exit
        detectedMode = GBS::STATUS_16::read();
        if ((detectedMode & 0x0a) > 0) {    // bit 1 or 3 active?
            return rto.videoStandardInput; // still RGBHV bypass, 14 or 15
        } else {
            return 0;
        }
    }

    detectedMode = GBS::STATUS_00::read();

    // note: if stat0 == 0x07, it's supposedly stable. if we then can't find a mode, it must be an MD problem
    if ((detectedMode & 0x07) == 0x07) {
        if ((detectedMode & 0x80) == 0x80) { // bit 7: SD flag (480i, 480P, 576i, 576P)
            if ((detectedMode & 0x08) == 0x08)
                return 1; // ntsc interlace
            if ((detectedMode & 0x20) == 0x20)
                return 2; // pal interlace
            if ((detectedMode & 0x10) == 0x10)
                return 3; // edtv 60 progressive
            if ((detectedMode & 0x40) == 0x40)
                return 4; // edtv 50 progressive
        }

        detectedMode = GBS::STATUS_03::read();
        if ((detectedMode & 0x10) == 0x10) {
            return 5;
        } // hdtv 720p

        if (rto.videoStandardInput == 4) {
            detectedMode = GBS::STATUS_04::read();
            if ((detectedMode & 0xFF) == 0x80) {
                return 4; // still edtv 50 progressive
            }
        }
    }

    detectedMode = GBS::STATUS_04::read();
    if ((detectedMode & 0x20) == 0x20) { // hd mode on
        if ((detectedMode & 0x61) == 0x61) {
            // hdtv 1080i // 576p mode tends to get misdetected as this, even with all the checks
            // real 1080i (PS2): h:199 v:1124
            // misdetected 576p (PS2): h:215 v:1249
            if (GBS::VPERIOD_IF::read() < 1160) {
                return 6;
            }
        }
        if ((detectedMode & 0x10) == 0x10) {
            if ((detectedMode & 0x04) == 0x04) {
                // normally HD2376_1250P (PAL FHD?), but using this for 24k
                return 8;
            }
            return 7; // hdtv 1080p
        }
    }

    // graphic modes, mostly used for ps2 doing rgb over yuv with sog
    if ((GBS::STATUS_05::read() & 0x0c) == 0x00) // 2: Horizontal unstable AND 3: Vertical unstable are 0?
    {
        if (GBS::STATUS_00::read() == 0x07) {            // the 3 stat0 stable indicators on, none of the SD indicators on
            if ((GBS::STATUS_03::read() & 0x02) == 0x02) // Graphic mode bit on (any of VGA/SVGA/XGA/SXGA at all detected Hz)
            {
                if (rto.inputIsYPbPr)
                    return 13;
                else
                    return 15; // switch to RGBS/HV handling
            } else {
                // this mode looks like it wants to be graphic mode, but the horizontal counter target in MD is very strict
                static uint8_t XGA_60HZ = GBS::MD_XGA_60HZ_CNTRL::read();
                static uint8_t XGA_70HZ = GBS::MD_XGA_70HZ_CNTRL::read();
                static uint8_t XGA_75HZ = GBS::MD_XGA_75HZ_CNTRL::read();
                static uint8_t XGA_85HZ = GBS::MD_XGA_85HZ_CNTRL::read();

                static uint8_t SXGA_60HZ = GBS::MD_SXGA_60HZ_CNTRL::read();
                static uint8_t SXGA_75HZ = GBS::MD_SXGA_75HZ_CNTRL::read();
                static uint8_t SXGA_85HZ = GBS::MD_SXGA_85HZ_CNTRL::read();

                static uint8_t SVGA_60HZ = GBS::MD_SVGA_60HZ_CNTRL::read();
                static uint8_t SVGA_75HZ = GBS::MD_SVGA_75HZ_CNTRL::read();
                static uint8_t SVGA_85HZ = GBS::MD_SVGA_85HZ_CNTRL::read();

                static uint8_t VGA_75HZ = GBS::MD_VGA_75HZ_CNTRL::read();
                static uint8_t VGA_85HZ = GBS::MD_VGA_85HZ_CNTRL::read();

                short hSkew = random(-2, 2); // skew the target a little
                // _WSF("0x%04X\n", XGA_60HZ + hSkew);
                GBS::MD_XGA_60HZ_CNTRL::write(XGA_60HZ + hSkew);
                GBS::MD_XGA_70HZ_CNTRL::write(XGA_70HZ + hSkew);
                GBS::MD_XGA_75HZ_CNTRL::write(XGA_75HZ + hSkew);
                GBS::MD_XGA_85HZ_CNTRL::write(XGA_85HZ + hSkew);
                GBS::MD_SXGA_60HZ_CNTRL::write(SXGA_60HZ + hSkew);
                GBS::MD_SXGA_75HZ_CNTRL::write(SXGA_75HZ + hSkew);
                GBS::MD_SXGA_85HZ_CNTRL::write(SXGA_85HZ + hSkew);
                GBS::MD_SVGA_60HZ_CNTRL::write(SVGA_60HZ + hSkew);
                GBS::MD_SVGA_75HZ_CNTRL::write(SVGA_75HZ + hSkew);
                GBS::MD_SVGA_85HZ_CNTRL::write(SVGA_85HZ + hSkew);
                GBS::MD_VGA_75HZ_CNTRL::write(VGA_75HZ + hSkew);
                GBS::MD_VGA_85HZ_CNTRL::write(VGA_85HZ + hSkew);
            }
        }
    }

    detectedMode = GBS::STATUS_00::read();
    if ((detectedMode & 0x2F) == 0x07) {
        // 0_00 H+V stable, not NTSCI, not PALI
        detectedMode = GBS::STATUS_16::read();
        if ((detectedMode & 0x02) == 0x02) { // SP H active
            uint16_t lineCount = GBS::STATUS_SYNC_PROC_VTOTAL::read();
            for (uint8_t i = 0; i < 2; i++) {
                delay(2);
                if (GBS::STATUS_SYNC_PROC_VTOTAL::read() < (lineCount - 1) ||
                    GBS::STATUS_SYNC_PROC_VTOTAL::read() > (lineCount + 1)) {
                    lineCount = 0;
                    rto.notRecognizedCounter = 0;
                    break;
                }
                detectedMode = GBS::STATUS_00::read();
                if ((detectedMode & 0x2F) != 0x07) {
                    lineCount = 0;
                    rto.notRecognizedCounter = 0;
                    break;
                }
            }
            if (lineCount != 0 && rto.notRecognizedCounter < 255) {
                rto.notRecognizedCounter++;
            }
        } else {
            rto.notRecognizedCounter = 0;
        }
    } else {
        rto.notRecognizedCounter = 0;
    }
    // ???
    if (rto.notRecognizedCounter == 255) {
        return 9;
    }

    return 0; // unknown mode
}

/**
 * @brief Set the And Latch Phase A D C object
 *
 */
void setAndLatchPhaseADC()
{
    GBS::PA_ADC_LAT::write(0);
    GBS::PA_ADC_S::write(rto.phaseADC);
    GBS::PA_ADC_LAT::write(1);
}

/**
 * @brief
 *
 */
void nudgeMD()
{
    GBS::MD_VS_FLIP::write(!GBS::MD_VS_FLIP::read());
    GBS::MD_VS_FLIP::write(!GBS::MD_VS_FLIP::read());
}

/**
 * @brief
 *
 * @param slaveRegister
 * @param values
 * @param numValues
 */
void writeBytes(uint8_t slaveRegister, uint8_t *values, uint8_t numValues)
{
    if (slaveRegister == 0xF0 && numValues == 1) {
        lastSegment = *values;
    } else
        GBS::write(lastSegment, slaveRegister, values, numValues);
}

/**
 * @brief
 *
 * @param slaveRegister
 * @param value
 */
void writeOneByte(uint8_t slaveRegister, uint8_t value)
{
    writeBytes(slaveRegister, &value, 1);
}

/**
 * @brief
 *
 * @param reg
 * @param bytesToRead
 * @param output
 */
void readFromRegister(uint8_t reg, int bytesToRead, uint8_t *output)
{
    return GBS::read(lastSegment, reg, output, bytesToRead);
}

/**
 * @brief
 *
 */
void loadHdBypassSection()
{
    uint16_t index = 0;
    uint8_t bank[16];
    writeOneByte(0xF0, 1);
    for (int j = 3; j <= 5; j++) { // start at 0x30
        copyBank(bank, presetHdBypassSection, &index);
        writeBytes(j * 16, bank, 16);
    }
}

/**
 * @brief Get the Single Byte From Preset object
 *
 * @param programArray
 * @param offset
 * @return uint8_t
 */
uint8_t getSingleByteFromPreset(const uint8_t *programArray, unsigned int offset)
{
    return pgm_read_byte(programArray + offset);
}

/**
 * @brief
 *
 * @param bank
 * @param programArray
 * @param index
 */
void copyBank(uint8_t *bank, const uint8_t *programArray, uint16_t *index)
{
    for (uint8_t x = 0; x < 16; ++x) {
        bank[x] = pgm_read_byte(programArray + *index);
        (*index)++;
    }
}

/**
 * @brief
 *
 * @param seg
 * @param reg
 */
void printReg(uint8_t seg, uint8_t reg)
{
    uint8_t readout;
    readFromRegister(reg, 1, &readout);
    // didn't think this HEX trick would work, but it does! (?)
    _WSF(PSTR("0x%02X, // s%d_0x%04X\n"), readout, seg, reg);
    // old:
    // _WS(readout); _WS(F(", // s")); _WS(seg); _WS(F("_")); _WSF("0x%04X", reg);
}

/**
 * @brief dumps the current chip configuration in a format
 *          that's ready to use as new preset :)
 *
 * @param segment
 */
void dumpRegisters(byte segment)
{
    if (segment > 5)
        return;
    writeOneByte(0xF0, segment);
    int x = 0x40;
    switch (segment) {
        case 0:
            do {
                printReg(0, x);
                x++;
            } while(x <= 0x5F);
            x = 0x90;
            do {
                printReg(0, x);
                x++;
            } while(x <= 0x9F);
            break;
        case 1:
            x = 0x0;
            do {
                printReg(1, x);
                x++;
            } while(x <= 0x2F);
            break;
        case 2:
            x = 0x0;
            do {
                printReg(2, x);
                x++;
            } while(x <= 0x3F);
            break;
        case 3:
            x = 0x0;
            do {
                printReg(3, x);
                x++;
            } while(x <= 0x7F);
            break;
        case 4:
            x = 0x0;
            do {
                printReg(4, x);
                x++;
            } while(x <= 0x5F);
            break;
        case 5:
            x = 0x0;
            do {
                printReg(5, x);
                x++;
            } while(x <= 0x6F);
            break;
    }
}

/**
 * @brief
 *
 */
// void readEeprom()
// {
//     int addr = 0;
//     const uint8_t eepromAddr = 0x50;
//     Wire.beginTransmission(eepromAddr);
//     // if (addr >= 0x1000) { addr = 0; }
//     Wire.write(addr >> 8);     // high addr byte, 4 bits +
//     Wire.write((uint8_t)addr); // low addr byte, 8 bits = 12 bits (0xfff max)
//     Wire.endTransmission();
//     Wire.requestFrom(eepromAddr, (uint8_t)128);
//     uint8_t readData = 0;
//     uint8_t i = 0;
//     while (Wire.available()) {
//         readData = Wire.read();
//         _WSF(PSTR("addr 0x%04X : 0x%04X\n"), i, readData);
//         // addr++;
//         i++;
//     }
// }

/**
 * @brief
 *
 */
void stopWire()
{
    pinMode(SCL, INPUT);
    pinMode(SDA, INPUT);
    delayMicroseconds(80);
}

/**
 * @brief
 *
 */
void startWire()
{
    Wire.begin();
    // The i2c wire library sets pullup resistors on by default.
    // Disable these to detect/work with GBS onboard pullups
    pinMode(SCL, OUTPUT_OPEN_DRAIN);
    pinMode(SDA, OUTPUT_OPEN_DRAIN);
    // no issues even at 700k, requires ESP8266 160Mhz CPU clock, else (80Mhz) uses 400k in library
    // no problem with Si5351 at 700k either
    Wire.setClock(400000);
    // Wire.setClock(700000);
}

/**
 * @brief Detect if external clock installed
 *
 * @return int8_t
 *          1 = device ready, init completed,
 *          0 - no device detected
 *         -1 - any other error
 */
int8_t utilsExternClockGenInit()
{
    const uint8_t xtal_cl = 0xD2; // 10pF, other choices are 8pF (0x92) and 6pF (0x52) NOTE: Per AN619, the low bytes should be written 0b010010

    // MHz: 27, 32.4, 40.5, 54, 64.8, 81, 108, 129.6, 162
    rto.freqExtClockGen = 81000000;
    rto.extClockGenDetected = 0;

    if (uopt.disableExternalClockGenerator) {
        _DBGN(F("(!) external clock generator disabled, skipping detection"));
        return 0;
    }

    uint8_t retVal = 0;
    Wire.beginTransmission(SIADDR);
    // returns:
    // 4 = line busy
    // 3 = received NACK on transmit of data
    // 2 = received NACK on transmit of address
    // 0 = success
    retVal = Wire.endTransmission();
    _DBGF(PSTR("(i) external clock, status: %d\n"), retVal);
    if (retVal != 0) {
        return -1;
    }

    Wire.beginTransmission(SIADDR);
    Wire.write(0); // Device Status
    Wire.endTransmission();
    size_t bytes_read = Wire.requestFrom((uint8_t)SIADDR, (size_t)1, false);

    if (bytes_read == 1) {
        retVal = Wire.read();
        if ((retVal & 0x80) == 0) {
            // SYS_INIT indicates device is ready.
            rto.extClockGenDetected = 1;
        } else {
            return 0;
        }
    } else {
        return -1;
    }

    _DBGN(F("external clock detected"));

    Si.init(25000000L); // many Si5351 boards come with 25MHz crystal; 27000000L for one with 27MHz
    Wire.beginTransmission(SIADDR);
    Wire.write(183); // XTAL_CL
    Wire.write(xtal_cl);
    Wire.endTransmission();
    Si.setPower(0, SIOUT_6mA);
    Si.setFreq(0, rto.freqExtClockGen);
    Si.disable(0);
    return 1;
}

/**
 * @brief Check if TV chip is acting as expeced when all systems are powered-on
 *        (this function sets rto.boardHasPower status variable)
 *
 * @return true
 * @return false
 */
bool utilsCheckBoardPower()
{
    // check DAC (disabled by default)
    if(!rto.isInLowPowerMode && GBS::DAC_RGBS_PWDNZ::read() == 0) {
        _DBGN(F("(i) DAC is in power down mode"));
        // goto board_power_bad;
    }

    // check ADC
    if(GBS::ADC_POWDZ::read() == 0) {
        _DBGN(F("(!) ADC is in power down mode"));
        goto board_power_bad;
    }

    // check PLL power
    // if(GBS::PLLAD_PDZ::read() == 0) {
    //     _DBGN(F("(i) PLL is in power down mode"));
    //     // goto board_power_bad;
    // }

    rto.boardHasPower = true;
    goto board_power_end;

board_power_bad:
    rto.boardHasPower = false;

board_power_end:
    return rto.boardHasPower;

    // GBS::ADC_UNUSED_69::write(0x6a); // 0110 1010
    // if (GBS::ADC_UNUSED_69::read() == 0x6a) {
    //     GBS::ADC_UNUSED_69::write(0);
    //     rto.boardHasPower = true;
    //     return true;
    // }

    // GBS::ADC_UNUSED_69::write(0); // attempt to clear
    // if (rto.boardHasPower == true) {
    //     _WSN(F("! power / i2c lost !"));
    // }
    // rto.boardHasPower = false;

    // return false;
}

/**
 * @brief Ref.13.2.2 Software reset: "In 5725, there are also some software reset register bits,
 *        these software reset only could reset state machine and internal
 *        timing, but could not reset register bits’ value. And host interface’s
 *        state machine is only reset by external reset..."
 *
 */
void utilsResetAllOffline() {
    GBS::PLL_VCORST::write(1);

    GBS::ADC_POWDZ::write(0);
    GBS::PLLAD_PDZ::write(0);
    GBS::PLLAD_LEN::write(0);
    GBS::DAC_RGBS_PWDNZ::write(0);
    GBS::OUT_SYNC_CNTRL::write(0);
    GBS::SFTRST_VDS_RSTZ::write(0);
    GBS::SFTRST_SYNC_RSTZ::write(0);
    GBS::SFTRST_DEC_RSTZ::write(0);
    GBS::SFTRST_IF_RSTZ::write(0);
    GBS::SFTRST_DEINT_RSTZ::write(0);
    GBS::SFTRST_MEM_FF_RSTZ::write(0);
    GBS::SFTRST_MEM_RSTZ::write(0);
    GBS::SFTRST_FIFO_RSTZ::write(0);
    GBS::SFTRST_OSD_RSTZ::write(0);
    GBS::SFTRST_MODE_RSTZ::write(0);
    GBS::SFTRST_HDBYPS_RSTZ::write(0);
    GBS::SFTRST_INT_RSTZ::write(0);

    GBS::PAD_CONTROL_00_0x48::write(0);
    GBS::PAD_CONTROL_01_0x49::write(0);
}

/**
 * @brief All systems online
 *
 */
void utilsResetOnline() {
    GBS::ADC_POWDZ::write(1);
    resetDigital();
    resetPLLAD();
    GBS::PLLAD_LEN::write(1);
    GBS::DAC_RGBS_PWDNZ::write(1);
    GBS::OUT_SYNC_CNTRL::write(1);
    GBS::SFTRST_SYNC_RSTZ::write(1);
    GBS::SFTRST_DEC_RSTZ::write(1);
    GBS::SFTRST_IF_RSTZ::write(1);
    GBS::SFTRST_DEINT_RSTZ::write(1);
    GBS::SFTRST_MEM_FF_RSTZ::write(1);
    GBS::SFTRST_MEM_RSTZ::write(1);
    GBS::SFTRST_FIFO_RSTZ::write(1);
    GBS::SFTRST_OSD_RSTZ::write(1);
    GBS::SFTRST_VDS_RSTZ::write(1);
    GBS::SFTRST_MODE_RSTZ::write(1);
    GBS::SFTRST_HDBYPS_RSTZ::write(1);
    GBS::SFTRST_INT_RSTZ::write(1);

    // GBS::PAD_CONTROL_00_0x48::write(0x2b);
    GBS::PAD_CKIN_ENZ::write(1);
    GBS::PAD_CKOUT_ENZ::write(1);
    GBS::PAD_SYNC_OUT_ENZ::write(1);
    GBS::PAD_BLK_OUT_ENZ::write(1);
    GBS::PAD_TRI_ENZ::write(1);
}

/**
 * @brief Clear tv5725 registers
 *      "All registers (except chapter 01 status register is read only)
 *       have default value “0x00” after power up."
 *      "All registers require segment for access.
 *       Segment is defined in address F0."
 *
 */
void utilsZeroAll()
{
    // this goes into utilsResetAllOffline()
    // turn processing units off first
    // writeOneByte(0xF0, 0);
    // writeOneByte(0x46, 0x00); // reset controls 1
    // writeOneByte(0x47, 0x00); // reset controls 2

    // zero out entire register space = (6 segments * 255 bytes)
    uint8_t y = 0;
    uint8_t z = 0;

    while (y < 6) {
        writeOneByte(0xF0, y);
        // skip RO registers
        if(y == 0) z = 3;
        while (z < 16) {
            // while(w < 16) {
            //     bank[w] = 0;
            //     // exceptions
            //     // if (y == 5 && z == 0 && w == 2) {
            //     //  bank[w] = 0x51; // 5_02
            //     //}
            //     // if (y == 5 && z == 5 && w == 6) {
            //     //  bank[w] = 0x01; // 5_56
            //     //}
            //     // if (y == 5 && z == 5 && w == 7) {
            //     //  bank[w] = 0xC0; // 5_57
            //     //}
            //     w++;
            // }
            uint8_t bank[16];
            memset(bank, 0, 16);
            writeBytes(z * 16, bank, 16);
            z++;
        }
        z = 0;
        y++;
    }
    _DBGN(F("(!) all registers reset"));
}

/**
 * @brief
 *
 */
void calibrateAdcOffset()
{
    GBS::PAD_BOUT_EN::write(0);          // disable output to pin for test
    GBS::PLL648_CONTROL_01::write(0xA5); // display clock to adc = 162mhz
    GBS::ADC_INPUT_SEL::write(2);        // 10 > R2/G2/B2 as input (not connected, so to isolate ADC)
    GBS::DEC_MATRIX_BYPS::write(1);
    GBS::DEC_TEST_ENABLE::write(1);
    GBS::ADC_5_03::write(0x31);    // bottom clamps, filter max (40mhz)
    GBS::ADC_TEST_04::write(0x00); // disable bit 1
    GBS::SP_CS_CLP_ST::write(0x00);
    GBS::SP_CS_CLP_SP::write(0x00);
    GBS::SP_5_56::write(0x05); // SP_SOG_MODE needs to be 1
    GBS::SP_5_57::write(0x80);
    GBS::ADC_5_00::write(0x02);
    GBS::TEST_BUS_SEL::write(0x0b); // 0x2b
    GBS::TEST_BUS_EN::write(1);
    resetDigital();

    uint16_t hitTargetCounter = 0;
    uint16_t readout16 = 0;
    uint8_t missTargetCounter = 0;
    uint8_t readout = 0;

    GBS::ADC_RGCTRL::write(0x7F);
    GBS::ADC_GGCTRL::write(0x7F);
    GBS::ADC_BGCTRL::write(0x7F);
    GBS::ADC_ROFCTRL::write(0x7F);
    GBS::ADC_GOFCTRL::write(0x3D); // start
    GBS::ADC_BOFCTRL::write(0x7F);
    GBS::DEC_TEST_SEL::write(1); // 5_1f = 0x1c

    // unsigned long overallTimer = millis();
    unsigned long startTimer = 0;
    for (uint8_t i = 0; i < 3; i++) {
        missTargetCounter = 0;
        hitTargetCounter = 0;
        delay(20);
        startTimer = millis();

        // loop breaks either when the timer runs out, or hitTargetCounter reaches target
        while ((millis() - startTimer) < 800) {
            readout16 = GBS::TEST_BUS::read() & 0x7fff;
            // _WSN(readout16, HEX);
            //  readout16 is unsigned, always >= 0
            if (readout16 < 7) {
                hitTargetCounter++;
                missTargetCounter = 0;
            } else if (missTargetCounter++ > 2) {
                if (i == 0) {
                    GBS::ADC_GOFCTRL::write(GBS::ADC_GOFCTRL::read() + 1); // incr. offset
                    readout = GBS::ADC_GOFCTRL::read();
                    _WS(F(" G: "));
                } else if (i == 1) {
                    GBS::ADC_ROFCTRL::write(GBS::ADC_ROFCTRL::read() + 1);
                    readout = GBS::ADC_ROFCTRL::read();
                    _WS(F(" R: "));
                } else if (i == 2) {
                    GBS::ADC_BOFCTRL::write(GBS::ADC_BOFCTRL::read() + 1);
                    readout = GBS::ADC_BOFCTRL::read();
                    _WS(F(" B: "));
                }
                _WSF(PSTR("0x%04X"), readout);

                if (readout >= 0x52) {
                    // some kind of failure
                    break;
                }

                delay(10);
                hitTargetCounter = 0;
                missTargetCounter = 0;
                startTimer = millis(); // extend timer
            }
            if (hitTargetCounter > 1500) {
                break;
            }
        }
        if (i == 0) {
            // G done, prep R
            adco.g_off = GBS::ADC_GOFCTRL::read();
            GBS::ADC_GOFCTRL::write(0x7F);
            GBS::ADC_ROFCTRL::write(0x3D);
            GBS::DEC_TEST_SEL::write(2); // 5_1f = 0x2c
        }
        if (i == 1) {
            adco.r_off = GBS::ADC_ROFCTRL::read();
            GBS::ADC_ROFCTRL::write(0x7F);
            GBS::ADC_BOFCTRL::write(0x3D);
            GBS::DEC_TEST_SEL::write(3); // 5_1f = 0x3c
        }
        if (i == 2) {
            adco.b_off = GBS::ADC_BOFCTRL::read();
        }
        _WSN();
    }

    if (readout >= 0x52) {
        // there was a problem; revert
        adco.r_off = adco.g_off = adco.b_off = 0x40;
    }

    GBS::ADC_GOFCTRL::write(adco.g_off);
    GBS::ADC_ROFCTRL::write(adco.r_off);
    GBS::ADC_BOFCTRL::write(adco.b_off);

    // _WSN(millis() - overallTimer);
}

/**
 * @brief Set the External Clock Gen Frequency Smooth object
 *
 * @param freq
 */
void setExternalClockGenFrequencySmooth(uint32_t freq) {
    uint32_t current = rto.freqExtClockGen;

    rto.freqExtClockGen = freq;

    constexpr uint32_t STEP_SIZE_HZ = 1000;

    if (current > rto.freqExtClockGen) {
        if ((current - rto.freqExtClockGen) < 750000) {
            while (current > (rto.freqExtClockGen + STEP_SIZE_HZ)) {
                current -= STEP_SIZE_HZ;
                Si.setFreq(0, current);
                // wifiLoop(0);
            }
        }
    } else if (current < rto.freqExtClockGen) {
        if ((rto.freqExtClockGen - current) < 750000) {
            while ((current + STEP_SIZE_HZ) < rto.freqExtClockGen) {
                current += STEP_SIZE_HZ;
                Si.setFreq(0, current);
                // wifiLoop(0);
            }
        }
    }

    Si.setFreq(0, rto.freqExtClockGen);
}