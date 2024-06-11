/*
#####################################################################################
# File: video.cpp                                                                   #
# File Created: Thursday, 2nd May 2024 4:07:57 pm                                   #
# Author:                                                                           #
# Last Modified: Monday, 10th June 2024 4:55:33 pm                        #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/
#include "video.h"

/**
 * @brief
 *
 */
void resetInterruptSogSwitchBit()
{
    GBS::INT_CONTROL_RST_SOGSWITCH::write(1);
    GBS::INT_CONTROL_RST_SOGSWITCH::write(0);
}

/**
 * @brief
 *
 */
void resetInterruptSogBadBit()
{
    GBS::INT_CONTROL_RST_SOGBAD::write(1);
    GBS::INT_CONTROL_RST_SOGBAD::write(0);
}

/**
 * @brief
 *
 */
void resetInterruptNoHsyncBadBit()
{
    GBS::INT_CONTROL_RST_NOHSYNC::write(1);
    GBS::INT_CONTROL_RST_NOHSYNC::write(0);
}

/**
 * @brief
 *
 * @return true
 * @return false
 */
bool videoStandardInputIsPalNtscSd()
{
    if (rto->videoStandardInput == 1 || rto->videoStandardInput == 2) {
        return true;
    }
    return false;
}

/**
 * @brief
 *
 */
void prepareSyncProcessor()
{
    writeOneByte(0xF0, 5);
    GBS::SP_SOG_P_ATO::write(0); // 5_20 disable sog auto polarity // hpw can be > ht, but auto is worse
    GBS::SP_JITTER_SYNC::write(0);
    // H active detect control
    writeOneByte(0x21, 0x18); // SP_SYNC_TGL_THD    H Sync toggle time threshold  0x20; lower than 5_33(not always); 0 to turn off (?) 0x18 for 53.69 system @ 33.33
    writeOneByte(0x22, 0x0F); // SP_L_DLT_REG       Sync pulse width difference threshold (less than this = equal)
    writeOneByte(0x23, 0x00); // UNDOCUMENTED       range from 0x00 to at least 0x1d
    writeOneByte(0x24, 0x40); // SP_T_DLT_REG       H total width difference threshold rgbhv: b // range from 0x02 upwards
    writeOneByte(0x25, 0x00); // SP_T_DLT_REG
    writeOneByte(0x26, 0x04); // SP_SYNC_PD_THD     H sync pulse width threshold // from 0(?) to 0x50 // in yuv 720p range only to 0x0a!
    writeOneByte(0x27, 0x00); // SP_SYNC_PD_THD
    writeOneByte(0x2a, 0x0F); // SP_PRD_EQ_THD      How many legal lines as valid; scales with 5_33 (needs to be below)
    // V active detect control
    // these 4 only have effect with HV input; test:  s5s2ds34 s5s2es24 s5s2fs16 s5s31s84
    writeOneByte(0x2d, 0x03); // SP_VSYNC_TGL_THD   V sync toggle time threshold // at 5 starts to drop many 0_16 vsync events
    writeOneByte(0x2e, 0x00); // SP_SYNC_WIDTH_DTHD V sync pulse width threshold
    writeOneByte(0x2f, 0x02); // SP_V_PRD_EQ_THD    How many continue legal v sync as valid // at 4 starts to drop 0_16 vsync events
    writeOneByte(0x31, 0x2f); // SP_VT_DLT_REG      V total difference threshold
    // Timer value control
    writeOneByte(0x33, 0x3a); // SP_H_TIMER_VAL     H timer value for h detect (was 0x28)
    writeOneByte(0x34, 0x06); // SP_V_TIMER_VAL     V timer for V detect // 0_16 vsactive // was 0x05
    // Sync separation control
    if (rto->videoStandardInput == 0)
        GBS::SP_DLT_REG::write(0x70); // 5_35  130 too much for ps2 1080i, 0xb0 for 1080p
    else if (rto->videoStandardInput <= 4)
        GBS::SP_DLT_REG::write(0xC0); // old: extended to 0x150 later if mode = 1 or 2
    else if (rto->videoStandardInput <= 6)
        GBS::SP_DLT_REG::write(0xA0);
    else if (rto->videoStandardInput == 7)
        GBS::SP_DLT_REG::write(0x70);
    else
        GBS::SP_DLT_REG::write(0x70);

    if (videoStandardInputIsPalNtscSd()) {
        GBS::SP_H_PULSE_IGNOR::write(0x6b);
    } else {
        GBS::SP_H_PULSE_IGNOR::write(0x02); // test with MS / Genesis mode (wsog 2) vs ps2 1080p (0x13 vs 0x05)
    }
    // leave out pre / post coast here
    // 5_3a  attempted 2 for 1chip snes 239 mode intermittency, works fine except for MD in MS mode
    // make sure this is stored in the presets as well, as it affects sync time
    GBS::SP_H_TOTAL_EQ_THD::write(3);

    GBS::SP_SDCS_VSST_REG_H::write(0);
    GBS::SP_SDCS_VSSP_REG_H::write(0);
    GBS::SP_SDCS_VSST_REG_L::write(4); // 5_3f // was 12
    GBS::SP_SDCS_VSSP_REG_L::write(1); // 5_40 // was 11

    GBS::SP_CS_HS_ST::write(0x10); // 5_45
    GBS::SP_CS_HS_SP::write(0x00); // 5_47 720p source needs ~20 range, may be necessary to adjust at runtime, based on source res

    writeOneByte(0x49, 0x00); // retime HS start for RGB+HV rgbhv: 20
    writeOneByte(0x4a, 0x00); //
    writeOneByte(0x4b, 0x44); // retime HS stop rgbhv: 50
    writeOneByte(0x4c, 0x00); //

    writeOneByte(0x51, 0x02); // 0x00 rgbhv: 2
    writeOneByte(0x52, 0x00); // 0xc0
    writeOneByte(0x53, 0x00); // 0x05 rgbhv: 6
    writeOneByte(0x54, 0x00); // 0xc0

    if (rto->videoStandardInput != 15 && (GBS::GBS_OPTION_SCALING_RGBHV::read() != 1)) {
        GBS::SP_CLAMP_MANUAL::write(0); // 0 = automatic on/off possible
        GBS::SP_CLP_SRC_SEL::write(0);  // clamp source 1: pixel clock, 0: 27mhz // was 1 but the pixel clock isn't available at first
        GBS::SP_NO_CLAMP_REG::write(1); // 5_57_0 unlock clamp
        GBS::SP_SOG_MODE::write(1);
        GBS::SP_H_CST_ST::write(0x10);   // 5_4d
        GBS::SP_H_CST_SP::write(0x100);  // 5_4f
        GBS::SP_DIS_SUB_COAST::write(0); // 5_3e 5
        GBS::SP_H_PROTECT::write(1);     // SP_H_PROTECT on for detection
        GBS::SP_HCST_AUTO_EN::write(0);
        GBS::SP_NO_COAST_REG::write(0);
    }

    GBS::SP_HS_REG::write(1);          // 5_57 7
    GBS::SP_HS_PROC_INV_REG::write(0); // no SP sync inversion
    GBS::SP_VS_PROC_INV_REG::write(0); //

    writeOneByte(0x58, 0x05); // rgbhv: 0
    writeOneByte(0x59, 0x00); // rgbhv: 0
    writeOneByte(0x5a, 0x01); // rgbhv: 0 // was 0x05 but 480p ps2 doesnt like it
    writeOneByte(0x5b, 0x00); // rgbhv: 0
    writeOneByte(0x5c, 0x03); // rgbhv: 0
    writeOneByte(0x5d, 0x02); // rgbhv: 0 // range: 0 - 0x20 (how long should clamp stay off)
}

/**
 * @brief Get the Status16 Sp Hs Stable object
 *      used to be a check for the length of the debug bus readout of 5_63 = 0x0f
 *      now just checks the chip status at 0_16 HS active (and Interrupt bit4 HS active for RGBHV)
 * @return true
 * @return false
 */
bool getStatus16SpHsStable()
{
    if (rto->videoStandardInput == 15) { // check RGBHV first
        if (GBS::STATUS_INT_INP_NO_SYNC::read() == 0) {
            return true;
        } else {
            resetInterruptNoHsyncBadBit();
            return false;
        }
    }

    // STAT_16 bit 1 is the "hsync active" flag, which appears to be a reliable indicator
    // checking the flag replaces checking the debug bus pulse length manually
    uint8_t status16 = GBS::STATUS_16::read();
    if ((status16 & 0x02) == 0x02) {
        if (rto->videoStandardInput == 1 || rto->videoStandardInput == 2) {
            if ((status16 & 0x01) != 0x01) { // pal / ntsc should be sync active low
                return true;
            }
        } else {
            return true; // not pal / ntsc
        }
    }

    return false;
}

/**
 * @brief Get the Source Field Rate object
 *
 * @param useSPBus
 * @return float
 */
float getSourceFieldRate(bool useSPBus)
{
    double esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
    uint8_t testBusSelBackup = GBS::TEST_BUS_SEL::read();
    uint8_t spBusSelBackup = GBS::TEST_BUS_SP_SEL::read();
    uint8_t ifBusSelBackup = GBS::IF_TEST_SEL::read();
    uint8_t debugPinBackup = GBS::PAD_BOUT_EN::read();

    if (debugPinBackup != 1)
        GBS::PAD_BOUT_EN::write(1); // enable output to pin for test

    if (ifBusSelBackup != 3)
        GBS::IF_TEST_SEL::write(3); // IF averaged frame time

    if (useSPBus) {
        if (rto->syncTypeCsync) {
            // _DBGN("TestBus for csync");
            if (testBusSelBackup != 0xa)
                GBS::TEST_BUS_SEL::write(0xa);
        } else {
            // _DBGN("TestBus for HV Sync");
            if (testBusSelBackup != 0x0)
                GBS::TEST_BUS_SEL::write(0x0); // RGBHV: TB 0x20 has vsync
        }
        if (spBusSelBackup != 0x0f)
            GBS::TEST_BUS_SP_SEL::write(0x0f);
    } else {
        if (testBusSelBackup != 0)
            GBS::TEST_BUS_SEL::write(0); // needs decimation + if
    }

    float retVal = 0;

    uint32_t fieldTimeTicks = FrameSync::getPulseTicks();
    if (fieldTimeTicks == 0) {
        // try again
        fieldTimeTicks = FrameSync::getPulseTicks();
    }

    if (fieldTimeTicks > 0) {
        retVal = esp8266_clock_freq / (double)fieldTimeTicks;
        if (retVal < 47.0f || retVal > 86.0f) {
            // try again
            fieldTimeTicks = FrameSync::getPulseTicks();
            if (fieldTimeTicks > 0) {
                retVal = esp8266_clock_freq / (double)fieldTimeTicks;
            }
        }
    }

    GBS::TEST_BUS_SEL::write(testBusSelBackup);
    GBS::PAD_BOUT_EN::write(debugPinBackup);
    if (spBusSelBackup != 0x0f)
        GBS::TEST_BUS_SP_SEL::write(spBusSelBackup);
    if (ifBusSelBackup != 3)
        GBS::IF_TEST_SEL::write(ifBusSelBackup);

    return retVal;
}

/**
 * @brief Get the Output Frame Rate object
 *
 * @return float
 */
float getOutputFrameRate()
{
    double esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
    uint8_t testBusSelBackup = GBS::TEST_BUS_SEL::read();
    uint8_t debugPinBackup = GBS::PAD_BOUT_EN::read();

    if (debugPinBackup != 1)
        GBS::PAD_BOUT_EN::write(1); // enable output to pin for test

    if (testBusSelBackup != 2)
        GBS::TEST_BUS_SEL::write(2); // 0x4d = 0x22 VDS test

    float retVal = 0;

    uint32_t fieldTimeTicks = FrameSync::getPulseTicks();
    if (fieldTimeTicks == 0) {
        // try again
        fieldTimeTicks = FrameSync::getPulseTicks();
    }

    if (fieldTimeTicks > 0) {
        retVal = esp8266_clock_freq / (double)fieldTimeTicks;
        if (retVal < 47.0f || retVal > 86.0f) {
            // try again
            fieldTimeTicks = FrameSync::getPulseTicks();
            if (fieldTimeTicks > 0) {
                retVal = esp8266_clock_freq / (double)fieldTimeTicks;
            }
        }
    }

    GBS::TEST_BUS_SEL::write(testBusSelBackup);
    GBS::PAD_BOUT_EN::write(debugPinBackup);

    return retVal;
}

/**
 * @brief
 *
 */
void externalClockGenSyncInOutRate()
{
    _DBGN("externalClockGenSyncInOutRate()");

    if (!rto->extClockGenDetected) {
        return;
    }
    if (GBS::PAD_CKIN_ENZ::read() != 0) {
        return;
    }
    if (rto->outModeHdBypass) {
        return;
    }
    if (GBS::PLL648_CONTROL_01::read() != 0x75) {
        return;
    }

    float sfr = getSourceFieldRate(0);
    if (sfr < 47.0f || sfr > 86.0f) {
        _WSF(PSTR("sync skipped sfr wrong: %.02f\n"), sfr);
        return;
    }

    float ofr = getOutputFrameRate();
    if (ofr < 47.0f || ofr > 86.0f) {
        _WSF(PSTR("sync skipped ofr wrong: %.02f\n"), ofr);
        return;
    }

    uint32_t old = rto->freqExtClockGen;
    FrameSync::initFrequency(ofr, old);

    setExternalClockGenFrequencySmooth((sfr / ofr) * rto->freqExtClockGen);

    int32_t diff = rto->freqExtClockGen - old;

    _WSF(PSTR("source Hz: %.5f new out: %.5f clock: %u (%s%d)\n"),
        sfr,
        getOutputFrameRate(),
        rto->freqExtClockGen,
        (diff >= 0 ? "+" : ""),
        diff);
}

/**
 * @brief
 *
 * @return true if init successful
 * @return false something wrong / try later
 */

/**
 * @brief Detect if external clock installed
 *
 * @return int8_t
 *          1 = device ready, init completed,
 *          0 - no device detected
 *         -1 - any other error
 */
int8_t externalClockGenDetectAndInitialize()
{
    const uint8_t xtal_cl = 0xD2; // 10pF, other choices are 8pF (0x92) and 6pF (0x52) NOTE: Per AN619, the low bytes should be written 0b010010

    // MHz: 27, 32.4, 40.5, 54, 64.8, 81, 108, 129.6, 162
    rto->freqExtClockGen = 81000000;
    rto->extClockGenDetected = 0;

    if (uopt->disableExternalClockGenerator) {
        _WSN(F("external clock generator disabled, skipping detection"));
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
    _DBGF(PSTR("a problem while detect external clock, err: %d\n"), retVal);
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
            rto->extClockGenDetected = 1;
        } else {
            return 0;
        }
    } else {
        return -1;
    }

    Si.init(25000000L); // many Si5351 boards come with 25MHz crystal; 27000000L for one with 27MHz
    Wire.beginTransmission(SIADDR);
    Wire.write(183); // XTAL_CL
    Wire.write(xtal_cl);
    Wire.endTransmission();
    Si.setPower(0, SIOUT_6mA);
    Si.setFreq(0, rto->freqExtClockGen);
    Si.disable(0);
    return 1;
}

/**
 * @brief
 *
 */
void externalClockGenResetClock()
{
    if (!rto->extClockGenDetected) {
        return;
    }
    _DBGN("externalClockGenResetClock()");

    uint8_t activeDisplayClock = GBS::PLL648_CONTROL_01::read();

    if (activeDisplayClock == 0x25)
        rto->freqExtClockGen = 40500000;
    else if (activeDisplayClock == 0x45)
        rto->freqExtClockGen = 54000000;
    else if (activeDisplayClock == 0x55)
        rto->freqExtClockGen = 64800000;
    else if (activeDisplayClock == 0x65)
        rto->freqExtClockGen = 81000000;
    else if (activeDisplayClock == 0x85)
        rto->freqExtClockGen = 108000000;
    else if (activeDisplayClock == 0x95)
        rto->freqExtClockGen = 129600000;
    else if (activeDisplayClock == 0xa5)
        rto->freqExtClockGen = 162000000;
    else if (activeDisplayClock == 0x35)
        rto->freqExtClockGen = 81000000; // clock unused
    else if (activeDisplayClock == 0)
        rto->freqExtClockGen = 81000000; // no preset loaded
    else if (!rto->outModeHdBypass) {
        _DBGF(PSTR("preset display clock: 0x%02X\n"), activeDisplayClock);
    }

    // problem: around 108MHz the library seems to double the clock
    // maybe there are regs to check for this and resetPLL
    if (rto->freqExtClockGen == 108000000) {
        Si.setFreq(0, 87000000);
        delay(1); // quick fix
    }
    // same thing it seems at 40500000
    if (rto->freqExtClockGen == 40500000) {
        Si.setFreq(0, 48500000);
        delay(1); // quick fix
    }

    Si.setFreq(0, rto->freqExtClockGen);
    GBS::PAD_CKIN_ENZ::write(0); // 0 = clock input enable (pin40)
    FrameSync::clearFrequency();

    _DBGF(PSTR("reset ext. clock gen. to: %ld\n"), rto->freqExtClockGen);
}

/**
 * @brief Set the External Clock Gen Frequency Smooth object
 *
 * @param freq
 */
void setExternalClockGenFrequencySmooth(uint32_t freq) {
    uint32_t current = rto->freqExtClockGen;

    rto->freqExtClockGen = freq;

    constexpr uint32_t STEP_SIZE_HZ = 1000;

    if (current > rto->freqExtClockGen) {
        if ((current - rto->freqExtClockGen) < 750000) {
            while (current > (rto->freqExtClockGen + STEP_SIZE_HZ)) {
                current -= STEP_SIZE_HZ;
                Si.setFreq(0, current);
                // wifiLoop(0);
            }
        }
    } else if (current < rto->freqExtClockGen) {
        if ((rto->freqExtClockGen - current) < 750000) {
            while ((current + STEP_SIZE_HZ) < rto->freqExtClockGen) {
                current += STEP_SIZE_HZ;
                Si.setFreq(0, current);
                // wifiLoop(0);
            }
        }
    }

    Si.setFreq(0, rto->freqExtClockGen);
}

/**
 * @brief
 *
 * @param bestHTotal
 * @return true
 * @return false
 */
bool applyBestHTotal(uint16_t bestHTotal)
{
    if (rto->outModeHdBypass) {
        return true; // false? doesn't matter atm
    }

    uint16_t orig_htotal = GBS::VDS_HSYNC_RST::read();
    int diffHTotal = bestHTotal - orig_htotal;
    uint16_t diffHTotalUnsigned = abs(diffHTotal);

    if (((diffHTotalUnsigned == 0) || (rto->extClockGenDetected && diffHTotalUnsigned == 1)) && // all this
        !rto->forceRetime)                                                                      // and that
    {
        if (!uopt->enableFrameTimeLock) { // FTL can double throw this when it resets to adjust
            _WS(F("HTotal Adjust (skipped)"));

            if (!rto->extClockGenDetected) {
                float sfr = getSourceFieldRate(0);
                yield(); // wifi
                float ofr = getOutputFrameRate();
                if (sfr < 1.0f) {
                    sfr = getSourceFieldRate(0); // retry
                }
                if (ofr < 1.0f) {
                    ofr = getOutputFrameRate(); // retry
                }
                _WSF(PSTR(", source Hz: %.3f, output Hz: %.3f\n"), sfr, ofr);
            } else {
                _WSN();
            }
        }
        return true; // nothing to do
    }

    if (GBS::GBS_OPTION_PALFORCED60_ENABLED::read() == 1) {
        // source is 50Hz, preset has to stay at 60Hz: return
        return true;
    }

    bool isLargeDiff = (diffHTotalUnsigned > (orig_htotal * 0.06f)) ? true : false; // typical diff: 1802 to 1794 (=8)

    if (isLargeDiff && (getVideoMode() == 8 || rto->videoStandardInput == 14)) {
        // arcade stuff syncs down from 60 to 52 Hz..
        isLargeDiff = (diffHTotalUnsigned > (orig_htotal * 0.16f)) ? true : false;
    }

    if (isLargeDiff) {
        _WSN(F("ABHT: large diff"));
    }

    // rto->forceRetime = true means the correction should be forced (command '.')
    if (isLargeDiff && (rto->forceRetime == false)) {
        if (rto->videoStandardInput != 14) {
            rto->failRetryAttempts++;
            if (rto->failRetryAttempts < 8) {
                _WSN(F("retry"));
                FrameSync::reset(uopt->frameTimeLockMethod);
                delay(60);
            } else {
                _WSN(F("give up"));
                rto->autoBestHtotalEnabled = false;
            }
        }
        return false; // large diff, no forced
    }

    // bestHTotal 0? could be an invald manual retime
    if (bestHTotal == 0) {
        _WSN(F("bestHTotal 0"));
        return false;
    }

    if (rto->forceRetime == false) {
        if (GBS::STATUS_INT_SOG_BAD::read() == 1) {
            // _WSN("prevented in apply");
            return false;
        }
    }

    rto->failRetryAttempts = 0; // else all okay!, reset to 0

    // move blanking (display)
    uint16_t h_blank_display_start_position = GBS::VDS_DIS_HB_ST::read();
    uint16_t h_blank_display_stop_position = GBS::VDS_DIS_HB_SP::read();
    uint16_t h_blank_memory_start_position = GBS::VDS_HB_ST::read();
    uint16_t h_blank_memory_stop_position = GBS::VDS_HB_SP::read();

    // new 25.10.2019
    // uint16_t blankingAreaTotal = bestHTotal * 0.233f;
    // h_blank_display_start_position += (diffHTotal / 2);
    // h_blank_display_stop_position += (diffHTotal / 2);
    // h_blank_memory_start_position = bestHTotal - (blankingAreaTotal * 0.212f);
    // h_blank_memory_stop_position = blankingAreaTotal * 0.788f;

    // h_blank_memory_start_position usually is == h_blank_display_start_position
    if (h_blank_memory_start_position == h_blank_display_start_position) {
        h_blank_display_start_position += (diffHTotal / 2);
        h_blank_display_stop_position += (diffHTotal / 2);
        h_blank_memory_start_position = h_blank_display_start_position; // normal case
        h_blank_memory_stop_position += (diffHTotal / 2);
    } else {
        h_blank_display_start_position += (diffHTotal / 2);
        h_blank_display_stop_position += (diffHTotal / 2);
        h_blank_memory_start_position += (diffHTotal / 2); // the exception (currently 1280x1024)
        h_blank_memory_stop_position += (diffHTotal / 2);
    }

    if (diffHTotal < 0) {
        h_blank_display_start_position &= 0xfffe;
        h_blank_display_stop_position &= 0xfffe;
        h_blank_memory_start_position &= 0xfffe;
        h_blank_memory_stop_position &= 0xfffe;
    } else if (diffHTotal > 0) {
        h_blank_display_start_position += 1;
        h_blank_display_start_position &= 0xfffe;
        h_blank_display_stop_position += 1;
        h_blank_display_stop_position &= 0xfffe;
        h_blank_memory_start_position += 1;
        h_blank_memory_start_position &= 0xfffe;
        h_blank_memory_stop_position += 1;
        h_blank_memory_stop_position &= 0xfffe;
    }

    // don't move HSync with small diffs
    uint16_t h_sync_start_position = GBS::VDS_HS_ST::read();
    uint16_t h_sync_stop_position = GBS::VDS_HS_SP::read();

    // fix over / underflows
    if (h_blank_display_start_position > (bestHTotal - 8) || isLargeDiff) {
        // typically happens when scaling Hz up (60 to 70)
        // _WSN("overflow h_blank_display_start_position");
        h_blank_display_start_position = bestHTotal * 0.936f;
    }
    if (h_blank_display_stop_position > bestHTotal || isLargeDiff) {
        // _WSN("overflow h_blank_display_stop_position");
        h_blank_display_stop_position = bestHTotal * 0.178f;
    }
    if ((h_blank_memory_start_position > bestHTotal) || (h_blank_memory_start_position > h_blank_display_start_position) || isLargeDiff) {
        // _WSN("overflow h_blank_memory_start_position");
        h_blank_memory_start_position = h_blank_display_start_position * 0.971f;
    }
    if (h_blank_memory_stop_position > bestHTotal || isLargeDiff) {
        // _WSN("overflow h_blank_memory_stop_position");
        h_blank_memory_stop_position = h_blank_display_stop_position * 0.64f;
    }

    // check whether HS spills over HBSPD
    if (h_sync_start_position > h_sync_stop_position && (h_sync_start_position < (bestHTotal / 2))) { // is neg HSync
        if (h_sync_start_position >= h_blank_display_stop_position) {
            h_sync_start_position = h_blank_display_stop_position * 0.8f;
            h_sync_stop_position = 4; // good idea to move this close to 0 as well
        }
    } else {
        if (h_sync_stop_position >= h_blank_display_stop_position) {
            h_sync_stop_position = h_blank_display_stop_position * 0.8f;
            h_sync_start_position = 4; //
        }
    }

    // just fix HS
    if (isLargeDiff) {
        if (h_sync_start_position > h_sync_stop_position && (h_sync_start_position < (bestHTotal / 2))) { // is neg HSync
            h_sync_stop_position = 4;
            // stop = at least start, then a bit outwards
            h_sync_start_position = 16 + (h_blank_display_stop_position * 0.3f);
        } else {
            h_sync_start_position = 4;
            h_sync_stop_position = 16 + (h_blank_display_stop_position * 0.3f);
        }
    }

    // finally, fix forced timings with large diff
    // update: doesn't seem necessary anymore
    // if (isLargeDiff) {
    //  h_blank_display_start_position = bestHTotal * 0.946f;
    //  h_blank_display_stop_position = bestHTotal * 0.22f;
    //  h_blank_memory_start_position = h_blank_display_start_position * 0.971f;
    //  h_blank_memory_stop_position = h_blank_display_stop_position * 0.64f;

    //  if (h_sync_start_position > h_sync_stop_position && (h_sync_start_position < (bestHTotal / 2))) { // is neg HSync
    //    h_sync_stop_position = 0;
    //    // stop = at least start, then a bit outwards
    //    h_sync_start_position = 16 + (h_blank_display_stop_position * 0.4f);
    //  }
    //  else {
    //    h_sync_start_position = 0;
    //    h_sync_stop_position = 16 + (h_blank_display_stop_position * 0.4f);
    //  }
    //}

    if (diffHTotal != 0) { // apply
        // delay the change to field start, a bit more compatible
        uint16_t timeout = 0;
        while ((GBS::STATUS_VDS_FIELD::read() == 1) && (++timeout < 400))
            ;
        while ((GBS::STATUS_VDS_FIELD::read() == 0) && (++timeout < 800))
            ;

        GBS::VDS_HSYNC_RST::write(bestHTotal);
        GBS::VDS_DIS_HB_ST::write(h_blank_display_start_position);
        GBS::VDS_DIS_HB_SP::write(h_blank_display_stop_position);
        GBS::VDS_HB_ST::write(h_blank_memory_start_position);
        GBS::VDS_HB_SP::write(h_blank_memory_stop_position);
        GBS::VDS_HS_ST::write(h_sync_start_position);
        GBS::VDS_HS_SP::write(h_sync_stop_position);
    }

    bool print = 1;
    if (uopt->enableFrameTimeLock) {
        if ((GBS::GBS_RUNTIME_FTL_ADJUSTED::read() == 1) && !rto->forceRetime) {
            // FTL enabled and regular update, so don't print
            print = 0;
        }
        GBS::GBS_RUNTIME_FTL_ADJUSTED::write(0);
    }

    rto->forceRetime = false;

    if (print) {
        _WS(F("HTotal Adjust: "));
        if (diffHTotal >= 0) {
            _WS(F(" ")); // formatting to align with negative value readouts
        }
        _WS(diffHTotal);

        if (!rto->extClockGenDetected) {
            float sfr = getSourceFieldRate(0);
            delay(0);
            float ofr = getOutputFrameRate();
            if (sfr < 1.0f) {
                sfr = getSourceFieldRate(0); // retry
            }
            if (ofr < 1.0f) {
                ofr = getOutputFrameRate(); // retry
            }
            _WSF(PSTR(", source Hz: %.3f, output Hz: %.3f\n"), sfr, ofr);
        } else {
            _WSN();
        }
    }

    return true;
}

/**
 * @brief
 *
 * @return true
 * @return false
 */
bool runAutoBestHTotal()
{
    if (!FrameSync::ready() && rto->autoBestHtotalEnabled == true && rto->videoStandardInput > 0 && rto->videoStandardInput < 15) {

        // _DBGN("running");
        // unsigned long startTime = millis();

        bool stableNow = 1;

        for (uint8_t i = 0; i < 64; i++) {
            if (!getStatus16SpHsStable()) {
                stableNow = 0;
                // _WSN("prevented: !getStatus16SpHsStable");
                break;
            }
        }

        if (stableNow) {
            if (GBS::STATUS_INT_SOG_BAD::read()) {
                // _WSN("prevented_2!");
                resetInterruptSogBadBit();
                delay(40);
                stableNow = false;
            }
            resetInterruptSogBadBit();

            if (stableNow && (getVideoMode() == rto->videoStandardInput)) {
                uint8_t testBusSelBackup = GBS::TEST_BUS_SEL::read();
                uint8_t vdsBusSelBackup = GBS::VDS_TEST_BUS_SEL::read();
                uint8_t ifBusSelBackup = GBS::IF_TEST_SEL::read();

                if (testBusSelBackup != 0)
                    GBS::TEST_BUS_SEL::write(0); // needs decimation + if
                if (vdsBusSelBackup != 0)
                    GBS::VDS_TEST_BUS_SEL::write(0); // VDS test # 0 = VBlank
                if (ifBusSelBackup != 3)
                    GBS::IF_TEST_SEL::write(3); // IF averaged frame time

                yield();
                uint16_t bestHTotal = FrameSync::init(); // critical task
                yield();

                GBS::TEST_BUS_SEL::write(testBusSelBackup); // always restore from backup (TB has changed)
                if (vdsBusSelBackup != 0)
                    GBS::VDS_TEST_BUS_SEL::write(vdsBusSelBackup);
                if (ifBusSelBackup != 3)
                    GBS::IF_TEST_SEL::write(ifBusSelBackup);

                if (GBS::STATUS_INT_SOG_BAD::read()) {
                    // _WSN("prevented_5 INT_SOG_BAD!");
                    stableNow = false;
                }
                for (uint8_t i = 0; i < 16; i++) {
                    if (!getStatus16SpHsStable()) {
                        stableNow = 0;
                        // _WSN("prevented_5: !getStatus16SpHsStable");
                        break;
                    }
                }
                resetInterruptSogBadBit();

                if (bestHTotal > 4095) {
                    if (!rto->forceRetime) {
                        stableNow = false;
                    } else {
                        // roll with it
                        bestHTotal = 4095;
                    }
                }

                if (stableNow) {
                    for (uint8_t i = 0; i < 24; i++) {
                        delay(1);
                        if (!getStatus16SpHsStable()) {
                            stableNow = false;
                            // _WSN("prevented_3!");
                            break;
                        }
                    }
                }

                if (bestHTotal > 0 && stableNow) {
                    bool success = applyBestHTotal(bestHTotal);
                    if (success) {
                        rto->syncLockFailIgnore = 16;
                        // _WS(F("ok, took: "));
                        // _WSN(millis() - startTime);
                        return true; // success
                    }
                }
            }
        }

        // reaching here can happen even if stableNow == 1
        if (!stableNow) {
            FrameSync::reset(uopt->frameTimeLockMethod);

            if (rto->syncLockFailIgnore > 0) {
                rto->syncLockFailIgnore--;
                if (rto->syncLockFailIgnore == 0) {
                    GBS::DAC_RGBS_PWDNZ::write(1); // xth chance
                    if (!uopt->wantOutputComponent) {
                        GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out // xth chance
                    }
                    rto->autoBestHtotalEnabled = false;
                }
            }
            _DBGF(PSTR("bestHtotal retry (%d)\n"), rto->syncLockFailIgnore);
        }
    } else if (FrameSync::ready()) {
        // FS ready but mode is 0 or 15 or autoBestHtotal is off
        return true;
    }

    if (rto->continousStableCounter != 0 && rto->continousStableCounter != 255) {
        rto->continousStableCounter++; // stop repetitions
    }

    return false;
}

/**
 * @brief
 *
 * @return true
 * @return false
 */
bool snapToIntegralFrameRate(void)
{
    // Fetch the current output frame rate
    float ofr = getOutputFrameRate();

    if (ofr < 1.0f) {
        delay(1);
        ofr = getOutputFrameRate();
    }

    float target;
    if (ofr > 56.5f && ofr < 64.5f) {
        target = 60.0f; // NTSC like
    } else if (ofr > 46.5f && ofr < 54.5f) {
        target = 50.0f; // PAL like
    } else {
        // too far out of spec for an auto adjust
        _WSN(F("out of bounds"));
        return false;
    }

    _WSF(PSTR("Snap to %.2fHz\n"), target);

    // We'll be adjusting the htotal incrementally, so store current and best match.
    uint16_t currentHTotal = GBS::VDS_HSYNC_RST::read();
    uint16_t closestHTotal = currentHTotal;

    // What's the closest we've been to the frame rate?
    float closestDifference = fabs(target - ofr);

    // Repeatedly adjust htotals until we find the closest match.
    for (;;) {

        delay(0);

        // Try to move closer to the desired framerate.
        if (target > ofr) {
            if (currentHTotal > 0 && applyBestHTotal(currentHTotal - 1)) {
                --currentHTotal;
            } else {
                return false;
            }
        } else if (target < ofr) {
            if (currentHTotal < 4095 && applyBestHTotal(currentHTotal + 1)) {
                ++currentHTotal;
            } else {
                return false;
            }
        } else {
            return true;
        }

        // Are we closer?
        ofr = getOutputFrameRate();

        if (ofr < 1.0f) {
            delay(1);
            ofr = getOutputFrameRate();
        }
        if (ofr < 1.0f) {
            return false;
        }

        // If we're getting closer, continue trying, otherwise break out of the test loop.
        float newDifference = fabs(target - ofr);
        if (newDifference < closestDifference) {
            closestDifference = newDifference;
            closestHTotal = currentHTotal;
        } else {
            break;
        }
    }

    // Reapply the closest htotal if need be.
    if (closestHTotal != currentHTotal) {
        applyBestHTotal(closestHTotal);
    }

    return true;
}

/**
 * @brief blue only mode: t0t44t1 t0t44t4
 *
 */
void applyRGBPatches()
{
    GBS::ADC_RYSEL_R::write(0);     // gnd clamp red
    GBS::ADC_RYSEL_B::write(0);     // gnd clamp blue
    GBS::ADC_RYSEL_G::write(0);     // gnd clamp green
    GBS::DEC_MATRIX_BYPS::write(0); // 5_1f 2 = 1 for YUV / 0 for RGB << using DEC matrix
    GBS::IF_MATRIX_BYPS::write(1);

    if (GBS::GBS_PRESET_CUSTOM::read() == 0) {
        // colors
        GBS::VDS_Y_GAIN::write(0x80); // 0x80 = 0
        GBS::VDS_UCOS_GAIN::write(0x1c);
        GBS::VDS_VCOS_GAIN::write(0x29); // 0x27 when using IF matrix, 0x29 for DEC matrix
        GBS::VDS_Y_OFST::write(0x00);    // 0
        GBS::VDS_U_OFST::write(0x00);    // 2
        GBS::VDS_V_OFST::write(0x00);    // 2
    }

    if (uopt->wantOutputComponent) {
        applyComponentColorMixing();
    }
}

/**
 * @brief Set the And Latch Phase S P object
 *
 */
void setAndLatchPhaseSP()
{
    GBS::PA_SP_LAT::write(0); // latch off
    GBS::PA_SP_S::write(rto->phaseSP);
    GBS::PA_SP_LAT::write(1); // latch on
}

/**
 * @brief Set the And Update Sog Level object
 *          Sync detect resolution: 5bits; comparator voltage range 10mv~320mv.-> 10mV
 *          per step; if cables and source are to standard (level 6 = 60mV)
 * @param level
 */
void setAndUpdateSogLevel(uint8_t level)
{
    rto->currentLevelSOG = level & 0x1f;
    GBS::ADC_SOGCTRL::write(level);
    setAndLatchPhaseSP();
    setAndLatchPhaseADC();
    latchPLLAD();
    GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
    GBS::INTERRUPT_CONTROL_00::write(0x00);
    // _WS(F("sog: ")); _WSN(rto->currentLevelSOG);
}

/**
 * @brief
 *
 * @return true
 * @return false
 */
bool optimizePhaseSP()
{
    uint16_t pixelClock = GBS::PLLAD_MD::read();
    uint8_t badHt = 0, prevBadHt = 0, worstBadHt = 0, worstPhaseSP = 0, prevPrevBadHt = 0, goodHt = 0;
    bool runTest = 1;

    if (GBS::STATUS_SYNC_PROC_HTOTAL::read() < (pixelClock - 8)) {
        return 0;
    }
    if (GBS::STATUS_SYNC_PROC_HTOTAL::read() > (pixelClock + 8)) {
        return 0;
    }

    if (rto->currentLevelSOG <= 2) {
        // not very stable, use fixed values
        rto->phaseSP = 16;
        rto->phaseADC = 16;
        if (rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
            if (rto->osr == 4) {
                rto->phaseADC += 16;
                rto->phaseADC &= 0x1f;
            }
        }
        delay(8);    // a bit longer, to match default run time
        runTest = 0; // skip to end
    }

    // unsigned long startTime = millis();

    if (runTest) {
        // 32 distinct phase settings, 3 average samples (missing 2 phase steps) > 34
        for (uint8_t u = 0; u < 34; u++) {
            rto->phaseSP++;
            rto->phaseSP &= 0x1f;
            setAndLatchPhaseSP();
            badHt = 0;
            delayMicroseconds(256);
            for (uint8_t i = 0; i < 20; i++) {
                if (GBS::STATUS_SYNC_PROC_HTOTAL::read() != pixelClock) {
                    badHt++;
                    delayMicroseconds(384);
                }
            }
            // if average 3 samples has more badHt than seen yet, this phase step is worse
            if ((badHt + prevBadHt + prevPrevBadHt) > worstBadHt) {
                worstBadHt = (badHt + prevBadHt + prevPrevBadHt);
                worstPhaseSP = (rto->phaseSP - 1) & 0x1f; // medium of 3 samples
            }

            if (badHt == 0) {
                // count good readings as well, to know whether the entire run is valid
                goodHt++;
            }

            prevPrevBadHt = prevBadHt;
            prevBadHt = badHt;
            // _WS(rto->phaseSP); _WS(" badHt: "); _WSN(badHt);
        }

        // _WSN(goodHt);

        if (goodHt < 17) {
            // _WSN("pxClk unstable");
            return 0;
        }

        // adjust global phase values according to test results
        if (worstBadHt != 0) {
            rto->phaseSP = (worstPhaseSP + 16) & 0x1f;
            // assume color signals arrive at same time: phase adc = phase sp
            // test in hdbypass mode shows this is more related to sog.. the assumptions seem fine at sog = 8
            rto->phaseADC = 16; //(rto->phaseSP) & 0x1f;

            // different OSR require different phase angles, also depending on bypass, etc
            // shift ADC phase 180 degrees for the following
            if (rto->videoStandardInput >= 5 && rto->videoStandardInput <= 7) {
                if (rto->osr == 2) {
                    // _WSN("shift adc phase");
                    rto->phaseADC += 16;
                    rto->phaseADC &= 0x1f;
                }
            } else if (rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
                if (rto->osr == 4) {
                    // _WSN("shift adc phase");
                    rto->phaseADC += 16;
                    rto->phaseADC &= 0x1f;
                }
            }
        } else {
            // test was always good, so choose any reasonable value
            rto->phaseSP = 16;
            rto->phaseADC = 16;
            if (rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
                if (rto->osr == 4) {
                    rto->phaseADC += 16;
                    rto->phaseADC &= 0x1f;
                }
            }
        }
    }

    // _WSN(millis() - startTime);
    // _WSN("worstPhaseSP: "); _WSN(worstPhaseSP);

    /*static uint8_t lastLevelSOG = 99;
  if (lastLevelSOG != rto->currentLevelSOG) {
    _WS(F("Phase: ")); _WS(rto->phaseSP);
    _WS(F(" SOG: "));  _WS(rto->currentLevelSOG);
    _WSN();
  }
  lastLevelSOG = rto->currentLevelSOG;*/

    setAndLatchPhaseSP();
    delay(1);
    setAndLatchPhaseADC();

    return 1;
}

/**
 * @brief Set the Over Sample Ratio object
 *
 * @param newRatio
 * @param prepareOnly
 */
void setOverSampleRatio(uint8_t newRatio, bool prepareOnly)
{
    uint8_t ks = GBS::PLLAD_KS::read();

    bool hi_res = rto->videoStandardInput == 8 || rto->videoStandardInput == 4 || rto->videoStandardInput == 3;
    // bool bypass = rto->presetID == PresetHdBypass;
    bool bypass = uopt->resolutionID == PresetHdBypass;

    switch (newRatio) {
        case 1:
            if (ks == 0)
                GBS::PLLAD_CKOS::write(0);
            if (ks == 1)
                GBS::PLLAD_CKOS::write(1);
            if (ks == 2)
                GBS::PLLAD_CKOS::write(2);
            if (ks == 3)
                GBS::PLLAD_CKOS::write(3);
            GBS::ADC_CLK_ICLK2X::write(0);
            GBS::ADC_CLK_ICLK1X::write(0);
            GBS::DEC1_BYPS::write(1); // dec1 couples to ADC_CLK_ICLK2X
            GBS::DEC2_BYPS::write(1);

            // Necessary to avoid a 2x-scaled image for some reason.
            if (hi_res && !bypass) {
                GBS::ADC_CLK_ICLK1X::write(1);
                // GBS::DEC2_BYPS::write(0);
            }

            rto->osr = 1;
            // if (!prepareOnly) _WSN(F("OSR 1x"));

            break;
        case 2:
            if (ks == 0) {
                setOverSampleRatio(1, false);
                return;
            } // 2x impossible
            if (ks == 1)
                GBS::PLLAD_CKOS::write(0);
            if (ks == 2)
                GBS::PLLAD_CKOS::write(1);
            if (ks == 3)
                GBS::PLLAD_CKOS::write(2);
            GBS::ADC_CLK_ICLK2X::write(0);
            GBS::ADC_CLK_ICLK1X::write(1);
            GBS::DEC2_BYPS::write(0);
            GBS::DEC1_BYPS::write(1); // dec1 couples to ADC_CLK_ICLK2X

            if (hi_res && !bypass) {
                // GBS::ADC_CLK_ICLK2X::write(1);
                // GBS::DEC1_BYPS::write(0);
                //  instead increase CKOS by 1 step
                GBS::PLLAD_CKOS::write(GBS::PLLAD_CKOS::read() + 1);
            }

            rto->osr = 2;
            // if (!prepareOnly) _WSN(F("OSR 2x"));

            break;
        case 4:
            if (ks == 0) {
                setOverSampleRatio(1, false);
                return;
            } // 4x impossible
            if (ks == 1) {
                setOverSampleRatio(1, false);
                return;
            } // 4x impossible
            if (ks == 2)
                GBS::PLLAD_CKOS::write(0);
            if (ks == 3)
                GBS::PLLAD_CKOS::write(1);
            GBS::ADC_CLK_ICLK2X::write(1);
            GBS::ADC_CLK_ICLK1X::write(1);
            GBS::DEC1_BYPS::write(0); // dec1 couples to ADC_CLK_ICLK2X
            GBS::DEC2_BYPS::write(0);

            rto->osr = 4;
            // if (!prepareOnly) _WSN(F("OSR 4x"));

            break;
        default:
            break;
    }

    if (!prepareOnly)
        latchPLLAD();
}

/**
 * @brief
 *
 * @param withCurrentVideoModeCheck
 */
void updateSpDynamic(bool withCurrentVideoModeCheck)
{
    if (!rto->boardHasPower || rto->sourceDisconnected) {
        return;
    }

    uint8_t vidModeReadout = getVideoMode();
    if (vidModeReadout == 0) {
        vidModeReadout = getVideoMode();
    }

    if (rto->videoStandardInput == 0 && vidModeReadout == 0) {
        if (GBS::SP_DLT_REG::read() > 0x30)
            GBS::SP_DLT_REG::write(0x30); // 5_35
        else
            GBS::SP_DLT_REG::write(0xC0);
        return;
    }
    // reset condition, allow most formats to detect
    // compare 1chip snes and ps2 1080p
    if (vidModeReadout == 0 && withCurrentVideoModeCheck) {
        if ((rto->noSyncCounter % 16) <= 8 && rto->noSyncCounter != 0) {
            GBS::SP_DLT_REG::write(0x30); // 5_35
        } else if ((rto->noSyncCounter % 16) > 8 && rto->noSyncCounter != 0) {
            GBS::SP_DLT_REG::write(0xC0); // may want to use lower, around 0x70
        } else {
            GBS::SP_DLT_REG::write(0x30);
        }
        GBS::SP_H_PULSE_IGNOR::write(0x02);
        // GBS::SP_DIS_SUB_COAST::write(1);
        GBS::SP_H_CST_ST::write(0x10);
        GBS::SP_H_CST_SP::write(0x100);
        GBS::SP_H_COAST::write(0);        // 5_3e 2 (just in case)
        GBS::SP_H_TIMER_VAL::write(0x3a); // new: 5_33 default 0x3a, set shorter for better hsync drop detect
        if (rto->syncTypeCsync) {
            GBS::SP_COAST_INV_REG::write(1); // new, allows SP to see otherwise potentially skipped vlines
        }
        rto->coastPositionIsSet = false;
        return;
    }

    if (rto->syncTypeCsync) {
        GBS::SP_COAST_INV_REG::write(0);
    }

    if (rto->videoStandardInput != 0) {
        if (rto->videoStandardInput <= 2) { // SD interlaced
            GBS::SP_PRE_COAST::write(7);
            GBS::SP_POST_COAST::write(3);
            GBS::SP_DLT_REG::write(0xC0);     // old: 0x140 works better than 0x130 with psx
            GBS::SP_H_TIMER_VAL::write(0x28); // 5_33

            if (rto->syncTypeCsync) {
                uint16_t hPeriod = GBS::HPERIOD_IF::read();
                for (int i = 0; i < 16; i++) {
                    if (hPeriod == 511 || hPeriod < 200) {
                        hPeriod = GBS::HPERIOD_IF::read(); // retry / overflow
                        if (i == 15) {
                            hPeriod = 300;
                            break; // give up, use short base to get low ignore value later
                        }
                    } else {
                        break;
                    }
                    delayMicroseconds(100);
                }

                uint16_t ignoreLength = hPeriod * 0.081f; // hPeriod is base length
                if (hPeriod <= 200) {                     // mode is NTSC / PAL, very likely overflow
                    ignoreLength = 0x18;                  // use neutral but low value
                }

                // get hpw to ht ratio. stability depends on hpll lock
                double ratioHs, ratioHsAverage = 0.0;
                uint8_t testOk = 0;
                for (int i = 0; i < 30; i++) {
                    ratioHs = (double)GBS::STATUS_SYNC_PROC_HLOW_LEN::read() / (double)(GBS::STATUS_SYNC_PROC_HTOTAL::read() + 1);
                    if (ratioHs > 0.041 && ratioHs < 0.152) { // 0.152 : (354 / 2345) is 9.5uS on NTSC (crtemudriver)
                        testOk++;
                        ratioHsAverage += ratioHs;
                        if (testOk == 12) {
                            ratioHs = ratioHsAverage / testOk;
                            break;
                        }
                        delayMicroseconds(30);
                    }
                }
                if (testOk != 12) {
                    // _WS("          testok: "); _WSN(testOk);
                    ratioHs = 0.032; // 0.032: (~100 / 2560) is ~2.5uS on NTSC (find with crtemudriver)
                }

                // _WS(" (debug) hPeriod: ");  _WSN(hPeriod);
                // _WS(" (debug) ratioHs: ");  _WSN(ratioHs, 5);
                // _WS(" (debug) ignoreBase: 0x");  _WSF("0x%04X\n", ignoreLength);
                uint16_t pllDiv = GBS::PLLAD_MD::read();
                ignoreLength = ignoreLength + (pllDiv * (ratioHs * 0.38)); // for factor: crtemudriver tests
                // _WS(F(" (debug) ign.length: 0x"); _WSF("0x%04X\n", ignoreLength);

                // > check relies on sync instability (potentially from too large ign. length) getting cought earlier
                if (ignoreLength > GBS::SP_H_PULSE_IGNOR::read() || GBS::SP_H_PULSE_IGNOR::read() >= 0x90) {
                    if (ignoreLength > 0x90) { // if higher, HPERIOD_IF probably was 511 / limit
                        ignoreLength = 0x90;
                    }
                    if (ignoreLength >= 0x1A && ignoreLength <= 0x42) {
                        ignoreLength = 0x1A; // at the low end should stick to 0x1A
                    }
                    if (ignoreLength != GBS::SP_H_PULSE_IGNOR::read()) {
                        GBS::SP_H_PULSE_IGNOR::write(ignoreLength);
                        rto->coastPositionIsSet = 0; // mustn't be skipped, needed when input changes dotclock / Hz
                        _WSF(PSTR("(debug) ign. length: 0x%04X\n"), ignoreLength);
                    }
                }
            }
        } else if (rto->videoStandardInput <= 4) {
            GBS::SP_PRE_COAST::write(7);  // these two were 7 and 6
            GBS::SP_POST_COAST::write(6); // and last 11 and 11
            // 3,3 fixes the ps2 issue but these are too low for format change detects
            // update: seems to be an SP bypass only problem (t5t57t6 to 0 also fixes it)
            GBS::SP_DLT_REG::write(0xA0);
            GBS::SP_H_PULSE_IGNOR::write(0x0E);    // ps3: up to 0x3e, ps2: < 0x14
        } else if (rto->videoStandardInput == 5) { // 720p
            GBS::SP_PRE_COAST::write(7);           // down to 4 ok with ps2
            GBS::SP_POST_COAST::write(7);          // down to 6 ok with ps2 // ps3: 8 too much
            GBS::SP_DLT_REG::write(0x30);
            GBS::SP_H_PULSE_IGNOR::write(0x08);    // ps3: 0xd too much
        } else if (rto->videoStandardInput <= 7) { // 1080i,p
            GBS::SP_PRE_COAST::write(9);
            GBS::SP_POST_COAST::write(18); // of 1124 input lines
            GBS::SP_DLT_REG::write(0x70);
            // ps2 up to 0x06
            // new test shows ps2 alternating between okay and not okay at 0x0a with 5_35=0x70
            GBS::SP_H_PULSE_IGNOR::write(0x06);
        } else if (rto->videoStandardInput >= 13) { // 13, 14 and 15 (was just 13 and 15)
            if (rto->syncTypeCsync == false) {
                GBS::SP_PRE_COAST::write(0x00);
                GBS::SP_POST_COAST::write(0x00);
                GBS::SP_H_PULSE_IGNOR::write(0xff); // required this because 5_02 0 is on
                GBS::SP_DLT_REG::write(0x00);       // sometimes enough on it's own, but not always
            } else {                                // csync
                GBS::SP_PRE_COAST::write(0x04);     // as in bypass mode set function
                GBS::SP_POST_COAST::write(0x07);    // as in bypass mode set function
                GBS::SP_DLT_REG::write(0x70);
                GBS::SP_H_PULSE_IGNOR::write(0x02);
            }
        }
    }
}

/**
 * @brief Set the Out Mode Hd Bypass object
 *          use t5t00t2 and adjust t5t11t5 to find this sources ideal sampling
 *          clock for this preset (affected by htotal) 2431 for psx, 2437 for MD
 *          in this mode, sampling clock is free to choose
 * @param regsInitialized
 */
void setOutModeHdBypass(bool regsInitialized)
{
    if (!rto->boardHasPower) {
        _WSN(F("GBS board not responding!"));
        return;
    }

    rto->autoBestHtotalEnabled = false; // disable while in this mode
    rto->outModeHdBypass = 1;           // skips waiting at end of doPostPresetLoadSteps

    externalClockGenResetClock();
    updateSpDynamic(0);
    loadHdBypassSection(); // this would be ignored otherwise

    // TODO: needs clarification (see: rto->debugView)
    // if (GBS::ADC_UNUSED_62::read() != 0x00) {
        // remember debug view
        // if (uopt->presetPreference != 2) {
        // if (uopt->resolutionID != OutputCustom) {
        // serialCommand = 'D';
        // }
    // }

    GBS::SP_NO_COAST_REG::write(0);  // enable vblank coast (just in case)
    GBS::SP_COAST_INV_REG::write(0); // also just in case

    FrameSync::cleanup();
    GBS::ADC_UNUSED_62::write(0x00);      // clear debug view
    GBS::RESET_CONTROL_0x46::write(0x00); // 0_46 all off, nothing needs to be enabled for bp mode
    GBS::RESET_CONTROL_0x47::write(0x00);
    GBS::PA_ADC_BYPSZ::write(1); // enable phase unit ADC
    GBS::PA_SP_BYPSZ::write(1);  // enable phase unit SP

    GBS::GBS_PRESET_ID::write(PresetHdBypass);
    // If loading from top-level, clear custom preset flag to avoid stale
    // values. If loading after applyPresets() called writeProgramArrayNew(), it
    // has already set the flag to 1.
    if (!regsInitialized) {
        GBS::GBS_PRESET_CUSTOM::write(0);
    }
    doPostPresetLoadSteps(); // todo: remove this, code path for hdbypass is hard to follow

    // doPostPresetLoadSteps() sets rto->presetID = GBS_PRESET_ID::read() =
    // PresetHdBypass, and rto->isCustomPreset = GBS_PRESET_CUSTOM::read().

    resetDebugPort();

    rto->autoBestHtotalEnabled = false; // need to re-set this
    GBS::OUT_SYNC_SEL::write(1);        // 0_4f 1=sync from HDBypass, 2=sync from SP, 0 = sync from VDS

    GBS::PLL_CKIS::write(0);    // 0_40 0 //  0: PLL uses OSC clock | 1: PLL uses input clock
    GBS::PLL_DIVBY2Z::write(0); // 0_40 1 // 1= no divider (full clock, ie 27Mhz) 0 = halved
    // GBS::PLL_ADS::write(0); // 0_40 3 test:  input clock is from PCLKIN (disconnected, not ADC clock)
    GBS::PAD_OSC_CNTRL::write(1); // test: noticed some wave pattern in 720p source, this fixed it
    GBS::PLL648_CONTROL_01::write(0x35);
    GBS::PLL648_CONTROL_03::write(0x00);
    GBS::PLL_LEN::write(1); // 0_43
    GBS::DAC_RGBS_R0ENZ::write(1);
    GBS::DAC_RGBS_G0ENZ::write(1); // 0_44
    GBS::DAC_RGBS_B0ENZ::write(1);
    GBS::DAC_RGBS_S1EN::write(1); // 0_45
    // from RGBHV tests: the memory bus can be tri stated for noise reduction
    GBS::PAD_TRI_ENZ::write(1);        // enable tri state
    GBS::PLL_MS::write(2);             // select feedback clock (but need to enable tri state!)
    GBS::MEM_PAD_CLK_INVERT::write(0); // helps also
    GBS::RESET_CONTROL_0x47::write(0x1f);

    // update: found the real use of HDBypass :D
    GBS::DAC_RGBS_BYPS2DAC::write(1);
    GBS::SP_HS_LOOP_SEL::write(1);
    GBS::SP_HS_PROC_INV_REG::write(0); // (5_56_5) do not invert HS
    GBS::SP_CS_P_SWAP::write(0);       // old default, set here to reset between HDBypass formats
    GBS::SP_HS2PLL_INV_REG::write(0);  // same

    GBS::PB_BYPASS::write(1);
    GBS::PLLAD_MD::write(2345); // 2326 looks "better" on my LCD but 2345 looks just correct on scope
    GBS::PLLAD_KS::write(2);    // 5_16 post divider 0 : FCKO1 > 87MHz, 3 : FCKO1<23MHz
    setOverSampleRatio(2, true);
    GBS::PLLAD_ICP::write(5);
    GBS::PLLAD_FS::write(1);

    if (rto->inputIsYPbPr) {
        GBS::DEC_MATRIX_BYPS::write(1); // 5_1f 2 = 1 for YUV / 0 for RGB
        GBS::HD_MATRIX_BYPS::write(0);  // 1_30 1 / input to jacks is yuv, adc leaves it as yuv > convert to rgb for output here
        GBS::HD_DYN_BYPS::write(0);     // don't bypass color expansion
                                        // GBS::HD_U_OFFSET::write(3);     // color adjust via scope
                                    // GBS::HD_V_OFFSET::write(3);     // color adjust via scope
    } else {
        GBS::DEC_MATRIX_BYPS::write(1); // this is normally RGB input for HDBYPASS out > no color matrix at all
        GBS::HD_MATRIX_BYPS::write(1);  // 1_30 1 / input is rgb, adc leaves it as rgb > bypass matrix
        GBS::HD_DYN_BYPS::write(1);     // bypass as well
    }

    GBS::HD_SEL_BLK_IN::write(0); // 0 enables HDB blank timing (1 would be DVI, not working atm)

    GBS::SP_SDCS_VSST_REG_H::write(0); // S5_3B
    GBS::SP_SDCS_VSSP_REG_H::write(0); // S5_3B
    GBS::SP_SDCS_VSST_REG_L::write(0); // S5_3F // 3 for SP sync
    GBS::SP_SDCS_VSSP_REG_L::write(2); // S5_40 // 10 for SP sync // check with interlaced sources

    GBS::HD_HSYNC_RST::write(0x3ff); // max 0x7ff
    GBS::HD_INI_ST::write(0);        // todo: test this at 0 / was 0x298
    // timing into HDB is PLLAD_MD with PLLAD_KS divider: KS = 0 > full PLLAD_MD
    if (rto->videoStandardInput <= 2) {
        // PAL and NTSC are rewrites, the rest is still handled normally
        // These 2 formats now have SP_HS2PLL_INV_REG set. That's the only way I know so far that
        // produces recovered HSyncs that align to the falling edge of the input
        // ToDo: find reliable input active flank detect to then set SP_HS2PLL_INV_REG correctly
        // (for PAL/NTSC polarity is known to be active low, but other formats are variable)
        GBS::SP_HS2PLL_INV_REG::write(1);  // 5_56 1 lock to falling HS edge // check > sync issues with MD
        GBS::SP_CS_P_SWAP::write(1);       // 5_3e 0 new: this should negate the problem with inverting HS2PLL
        GBS::SP_HS_PROC_INV_REG::write(1); // (5_56_5) invert HS to DEC
        // invert mode detect HS/VS triggers, helps PSX NTSC detection. required with 5_3e 0 set
        GBS::MD_HS_FLIP::write(1);
        GBS::MD_VS_FLIP::write(1);
        GBS::OUT_SYNC_SEL::write(2);   // new: 0_4f 1=sync from HDBypass, 2=sync from SP, 0 = sync from VDS
        GBS::SP_HS_LOOP_SEL::write(0); // 5_57 6 new: use full SP sync, enable HS positioning and pulse length control
        GBS::ADC_FLTR::write(3);       // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
        // GBS::HD_INI_ST::write(0x76); // 1_39

        GBS::HD_HSYNC_RST::write((GBS::PLLAD_MD::read() / 2) + 8); // ADC output pixel count determined
        GBS::HD_HB_ST::write(GBS::PLLAD_MD::read() * 0.945f);      // 1_3B  // no idea why it's not coupled to HD_RST
        GBS::HD_HB_SP::write(0x90);                                // 1_3D
        GBS::HD_HS_ST::write(0x80);                                // 1_3F  // but better to use SP sync directly (OUT_SYNC_SEL = 2)
        GBS::HD_HS_SP::write(0x00);                                // 1_41  //
        // to use SP sync directly; prepare reasonable out HS length
        GBS::SP_CS_HS_ST::write(0xA0);
        GBS::SP_CS_HS_SP::write(0x00);

        if (rto->videoStandardInput == 1) {
            setCsVsStart(250);         // don't invert VS with direct SP sync mode
            setCsVsStop(1);            // stop relates to HS pulses from CS decoder directly, so mind EQ pulses
            GBS::HD_VB_ST::write(500); // 1_43
            GBS::HD_VS_ST::write(3);   // 1_47 // but better to use SP sync directly (OUT_SYNC_SEL = 2)
            GBS::HD_VS_SP::write(522); // 1_49 //
            GBS::HD_VB_SP::write(16);  // 1_45
        }
        if (rto->videoStandardInput == 2) {
            setCsVsStart(301);         // don't invert
            setCsVsStop(5);            // stop past EQ pulses (6 on psx) normally, but HDMI adapter works with -=1 (5)
            GBS::HD_VB_ST::write(605); // 1_43
            GBS::HD_VS_ST::write(1);   // 1_47
            GBS::HD_VS_SP::write(621); // 1_49
            GBS::HD_VB_SP::write(16);  // 1_45
        }
    } else if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4) { // 480p, 576p
        GBS::ADC_FLTR::write(2);                                               // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
        GBS::PLLAD_KS::write(1);                                               // 5_16 post divider
        GBS::PLLAD_CKOS::write(0);                                             // 5_16 2x OS (with KS=1)
        // GBS::HD_INI_ST::write(0x76); // 1_39
        GBS::HD_HB_ST::write(0x864); // 1_3B
                                     // you *must* begin hblank before hsync.
        GBS::HD_HB_SP::write(0xa0); // 1_3D
        GBS::HD_VB_ST::write(0x00); // 1_43
        GBS::HD_VB_SP::write(0x40); // 1_45
        if (rto->videoStandardInput == 3) {
            GBS::HD_HS_ST::write(0x54);  // 1_3F
            GBS::HD_HS_SP::write(0x864); // 1_41
            GBS::HD_VS_ST::write(0x06);  // 1_47 // VS neg
            GBS::HD_VS_SP::write(0x00);  // 1_49
            setCsVsStart(525 - 5);
            setCsVsStop(525 - 3);
        }
        if (rto->videoStandardInput == 4) {
            GBS::HD_HS_ST::write(0x10);  // 1_3F
            GBS::HD_HS_SP::write(0x880); // 1_41
            GBS::HD_VS_ST::write(0x06);  // 1_47 // VS neg
            GBS::HD_VS_SP::write(0x00);  // 1_49
            setCsVsStart(48);
            setCsVsStop(46);
        }
    } else if (rto->videoStandardInput <= 7 || rto->videoStandardInput == 13) {
        // GBS::SP_HS2PLL_INV_REG::write(0); // 5_56 1 use rising edge of tri-level sync // always 0 now
        if (rto->videoStandardInput == 5) { // 720p
            GBS::PLLAD_MD::write(2474);     // override from 2345
            GBS::HD_HSYNC_RST::write(550);  // 1_37
            // GBS::HD_INI_ST::write(78);     // 1_39
            //  720p has high pllad vco output clock, so don't do oversampling
            GBS::PLLAD_KS::write(0);       // 5_16 post divider 0 : FCKO1 > 87MHz, 3 : FCKO1<23MHz
            GBS::PLLAD_CKOS::write(0);     // 5_16 1x OS (with KS=CKOS=0)
            GBS::ADC_FLTR::write(0);       // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
            GBS::ADC_CLK_ICLK1X::write(0); // 5_00 4 (OS=1)
            GBS::DEC2_BYPS::write(1);      // 5_1f 1 // dec2 disabled (OS=1)
            GBS::PLLAD_ICP::write(6);      // fine at 6 only, FS is 1
            GBS::PLLAD_FS::write(1);
            GBS::HD_HB_ST::write(0);     // 1_3B
            GBS::HD_HB_SP::write(0x140); // 1_3D
            GBS::HD_HS_ST::write(0x20);  // 1_3F
            GBS::HD_HS_SP::write(0x80);  // 1_41
            GBS::HD_VB_ST::write(0x00);  // 1_43
            GBS::HD_VB_SP::write(0x6c);  // 1_45 // ps3 720p tested
            GBS::HD_VS_ST::write(0x00);  // 1_47
            GBS::HD_VS_SP::write(0x05);  // 1_49
            setCsVsStart(2);
            setCsVsStop(0);
        }
        if (rto->videoStandardInput == 6) { // 1080i
            // interl. source
            GBS::HD_HSYNC_RST::write(0x710); // 1_37
            // GBS::HD_INI_ST::write(2);    // 1_39
            GBS::PLLAD_KS::write(1);    // 5_16 post divider
            GBS::PLLAD_CKOS::write(0);  // 5_16 2x OS (with KS=1)
            GBS::ADC_FLTR::write(1);    // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
            GBS::HD_HB_ST::write(0);    // 1_3B
            GBS::HD_HB_SP::write(0xb8); // 1_3D
            GBS::HD_HS_ST::write(0x04); // 1_3F
            GBS::HD_HS_SP::write(0x50); // 1_41
            GBS::HD_VB_ST::write(0x00); // 1_43
            GBS::HD_VB_SP::write(0x1e); // 1_45
            GBS::HD_VS_ST::write(0x04); // 1_47
            GBS::HD_VS_SP::write(0x09); // 1_49
            setCsVsStart(8);
            setCsVsStop(6);
        }
        if (rto->videoStandardInput == 7) {  // 1080p
            GBS::PLLAD_MD::write(2749);      // override from 2345
            GBS::HD_HSYNC_RST::write(0x710); // 1_37
            // GBS::HD_INI_ST::write(0xf0);     // 1_39
            //  1080p has highest pllad vco output clock, so don't do oversampling
            GBS::PLLAD_KS::write(0);       // 5_16 post divider 0 : FCKO1 > 87MHz, 3 : FCKO1<23MHz
            GBS::PLLAD_CKOS::write(0);     // 5_16 1x OS (with KS=CKOS=0)
            GBS::ADC_FLTR::write(0);       // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
            GBS::ADC_CLK_ICLK1X::write(0); // 5_00 4 (OS=1)
            GBS::DEC2_BYPS::write(1);      // 5_1f 1 // dec2 disabled (OS=1)
            GBS::PLLAD_ICP::write(6);      // was 5, fine at 6 as well, FS is 1
            GBS::PLLAD_FS::write(1);
            GBS::HD_HB_ST::write(0x00); // 1_3B
            GBS::HD_HB_SP::write(0xb0); // 1_3D // d0
            GBS::HD_HS_ST::write(0x20); // 1_3F
            GBS::HD_HS_SP::write(0x70); // 1_41
            GBS::HD_VB_ST::write(0x00); // 1_43
            GBS::HD_VB_SP::write(0x2f); // 1_45
            GBS::HD_VS_ST::write(0x04); // 1_47
            GBS::HD_VS_SP::write(0x0A); // 1_49
        }
        if (rto->videoStandardInput == 13) { // odd HD mode (PS2 "VGA" over Component)
            applyRGBPatches();               // treat mostly as RGB, clamp R/B to gnd
            rto->syncTypeCsync = true;       // used in loop to set clamps and SP dynamic
            GBS::DEC_MATRIX_BYPS::write(1);  // overwrite for this mode
            GBS::SP_PRE_COAST::write(4);
            GBS::SP_POST_COAST::write(4);
            GBS::SP_DLT_REG::write(0x70);
            GBS::HD_MATRIX_BYPS::write(1);     // bypass since we'll treat source as RGB
            GBS::HD_DYN_BYPS::write(1);        // bypass since we'll treat source as RGB
            GBS::SP_VS_PROC_INV_REG::write(0); // don't invert
            // same as with RGBHV, the ps2 resolution can vary widely
            GBS::PLLAD_KS::write(0);       // 5_16 post divider 0 : FCKO1 > 87MHz, 3 : FCKO1<23MHz
            GBS::PLLAD_CKOS::write(0);     // 5_16 1x OS (with KS=CKOS=0)
            GBS::ADC_CLK_ICLK1X::write(0); // 5_00 4 (OS=1)
            GBS::ADC_CLK_ICLK2X::write(0); // 5_00 3 (OS=1)
            GBS::DEC1_BYPS::write(1);      // 5_1f 1 // dec1 disabled (OS=1)
            GBS::DEC2_BYPS::write(1);      // 5_1f 1 // dec2 disabled (OS=1)
            GBS::PLLAD_MD::write(512);     // could try 856
        }
    }

    if (rto->videoStandardInput == 13) {
        // section is missing HD_HSYNC_RST and HD_INI_ST adjusts
        uint16_t vtotal = GBS::STATUS_SYNC_PROC_VTOTAL::read();
        if (vtotal < 532) { // 640x480 or less
            GBS::PLLAD_KS::write(3);
            GBS::PLLAD_FS::write(1);
        } else if (vtotal >= 532 && vtotal < 810) { // 800x600, 1024x768
            // GBS::PLLAD_KS::write(3); // just a little too much at 1024x768
            GBS::PLLAD_FS::write(0);
            GBS::PLLAD_KS::write(2);
        } else { // if (vtotal > 1058 && vtotal < 1074)  // 1280x1024
            GBS::PLLAD_KS::write(2);
            GBS::PLLAD_FS::write(1);
        }
    }

    GBS::DEC_IDREG_EN::write(1); // 5_1f 7
    GBS::DEC_WEN_MODE::write(1); // 5_1e 7 // 1 keeps ADC phase consistent. around 4 lock positions vs totally random
    rto->phaseSP = 8;
    rto->phaseADC = 24;                         // fix value // works best with yuv input in tests
    setAndUpdateSogLevel(rto->currentLevelSOG); // also re-latch everything

    rto->outModeHdBypass = 1;

    unsigned long timeout = millis();
    while ((!getStatus16SpHsStable()) && (millis() - timeout < 2002)) {
        delay(1);
    }
    while ((getVideoMode() == 0) && (millis() - timeout < 1502)) {
        delay(1);
    }
    // currently SP is using generic settings, switch to format specific ones
    updateSpDynamic(0);
    while ((getVideoMode() == 0) && (millis() - timeout < 1502)) {
        delay(1);
    }

    GBS::DAC_RGBS_PWDNZ::write(1);   // enable DAC
    GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out
    delay(200);
    optimizePhaseSP();
    _WSN(F("output mode HD bypass"));
}

/**
 * @brief Set the Adc Gain object
 *          Write ADC gain registers, and save in adco->r_gain to properly transfer it
 *          across loading presets or passthrough.
 * @param gain
 */
void setAdcGain(uint8_t gain)
{
    // gain is actually range, increasing it dims the image.
    GBS::ADC_RGCTRL::write(gain);
    GBS::ADC_GGCTRL::write(gain);
    GBS::ADC_BGCTRL::write(gain);

    // Save gain for applying preset. (Gain affects passthrough presets, and
    // loading a passthrough preset loads from adco but doesn't save to it.)
    adco->r_gain = gain;
    adco->g_gain = gain;
    adco->b_gain = gain;
}

/**
 * @brief
 *
 */
void resetSyncProcessor()
{
    GBS::SFTRST_SYNC_RSTZ::write(0);
    delayMicroseconds(10);
    GBS::SFTRST_SYNC_RSTZ::write(1);
    // rto->clampPositionIsSet = false;  // resetSyncProcessor is part of autosog
    // rto->coastPositionIsSet = false;
}

/**
 * @brief
 *
 */
void togglePhaseAdjustUnits()
{
    GBS::PA_SP_BYPSZ::write(0); // yes, 0 means bypass on here
    GBS::PA_SP_BYPSZ::write(1);
    delay(2);
    GBS::PA_ADC_BYPSZ::write(0);
    GBS::PA_ADC_BYPSZ::write(1);
    delay(2);
}

/**
 * @brief
 *
 */
void bypassModeSwitch_RGBHV()
{
    if (!rto->boardHasPower) {
        _WSN(F("(!) GBS board not responding!"));
        return;
    }

    GBS::DAC_RGBS_PWDNZ::write(0);   // disable DAC
    GBS::PAD_SYNC_OUT_ENZ::write(1); // disable sync out

    loadHdBypassSection();
    externalClockGenResetClock();
    FrameSync::cleanup();
    GBS::ADC_UNUSED_62::write(0x00); // clear debug view
    GBS::PA_ADC_BYPSZ::write(1);     // enable phase unit ADC
    GBS::PA_SP_BYPSZ::write(1);      // enable phase unit SP
    applyRGBPatches();
    resetDebugPort();
    rto->videoStandardInput = 15;       // making sure
    rto->autoBestHtotalEnabled = false; // not necessary, since VDS is off / bypassed // todo: mode 14 (works anyway)
    rto->clampPositionIsSet = false;
    rto->HPLLState = 0;

    GBS::PLL_CKIS::write(0);           // 0_40 0 //  0: PLL uses OSC clock | 1: PLL uses input clock
    GBS::PLL_DIVBY2Z::write(0);        // 0_40 1 // 1= no divider (full clock, ie 27Mhz) 0 = halved clock
    GBS::PLL_ADS::write(0);            // 0_40 3 test:  input clock is from PCLKIN (disconnected, not ADC clock)
    GBS::PLL_MS::write(2);             // 0_40 4-6 select feedback clock (but need to enable tri state!)
    GBS::PAD_TRI_ENZ::write(1);        // enable some pad's tri state (they become high-z / inputs), helps noise
    GBS::MEM_PAD_CLK_INVERT::write(0); // helps also
    GBS::PLL648_CONTROL_01::write(0x35);
    GBS::PLL648_CONTROL_03::write(0x00); // 0_43
    GBS::PLL_LEN::write(1);              // 0_43

    GBS::DAC_RGBS_ADC2DAC::write(1);
    GBS::OUT_SYNC_SEL::write(1); // 0_4f 1=sync from HDBypass, 2=sync from SP, (00 = from VDS)

    GBS::SFTRST_HDBYPS_RSTZ::write(1); // enable
    GBS::HD_INI_ST::write(0);          // needs to be some small value or apparently 0 works
                                       // GBS::DAC_RGBS_BYPS2DAC::write(1);
                              // GBS::OUT_SYNC_SEL::write(2); // 0_4f sync from SP
                              // GBS::SFTRST_HDBYPS_RSTZ::write(1); // need HDBypass
                              // GBS::SP_HS_LOOP_SEL::write(1); // (5_57_6) // can bypass since HDBypass does sync
    GBS::HD_MATRIX_BYPS::write(1); // bypass since we'll treat source as RGB
    GBS::HD_DYN_BYPS::write(1);    // bypass since we'll treat source as RGB
                                   // GBS::HD_SEL_BLK_IN::write(1); // "DVI", not regular

    GBS::PAD_SYNC1_IN_ENZ::write(0); // filter H/V sync input1 (0 = on)
    GBS::PAD_SYNC2_IN_ENZ::write(0); // filter H/V sync input2 (0 = on)

    GBS::SP_SOG_P_ATO::write(1); // 5_20 1 corrects hpw readout and slightly affects sync
    if (rto->syncTypeCsync == false) {
        GBS::SP_SOG_SRC_SEL::write(0);  // 5_20 0 | 0: from ADC 1: from hs // use ADC and turn it off = no SOG
        GBS::ADC_SOGEN::write(1);       // 5_02 0 ADC SOG // having it 0 drags down the SOG (hsync) input; = 1: need to supress SOG decoding
        GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input ( 5_20 bit 3 )
        GBS::SP_SOG_MODE::write(0);     // 5_56 bit 0 // 0: normal, 1: SOG
        GBS::SP_NO_COAST_REG::write(1); // vblank coasting off
        GBS::SP_PRE_COAST::write(0);
        GBS::SP_POST_COAST::write(0);
        GBS::SP_H_PULSE_IGNOR::write(0xff); // cancel out SOG decoding
        GBS::SP_SYNC_BYPS::write(0);        // external H+V sync for decimator (+ sync out) | 1 to mirror in sync, 0 to output processed sync
        GBS::SP_HS_POL_ATO::write(1);       // 5_55 4 auto polarity for retiming
        GBS::SP_VS_POL_ATO::write(1);       // 5_55 6
        GBS::SP_HS_LOOP_SEL::write(1);      // 5_57_6 | 0 enables retiming on SP | 1 to bypass input to HDBYPASS
        GBS::SP_H_PROTECT::write(0);        // 5_3e 4 disable for H/V
        rto->phaseADC = 16;
        rto->phaseSP = 8;
    } else {
        // todo: SOG SRC can be ADC or HS input pin. HS requires TTL level, ADC can use lower levels
        // HS seems to have issues at low PLL speeds
        // maybe add detection whether ADC Sync is needed
        GBS::SP_SOG_SRC_SEL::write(0);  // 5_20 0 | 0: from ADC 1: hs is sog source
        GBS::SP_EXT_SYNC_SEL::write(1); // disconnect HV input
        GBS::ADC_SOGEN::write(1);       // 5_02 0 ADC SOG
        GBS::SP_SOG_MODE::write(1);     // apparently needs to be off for HS input (on for ADC)
        GBS::SP_NO_COAST_REG::write(0); // vblank coasting on
        GBS::SP_PRE_COAST::write(4);    // 5_38, > 4 can be seen with clamp invert on the lower lines
        GBS::SP_POST_COAST::write(7);
        GBS::SP_SYNC_BYPS::write(0);   // use regular sync for decimator (and sync out) path
        GBS::SP_HS_LOOP_SEL::write(1); // 5_57_6 | 0 enables retiming on SP | 1 to bypass input to HDBYPASS
        GBS::SP_H_PROTECT::write(1);   // 5_3e 4 enable for SOG
        rto->currentLevelSOG = 24;
        rto->phaseADC = 16;
        rto->phaseSP = 8;
    }
    GBS::SP_CLAMP_MANUAL::write(1);  // needs to be 1
    GBS::SP_COAST_INV_REG::write(0); // just in case

    GBS::SP_DIS_SUB_COAST::write(1);   // 5_3e 5
    GBS::SP_HS_PROC_INV_REG::write(0); // 5_56 5
    GBS::SP_VS_PROC_INV_REG::write(0); // 5_56 6
    GBS::PLLAD_KS::write(1);           // 0 - 3
    setOverSampleRatio(2, true);       // prepare only = true
    GBS::DEC_MATRIX_BYPS::write(1);    // 5_1f with adc to dac mode
    GBS::ADC_FLTR::write(0);           // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz

    GBS::PLLAD_ICP::write(4);
    GBS::PLLAD_FS::write(0);    // low gain
    GBS::PLLAD_MD::write(1856); // 1349 perfect for for 1280x+ ; 1856 allows lower res to detect

    // T4R0x2B Bit: 3 (was 0x7) is now: 0xF
    // S0R0x4F (was 0x80) is now: 0xBC
    // 0_43 1a
    // S5R0x2 (was 0x48) is now: 0x54
    // s5s11sb2
    // 0x25, // s0_44
    // 0x11, // s0_45
    // new: do without running default preset first
    GBS::ADC_TA_05_CTRL::write(0x02); // 5_05 1 // minor SOG clamp effect
    GBS::ADC_TEST_04::write(0x02);    // 5_04
    GBS::ADC_TEST_0C::write(0x12);    // 5_0c 1 4
    GBS::DAC_RGBS_R0ENZ::write(1);
    GBS::DAC_RGBS_G0ENZ::write(1);
    GBS::DAC_RGBS_B0ENZ::write(1);
    GBS::OUT_SYNC_CNTRL::write(1);
    // resetPLL();   // try to avoid this
    resetDigital();       // this will leave 0_46 all 0
    resetSyncProcessor(); // required to initialize SOG status
    delay(2);
    ResetSDRAM();
    delay(2);
    resetPLLAD();
    togglePhaseAdjustUnits();
    delay(20);
    GBS::PLLAD_LEN::write(1);        // 5_11 1
    GBS::DAC_RGBS_PWDNZ::write(1);   // enable DAC
    GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out

    // todo: detect if H-PLL parameters fit the source before aligning clocks (5_11 etc)

    setAndLatchPhaseSP(); // different for CSync and pure HV modes
    setAndLatchPhaseADC();
    latchPLLAD();

    if (uopt->enableAutoGain == 1 && adco->r_gain == 0) {
        // _WSN(F("ADC gain: reset"));
        setAdcGain(AUTO_GAIN_INIT);
        GBS::DEC_TEST_ENABLE::write(1);
    } else if (uopt->enableAutoGain == 1 && adco->r_gain != 0) {
        /*_WSN(F("ADC gain: keep previous"));
    _WSF("0x%04X", adco->r_gain); _WS(F(" "));
    _WSF("0x%04X", adco->g_gain); _WS(F(" "));
    _WSF("0x%04X", adco->b_gain); _WSN(F(" "));*/
        GBS::ADC_RGCTRL::write(adco->r_gain);
        GBS::ADC_GGCTRL::write(adco->g_gain);
        GBS::ADC_BGCTRL::write(adco->b_gain);
        GBS::DEC_TEST_ENABLE::write(1);
    } else {
        GBS::DEC_TEST_ENABLE::write(0); // no need for decimation test to be enabled
    }

    // rto->presetID = PresetBypassRGBHV; // bypass flavor 2, used to signal buttons in web ui
    uopt->resolutionID = PresetBypassRGBHV; // bypass flavor 2, used to signal buttons in web ui
    GBS::GBS_PRESET_ID::write(PresetBypassRGBHV);
    delay(200);
}

/**
 * @brief
 *
 */
void runAutoGain()
{
    static unsigned long lastTimeAutoGain = millis();
    uint8_t limit_found = 0, greenValue = 0;
    uint8_t loopCeiling = 0;
    uint8_t status00reg = GBS::STATUS_00::read(); // confirm no mode changes happened

    // GBS::DEC_TEST_SEL::write(5);

    // for (uint8_t i = 0; i < 14; i++) {
    //   uint8_t greenValue = GBS::TEST_BUS_2E::read();
    //   if (greenValue >= 0x28 && greenValue <= 0x2f) {  // 0x2c seems to be "highest" (haven't seen 0x2b yet)
    //     if (getStatus16SpHsStable() && (GBS::STATUS_00::read() == status00reg)) {
    //       limit_found++;
    //     }
    //     else return;
    //   }
    // }

    if ((millis() - lastTimeAutoGain) < 30000) {
        loopCeiling = 61;
    } else {
        loopCeiling = 8;
    }

    for (uint8_t i = 0; i < loopCeiling; i++) {
        if (i % 20 == 0) {
            // wifiLoop(0);
            limit_found = 0;
        }
        greenValue = GBS::TEST_BUS_2F::read();

        if (greenValue == 0x7f) {
            if (getStatus16SpHsStable() && (GBS::STATUS_00::read() == status00reg)) {
                limit_found++;
                // 240p test suite (SNES ver): display vertical lines (hor. line test)
                // _WS("g: "); _WSN(greenValue, HEX);
                // _WS("--"); _WSN();
            } else
                return;

            if (limit_found == 2) {
                limit_found = 0;
                uint8_t level = GBS::ADC_GGCTRL::read();
                if (level < 0xfe) {
                    setAdcGain(level + 2);

                    // remember these gain settings
                    adco->r_gain = GBS::ADC_RGCTRL::read();
                    adco->g_gain = GBS::ADC_GGCTRL::read();
                    adco->b_gain = GBS::ADC_BGCTRL::read();
                    // @sk: suspended
                    // printInfo();
                    // delay(2); // let it settle a little
                    lastTimeAutoGain = millis();
                }
            }
        }
    }
}

/**
 * @brief
 *
 */
void enableScanlines()
{
    if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 0) {
        // _WSN(F("enableScanlines())"));

        // GBS::RFF_ADR_ADD_2::write(0);
        // GBS::RFF_REQ_SEL::write(1);
        // GBS::RFF_MASTER_FLAG::write(0x3f);
        // GBS::RFF_WFF_OFFSET::write(0); // scanline fix
        // GBS::RFF_FETCH_NUM::write(0);
        // GBS::RFF_ENABLE::write(1); //GBS::WFF_ENABLE::write(1);
        // delay(10);
        // GBS::RFF_ENABLE::write(0); //GBS::WFF_ENABLE::write(0);

        // following lines set up UV deinterlacer (on top of normal Y)
        GBS::MADPT_UVDLY_PD_SP::write(0);                       // 2_39 0..3
        GBS::MADPT_UVDLY_PD_ST::write(0);                       // 2_39 4..7
        GBS::MADPT_EN_UV_DEINT::write(1);                       // 2_3a 0
        GBS::MADPT_UV_MI_DET_BYPS::write(1);                    // 2_3a 7 enables 2_3b adjust
        GBS::MADPT_UV_MI_OFFSET::write(uopt->scanlineStrength); // 2_3b offset (mixing factor here)
        GBS::MADPT_MO_ADP_UV_EN::write(1);                      // 2_16 5 (try to do this some other way?)

        GBS::DIAG_BOB_PLDY_RAM_BYPS::write(0); // 2_00 7 enabled, looks better
        GBS::MADPT_PD_RAM_BYPS::write(0);      // 2_24 2
        GBS::RFF_YUV_DEINTERLACE::write(1);    // scanline fix 2
        GBS::MADPT_Y_MI_DET_BYPS::write(1);    // make sure, so that mixing works
        // GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() + 0x30); // more luma gain
        // GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() + 4);
        GBS::VDS_WLEV_GAIN::write(0x08);                       // 3_58
        GBS::VDS_W_LEV_BYPS::write(0);                         // brightness
        GBS::MADPT_VIIR_COEF::write(0x08);                     // 2_27 VIIR filter strength
        GBS::MADPT_Y_MI_OFFSET::write(uopt->scanlineStrength); // 2_0b offset (mixing factor here)
        GBS::MADPT_VIIR_BYPS::write(0);                        // 2_26 6 VIIR line fifo // 1 = off
        GBS::RFF_LINE_FLIP::write(1);                          // clears potential garbage in rff buffer

        GBS::MAPDT_VT_SEL_PRGV::write(0);
        GBS::GBS_OPTION_SCANLINES_ENABLED::write(1);
    }
    rto->scanlinesEnabled = 1;
}

/**
 * @brief
 *
 */
void disableScanlines()
{
    if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
        // _WSN(F("disableScanlines())"));
        GBS::MAPDT_VT_SEL_PRGV::write(1);

        // following lines set up UV deinterlacer (on top of normal Y)
        GBS::MADPT_UVDLY_PD_SP::write(4);    // 2_39 0..3
        GBS::MADPT_UVDLY_PD_ST::write(4);    // 2_39 4..77
        GBS::MADPT_EN_UV_DEINT::write(0);    // 2_3a 0
        GBS::MADPT_UV_MI_DET_BYPS::write(0); // 2_3a 7 enables 2_3b adjust
        GBS::MADPT_UV_MI_OFFSET::write(4);   // 2_3b
        GBS::MADPT_MO_ADP_UV_EN::write(0);   // 2_16 5

        GBS::DIAG_BOB_PLDY_RAM_BYPS::write(1); // 2_00 7
        GBS::VDS_W_LEV_BYPS::write(1);         // brightness
        // GBS::VDS_Y_GAIN::write(GBS::VDS_Y_GAIN::read() - 0x30);
        // GBS::VDS_Y_OFST::write(GBS::VDS_Y_OFST::read() - 4);
        GBS::MADPT_Y_MI_OFFSET::write(0xff); // 2_0b offset 0xff disables mixing
        GBS::MADPT_VIIR_BYPS::write(1);      // 2_26 6 disable VIIR
        GBS::MADPT_PD_RAM_BYPS::write(1);
        GBS::RFF_LINE_FLIP::write(0); // back to default

        GBS::GBS_OPTION_SCANLINES_ENABLED::write(0);
    }
    rto->scanlinesEnabled = 0;
}

/**
 * @brief
 *
 */
void enableMotionAdaptDeinterlace()
{
    freezeVideo();
    GBS::DEINT_00::write(0x19);          // 2_00 // bypass angular (else 0x00)
    GBS::MADPT_Y_MI_OFFSET::write(0x00); // 2_0b  // also used for scanline mixing
    // GBS::MADPT_STILL_NOISE_EST_EN::write(1); // 2_0A 5 (was 0 before)
    GBS::MADPT_Y_MI_DET_BYPS::write(0); // 2_0a_7  // switch to automatic motion indexing
    // GBS::MADPT_UVDLY_PD_BYPS::write(0); // 2_35_5 // UVDLY
    // GBS::MADPT_EN_UV_DEINT::write(0);   // 2_3a 0
    // GBS::MADPT_EN_STILL_FOR_NRD::write(1); // 2_3a 3 (new)

    if (rto->videoStandardInput == 1)
        GBS::MADPT_VTAP2_COEFF::write(6); // 2_19 vertical filter
    if (rto->videoStandardInput == 2)
        GBS::MADPT_VTAP2_COEFF::write(4);

    // GBS::RFF_WFF_STA_ADDR_A::write(0);
    // GBS::RFF_WFF_STA_ADDR_B::write(1);
    GBS::RFF_ADR_ADD_2::write(1);
    GBS::RFF_REQ_SEL::write(3);
    // GBS::RFF_MASTER_FLAG::write(0x24);  // use preset's value
    // GBS::WFF_SAFE_GUARD::write(0); // 4_42 3
    GBS::RFF_FETCH_NUM::write(0x80);    // part of RFF disable fix, could leave 0x80 always otherwise
    GBS::RFF_WFF_OFFSET::write(0x100);  // scanline fix
    GBS::RFF_YUV_DEINTERLACE::write(0); // scanline fix 2
    GBS::WFF_FF_STA_INV::write(0);      // 4_42_2 // 22.03.19 : turned off // update: only required in PAL?
    // GBS::WFF_LINE_FLIP::write(0); // 4_4a_4 // 22.03.19 : turned off // update: only required in PAL?
    GBS::WFF_ENABLE::write(1); // 4_42 0 // enable before RFF
    GBS::RFF_ENABLE::write(1); // 4_4d 7
    // delay(60); // 55 first good
    unfreezeVideo();
    delay(60);
    GBS::MAPDT_VT_SEL_PRGV::write(0); // 2_16_7
    rto->motionAdaptiveDeinterlaceActive = true;
}

/**
 * @brief
 *
 */
void disableMotionAdaptDeinterlace()
{
    GBS::MAPDT_VT_SEL_PRGV::write(1); // 2_16_7
    GBS::DEINT_00::write(0xff);       // 2_00

    GBS::RFF_FETCH_NUM::write(0x1);  // RFF disable fix
    GBS::RFF_WFF_OFFSET::write(0x1); // RFF disable fix
    delay(2);
    GBS::WFF_ENABLE::write(0);
    GBS::RFF_ENABLE::write(0); // can cause mem reset requirement, procedure above should fix it

    // GBS::WFF_ADR_ADD_2::write(0);
    GBS::WFF_FF_STA_INV::write(1); // 22.03.19 : turned off // update: only required in PAL?
    // GBS::WFF_LINE_FLIP::write(1); // 22.03.19 : turned off // update: only required in PAL?
    GBS::MADPT_Y_MI_OFFSET::write(0x7f);
    // GBS::MADPT_STILL_NOISE_EST_EN::write(0); // new
    GBS::MADPT_Y_MI_DET_BYPS::write(1);
    // GBS::MADPT_UVDLY_PD_BYPS::write(1); // 2_35_5
    // GBS::MADPT_EN_UV_DEINT::write(0); // 2_3a 0
    // GBS::MADPT_EN_STILL_FOR_NRD::write(0); // 2_3a 3 (new)
    rto->motionAdaptiveDeinterlaceActive = false;
}

/**
 * @brief
 *
 */
void zeroAll()
{
    // turn processing units off first
    writeOneByte(0xF0, 0);
    writeOneByte(0x46, 0x00); // reset controls 1
    writeOneByte(0x47, 0x00); // reset controls 2

    // zero out entire register space
    for (int y = 0; y < 6; y++) {
        writeOneByte(0xF0, (uint8_t)y);
        for (int z = 0; z < 16; z++) {
            uint8_t bank[16];
            for (int w = 0; w < 16; w++) {
                bank[w] = 0;
                // exceptions
                // if (y == 5 && z == 0 && w == 2) {
                //  bank[w] = 0x51; // 5_02
                //}
                // if (y == 5 && z == 5 && w == 6) {
                //  bank[w] = 0x01; // 5_56
                //}
                // if (y == 5 && z == 5 && w == 7) {
                //  bank[w] = 0xC0; // 5_57
                //}
            }
            writeBytes(z * 16, bank, 16);
        }
    }
}

/**
 * @brief programs all valid registers (the register map has holes in
 *      it, so it's not straight forward) 'index' keeps track of the current
 *      preset data location.
 *
 * @param programArray
 * @param skipMDSection
 */
void writeProgramArrayNew(const uint8_t *programArray, bool skipMDSection)
{
    uint16_t index = 0;
    uint8_t bank[16];
    uint8_t y = 0;

    // GBS::PAD_SYNC_OUT_ENZ::write(1);
    // GBS::DAC_RGBS_PWDNZ::write(0);    // no DAC
    // GBS::SFTRST_MEM_FF_RSTZ::write(0);  // stop mem fifos

    FrameSync::cleanup();

    // should only be possible if previously was in RGBHV bypass, then hit a manual preset switch
    if (rto->videoStandardInput == 15) {
        rto->videoStandardInput = 0;
    }

    rto->outModeHdBypass = 0; // the default at this stage
    if (GBS::ADC_INPUT_SEL::read() == 0) {
        // if (rto->inputIsYPbPr == 0) _WSN(F("rto->inputIsYPbPr was 0, fixing to 1");
        rto->inputIsYPbPr = 1; // new: update the var here, allow manual preset loads
    } else {
        // if (rto->inputIsYPbPr == 1) _WSN(F("rto->inputIsYPbPr was 1, fixing to 0");
        rto->inputIsYPbPr = 0;
    }

    uint8_t reset46 = GBS::RESET_CONTROL_0x46::read(); // for keeping these as they are now
    uint8_t reset47 = GBS::RESET_CONTROL_0x47::read();

    while(y < 6) {
        writeOneByte(0xF0, y);
        switch (y) {
            case 0:
                for (int j = 0; j <= 1; j++) { // 2 times
                    for (int x = 0; x <= 15; x++) {
                        if (j == 0 && x == 4) {
                            // keep DAC off
                            if (rto->useHdmiSyncFix) {
                                bank[x] = pgm_read_byte(programArray + index) & ~(1 << 0);
                            } else {
                                bank[x] = pgm_read_byte(programArray + index);
                            }
                        } else if (j == 0 && x == 6) {
                            bank[x] = reset46;
                        } else if (j == 0 && x == 7) {
                            bank[x] = reset47;
                        } else if (j == 0 && x == 9) {
                            // keep sync output off
                            if (rto->useHdmiSyncFix) {
                                bank[x] = pgm_read_byte(programArray + index) | (1 << 2);
                            } else {
                                bank[x] = pgm_read_byte(programArray + index);
                            }
                        } else {
                            // use preset values
                            bank[x] = pgm_read_byte(programArray + index);
                        }

                        index++;
                    }
                    writeBytes(0x40 + (j * 16), bank, 16);
                }
                copyBank(bank, programArray, &index);
                writeBytes(0x90, bank, 16);
                break;
            case 1:
                for (int j = 0; j <= 2; j++) { // 3 times
                    copyBank(bank, programArray, &index);
                    if (j == 0) {
                        bank[0] = bank[0] & ~(1 << 5); // clear 1_00 5
                        bank[1] = bank[1] | (1 << 0);  // set 1_01 0
                        bank[12] = bank[12] & 0x0f;    // clear 1_0c upper bits
                        bank[13] = 0;                  // clear 1_0d
                    }
                    writeBytes(j * 16, bank, 16);
                }
                if (!skipMDSection) {
                    loadPresetMdSection();
                    if (rto->syncTypeCsync)
                        GBS::MD_SEL_VGA60::write(0); // EDTV possible
                    else
                        GBS::MD_SEL_VGA60::write(1); // VGA 640x480 more likely

                    GBS::MD_HD1250P_CNTRL::write(rto->medResLineCount); // patch med res support
                }
                break;
            case 2:
                loadPresetDeinterlacerSection();
                break;
            case 3:
                for (int j = 0; j <= 7; j++) { // 8 times
                    copyBank(bank, programArray, &index);
                    // if (rto->useHdmiSyncFix) {
                    //   if (j == 0) {
                    //     bank[0] = bank[0] | (1 << 0); // 3_00 0 sync lock
                    //   }
                    //   if (j == 1) {
                    //     bank[10] = bank[10] | (1 << 4); // 3_1a 4 frame lock
                    //   }
                    // }
                    writeBytes(j * 16, bank, 16);
                }
                // blank out VDS PIP registers, otherwise they can end up uninitialized
                for (int x = 0; x <= 15; x++) {
                    writeOneByte(0x80 + x, 0x00);
                }
                break;
            case 4:
                for (int j = 0; j <= 5; j++) { // 6 times
                    copyBank(bank, programArray, &index);
                    writeBytes(j * 16, bank, 16);
                }
                break;
            case 5:
                for (int j = 0; j <= 6; j++) { // 7 times
                    for (int x = 0; x <= 15; x++) {
                        bank[x] = pgm_read_byte(programArray + index);
                        if (index == 322) { // s5_02 bit 6+7 = input selector (only bit 6 is relevant)
                            if (rto->inputIsYPbPr)
                                bitClear(bank[x], 6);
                            else
                                bitSet(bank[x], 6);
                        }
                        if (index == 323) { // s5_03 set clamps according to input channel
                            if (rto->inputIsYPbPr) {
                                bitClear(bank[x], 2); // G bottom clamp
                                bitSet(bank[x], 1);   // R mid clamp
                                bitSet(bank[x], 3);   // B mid clamp
                            } else {
                                bitClear(bank[x], 2); // G bottom clamp
                                bitClear(bank[x], 1); // R bottom clamp
                                bitClear(bank[x], 3); // B bottom clamp
                            }
                        }
                        // if (index == 324) { // s5_04 reset(0) for ADC REF init
                        //   bank[x] = 0x00;
                        // }
                        if (index == 352) { // s5_20 always force to 0x02 (only SP_SOG_P_ATO)
                            bank[x] = 0x02;
                        }
                        if (index == 375) { // s5_37
                            if (videoStandardInputIsPalNtscSd()) {
                                bank[x] = 0x6b;
                            } else {
                                bank[x] = 0x02;
                            }
                        }
                        if (index == 382) {     // s5_3e
                            bitSet(bank[x], 5); // SP_DIS_SUB_COAST = 1
                        }
                        if (index == 407) {     // s5_57
                            bitSet(bank[x], 0); // SP_NO_CLAMP_REG = 1
                        }
                        index++;
                    }
                    writeBytes(j * 16, bank, 16);
                }
                break;
        }
        y++;
    }

    // scaling RGBHV mode
    if (uopt->preferScalingRgbhv && rto->isValidForScalingRGBHV) {
        GBS::GBS_OPTION_SCALING_RGBHV::write(1);
        rto->videoStandardInput = 3;
    }
}

/**
 * @brief
 *
 */
void activeFrameTimeLockInitialSteps()
{
    // skip if using external clock gen
    if (rto->extClockGenDetected) {
        _WSN(F("Active FrameTime Lock enabled, adjusting external clock gen frequency"));
        return;
    }
    // skip when out mode = bypass
    // if (rto->presetID != PresetHdBypass && rto->presetID != PresetBypassRGBHV) {
    if (uopt->resolutionID != PresetHdBypass && uopt->resolutionID != PresetBypassRGBHV) {
        _WS(F("Active FrameTime Lock enabled, disable if display unstable or stays blank! Method: "));
        if (uopt->frameTimeLockMethod == 0) {
            _WSN(F("0 (vtotal + VSST)"));
        }
        if (uopt->frameTimeLockMethod == 1) {
            _WSN(F("1 (vtotal only)"));
        }
        if (GBS::VDS_VS_ST::read() == 0) {
            // VS_ST needs to be at least 1, so method 1 can decrease it when needed (but currently only increases VS_ST)
            // don't force this here, instead make sure to have all presets follow the rule (easier dev)
            _WSN(F("Warning: Check VDS_VS_ST!"));
        }
    }
}

/**
 * @brief
 *
 */
void applyComponentColorMixing()
{
    GBS::VDS_Y_GAIN::write(0x64);    // 3_35
    GBS::VDS_UCOS_GAIN::write(0x19); // 3_36
    GBS::VDS_VCOS_GAIN::write(0x19); // 3_37
    GBS::VDS_Y_OFST::write(0xfe);    // 3_3a
    GBS::VDS_U_OFST::write(0x01);    // 3_3b
}

/**
 * @brief blue only mode: t0t44t1 t0t44t4
 *
 */
void applyYuvPatches()
{
    GBS::ADC_RYSEL_R::write(1);     // midlevel clamp red
    GBS::ADC_RYSEL_B::write(1);     // midlevel clamp blue
    GBS::ADC_RYSEL_G::write(0);     // gnd clamp green
    GBS::DEC_MATRIX_BYPS::write(1); // ADC
    GBS::IF_MATRIX_BYPS::write(1);

    if (GBS::GBS_PRESET_CUSTOM::read() == 0) {
        // colors
        GBS::VDS_Y_GAIN::write(0x80);    // 3_25
        GBS::VDS_UCOS_GAIN::write(0x1c); // 3_26
        GBS::VDS_VCOS_GAIN::write(0x29); // 3_27
        GBS::VDS_Y_OFST::write(0xFE);
        GBS::VDS_U_OFST::write(0x03);
        GBS::VDS_V_OFST::write(0x03);
        if (rto->videoStandardInput >= 5 && rto->videoStandardInput <= 7) {
            // todo: Rec. 709 (vs Rec. 601 used normally
            // needs this on VDS and HDBypass
        }
    }

    // if using ADC auto offset for yuv input / rgb output
    // GBS::ADC_AUTO_OFST_EN::write(1);
    // GBS::VDS_U_OFST::write(0x36);     //3_3b
    // GBS::VDS_V_OFST::write(0x4d);     //3_3c

    if (uopt->wantOutputComponent) {
        applyComponentColorMixing();
    }
}

/**
 * @brief
 *
 */
void OutputComponentOrVGA()
{

    // TODO replace with rto->isCustomPreset?
    bool isCustomPreset = GBS::GBS_PRESET_CUSTOM::read();
    if (uopt->wantOutputComponent) {
        _WSN(F("Output Format: Component"));
        GBS::VDS_SYNC_LEV::write(0x80); // 0.25Vpp sync (leave more room for Y)
        GBS::VDS_CONVT_BYPS::write(1);  // output YUV
        GBS::OUT_SYNC_CNTRL::write(0);  // no H / V sync out to PAD
    } else {
        GBS::VDS_SYNC_LEV::write(0);
        GBS::VDS_CONVT_BYPS::write(0); // output RGB
        GBS::OUT_SYNC_CNTRL::write(1); // H / V sync out enable
    }

    if (!isCustomPreset) {
        if (rto->inputIsYPbPr == true) {
            applyYuvPatches();
        } else {
            applyRGBPatches();
        }
    }
}

/**
 * @brief
 *
 */
void toggleIfAutoOffset()
{
    if (GBS::IF_AUTO_OFST_EN::read() == 0) {
        // and different ADC offsets
        GBS::ADC_ROFCTRL::write(0x40);
        GBS::ADC_GOFCTRL::write(0x42);
        GBS::ADC_BOFCTRL::write(0x40);
        // enable
        GBS::IF_AUTO_OFST_EN::write(1);
        GBS::IF_AUTO_OFST_PRD::write(0); // 0 = by line, 1 = by frame
    } else {
        if (adco->r_off != 0 && adco->g_off != 0 && adco->b_off != 0) {
            GBS::ADC_ROFCTRL::write(adco->r_off);
            GBS::ADC_GOFCTRL::write(adco->g_off);
            GBS::ADC_BOFCTRL::write(adco->b_off);
        }
        // adco->r_off = 0: auto calibration on boot failed, leave at current values
        GBS::IF_AUTO_OFST_EN::write(0);
        GBS::IF_AUTO_OFST_PRD::write(0); // 0 = by line, 1 = by frame
    }
}

/**
 * @brief Set the Adc Parameters Gain And Offset object
 *
 */
void setAdcParametersGainAndOffset()
{
    GBS::ADC_ROFCTRL::write(0x40);
    GBS::ADC_GOFCTRL::write(0x40);
    GBS::ADC_BOFCTRL::write(0x40);

    // Do not call setAdcGain() and overwrite adco->r_gain. This function should
    // only be called during startup, or during doPostPresetLoadSteps(), and if
    // `uopt->enableAutoGain == 1` then doPostPresetLoadSteps() will revert
    // these writes with `adco->r_gain`.
    GBS::ADC_RGCTRL::write(0x7B);
    GBS::ADC_GGCTRL::write(0x7B);
    GBS::ADC_BGCTRL::write(0x7B);
}

/**
 * @brief
 *
 */
void updateHVSyncEdge()
{
    static uint8_t printHS = 0, printVS = 0;
    uint16_t temp = 0;

    if (GBS::STATUS_INT_SOG_BAD::read() == 1) {
        resetInterruptSogBadBit();
        return;
    }

    uint8_t syncStatus = GBS::STATUS_16::read();
    if (rto->syncTypeCsync) {
        // sog check, only check H
        if ((syncStatus & 0x02) != 0x02)
            return;
    } else {
        // HV check, check H + V
        if ((syncStatus & 0x0a) != 0x0a)
            return;
    }

    if ((syncStatus & 0x02) != 0x02) // if !hs active
    {
        // _WSN(F("(SP) can't detect sync edge"));
    } else {
        if ((syncStatus & 0x01) == 0x00) {
            if (printHS != 1) {
                _WSN(F("(SP) HS active low"));
            }
            printHS = 1;

            temp = GBS::HD_HS_SP::read();
            if (GBS::HD_HS_ST::read() < temp) { // if out sync = ST < SP
                GBS::HD_HS_SP::write(GBS::HD_HS_ST::read());
                GBS::HD_HS_ST::write(temp);
                GBS::SP_HS2PLL_INV_REG::write(1);
                // GBS::SP_SOG_P_INV::write(0); // 5_20 2 //could also use 5_20 1 "SP_SOG_P_ATO"
            }
        } else {
            if (printHS != 2) {
                _WSN(F("(SP) HS active high"));
            }
            printHS = 2;

            temp = GBS::HD_HS_SP::read();
            if (GBS::HD_HS_ST::read() > temp) { // if out sync = ST > SP
                GBS::HD_HS_SP::write(GBS::HD_HS_ST::read());
                GBS::HD_HS_ST::write(temp);
                GBS::SP_HS2PLL_INV_REG::write(0);
                // GBS::SP_SOG_P_INV::write(1); // 5_20 2 //could also use 5_20 1 "SP_SOG_P_ATO"
            }
        }

        // VS check, but only necessary for separate sync (CS should have VS always active low)
        if (rto->syncTypeCsync == false) {
            if ((syncStatus & 0x08) != 0x08) // if !vs active
            {
                _WSN(F("VS can't detect sync edge"));
            } else {
                if ((syncStatus & 0x04) == 0x00) {
                    if (printVS != 1) {
                        _WSN(F("(SP) VS active low"));
                    }
                    printVS = 1;

                    temp = GBS::HD_VS_SP::read();
                    if (GBS::HD_VS_ST::read() < temp) { // if out sync = ST < SP
                        GBS::HD_VS_SP::write(GBS::HD_VS_ST::read());
                        GBS::HD_VS_ST::write(temp);
                    }
                } else {
                    if (printVS != 2) {
                        _WSN(F("(SP) VS active high"));
                    }
                    printVS = 2;

                    temp = GBS::HD_VS_SP::read();
                    if (GBS::HD_VS_ST::read() > temp) { // if out sync = ST > SP
                        GBS::HD_VS_SP::write(GBS::HD_VS_ST::read());
                        GBS::HD_VS_ST::write(temp);
                    }
                }
            }
        }
    }
}

/**
 * @brief Generally, the ADC has to stay enabled to perform SOG separation and
 *      thus "see" a source. It is possible to run in low power mode. Function
 *      should not further nest, so it can be called in syncwatcher
 *
 */
void goLowPowerWithInputDetection()
{
    // in operation: t5t04t1 for 10% lower power on ADC
    // also: s0s40s1c for 5% (lower memclock of 108mhz)
    // for some reason: t0t45t2 t0t45t4 (enable SDAC, output max voltage) 5% lower  done in presets
    // t0t4ft4 clock out should be off
    // s4s01s20 (was 30) faster latency // unstable at 108mhz
    // both phase controls off saves some power 506ma > 493ma
    // oversample ratio can save 10% at 1x
    // t3t24t3 VDS_TAP6_BYPS on can save 2%
    GBS::OUT_SYNC_CNTRL::write(0); // no H / V sync out to PAD
    GBS::DAC_RGBS_PWDNZ::write(0); // direct disableDAC()
    // zeroAll();
    _DBGN(F("(!) reset runtime parameters while going LowPower"));
    setResetParameters(); // includes rto->videoStandardInput = 0
    prepareSyncProcessor();
    delay(100);
    rto->isInLowPowerMode = true;
    _DBGN(F("Scanning inputs for sources ..."));
    LEDOFF;
}

/**
 * @brief
 *
 */
void optimizeSogLevel()
{
    if (rto->boardHasPower == false) // checkBoardPower is too invasive now
    {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 13;
        return;
    }
    if (rto->videoStandardInput == 15 || GBS::SP_SOG_MODE::read() != 1 || rto->syncTypeCsync == false) {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 13;
        return;
    }

    if (rto->inputIsYPbPr) {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 14;
    } else {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 13; // similar to yuv, allow variations
    }
    setAndUpdateSogLevel(rto->currentLevelSOG);

    uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
    uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
    if (debug_backup != 0xa) {
        GBS::TEST_BUS_SEL::write(0xa);
        delay(1);
    }
    if (debug_backup_SP != 0x0f) {
        GBS::TEST_BUS_SP_SEL::write(0x0f);
        delay(1);
    }

    GBS::TEST_BUS_EN::write(1);

    delay(100);
    while (1) {
        uint16_t syncGoodCounter = 0;
        unsigned long timeout = millis();
        while ((millis() - timeout) < 60) {
            if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
                syncGoodCounter++;
                if (syncGoodCounter >= 60) {
                    break;
                }
            } else if (syncGoodCounter >= 4) {
                syncGoodCounter -= 3;
            }
        }

        if (syncGoodCounter >= 60) {
            syncGoodCounter = 0;
            // if (getVideoMode() != 0)
            if (GBS::TEST_BUS_2F::read() > 0) {
                delay(20);
                for (int a = 0; a < 50; a++) {
                    syncGoodCounter++;
                    if (GBS::STATUS_SYNC_PROC_HSACT::read() == 0 || GBS::TEST_BUS_2F::read() == 0) {
                        syncGoodCounter = 0;
                        break;
                    }
                }
                if (syncGoodCounter >= 49) {
                    // _WS(F("OK @SOG ")); _WSN(rto->currentLevelSOG); printInfo();
                    break; // found, exit
                } else {
                    // _WS(F(" inner test failed syncGoodCounter: "); _WSN(syncGoodCounter));
                }
            } else { // getVideoMode() == 0
                     // _WS(F("sog-- syncGoodCounter: "); _WSN(syncGoodCounter));
            }
        } else { // syncGoodCounter < 40
                 // _WS(F("outer test failed syncGoodCounter: ")); _WSN(syncGoodCounter);
        }

        if (rto->currentLevelSOG >= 2) {
            rto->currentLevelSOG -= 1;
            setAndUpdateSogLevel(rto->currentLevelSOG);
            delay(8); // time for sog to settle
        } else {
            rto->currentLevelSOG = 13; // leave at default level
            setAndUpdateSogLevel(rto->currentLevelSOG);
            delay(8);
            break; // break and exit
        }
    }

    rto->thisSourceMaxLevelSOG = rto->currentLevelSOG;
    if (rto->thisSourceMaxLevelSOG == 0) {
        rto->thisSourceMaxLevelSOG = 1; // fail safe
    }

    if (debug_backup != 0xa) {
        GBS::TEST_BUS_SEL::write(debug_backup);
    }
    if (debug_backup_SP != 0x0f) {
        GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
    }
}

/**
 * @brief
 *
 */
void resetModeDetect()
{
    GBS::SFTRST_MODE_RSTZ::write(0);
    delay(1); // needed
    GBS::SFTRST_MODE_RSTZ::write(1);
    // rto->clampPositionIsSet = false;
    // rto->coastPositionIsSet = false;
}

/**
 * @brief
 *
 * @param amountToShift
 * @param subtracting
 */
void shiftHorizontal(uint16_t amountToShift, bool subtracting)
{
    uint16_t hrst = GBS::VDS_HSYNC_RST::read();
    uint16_t hbst = GBS::VDS_HB_ST::read();
    uint16_t hbsp = GBS::VDS_HB_SP::read();

    // Perform the addition/subtraction
    if (subtracting) {
        if ((int16_t)hbst - amountToShift >= 0) {
            hbst -= amountToShift;
        } else {
            hbst = hrst - (amountToShift - hbst);
        }
        if ((int16_t)hbsp - amountToShift >= 0) {
            hbsp -= amountToShift;
        } else {
            hbsp = hrst - (amountToShift - hbsp);
        }
    } else {
        if ((int16_t)hbst + amountToShift <= hrst) {
            hbst += amountToShift;
            // also extend hbst_d to maximum hrst-1
            if (hbst > GBS::VDS_DIS_HB_ST::read()) {
                GBS::VDS_DIS_HB_ST::write(hbst);
            }
        } else {
            hbst = 0 + (amountToShift - (hrst - hbst));
        }
        if ((int16_t)hbsp + amountToShift <= hrst) {
            hbsp += amountToShift;
        } else {
            hbsp = 0 + (amountToShift - (hrst - hbsp));
        }
    }

    GBS::VDS_HB_ST::write(hbst);
    GBS::VDS_HB_SP::write(hbsp);
    // _WS("hbst: "); _WSN(hbst);
    // _WS("hbsp: "); _WSN(hbsp);
}

/**
 * @brief
 *
 */
void shiftHorizontalLeft()
{
    shiftHorizontal(4, true);
}

/**
 * @brief
 *
 */
void shiftHorizontalRight()
{
    shiftHorizontal(4, false);
}

/**
 * @brief unused but may become useful
 *
 * @param amount
 */
// void shiftHorizontalLeftIF(uint8_t amount)
// {
//     uint16_t IF_HB_ST2 = GBS::IF_HB_ST2::read() + amount;
//     uint16_t IF_HB_SP2 = GBS::IF_HB_SP2::read() + amount;
//     uint16_t PLLAD_MD = GBS::PLLAD_MD::read();

//     if (rto->videoStandardInput <= 2) {
//         GBS::IF_HSYNC_RST::write(PLLAD_MD / 2); // input line length from pll div
//     } else if (rto->videoStandardInput <= 7) {
//         GBS::IF_HSYNC_RST::write(PLLAD_MD);
//     }
//     uint16_t IF_HSYNC_RST = GBS::IF_HSYNC_RST::read();

//     GBS::IF_LINE_SP::write(IF_HSYNC_RST + 1);

//     // start
//     if (IF_HB_ST2 < IF_HSYNC_RST) {
//         GBS::IF_HB_ST2::write(IF_HB_ST2);
//     } else {
//         GBS::IF_HB_ST2::write(IF_HB_ST2 - IF_HSYNC_RST);
//     }
//     // _WS(F("IF_HB_ST2:  ")); _WSN(GBS::IF_HB_ST2::read());

//     // stop
//     if (IF_HB_SP2 < IF_HSYNC_RST) {
//         GBS::IF_HB_SP2::write(IF_HB_SP2);
//     } else {
//         GBS::IF_HB_SP2::write((IF_HB_SP2 - IF_HSYNC_RST) + 1);
//     }
//     // _WS(F("IF_HB_SP2:  ")); _WSN(GBS::IF_HB_SP2::read());
// }

/**
 * @brief unused but may become useful
 *
 * @param amount
 */
// void shiftHorizontalRightIF(uint8_t amount)
// {
//     int16_t IF_HB_ST2 = GBS::IF_HB_ST2::read() - amount;
//     int16_t IF_HB_SP2 = GBS::IF_HB_SP2::read() - amount;
//     uint16_t PLLAD_MD = GBS::PLLAD_MD::read();

//     if (rto->videoStandardInput <= 2) {
//         GBS::IF_HSYNC_RST::write(PLLAD_MD / 2); // input line length from pll div
//     } else if (rto->videoStandardInput <= 7) {
//         GBS::IF_HSYNC_RST::write(PLLAD_MD);
//     }
//     int16_t IF_HSYNC_RST = GBS::IF_HSYNC_RST::read();

//     GBS::IF_LINE_SP::write(IF_HSYNC_RST + 1);

//     if (IF_HB_ST2 > 0) {
//         GBS::IF_HB_ST2::write(IF_HB_ST2);
//     } else {
//         GBS::IF_HB_ST2::write(IF_HSYNC_RST - 1);
//     }
//     // _WS(F("IF_HB_ST2:  ")); _WSN(GBS::IF_HB_ST2::read());

//     if (IF_HB_SP2 > 0) {
//         GBS::IF_HB_SP2::write(IF_HB_SP2);
//     } else {
//         GBS::IF_HB_SP2::write(IF_HSYNC_RST - 1);
//         // GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() - 2);
//     }
//     // _WS(F("IF_HB_SP2:  ")); _WSN(GBS::IF_HB_SP2::read());
// }

/**
 * @brief Output video control horizontal scale serial commands (h, z)
 *
 * @param amountToScale
 * @param subtracting
 */
void scaleHorizontal(uint16_t amountToScale, bool subtracting)
{
    uint16_t hscale = GBS::VDS_HSCALE::read();

    // smooth out areas of interest
    if (subtracting && (hscale == 513 || hscale == 512))
        amountToScale = 1;
    if (!subtracting && (hscale == 511 || hscale == 512))
        amountToScale = 1;

    if (subtracting && (((int)hscale - amountToScale) <= 256)) {
        hscale = 256;
        GBS::VDS_HSCALE::write(hscale);
        _WSN(F("limit"));
        return;
    }

    if (subtracting && (hscale - amountToScale > 255)) {
        hscale -= amountToScale;
    } else if (hscale + amountToScale < 1023) {
        hscale += amountToScale;
    } else if (hscale + amountToScale == 1023) { // exact max > bypass but apply VDS fetch changes
        hscale = 1023;
        GBS::VDS_HSCALE::write(hscale);
        GBS::VDS_HSCALE_BYPS::write(1);
    } else if (hscale + amountToScale > 1023) { // max + overshoot > bypass and no VDS fetch adjust
        hscale = 1023;
        GBS::VDS_HSCALE::write(hscale);
        GBS::VDS_HSCALE_BYPS::write(1);
        _WSN(F("limit"));
        return;
    }

    // will be scaling
    GBS::VDS_HSCALE_BYPS::write(0);

    // move within VDS VB fetch area (within reason)
    uint16_t htotal = GBS::VDS_HSYNC_RST::read();
    uint16_t toShift = 0;
    if (hscale < 540)
        toShift = 4;
    else if (hscale < 640)
        toShift = 3;
    else
        toShift = 2;

    if (subtracting) {
        shiftHorizontal(toShift, true);
        if ((GBS::VDS_HB_ST::read() + 5) < GBS::VDS_DIS_HB_ST::read()) {
            GBS::VDS_HB_ST::write(GBS::VDS_HB_ST::read() + 5);
        } else if ((GBS::VDS_DIS_HB_ST::read() + 5) < htotal) {
            GBS::VDS_DIS_HB_ST::write(GBS::VDS_DIS_HB_ST::read() + 5);
            GBS::VDS_HB_ST::write(GBS::VDS_DIS_HB_ST::read()); // dis_hbst = hbst
        }

        // fix HB_ST > HB_SP conditions
        if (GBS::VDS_HB_SP::read() < (GBS::VDS_HB_ST::read() + 16)) { // HB_SP < HB_ST
            if ((GBS::VDS_HB_SP::read()) > (htotal / 2)) {            // but HB_SP > some small value
                GBS::VDS_HB_ST::write(GBS::VDS_HB_SP::read() - 16);
            }
        }
    }

    // !subtracting check just for readability
    if (!subtracting) {
        shiftHorizontal(toShift, false);
        if ((GBS::VDS_HB_ST::read() - 5) > 0) {
            GBS::VDS_HB_ST::write(GBS::VDS_HB_ST::read() - 5);
        }
    }

    // fix scaling < 512 glitch: factor even, htotal even: hbst / hbsp should be even, etc
    if (hscale < 512) {
        if (hscale % 2 == 0) { // hscale 512 / even
            if (GBS::VDS_HB_ST::read() % 2 == 1) {
                GBS::VDS_HB_ST::write(GBS::VDS_HB_ST::read() + 1);
            }
            if (htotal % 2 == 1) {
                if (GBS::VDS_HB_SP::read() % 2 == 0) {
                    GBS::VDS_HB_SP::write(GBS::VDS_HB_SP::read() - 1);
                }
            } else {
                if (GBS::VDS_HB_SP::read() % 2 == 1) {
                    GBS::VDS_HB_SP::write(GBS::VDS_HB_SP::read() - 1);
                }
            }
        } else { // hscale 499 / uneven
            if (GBS::VDS_HB_ST::read() % 2 == 1) {
                GBS::VDS_HB_ST::write(GBS::VDS_HB_ST::read() + 1);
            }
            if (htotal % 2 == 0) {
                if (GBS::VDS_HB_SP::read() % 2 == 1) {
                    GBS::VDS_HB_SP::write(GBS::VDS_HB_SP::read() - 1);
                }
            } else {
                if (GBS::VDS_HB_SP::read() % 2 == 0) {
                    GBS::VDS_HB_SP::write(GBS::VDS_HB_SP::read() - 1);
                }
            }
        }
        // if scaling was < 512 and HB_ST moved, align with VDS_DIS_HB_ST
        if (GBS::VDS_DIS_HB_ST::read() < GBS::VDS_HB_ST::read()) {
            GBS::VDS_DIS_HB_ST::write(GBS::VDS_HB_ST::read());
        }
    }

    // _WS(F("HB_ST: ")); _WSN(GBS::VDS_HB_ST::read());
    // _WS(F("HB_SP: ")); _WSN(GBS::VDS_HB_SP::read());
    _WSF(PSTR("HScale: %d\n"), hscale);
    GBS::VDS_HSCALE::write(hscale);
}

/**
 * @brief moves horizontal sync (VDS or HDB as needed)
 *
 * @param amountToAdd
 * @param subtracting
 */
void moveHS(uint16_t amountToAdd, bool subtracting)
{
    if (rto->outModeHdBypass) {
        uint16_t SP_CS_HS_ST = GBS::SP_CS_HS_ST::read();
        uint16_t SP_CS_HS_SP = GBS::SP_CS_HS_SP::read();
        uint16_t htotal = GBS::HD_HSYNC_RST::read();

        if (videoStandardInputIsPalNtscSd()) {
            htotal -= 8; // account for the way hdbypass is setup in setOutModeHdBypass()
            htotal *= 2;
        }

        if (htotal == 0)
            return; // safety
        int16_t amount = subtracting ? (0 - amountToAdd) : amountToAdd;

        if (SP_CS_HS_ST + amount < 0) {
            SP_CS_HS_ST = htotal + SP_CS_HS_ST; // yep, this works :p
        }
        if (SP_CS_HS_SP + amount < 0) {
            SP_CS_HS_SP = htotal + SP_CS_HS_SP;
        }

        GBS::SP_CS_HS_ST::write((SP_CS_HS_ST + amount) % htotal);
        GBS::SP_CS_HS_SP::write((SP_CS_HS_SP + amount) % htotal);

        _WSF(PSTR("HSST: %d HSSP: %d\n"), GBS::SP_CS_HS_ST::read(), GBS::SP_CS_HS_SP::read());
    } else {
        uint16_t VDS_HS_ST = GBS::VDS_HS_ST::read();
        uint16_t VDS_HS_SP = GBS::VDS_HS_SP::read();
        uint16_t htotal = GBS::VDS_HSYNC_RST::read();

        if (htotal == 0)
            return; // safety
        int16_t amount = subtracting ? (0 - amountToAdd) : amountToAdd;

        if (VDS_HS_ST + amount < 0) {
            VDS_HS_ST = htotal + VDS_HS_ST; // yep, this works :p
        }
        if (VDS_HS_SP + amount < 0) {
            VDS_HS_SP = htotal + VDS_HS_SP;
        }

        GBS::VDS_HS_ST::write((VDS_HS_ST + amount) % htotal);
        GBS::VDS_HS_SP::write((VDS_HS_SP + amount) % htotal);

        // _WS(F("HSST: ")); _WS(GBS::VDS_HS_ST::read());
        // _WS(F(" HSSP: ")); _WSN(GBS::VDS_HS_SP::read());
    }
    printVideoTimings();
}

/**
 * @brief
 *
 * @param amountToAdd
 * @param subtracting
 */
// void moveVS(uint16_t amountToAdd, bool subtracting)
// {
//     uint16_t vtotal = GBS::VDS_VSYNC_RST::read();
//     if (vtotal == 0)
//         return; // safety
//     uint16_t VDS_DIS_VB_ST = GBS::VDS_DIS_VB_ST::read();
//     uint16_t newVDS_VS_ST = GBS::VDS_VS_ST::read();
//     uint16_t newVDS_VS_SP = GBS::VDS_VS_SP::read();

//     if (subtracting) {
//         if ((newVDS_VS_ST - amountToAdd) > VDS_DIS_VB_ST) {
//             newVDS_VS_ST -= amountToAdd;
//             newVDS_VS_SP -= amountToAdd;
//         } else
//             _WSN(F("limit"));
//     } else {
//         if ((newVDS_VS_SP + amountToAdd) < vtotal) {
//             newVDS_VS_ST += amountToAdd;
//             newVDS_VS_SP += amountToAdd;
//         } else
//             _WSN(F("limit"));
//     }
//     // _WS(F("VSST: ")); _WS(newVDS_VS_ST);
//     // _WS(F(" VSSP: ")); _WSN(newVDS_VS_SP);

//     GBS::VDS_VS_ST::write(newVDS_VS_ST);
//     GBS::VDS_VS_SP::write(newVDS_VS_SP);
// }

/**
 * @brief
 *
 */
void invertHS()
{
    uint8_t high, low;
    uint16_t newST, newSP;

    writeOneByte(0xf0, 3);
    readFromRegister(0x0a, 1, &low);
    readFromRegister(0x0b, 1, &high);
    newST = ((((uint16_t)high) & 0x000f) << 8) | (uint16_t)low;
    readFromRegister(0x0b, 1, &low);
    readFromRegister(0x0c, 1, &high);
    newSP = ((((uint16_t)high) & 0x00ff) << 4) | ((((uint16_t)low) & 0x00f0) >> 4);

    uint16_t temp = newST;
    newST = newSP;
    newSP = temp;

    writeOneByte(0x0a, (uint8_t)(newST & 0x00ff));
    writeOneByte(0x0b, ((uint8_t)(newSP & 0x000f) << 4) | ((uint8_t)((newST & 0x0f00) >> 8)));
    writeOneByte(0x0c, (uint8_t)((newSP & 0x0ff0) >> 4));
}

/**
 * @brief
 *
 */
void invertVS()
{
    uint8_t high, low;
    uint16_t newST, newSP;

    writeOneByte(0xf0, 3);
    readFromRegister(0x0d, 1, &low);
    readFromRegister(0x0e, 1, &high);
    newST = ((((uint16_t)high) & 0x000f) << 8) | (uint16_t)low;
    readFromRegister(0x0e, 1, &low);
    readFromRegister(0x0f, 1, &high);
    newSP = ((((uint16_t)high) & 0x00ff) << 4) | ((((uint16_t)low) & 0x00f0) >> 4);

    uint16_t temp = newST;
    newST = newSP;
    newSP = temp;

    writeOneByte(0x0d, (uint8_t)(newST & 0x00ff));
    writeOneByte(0x0e, ((uint8_t)(newSP & 0x000f) << 4) | ((uint8_t)((newST & 0x0f00) >> 8)));
    writeOneByte(0x0f, (uint8_t)((newSP & 0x0ff0) >> 4));
}

/**
 * @brief
 *
 * @param amountToScale
 * @param subtracting
 */
void scaleVertical(uint16_t amountToScale, bool subtracting)
{
    uint16_t vscale = GBS::VDS_VSCALE::read();

    // least invasive "is vscaling enabled" check
    if (vscale == 1023) {
        GBS::VDS_VSCALE_BYPS::write(0);
    }

    // smooth out areas of interest
    if (subtracting && (vscale == 513 || vscale == 512))
        amountToScale = 1;
    if (subtracting && (vscale == 684 || vscale == 683))
        amountToScale = 1;
    if (!subtracting && (vscale == 511 || vscale == 512))
        amountToScale = 1;
    if (!subtracting && (vscale == 682 || vscale == 683))
        amountToScale = 1;

    if (subtracting && (vscale - amountToScale > 128)) {
        vscale -= amountToScale;
    } else if (subtracting) {
        vscale = 128;
    } else if (vscale + amountToScale <= 1023) {
        vscale += amountToScale;
    } else if (vscale + amountToScale > 1023) {
        vscale = 1023;
        // don't enable vscale bypass here, since that disables ie line filter
    }

    _WSF(PSTR("VScale: %d\n"), vscale);
    GBS::VDS_VSCALE::write(vscale);
}

/**
 * @brief modified to move VBSP, set VBST to VBSP-2
 *
 * @param amountToAdd
 * @param subtracting
 */
void shiftVertical(uint16_t amountToAdd, bool subtracting)
{
    typedef GBS::Tie<GBS::VDS_VB_ST, GBS::VDS_VB_SP> Regs;
    uint16_t vrst = GBS::VDS_VSYNC_RST::read() - FrameSync::getSyncLastCorrection();
    uint16_t vbst = 0, vbsp = 0;
    int16_t newVbst = 0, newVbsp = 0;

    Regs::read(vbst, vbsp);
    newVbst = vbst;
    newVbsp = vbsp;

    if (subtracting) {
        // newVbst -= amountToAdd;
        newVbsp -= amountToAdd;
    } else {
        // newVbst += amountToAdd;
        newVbsp += amountToAdd;
    }

    // handle the case where hbst or hbsp have been decremented below 0
    if (newVbst < 0) {
        newVbst = vrst + newVbst;
    }
    if (newVbsp < 0) {
        newVbsp = vrst + newVbsp;
    }

    // handle the case where vbst or vbsp have been incremented above vrstValue
    if (newVbst > (int16_t)vrst) {
        newVbst = newVbst - vrst;
    }
    if (newVbsp > (int16_t)vrst) {
        newVbsp = newVbsp - vrst;
    }

    // mod: newVbsp needs to be at least newVbst+2
    if (newVbsp < (newVbst + 2)) {
        newVbsp = newVbst + 2;
    }
    // mod: -= 2
    newVbst = newVbsp - 2;

    Regs::write(newVbst, newVbsp);
    // _WS(F("VSST: ")); _WS(newVbst); _WS(F(" VSSP: ")); _WSN(newVbsp);
}

/**
 * @brief
 *
 */
// void shiftVerticalUp()
// {
//     shiftVertical(1, true);
// }

/**
 * @brief
 *
 */
// void shiftVerticalDown()
// {
//     shiftVertical(1, false);
// }

/**
 * @brief
 *
 */
void shiftVerticalUpIF()
{
    // -4 to allow variance in source line count
    uint8_t offset = rto->videoStandardInput == 2 ? 4 : 1;
    uint16_t sourceLines = GBS::VPERIOD_IF::read() - offset;
    // add an override for sourceLines, in case where the IF data is not available
    if ((GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) && rto->videoStandardInput == 14) {
        sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
    }
    int16_t stop = GBS::IF_VB_SP::read();
    int16_t start = GBS::IF_VB_ST::read();

    if (stop < (sourceLines - 1) && start < (sourceLines - 1)) {
        stop += 2;
        start += 2;
    } else {
        start = 0;
        stop = 2;
    }
    GBS::IF_VB_SP::write(stop);
    GBS::IF_VB_ST::write(start);
}

/**
 * @brief
 *
 */
void shiftVerticalDownIF()
{
    uint8_t offset = rto->videoStandardInput == 2 ? 4 : 1;
    uint16_t sourceLines = GBS::VPERIOD_IF::read() - offset;
    // add an override for sourceLines, in case where the IF data is not available
    if ((GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) && rto->videoStandardInput == 14) {
        sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
    }

    int16_t stop = GBS::IF_VB_SP::read();
    int16_t start = GBS::IF_VB_ST::read();

    if (stop > 1 && start > 1) {
        stop -= 2;
        start -= 2;
    } else {
        start = sourceLines - 2;
        stop = sourceLines;
    }
    GBS::IF_VB_SP::write(stop);
    GBS::IF_VB_ST::write(start);
}

/**
 * @brief
 *
 * @param value
 */
void setHSyncStartPosition(uint16_t value)
{
    if (rto->outModeHdBypass) {
        // GBS::HD_HS_ST::write(value);
        GBS::SP_CS_HS_ST::write(value);
    } else {
        GBS::VDS_HS_ST::write(value);
    }
}

/**
 * @brief
 *
 * @param value
 */
void setHSyncStopPosition(uint16_t value)
{
    if (rto->outModeHdBypass) {
        // GBS::HD_HS_SP::write(value);
        GBS::SP_CS_HS_SP::write(value);
    } else {
        GBS::VDS_HS_SP::write(value);
    }
}

/**
 * @brief Set the Memory Hblank Start Position object
 *
 * @param value
 */
void setMemoryHblankStartPosition(uint16_t value)
{
    GBS::VDS_HB_ST::write(value);
    GBS::HD_HB_ST::write(value);
}

/**
 * @brief Set the Memory Hblank Stop Position object
 *
 * @param value
 */
void setMemoryHblankStopPosition(uint16_t value)
{
    GBS::VDS_HB_SP::write(value);
    GBS::HD_HB_SP::write(value);
}

/**
 * @brief Set the Display Hblank Start Position object
 *
 * @param value
 */
void setDisplayHblankStartPosition(uint16_t value)
{
    GBS::VDS_DIS_HB_ST::write(value);
}

/**
 * @brief Set the Display Hblank Stop Position object
 *
 * @param value
 */
void setDisplayHblankStopPosition(uint16_t value)
{
    GBS::VDS_DIS_HB_SP::write(value);
}

/**
 * @brief
 *
 * @param value
 */
void setVSyncStartPosition(uint16_t value)
{
    GBS::VDS_VS_ST::write(value);
    GBS::HD_VS_ST::write(value);
}

/**
 * @brief
 *
 * @param value
 */
void setVSyncStopPosition(uint16_t value)
{
    GBS::VDS_VS_SP::write(value);
    GBS::HD_VS_SP::write(value);
}

/**
 * @brief Set the Memory Vblank Start Position object
 *
 * @param value
 */
void setMemoryVblankStartPosition(uint16_t value)
{
    GBS::VDS_VB_ST::write(value);
    GBS::HD_VB_ST::write(value);
}

/**
 * @brief Set the Memory Vblank Stop Position object
 *
 * @param value
 */
void setMemoryVblankStopPosition(uint16_t value)
{
    GBS::VDS_VB_SP::write(value);
    GBS::HD_VB_SP::write(value);
}

/**
 * @brief Set the Display Vblank Start Position object
 *
 * @param value
 */
void setDisplayVblankStartPosition(uint16_t value)
{
    GBS::VDS_DIS_VB_ST::write(value);
}

/**
 * @brief Set the Display Vblank Stop Position object
 *
 * @param value
 */
void setDisplayVblankStopPosition(uint16_t value)
{
    GBS::VDS_DIS_VB_SP::write(value);
}

/**
 * @brief Get the Cs Vs Start object
 *
 * @return uint16_t
 */
uint16_t getCsVsStart()
{
    return (GBS::SP_SDCS_VSST_REG_H::read() << 8) + GBS::SP_SDCS_VSST_REG_L::read();
}

/**
 * @brief Get the Cs Vs Stop object
 *
 * @return uint16_t
 */
uint16_t getCsVsStop()
{
    return (GBS::SP_SDCS_VSSP_REG_H::read() << 8) + GBS::SP_SDCS_VSSP_REG_L::read();
}

/**
 * @brief Set the htotal object
 *
 * @param htotal
 */
// void set_htotal(uint16_t htotal)
// {
//     // ModeLine "1280x960" 108.00 1280 1376 1488 1800 960 961 964 1000 +HSync +VSync
//     // front porch: H2 - H1: 1376 - 1280
//     // back porch : H4 - H3: 1800 - 1488
//     // sync pulse : H3 - H2: 1488 - 1376

//     uint16_t h_blank_display_start_position = htotal - 1;
//     uint16_t h_blank_display_stop_position = htotal - ((htotal * 3) / 4);
//     uint16_t center_blank = ((h_blank_display_stop_position / 2) * 3) / 4; // a bit to the left
//     uint16_t h_sync_start_position = center_blank - (center_blank / 2);
//     uint16_t h_sync_stop_position = center_blank + (center_blank / 2);
//     uint16_t h_blank_memory_start_position = h_blank_display_start_position - 1;
//     uint16_t h_blank_memory_stop_position = h_blank_display_stop_position - (h_blank_display_stop_position / 50);

//     GBS::VDS_HSYNC_RST::write(htotal);
//     GBS::VDS_HS_ST::write(h_sync_start_position);
//     GBS::VDS_HS_SP::write(h_sync_stop_position);
//     GBS::VDS_DIS_HB_ST::write(h_blank_display_start_position);
//     GBS::VDS_DIS_HB_SP::write(h_blank_display_stop_position);
//     GBS::VDS_HB_ST::write(h_blank_memory_start_position);
//     GBS::VDS_HB_SP::write(h_blank_memory_stop_position);
// }

/**
 * @brief Set the vtotal object
 *
 * @param vtotal
 */
void set_vtotal(uint16_t vtotal)
{
    uint16_t VDS_DIS_VB_ST = vtotal - 2;                         // just below vtotal
    uint16_t VDS_DIS_VB_SP = (vtotal >> 6) + 8;                  // positive, above new sync stop position
    uint16_t VDS_VB_ST = ((uint16_t)(vtotal * 0.016f)) & 0xfffe; // small fraction of vtotal
    uint16_t VDS_VB_SP = VDS_VB_ST + 2;                          // always VB_ST + 2
    uint16_t v_sync_start_position = 1;
    uint16_t v_sync_stop_position = 5;
    // most low line count formats have negative sync!
    // exception: 1024x768 (1344x806 total) has both sync neg. // also 1360x768 (1792x795 total)
    if ((vtotal < 530) || (vtotal >= 803 && vtotal <= 809) || (vtotal >= 793 && vtotal <= 798)) {
        uint16_t temp = v_sync_start_position;
        v_sync_start_position = v_sync_stop_position;
        v_sync_stop_position = temp;
    }

    GBS::VDS_VSYNC_RST::write(vtotal);
    GBS::VDS_VS_ST::write(v_sync_start_position);
    GBS::VDS_VS_SP::write(v_sync_stop_position);
    GBS::VDS_VB_ST::write(VDS_VB_ST);
    GBS::VDS_VB_SP::write(VDS_VB_SP);
    GBS::VDS_DIS_VB_ST::write(VDS_DIS_VB_ST);
    GBS::VDS_DIS_VB_SP::write(VDS_DIS_VB_SP);

    // VDS_VSYN_SIZE1 + VDS_VSYN_SIZE2 to VDS_VSYNC_RST + 2
    GBS::VDS_VSYN_SIZE1::write(GBS::VDS_VSYNC_RST::read() + 2);
    GBS::VDS_VSYN_SIZE2::write(GBS::VDS_VSYNC_RST::read() + 2);
}

/**
 * @brief Set the If Hblank Parameters object
 *
 */
// void setIfHblankParameters()
// {
//     if (!rto->outModeHdBypass) {
//         uint16_t pll_divider = GBS::PLLAD_MD::read();

//         // if line doubling (PAL, NTSC), div 2 + a couple pixels
//         GBS::IF_HSYNC_RST::write(((pll_divider >> 1) + 13) & 0xfffe); // 1_0e
//         GBS::IF_LINE_SP::write(GBS::IF_HSYNC_RST::read() + 1);        // 1_22
//         if (rto->presetID == 0x05) {
//             // override for 1080p manually for now (pll_divider alone isn't correct :/)
//             GBS::IF_HSYNC_RST::write(GBS::IF_HSYNC_RST::read() + 32);
//             GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() + 32);
//         }
//         if (rto->presetID == 0x15) {
//             GBS::IF_HSYNC_RST::write(GBS::IF_HSYNC_RST::read() + 20);
//             GBS::IF_LINE_SP::write(GBS::IF_LINE_SP::read() + 20);
//         }

//         if (GBS::IF_LD_RAM_BYPS::read()) {
//             // no LD = EDTV or similar
//             GBS::IF_HB_SP2::write((uint16_t)((float)pll_divider * 0.06512f) & 0xfffe); // 1_1a // 0.06512f
//             // pll_divider / 2 - 3 is minimum IF_HB_ST2
//             GBS::IF_HB_ST2::write((uint16_t)((float)pll_divider * 0.4912f) & 0xfffe); // 1_18
//         } else {
//             // LD mode (PAL, NTSC)
//             GBS::IF_HB_SP2::write(4 + ((uint16_t)((float)pll_divider * 0.0224f) & 0xfffe)); // 1_1a
//             GBS::IF_HB_ST2::write((uint16_t)((float)pll_divider * 0.4550f) & 0xfffe);       // 1_18

//             if (GBS::IF_HB_ST2::read() >= 0x420) {
//                 // limit (fifo?) (0x420 = 1056) (might be 0x424 instead)
//                 // limit doesn't apply to EDTV modes, where 1_18 typically = 0x4B0
//                 GBS::IF_HB_ST2::write(0x420);
//             }

//             if (rto->presetID == 0x05 || rto->presetID == 0x15) {
//                 // override 1_1a for 1080p manually for now (pll_divider alone isn't correct :/)
//                 GBS::IF_HB_SP2::write(0x2A);
//             }

//             // position move via 1_26 and reserve for deinterlacer: add IF RST pixels
//             // seems no extra pixels available at around PLLAD:84A or 2122px
//             // uint16_t currentRst = GBS::IF_HSYNC_RST::read();
//             // GBS::IF_HSYNC_RST::write((currentRst + (currentRst / 15)) & 0xfffe);  // 1_0e
//             // GBS::IF_LINE_SP::write(GBS::IF_HSYNC_RST::read() + 1);                // 1_22
//         }
//     }
// }

//
/**
 * @brief Get the Sync Present object
 *      if testbus has 0x05, sync is present and line counting active. if it has 0x04,
 *      sync is present but no line counting
 * @return true
 * @return false
 */
bool getSyncPresent()
{
    uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
    uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
    if (debug_backup != 0xa) {
        GBS::TEST_BUS_SEL::write(0xa);
    }
    if (debug_backup_SP != 0x0f) {
        GBS::TEST_BUS_SP_SEL::write(0x0f);
    }

    uint16_t readout = GBS::TEST_BUS::read();

    if (debug_backup != 0xa) {
        GBS::TEST_BUS_SEL::write(debug_backup);
    }
    if (debug_backup_SP != 0x0f) {
        GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
    }
    // if (((readout & 0x0500) == 0x0500) || ((readout & 0x0500) == 0x0400)) {
    if (readout > 0x0180) {
        return true;
    }

    return false;
}

/**
 * @brief Get the Status00 If Hs Vs Stable object
 *          returns 0_00 bit 2 = H+V both stable (for the IF, not SP)
 * @return true
 * @return false
 */
// bool getStatus00IfHsVsStable()
// {
//     return ((GBS::STATUS_00::read() & 0x04) == 0x04) ? 1 : 0;
// }

/**
 * @brief
 *
 */
void advancePhase()
{
    rto->phaseADC = (rto->phaseADC + 1) & 0x1f;
    setAndLatchPhaseADC();
}

/**
 * @brief
 *
 */
// void movePhaseThroughRange()
// {
//     for (uint8_t i = 0; i < 128; i++) { // 4x for 4x oversampling?
//         advancePhase();
//     }
// }

/**
 * @brief
 *
 * @param autoCoast
 */
void updateCoastPosition(bool autoCoast)
{
    if (((rto->videoStandardInput == 0) || (rto->videoStandardInput > 14)) ||
        !rto->boardHasPower || rto->sourceDisconnected) {
        return;
    }

    uint32_t accInHlength = 0;
    uint16_t prevInHlength = GBS::HPERIOD_IF::read();
    for (uint8_t i = 0; i < 8; i++) {
        // psx jitters between 427, 428
        uint16_t thisInHlength = GBS::HPERIOD_IF::read();
        if ((thisInHlength > (prevInHlength - 3)) && (thisInHlength < (prevInHlength + 3))) {
            accInHlength += thisInHlength;
        } else {
            return;
        }
        if (!getStatus16SpHsStable()) {
            return;
        }

        prevInHlength = thisInHlength;
    }
    accInHlength = (accInHlength * 4) / 8;

    // 30.09.19 new: especially in low res VGA input modes, it can clip at "511 * 4 = 2044"
    // limit to more likely actual value of 430
    if (accInHlength >= 2040) {
        accInHlength = 1716;
    }

    if (accInHlength <= 240) {
        // check for low res, low Hz > can overflow HPERIOD_IF
        if (GBS::STATUS_SYNC_PROC_VTOTAL::read() <= 322) {
            delay(4);
            if (GBS::STATUS_SYNC_PROC_VTOTAL::read() <= 322) {
                _WSN(F(" (debug) updateCoastPosition: low res, low hz"));
                accInHlength = 2000;
                // usually need to lower charge pump. todo: write better check
                if (rto->syncTypeCsync && rto->videoStandardInput > 0 && rto->videoStandardInput <= 4) {
                    if (GBS::PLLAD_ICP::read() >= 5 && GBS::PLLAD_FS::read() == 1) {
                        GBS::PLLAD_ICP::write(5);
                        GBS::PLLAD_FS::write(0);
                        latchPLLAD();
                        rto->phaseIsSet = 0;
                    }
                }
            }
        }
    }

    // accInHlength around 1732 here / NTSC
    // scope on sync-in, enable 5_3e 2 > shows coast borders
    if (accInHlength > 32) {
        if (autoCoast) {
            // autoCoast (5_55 7 = on)
            GBS::SP_H_CST_ST::write((uint16_t)(accInHlength * 0.0562f)); // ~0x61, right after color burst
            GBS::SP_H_CST_SP::write((uint16_t)(accInHlength * 0.1550f)); // ~0x10C, shortly before sync falling edge
            GBS::SP_HCST_AUTO_EN::write(1);
        } else {
            // test: psx pal black license screen, then ntsc SMPTE color bars 100%; or MS
            // scope test psx: t5t11t3, 5_3e = 0x01, 5_36/5_35 = 0x30 5_37 = 0x10 :
            // cst sp should be > 0x62b to clean out HS from eq pulses
            // cst st should be 0, sp should be 0x69f when t5t57t7 = disabled
            // GBS::SP_H_CST_ST::write((uint16_t)(accInHlength * 0.088f)); // ~0x9a, leave some room for SNES 239 mode
            // new: with SP_H_PROTECT disabled, even SNES can be a small value. Small value greatly improves Mega Drive
            GBS::SP_H_CST_ST::write(0x10);

            GBS::SP_H_CST_SP::write((uint16_t)(accInHlength * 0.968f)); // ~0x678
            // need a bit earlier, making 5_3e 2 more stable
            // GBS::SP_H_CST_SP::write((uint16_t)(accInHlength * 0.7383f));  // ~0x4f0, before sync
            GBS::SP_HCST_AUTO_EN::write(0);
        }
        rto->coastPositionIsSet = 1;

        /*_WS("coast ST: "); _WS("0x"); _WSF("0x%04X", GBS::SP_H_CST_ST::read());
    _WS(", ");
    _WS("SP: "); _WS("0x"); _WSF("0x%04X", GBS::SP_H_CST_SP::read());
    _WS("  total: "); _WS("0x"); _WSF("0x%04X", accInHlength);
    _WS(" ~ "); _WSN(accInHlength / 4);*/
    }
}

/**
 * @brief
 *
 */
void updateClampPosition()
{
    if ((rto->videoStandardInput == 0) || !rto->boardHasPower || rto->sourceDisconnected) {
        return;
    }
    // this is required especially on mode changes with ypbpr
    if (getVideoMode() == 0) {
        return;
    }

    if (rto->inputIsYPbPr) {
        GBS::SP_CLAMP_MANUAL::write(0);
    } else {
        GBS::SP_CLAMP_MANUAL::write(1); // no auto clamp for RGB
    }

    // STATUS_SYNC_PROC_HTOTAL is "ht: " value; use with SP_CLP_SRC_SEL = 1 pixel clock
    // GBS::HPERIOD_IF::read()  is "h: " value; use with SP_CLP_SRC_SEL = 0 osc clock
    // update: in RGBHV bypass it seems both clamp source modes use pixel clock for calculation
    // but with sog modes, it uses HPERIOD_IF ... k
    // update2: if the clamp is already short, yet creeps into active video, check sog invert (t5t20t2)
    uint32_t accInHlength = 0;
    uint16_t prevInHlength = 0;
    uint16_t thisInHlength = 0;
    if (rto->syncTypeCsync)
        prevInHlength = GBS::HPERIOD_IF::read();
    else
        prevInHlength = GBS::STATUS_SYNC_PROC_HTOTAL::read();
    for (uint8_t i = 0; i < 16; i++) {
        if (rto->syncTypeCsync)
            thisInHlength = GBS::HPERIOD_IF::read();
        else
            thisInHlength = GBS::STATUS_SYNC_PROC_HTOTAL::read();
        if ((thisInHlength > (prevInHlength - 3)) && (thisInHlength < (prevInHlength + 3))) {
            accInHlength += thisInHlength;
        } else {
            // _WSN("updateClampPosition unstable");
            return;
        }
        if (!getStatus16SpHsStable()) {
            return;
        }

        prevInHlength = thisInHlength;
        delayMicroseconds(100);
    }
    accInHlength = accInHlength / 16; // for the 16x loop

    // HPERIOD_IF: 9 bits (0-511, represents actual scanline time / 4, can overflow to low values)
    // if it overflows, the calculated clamp positions are likely around 1 to 4. good enough
    // STATUS_SYNC_PROC_HTOTAL: 12 bits (0-4095)
    if (accInHlength > 4095) {
        return;
    }

    uint16_t oldClampST = GBS::SP_CS_CLP_ST::read();
    uint16_t oldClampSP = GBS::SP_CS_CLP_SP::read();
    float multiSt = rto->syncTypeCsync == 1 ? 0.032f : 0.010f;
    float multiSp = rto->syncTypeCsync == 1 ? 0.174f : 0.058f;
    uint16_t start = 1 + (accInHlength * multiSt); // HPERIOD_IF: *0.04 seems good
    uint16_t stop = 2 + (accInHlength * multiSp);  // HPERIOD_IF: *0.178 starts to creep into some EDTV modes

    if (rto->inputIsYPbPr) {
        // YUV: // ST shift forward to pass blacker than black HSync, sog: min * 0.08
        multiSt = rto->syncTypeCsync == 1 ? 0.089f : 0.032f;
        start = 1 + (accInHlength * multiSt);

        // new: HDBypass rewrite to sync to falling HS edge: move clamp position forward
        // RGB can stay the same for now (clamp will start on sync pulse, a benefit in RGB
        if (rto->outModeHdBypass) {
            if (videoStandardInputIsPalNtscSd()) {
                start += 0x60;
                stop += 0x60;
            }
            // raise blank level a bit that's not handled 100% by clamping
            GBS::HD_BLK_GY_DATA::write(0x05);
            GBS::HD_BLK_BU_DATA::write(0x00);
            GBS::HD_BLK_RV_DATA::write(0x00);
        }
    }

    if ((start < (oldClampST - 1) || start > (oldClampST + 1)) ||
        (stop < (oldClampSP - 1) || stop > (oldClampSP + 1))) {
        GBS::SP_CS_CLP_ST::write(start);
        GBS::SP_CS_CLP_SP::write(stop);
        /*_WS("clamp ST: "); _WS("0x"); _WS(start, HEX);
    _WS(", ");
    _WS("SP: "); _WS("0x"); _WS(stop, HEX);
    _WS("   total: "); _WS("0x"); _WS(accInHlength, HEX);
    _WS(" / "); _WSN(accInHlength);*/
    }

    rto->clampPositionIsSet = true;
}

/**
 * @brief
 *
 */
// void fastSogAdjust()
// {
//     if (rto->noSyncCounter <= 5) {
//         uint8_t debug_backup = GBS::TEST_BUS_SEL::read();
//         uint8_t debug_backup_SP = GBS::TEST_BUS_SP_SEL::read();
//         if (debug_backup != 0xa) {
//             GBS::TEST_BUS_SEL::write(0xa);
//         }
//         if (debug_backup_SP != 0x0f) {
//             GBS::TEST_BUS_SP_SEL::write(0x0f);
//         }

//         if ((GBS::TEST_BUS_2F::read() & 0x05) != 0x05) {
//             while ((GBS::TEST_BUS_2F::read() & 0x05) != 0x05) {
//                 if (rto->currentLevelSOG >= 4) {
//                     rto->currentLevelSOG -= 2;
//                 } else {
//                     rto->currentLevelSOG = 13;
//                     setAndUpdateSogLevel(rto->currentLevelSOG);
//                     delay(40);
//                     break; // abort / restart next round
//                 }
//                 setAndUpdateSogLevel(rto->currentLevelSOG);
//                 delay(28); // 4
//             }
//             delay(10);
//         }

//         if (debug_backup != 0xa) {
//             GBS::TEST_BUS_SEL::write(debug_backup);
//         }
//         if (debug_backup_SP != 0x0f) {
//             GBS::TEST_BUS_SP_SEL::write(debug_backup_SP);
//         }
//     }
// }

/**
 * @brief Get the Pll Rate object
 *      used for RGBHV to determine the ADPLL speed "level" / can jitter with SOG Sync
 * @return uint32_t
 */
uint32_t getPllRate()
{
    uint32_t esp8266_clock_freq = ESP.getCpuFreqMHz() * 1000000;
    uint8_t testBusSelBackup = GBS::TEST_BUS_SEL::read();
    uint8_t spBusSelBackup = GBS::TEST_BUS_SP_SEL::read();
    uint8_t debugPinBackup = GBS::PAD_BOUT_EN::read();

    if (testBusSelBackup != 0xa) {
        GBS::TEST_BUS_SEL::write(0xa);
    }
    if (rto->syncTypeCsync) {
        if (spBusSelBackup != 0x6b)
            GBS::TEST_BUS_SP_SEL::write(0x6b);
    } else {
        if (spBusSelBackup != 0x09)
            GBS::TEST_BUS_SP_SEL::write(0x09);
    }
    GBS::PAD_BOUT_EN::write(1); // enable output to pin for test
    yield();                    // BOUT signal and wifi
    delayMicroseconds(200);
    uint32_t ticks = FrameSync::getPulseTicks();

    // restore
    GBS::PAD_BOUT_EN::write(debugPinBackup);
    if (testBusSelBackup != 0xa) {
        GBS::TEST_BUS_SEL::write(testBusSelBackup);
    }
    GBS::TEST_BUS_SP_SEL::write(spBusSelBackup);

    uint32_t retVal = 0;
    if (ticks > 0) {
        retVal = esp8266_clock_freq / ticks;
    }

    return retVal;
}

/**
 * @brief
 *
 */
void runSyncWatcher()
{
    if (!rto->boardHasPower) {
        return;
    }

    static uint8_t newVideoModeCounter = 0;
    static uint16_t activeStableLineCount = 0;
    static unsigned long lastSyncDrop = millis();
    static unsigned long lastLineCountMeasure = millis();

    uint16_t thisStableLineCount = 0;
    uint8_t detectedVideoMode = getVideoMode();
    bool status16SpHsStable = getStatus16SpHsStable();

    if (rto->outModeHdBypass && status16SpHsStable) {
        if (videoStandardInputIsPalNtscSd()) {
            if (millis() - lastLineCountMeasure > 765) {
                thisStableLineCount = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                for (uint8_t i = 0; i < 3; i++) {
                    delay(2);
                    if (GBS::STATUS_SYNC_PROC_VTOTAL::read() < (thisStableLineCount - 3) ||
                        GBS::STATUS_SYNC_PROC_VTOTAL::read() > (thisStableLineCount + 3)) {
                        thisStableLineCount = 0;
                        break;
                    }
                }

                if (thisStableLineCount != 0) {
                    if (thisStableLineCount < (activeStableLineCount - 3) ||
                        thisStableLineCount > (activeStableLineCount + 3)) {
                        activeStableLineCount = thisStableLineCount;
                        if (activeStableLineCount < 230 || activeStableLineCount > 340) {
                            // only doing NTSC/PAL currently, an unusual line count probably means a format change
                            setCsVsStart(1);
                            if (getCsVsStop() == 1) {
                                setCsVsStop(2);
                            }
                            // MD likes to get stuck as usual
                            nudgeMD();
                        } else {
                            setCsVsStart(thisStableLineCount - 9);
                        }

                        _DBGF(PSTR("HDBypass CsVsSt: %d\n"), getCsVsStart());
                        delay(150);
                    }
                }

                lastLineCountMeasure = millis();
            }
        }
    }

    if (rto->videoStandardInput == 13) { // using flaky graphic modes
        if (detectedVideoMode == 0) {
            if (GBS::STATUS_INT_SOG_BAD::read() == 0) {
                detectedVideoMode = 13; // then keep it
            }
        }
    }

    static unsigned long preemptiveSogWindowStart = millis();
    static const uint16_t sogWindowLen = 3000; // ms
    static uint16_t badHsActive = 0;
    static bool lastAdjustWasInActiveWindow = 0;

    if (rto->syncTypeCsync && !rto->inputIsYPbPr && (newVideoModeCounter == 0)) {
        // look for SOG instability
        if (GBS::STATUS_INT_SOG_BAD::read() == 1 || GBS::STATUS_INT_SOG_SW::read() == 1) {
            resetInterruptSogSwitchBit();
            if ((millis() - preemptiveSogWindowStart) > sogWindowLen) {
                // start new window
                preemptiveSogWindowStart = millis();
                badHsActive = 0;
            }
            lastVsyncLock = millis(); // best reset this
        }

        if ((millis() - preemptiveSogWindowStart) < sogWindowLen) {
            for (uint8_t i = 0; i < 16; i++) {
                if (GBS::STATUS_INT_SOG_BAD::read() == 1 || GBS::STATUS_SYNC_PROC_HSACT::read() == 0) {
                    resetInterruptSogBadBit();
                    uint16_t hlowStart = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
                    if (rto->videoStandardInput == 0)
                        hlowStart = 777; // fix initial state no HLOW_LEN
                    for (int a = 0; a < 20; a++) {
                        if (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() != hlowStart) {
                            // okay, source still active so count this one, break back to outer for loop
                            badHsActive++;
                            lastVsyncLock = millis(); // delay this
                            // _WS(badHsActive); _WS(" ");
                            break;
                        }
                    }
                }
                if ((i % 3) == 0) {
                    delay(1);
                } else {
                    delay(0);
                }
            }

            if (badHsActive >= 17) {
                if (rto->currentLevelSOG >= 2) {
                    rto->currentLevelSOG -= 1;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                    delay(30);
                    updateSpDynamic(0);
                    badHsActive = 0;
                    lastAdjustWasInActiveWindow = 1;
                } else if (badHsActive > 40) {
                    optimizeSogLevel();
                    badHsActive = 0;
                    lastAdjustWasInActiveWindow = 1;
                }
                preemptiveSogWindowStart = millis(); // restart window
            }
        } else if (lastAdjustWasInActiveWindow) {
            lastAdjustWasInActiveWindow = 0;
            if (rto->currentLevelSOG >= 8) {
                rto->currentLevelSOG -= 1;
                setAndUpdateSogLevel(rto->currentLevelSOG);
                delay(30);
                updateSpDynamic(0);
                badHsActive = 0;
                rto->phaseIsSet = 0;
            }
        }
    }

    if ((detectedVideoMode == 0 || !status16SpHsStable) && rto->videoStandardInput != 15) {
        rto->noSyncCounter++;
        rto->continousStableCounter = 0;
        lastVsyncLock = millis(); // best reset this
        if (rto->noSyncCounter == 1) {
            freezeVideo();
            return; // do nothing else
        }

        rto->phaseIsSet = 0;

        if (rto->noSyncCounter <= 3 || GBS::STATUS_SYNC_PROC_HSACT::read() == 0) {
            freezeVideo();
        }

        if (newVideoModeCounter == 0) {
            LEDOFF; // LEDOFF on sync loss

            if (rto->noSyncCounter == 2) { // this usually repeats
                // printInfo(); printInfo(); _WSN();
                // rto->printInfos = 0;
                if ((millis() - lastSyncDrop) > 1500) { // minimum space between runs
                    if (rto->printInfos == false) {
                        _WS(F("."));
                    }
                } else {
                    if (rto->printInfos == false) {
                        _WS(F("."));
                    }
                }

                // if sog is lowest, adjust up
                if (rto->currentLevelSOG <= 1 && videoStandardInputIsPalNtscSd()) {
                    rto->currentLevelSOG += 1;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                    delay(30);
                }
                lastSyncDrop = millis(); // restart timer
            }
        }

        if (rto->noSyncCounter == 8) {
            GBS::SP_H_CST_ST::write(0x10);
            GBS::SP_H_CST_SP::write(0x100);
            // GBS::SP_H_PROTECT::write(1);  // at noSyncCounter = 32 will alternate on / off
            if (videoStandardInputIsPalNtscSd()) {
                // this can early detect mode changes (before updateSpDynamic resets all)
                GBS::SP_PRE_COAST::write(9);
                GBS::SP_POST_COAST::write(9);
                // new: test SD<>EDTV changes
                uint8_t ignore = GBS::SP_H_PULSE_IGNOR::read();
                if (ignore >= 0x33) {
                    GBS::SP_H_PULSE_IGNOR::write(ignore / 2);
                }
            }
            rto->coastPositionIsSet = 0;
        }

        if (rto->noSyncCounter % 27 == 0) {
            // the * check needs to be first (go before auto sog level) to support SD > HDTV detection
            _WS(F("*"));
            updateSpDynamic(1);
        }

        if (rto->noSyncCounter % 32 == 0) {
            if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
                unfreezeVideo();
            } else {
                freezeVideo();
            }
        }

        if (rto->inputIsYPbPr && (rto->noSyncCounter == 34)) {
            GBS::SP_NO_CLAMP_REG::write(1); // unlock clamp
            rto->clampPositionIsSet = false;
        }

        if (rto->noSyncCounter == 38) {
            nudgeMD();
        }

        if (rto->syncTypeCsync) {
            if (rto->noSyncCounter > 47) {
                if (rto->noSyncCounter % 16 == 0) {
                    GBS::SP_H_PROTECT::write(!GBS::SP_H_PROTECT::read());
                }
            }
        }

        if (rto->noSyncCounter % 150 == 0) {
            if (rto->noSyncCounter == 150 || rto->noSyncCounter % 900 == 0) {
                _WS(F("\nno signal\n"));
                // check whether discrete VSync is present. if so, need to go to input detect
                uint8_t extSyncBackup = GBS::SP_EXT_SYNC_SEL::read();
                GBS::SP_EXT_SYNC_SEL::write(0);
                delay(240);
    _DBGN(F("printInfo from runSyncWatcher"));
                printInfo();
                if (GBS::STATUS_SYNC_PROC_VSACT::read() == 1) {
                    delay(10);
                    if (GBS::STATUS_SYNC_PROC_VSACT::read() == 1) {
                        rto->noSyncCounter = 0x07fe;
                    }
                }
                GBS::SP_EXT_SYNC_SEL::write(extSyncBackup);
            }
            GBS::SP_H_COAST::write(0);   // 5_3e 2
            GBS::SP_H_PROTECT::write(0); // 5_3e 4
            GBS::SP_H_CST_ST::write(0x10);
            GBS::SP_H_CST_SP::write(0x100); // instead of disabling 5_3e 5 coast
            GBS::SP_CS_CLP_ST::write(32);   // neutral clamp values
            GBS::SP_CS_CLP_SP::write(48);   //
            updateSpDynamic(1);
            nudgeMD(); // can fix MD not noticing a line count update
            delay(80);

            // prepare optimizeSogLevel
            // use STATUS_SYNC_PROC_HLOW_LEN changes to determine whether source is still active
            uint16_t hlowStart = GBS::STATUS_SYNC_PROC_HLOW_LEN::read();
            if (GBS::PLLAD_VCORST::read() == 1) {
                // exception: we're in startup and pllad isn't locked yet > HLOW_LEN always 0
                hlowStart = 777; // now it'll run optimizeSogLevel if needed
            }
            for (int a = 0; a < 128; a++) {
                if (GBS::STATUS_SYNC_PROC_HLOW_LEN::read() != hlowStart) {
                    // source still there
                    if (rto->noSyncCounter % 450 == 0) {
                        rto->currentLevelSOG = 0; // worst case, sometimes necessary, will be unstable but at least detect
                        setAndUpdateSogLevel(rto->currentLevelSOG);
                    } else {
                        optimizeSogLevel();
                    }
                    break;
                } else if (a == 127) {
                    // set sog to be able to see something
                    rto->currentLevelSOG = 5;
                    setAndUpdateSogLevel(rto->currentLevelSOG);
                }
                delay(0);
            }

            resetSyncProcessor();
            delay(8);
            resetModeDetect();
            delay(8);
        }

        // long no signal time, check other input
        if (rto->noSyncCounter % 413 == 0) {
            if (GBS::ADC_INPUT_SEL::read() == 1) {
                GBS::ADC_INPUT_SEL::write(0);
            } else {
                GBS::ADC_INPUT_SEL::write(1);
            }
            delay(40);
            unsigned long timeout = millis();
            while (millis() - timeout <= 210) {
                if (getStatus16SpHsStable()) {
                    rto->noSyncCounter = 0x07fe; // will cause a return
                    break;
                }
                // wifiLoop(0);
                delay(1);
            }

            if (millis() - timeout > 210) {
                if (GBS::ADC_INPUT_SEL::read() == 1) {
                    GBS::ADC_INPUT_SEL::write(0);
                } else {
                    GBS::ADC_INPUT_SEL::write(1);
                }
            }
        }

        newVideoModeCounter = 0;
        // sog unstable check end
    }

    // if format changed to valid, potentially new video mode
    if (((detectedVideoMode != 0 && detectedVideoMode != rto->videoStandardInput) ||
        (detectedVideoMode != 0 && rto->videoStandardInput == 0)) &&
        rto->videoStandardInput != 15)
    {
        // before thoroughly checking for a mode change, watch format via newVideoModeCounter
        if (newVideoModeCounter < 255) {
            newVideoModeCounter++;
            rto->continousStableCounter = 0; // usually already 0, but occasionally not
            if (newVideoModeCounter > 1) {   // help debug a few commits worth
                // if (newVideoModeCounter == 2) {
                //     _WSN();
                // }
                _WSF(PSTR("video mode counter: %d\n"), newVideoModeCounter);
            }
            if (newVideoModeCounter == 3) {
                freezeVideo();
                GBS::SP_H_CST_ST::write(0x10);
                GBS::SP_H_CST_SP::write(0x100);
                rto->coastPositionIsSet = 0;
                delay(10);
                if (getVideoMode() == 0) {
                    updateSpDynamic(1); // check ntsc to 480p and back
                    delay(40);
                }
            }
        }

        if (newVideoModeCounter >= 8) {
            uint8_t vidModeReadout = 0;
            _WS(F("\nFormat change:"));
            for (int a = 0; a < 30; a++) {
                vidModeReadout = getVideoMode();
                if (vidModeReadout == 13) {
                    newVideoModeCounter = 5;
                } // treat ps2 quasi rgb as stable
                if (vidModeReadout != detectedVideoMode) {
                    newVideoModeCounter = 0;
                }
            }
            if (newVideoModeCounter != 0) {
                // apply new mode
                _WSF(PSTR(" %d <stable> (%d -> %d)\n"), vidModeReadout, rto->videoStandardInput, detectedVideoMode);
                // _WS("Old: "); _WS(rto->videoStandardInput);
                // _WS(" New: "); _WSN(detectedVideoMode);
                rto->videoIsFrozen = false;

                if (GBS::SP_SOG_MODE::read() == 1) {
                    rto->syncTypeCsync = true;
                } else {
                    rto->syncTypeCsync = false;
                }
                // bool wantPassThroughMode = uopt->presetPreference == 10;
                bool wantPassThroughMode = uopt->resolutionID == OutputBypass;

                if (((rto->videoStandardInput == 1 || rto->videoStandardInput == 3) && (detectedVideoMode == 2 || detectedVideoMode == 4)) ||
                    rto->videoStandardInput == 0 ||
                    ((rto->videoStandardInput == 2 || rto->videoStandardInput == 4) && (detectedVideoMode == 1 || detectedVideoMode == 3))) {
                    rto->useHdmiSyncFix = 1;
                    // _WSN(F("hdmi sync fix: yes"));
                } else {
                    rto->useHdmiSyncFix = 0;
                    // _WSN(F("hdmi sync fix: no"));
                }

                if (!wantPassThroughMode) {
                    // needs to know the sync type for early updateclamp (set above)
                    applyPresets(detectedVideoMode);
                } else {
                    rto->videoStandardInput = detectedVideoMode;
                    setOutModeHdBypass(false);
                }
                rto->videoStandardInput = detectedVideoMode;
                rto->noSyncCounter = 0;
                rto->continousStableCounter = 0; // also in postloadsteps
                newVideoModeCounter = 0;
                activeStableLineCount = 0;
                delay(20); // post delay
                badHsActive = 0;
                preemptiveSogWindowStart = millis();
            } else {
                unfreezeVideo(); // (whops)
                _WSF(PSTR(" %d <not stable>\n"), vidModeReadout);
                printInfo();
                newVideoModeCounter = 0;
                if (rto->videoStandardInput == 0) {
                    // if we got here from standby mode, return there soon
                    // but occasionally, this is a regular new mode that needs a SP parameter change to work
                    // ie: 1080p needs longer post coast, which the syncwatcher loop applies at some point
                    rto->noSyncCounter = 0x05ff; // give some time in normal loop
                }
            }
        }
    } else if (getStatus16SpHsStable() && detectedVideoMode != 0 && rto->videoStandardInput != 15 && (rto->videoStandardInput == detectedVideoMode)) {
        // last used mode reappeared / stable again
        if (rto->continousStableCounter < 255) {
            rto->continousStableCounter++;
        }

        static bool doFullRestore = 0;
        if (rto->noSyncCounter >= 150) {
            // source was gone for longer // clamp will be updated at continousStableCounter 50
            rto->coastPositionIsSet = false;
            rto->phaseIsSet = false;
            FrameSync::reset(uopt->frameTimeLockMethod);
            doFullRestore = 1;
            _WSN();
        }

        rto->noSyncCounter = 0;
        newVideoModeCounter = 0;

        if (rto->continousStableCounter == 1 && !doFullRestore) {
            rto->videoIsFrozen = true; // ensures unfreeze
            unfreezeVideo();
        }

        if (rto->continousStableCounter == 2) {
            updateSpDynamic(0);
            if (doFullRestore) {
                delay(20);
                optimizeSogLevel();
                doFullRestore = 0;
            }
            rto->videoIsFrozen = true; // ensures unfreeze
            unfreezeVideo();           // called 2nd time here to make sure
        }

        if (rto->continousStableCounter == 4) {
            LEDON;
        }

        if (!rto->phaseIsSet) {
            if (rto->continousStableCounter >= 10 && rto->continousStableCounter < 61) {
                // added < 61 to make a window, else sources with little pll lock hammer this
                if ((rto->continousStableCounter % 10) == 0) {
                    rto->phaseIsSet = optimizePhaseSP();
                }
            }
        }

        // 5_3e 2 SP_H_COAST test
        // if (rto->continousStableCounter == 11) {
        //  if (rto->coastPositionIsSet) {
        //    GBS::SP_H_COAST::write(1);
        //  }
        //}

        if (rto->continousStableCounter == 160) {
            resetInterruptSogBadBit();
        }

        if (rto->continousStableCounter == 45) {
            GBS::ADC_UNUSED_67::write(0); // clear sync fix temp registers (67/68)
            // rto->coastPositionIsSet = 0; // leads to a flicker
            rto->clampPositionIsSet = 0; // run updateClampPosition occasionally
        }

        if (rto->continousStableCounter % 31 == 0) {
            // new: 8 regular interval checks up until 255
            updateSpDynamic(0);
        }

        if (rto->continousStableCounter >= 3) {
            if ((rto->videoStandardInput == 1 || rto->videoStandardInput == 2) &&
                !rto->outModeHdBypass && rto->noSyncCounter == 0) {
                // deinterlacer and scanline code
                static uint8_t timingAdjustDelay = 0;
                static uint8_t oddEvenWhenArmed = 0;
                bool preventScanlines = 0;

                if (rto->deinterlaceAutoEnabled) {
                    uint16_t VPERIOD_IF = GBS::VPERIOD_IF::read();
                    static uint8_t filteredLineCountMotionAdaptiveOn = 0, filteredLineCountMotionAdaptiveOff = 0;
                    static uint16_t VPERIOD_IF_OLD = VPERIOD_IF; // for glitch filter

                    if (VPERIOD_IF_OLD != VPERIOD_IF) {
                        // freezeVideo(); // glitch filter
                        preventScanlines = 1;
                        filteredLineCountMotionAdaptiveOn = 0;
                        filteredLineCountMotionAdaptiveOff = 0;
                        if (uopt->enableFrameTimeLock || rto->extClockGenDetected) {
                            if (uopt->deintMode == 1) { // using bob
                                timingAdjustDelay = 11; // arm timer (always)
                                oddEvenWhenArmed = VPERIOD_IF % 2;
                            }
                        }
                    }

                    if (VPERIOD_IF == 522 || VPERIOD_IF == 524 || VPERIOD_IF == 526 ||
                        VPERIOD_IF == 622 || VPERIOD_IF == 624 || VPERIOD_IF == 626) { // ie v:524, even counts > enable
                        filteredLineCountMotionAdaptiveOn++;
                        filteredLineCountMotionAdaptiveOff = 0;
                        if (filteredLineCountMotionAdaptiveOn >= 2) // at least >= 2
                        {
                            if (uopt->deintMode == 0 && !rto->motionAdaptiveDeinterlaceActive) {
                                if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) { // don't rely on rto->scanlinesEnabled
                                    disableScanlines();
                                }
                                enableMotionAdaptDeinterlace();
                                if (timingAdjustDelay == 0) {
                                    timingAdjustDelay = 11; // arm timer only if it's not already armed
                                    oddEvenWhenArmed = VPERIOD_IF % 2;
                                } else {
                                    timingAdjustDelay = 0; // cancel timer
                                }
                                preventScanlines = 1;
                            }
                            filteredLineCountMotionAdaptiveOn = 0;
                        }
                    } else if (VPERIOD_IF == 521 || VPERIOD_IF == 523 || VPERIOD_IF == 525 ||
                               VPERIOD_IF == 623 || VPERIOD_IF == 625 || VPERIOD_IF == 627) { // ie v:523, uneven counts > disable
                        filteredLineCountMotionAdaptiveOff++;
                        filteredLineCountMotionAdaptiveOn = 0;
                        if (filteredLineCountMotionAdaptiveOff >= 2) // at least >= 2
                        {
                            if (uopt->deintMode == 0 && rto->motionAdaptiveDeinterlaceActive) {
                                disableMotionAdaptDeinterlace();
                                if (timingAdjustDelay == 0) {
                                    timingAdjustDelay = 11; // arm timer only if it's not already armed
                                    oddEvenWhenArmed = VPERIOD_IF % 2;
                                } else {
                                    timingAdjustDelay = 0; // cancel timer
                                }
                            }
                            filteredLineCountMotionAdaptiveOff = 0;
                        }
                    } else {
                        filteredLineCountMotionAdaptiveOn = filteredLineCountMotionAdaptiveOff = 0;
                    }

                    VPERIOD_IF_OLD = VPERIOD_IF; // part of glitch filter

                    if (uopt->deintMode == 1) { // using bob
                        if (rto->motionAdaptiveDeinterlaceActive) {
                            disableMotionAdaptDeinterlace();
                            FrameSync::reset(uopt->frameTimeLockMethod);
                            GBS::GBS_RUNTIME_FTL_ADJUSTED::write(1);
                            lastVsyncLock = millis();
                        }
                        if (uopt->wantScanlines && !rto->scanlinesEnabled) {
                            enableScanlines();
                        } else if (!uopt->wantScanlines && rto->scanlinesEnabled) {
                            disableScanlines();
                        }
                    }

                    // timing adjust after a few stable cycles
                    // should arrive here with either odd or even VPERIOD_IF
                    /*if (timingAdjustDelay != 0) {
            _WS(timingAdjustDelay); _WS(" ");
          }*/
                    if (timingAdjustDelay != 0) {
                        if ((VPERIOD_IF % 2) == oddEvenWhenArmed) {
                            timingAdjustDelay--;
                            if (timingAdjustDelay == 0) {
                                if (uopt->enableFrameTimeLock) {
                                    FrameSync::reset(uopt->frameTimeLockMethod);
                                    GBS::GBS_RUNTIME_FTL_ADJUSTED::write(1);
                                    delay(10);
                                    lastVsyncLock = millis();
                                }
                                externalClockGenSyncInOutRate();
                            }
                        }
                        /*else {
              _WSN("!!!");
            }*/
                    }
                }

                // scanlines
                if (uopt->wantScanlines) {
                    if (!rto->scanlinesEnabled && !rto->motionAdaptiveDeinterlaceActive && !preventScanlines) {
                        enableScanlines();
                    } else if (!uopt->wantScanlines && rto->scanlinesEnabled) {
                        disableScanlines();
                    }
                }
            }
        }
    }

    if (rto->videoStandardInput >= 14) { // RGBHV checks
        static uint16_t RGBHVNoSyncCounter = 0;

        if (uopt->preferScalingRgbhv && rto->continousStableCounter >= 2) {
            bool needPostAdjust = 0;
            static uint16_t activePresetLineCount = 0;
            // is the source in range for scaling RGBHV and is it currently in mode 15?
            uint16 sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read(); // if sourceLines = 0, might be in some reset state
            if ((sourceLines <= 535 && sourceLines != 0) && rto->videoStandardInput == 15) {
                uint16_t firstDetectedSourceLines = sourceLines;
                bool moveOn = 1;
                for (int i = 0; i < 30; i++) { // not the best check, but we don't want to try if this is not stable (usually is though)
                    sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                    // range needed for interlace
                    if ((sourceLines < firstDetectedSourceLines - 3) || (sourceLines > firstDetectedSourceLines + 3)) {
                        moveOn = 0;
                        break;
                    }
                    delay(10);
                }
                if (moveOn) {
                    _WSN(F(" RGB/HV upscale mode"));
                    rto->isValidForScalingRGBHV = true;
                    GBS::GBS_OPTION_SCALING_RGBHV::write(1);
                    rto->autoBestHtotalEnabled = 1;

                    if (rto->syncTypeCsync == false) {
                        GBS::SP_SOG_MODE::write(0);
                        GBS::SP_NO_COAST_REG::write(1);
                        GBS::ADC_5_00::write(0x10); // 5_00 might be required
                        GBS::PLL_IS::write(0);      // 0_40 2: this provides a clock for IF and test bus readings
                        GBS::PLL_VCORST::write(1);  // 0_43 5: also required for clock
                        delay(320);                 // min 250
                    } else {
                        GBS::SP_SOG_MODE::write(1);
                        GBS::SP_H_CST_ST::write(0x10); // 5_4d  // set some default values
                        GBS::SP_H_CST_SP::write(0x80); // will be updated later
                        GBS::SP_H_PROTECT::write(1);   // some modes require this (or invert SOG)
                    }
                    delay(4);

                    float sourceRate = getSourceFieldRate(1);
                    _WSN(sourceRate);

                    // todo: this hack is hard to understand when looking at applypreset and mode is suddenly 1,2 or 3
                    // if (uopt->presetPreference == 2) {
                    // if (uopt->resolutionID == OutputCustom) {
                    if (uopt->resolutionID != Output240p
                        && uopt->resolutionID != OutputBypass
                            && uopt->resolutionID != PresetHdBypass
                                && uopt->resolutionID != PresetBypassRGBHV) {
                        // custom preset defined, try to load (set mode = 14 here early)
                        rto->videoStandardInput = 14;
                    } else {
                        if (sourceLines < 280) {
                            // this is "NTSC like?" check, seen 277 lines in "512x512 interlaced (emucrt)"
                            rto->videoStandardInput = 1;
                        } else if (sourceLines < 380) {
                            // this is "PAL like?" check, seen vt:369 (MDA mode)
                            rto->videoStandardInput = 2;
                        } else if (sourceRate > 44.0f && sourceRate < 53.8f) {
                            // not low res but PAL = "EDTV"
                            rto->videoStandardInput = 4;
                            needPostAdjust = 1;
                        } else { // sourceRate > 53.8f
                            // "60Hz EDTV"
                            rto->videoStandardInput = 3;
                            needPostAdjust = 1;
                        }
                    }

                    // if (uopt->presetPreference == 10) {
                    if (uopt->resolutionID == OutputBypass) {
                        // uopt->presetPreference = Output960P; // fix presetPreference which can be "bypass"
                        uopt->resolutionID = Output960p; // fix presetPreference which can be "bypass"
                    }

                    activePresetLineCount = sourceLines;
                    applyPresets(rto->videoStandardInput);

                    GBS::GBS_OPTION_SCALING_RGBHV::write(1);
                    GBS::IF_INI_ST::write(16);   // fixes pal(at least) interlace
                    GBS::SP_SOG_P_ATO::write(1); // 5_20 1 auto SOG polarity (now "hpw" should never be close to "ht")

                    GBS::SP_SDCS_VSST_REG_L::write(2); // 5_3f
                    GBS::SP_SDCS_VSSP_REG_L::write(0); // 5_40

                    rto->coastPositionIsSet = rto->clampPositionIsSet = 0;
                    rto->videoStandardInput = 14;

                    if (GBS::PLLAD_ICP::read() >= 6) {
                        GBS::PLLAD_ICP::write(5); // reduce charge pump current for more general use
                        latchPLLAD();
                        delay(40);
                    }

                    updateSpDynamic(1);
                    if (rto->syncTypeCsync == false) {
                        GBS::SP_SOG_MODE::write(0);
                        GBS::SP_CLAMP_MANUAL::write(1);
                        GBS::SP_NO_COAST_REG::write(1);
                    } else {
                        GBS::SP_SOG_MODE::write(1);
                        GBS::SP_H_CST_ST::write(0x10); // 5_4d  // set some default values
                        GBS::SP_H_CST_SP::write(0x80); // will be updated later
                        GBS::SP_H_PROTECT::write(1);   // some modes require this (or invert SOG)
                    }
                    delay(300);

                    if (rto->extClockGenDetected) {
                        // switch to ext clock
                        if (!rto->outModeHdBypass) {
                            if (GBS::PLL648_CONTROL_01::read() != 0x35 && GBS::PLL648_CONTROL_01::read() != 0x75) {
                                // first store original in an option byte in 1_2D
                                GBS::GBS_PRESET_DISPLAY_CLOCK::write(GBS::PLL648_CONTROL_01::read());
                                // enable and switch input
                                Si.enable(0);
                                delayMicroseconds(800);
                                GBS::PLL648_CONTROL_01::write(0x75);
                            }
                        }
                        // sync clocks now
                        externalClockGenSyncInOutRate();
                    }

                    // note: this is all duplicated below. unify!
                    if (needPostAdjust) {
                        // base preset was "3" / no line doubling
                        // info: actually the position needs to be adjusted based on hor. freq or "h:" value (todo!)
                        GBS::IF_HB_ST2::write(0x08);  // patches
                        GBS::IF_HB_SP2::write(0x68);  // image
                        GBS::IF_HBIN_SP::write(0x50); // position
                        // if (rto->presetID == 0x05) {
                        if (uopt->resolutionID == Output1080p) {
                            GBS::IF_HB_ST2::write(0x480);
                            GBS::IF_HB_SP2::write(0x8E);
                        }

                        float sfr = getSourceFieldRate(0);
                        if (sfr >= 69.0) {
                            _WSN(F("source >= 70Hz"));
                            // increase vscale; vscale -= 57 seems to hit magic factor often
                            // 512 + 57 = 569 + 57 = 626 + 57 = 683
                            GBS::VDS_VSCALE::write(GBS::VDS_VSCALE::read() - 57);

                        } else {
                            // 50/60Hz, presumably
                            // adjust vposition
                            GBS::IF_VB_SP::write(8);
                            GBS::IF_VB_ST::write(6);
                        }
                    }
                }
            }
            // if currently in scaling RGB/HV, check for "SD" < > "EDTV" style source changes
            else if ((sourceLines <= 535 && sourceLines != 0) && rto->videoStandardInput == 14) {
                // todo: custom presets?
                if (sourceLines < 280 && activePresetLineCount > 280) {
                    rto->videoStandardInput = 1;
                } else if (sourceLines < 380 && activePresetLineCount > 380) {
                    rto->videoStandardInput = 2;
                } else if (sourceLines > 380 && activePresetLineCount < 380) {
                    rto->videoStandardInput = 3;
                    needPostAdjust = 1;
                }

                if (rto->videoStandardInput != 14) {
                    // check thoroughly first
                    uint16_t firstDetectedSourceLines = sourceLines;
                    bool moveOn = 1;
                    for (int i = 0; i < 30; i++) {
                        sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                        if ((sourceLines < firstDetectedSourceLines - 3) || (sourceLines > firstDetectedSourceLines + 3)) {
                            moveOn = 0;
                            break;
                        }
                        delay(10);
                    }

                    if (moveOn) {
                        // need to change presets
                        if (rto->videoStandardInput <= 2) {
                            _WSN(F(" RGB/HV upscale mode base 15kHz"));
                        } else {
                            _WSN(F(" RGB/HV upscale mode base 31kHz"));
                        }

                        // if (uopt->presetPreference == 10) {
                        if (uopt->resolutionID == OutputBypass) {
                            // uopt->presetPreference = Output960P; // fix presetPreference which can be "bypass"
                            uopt->resolutionID = Output960p; // fix presetPreference which can be "bypass"
                        }

                        activePresetLineCount = sourceLines;
                        applyPresets(rto->videoStandardInput);

                        GBS::GBS_OPTION_SCALING_RGBHV::write(1);
                        GBS::IF_INI_ST::write(16);   // fixes pal(at least) interlace
                        GBS::SP_SOG_P_ATO::write(1); // 5_20 1 auto SOG polarity

                        // adjust vposition
                        GBS::SP_SDCS_VSST_REG_L::write(2); // 5_3f
                        GBS::SP_SDCS_VSSP_REG_L::write(0); // 5_40

                        rto->coastPositionIsSet = rto->clampPositionIsSet = 0;
                        rto->videoStandardInput = 14;

                        if (GBS::PLLAD_ICP::read() >= 6) {
                            GBS::PLLAD_ICP::write(5); // reduce charge pump current for more general use
                            latchPLLAD();
                        }

                        updateSpDynamic(1);
                        if (rto->syncTypeCsync == false) {
                            GBS::SP_SOG_MODE::write(0);
                            GBS::SP_CLAMP_MANUAL::write(1);
                            GBS::SP_NO_COAST_REG::write(1);
                        } else {
                            GBS::SP_SOG_MODE::write(1);
                            GBS::SP_H_CST_ST::write(0x10); // 5_4d  // set some default values
                            GBS::SP_H_CST_SP::write(0x80); // will be updated later
                            GBS::SP_H_PROTECT::write(1);   // some modes require this (or invert SOG)
                        }
                        delay(300);

                        if (rto->extClockGenDetected) {
                            // switch to ext clock
                            if (!rto->outModeHdBypass) {
                                if (GBS::PLL648_CONTROL_01::read() != 0x35 && GBS::PLL648_CONTROL_01::read() != 0x75) {
                                    // first store original in an option byte in 1_2D
                                    GBS::GBS_PRESET_DISPLAY_CLOCK::write(GBS::PLL648_CONTROL_01::read());
                                    // enable and switch input
                                    Si.enable(0);
                                    delayMicroseconds(800);
                                    GBS::PLL648_CONTROL_01::write(0x75);
                                }
                            }
                            // sync clocks now
                            externalClockGenSyncInOutRate();
                        }

                        // note: this is all duplicated above. unify!
                        if (needPostAdjust) {
                            // base preset was "3" / no line doubling
                            // info: actually the position needs to be adjusted based on hor. freq or "h:" value (todo!)
                            GBS::IF_HB_ST2::write(0x08);  // patches
                            GBS::IF_HB_SP2::write(0x68);  // image
                            GBS::IF_HBIN_SP::write(0x50); // position
                            // if (rto->presetID == 0x05) {
                            if (uopt->resolutionID == Output1080p) {
                                GBS::IF_HB_ST2::write(0x480);
                                GBS::IF_HB_SP2::write(0x8E);
                            }

                            float sfr = getSourceFieldRate(0);
                            if (sfr >= 69.0) {
                                _WSN(F("source >= 70Hz"));
                                // increase vscale; vscale -= 57 seems to hit magic factor often
                                // 512 + 57 = 569 + 57 = 626 + 57 = 683
                                GBS::VDS_VSCALE::write(GBS::VDS_VSCALE::read() - 57);

                            } else {
                                // 50/60Hz, presumably
                                // adjust vposition
                                GBS::IF_VB_SP::write(8);
                                GBS::IF_VB_ST::write(6);
                            }
                        }
                    } else {
                        // was unstable, undo videoStandardInput change
                        rto->videoStandardInput = 14;
                    }
                }
            }
            // check whether to revert back to full bypass
            else if ((sourceLines > 535) && rto->videoStandardInput == 14) {
                uint16_t firstDetectedSourceLines = sourceLines;
                bool moveOn = 1;
                for (int i = 0; i < 30; i++) {
                    sourceLines = GBS::STATUS_SYNC_PROC_VTOTAL::read();
                    // range needed for interlace
                    if ((sourceLines < firstDetectedSourceLines - 3) || (sourceLines > firstDetectedSourceLines + 3)) {
                        moveOn = 0;
                        break;
                    }
                    delay(10);
                }
                if (moveOn) {
                    _WSN(F(" RGB/HV upscale mode disabled"));
                    rto->videoStandardInput = 15;
                    rto->isValidForScalingRGBHV = false;

                    activePresetLineCount = 0;
                    applyPresets(rto->videoStandardInput); // exception: apply preset here, not later in syncwatcher

                    delay(300);
                }
            }
        } // done preferScalingRgbhv

        if (!uopt->preferScalingRgbhv && rto->videoStandardInput == 14) {
            // user toggled the web ui button / revert scaling rgbhv
            rto->videoStandardInput = 15;
            rto->isValidForScalingRGBHV = false;
            applyPresets(rto->videoStandardInput);
            delay(300);
        }

        // stability check, for CSync and HV separately
        uint16_t limitNoSync = 0;
        uint8_t VSHSStatus = 0;
        bool stable = 0;
        if (rto->syncTypeCsync == true) {
            if (GBS::STATUS_INT_SOG_BAD::read() == 1) {
                // STATUS_INT_SOG_BAD = 0x0f bit 0, interrupt reg
                resetModeDetect();
                stable = 0;
                _WS(F("`"));
                delay(10);
                resetInterruptSogBadBit();
            } else {
                stable = 1;
                VSHSStatus = GBS::STATUS_00::read();
                // this status can get stuck (regularly does)
                stable = ((VSHSStatus & 0x04) == 0x04); // RGBS > check h+v from 0_00
            }
            limitNoSync = 200; // 100
        } else {
            VSHSStatus = GBS::STATUS_16::read();
            // this status usually updates when a source goes off
            stable = ((VSHSStatus & 0x0a) == 0x0a); // RGBHV > check h+v from 0_16
            limitNoSync = 300;
        }

        if (!stable) {
            LEDOFF;
            RGBHVNoSyncCounter++;
            rto->continousStableCounter = 0;
            if (RGBHVNoSyncCounter % 20 == 0) {
                _WS(F("`"));
            }
        } else {
            RGBHVNoSyncCounter = 0;
            LEDON;
            if (rto->continousStableCounter < 255) {
                rto->continousStableCounter++;
                if (rto->continousStableCounter == 6) {
                    updateSpDynamic(1);
                }
            }
        }

        if (RGBHVNoSyncCounter > limitNoSync) {
            RGBHVNoSyncCounter = 0;
            _DBGN(F("(!) reset runtime parameters while running syncWatcher"));
            setResetParameters();
            prepareSyncProcessor();
            resetSyncProcessor(); // todo: fix MD being stuck in last mode when sync disappears
            // resetModeDetect();
            rto->noSyncCounter = 0;
            // _WSN("RGBHV limit no sync");
        }

        static unsigned long lastTimeSogAndPllRateCheck = millis();
        if ((millis() - lastTimeSogAndPllRateCheck) > 900) {
            if (rto->videoStandardInput == 15) {
                // start out by adjusting sync polarity, may reset sog unstable interrupt flag
                updateHVSyncEdge();
                delay(100);
            }

            static uint8_t runsWithSogBadStatus = 0;
            static uint8_t oldHPLLState = 0;
            if (rto->syncTypeCsync == false) {
                if (GBS::STATUS_INT_SOG_BAD::read()) { // SOG source unstable indicator
                    runsWithSogBadStatus++;
                    // _WS(F("test: ")); _WSN(runsWithSogBadStatus);
                    if (runsWithSogBadStatus >= 4) {
                        _WSN(F("RGB/HV < > SOG"));
                        rto->syncTypeCsync = true;
                        rto->HPLLState = runsWithSogBadStatus = RGBHVNoSyncCounter = 0;
                        rto->noSyncCounter = 0x07fe; // will cause a return
                    }
                } else {
                    runsWithSogBadStatus = 0;
                }
            }

            uint32_t currentPllRate = 0;
            static uint32_t oldPllRate = 10;

            // how fast is the PLL running? needed to set charge pump and gain
            // typical: currentPllRate: 1560, currentPllRate: 3999 max seen the pll reach: 5008 for 1280x1024@75
            if (GBS::STATUS_INT_SOG_BAD::read() == 0) {
                currentPllRate = getPllRate();
                // _WSN(currentPllRate);
                if (currentPllRate > 100 && currentPllRate < 7500) {
                    if ((currentPllRate < (oldPllRate - 3)) || (currentPllRate > (oldPllRate + 3))) {
                        delay(40);
                        if (GBS::STATUS_INT_SOG_BAD::read() == 1)
                            delay(100);
                        currentPllRate = getPllRate(); // test again, guards against random spurs
                        // but don't force currentPllRate to = 0 if these inner checks fail,
                        // prevents csync <> hvsync changes
                        if ((currentPllRate < (oldPllRate - 3)) || (currentPllRate > (oldPllRate + 3))) {
                            oldPllRate = currentPllRate; // okay, it changed
                        }
                    }
                } else {
                    currentPllRate = 0;
                }
            }

            resetInterruptSogBadBit();

            // short activeChargePumpLevel = GBS::PLLAD_ICP::read();
            // short activeGainBoost = GBS::PLLAD_FS::read();
            // _WS(F(" rto->HPLLState: ")); _WSN(rto->HPLLState);
            // _WS(F(" currentPllRate: ")); _WSN(currentPllRate);
            // _WS(F(" CPL: ")); _WS(activeChargePumpLevel);
            // _WS(F(" Gain: ")); _WS(activeGainBoost);
            // _WS(F(" KS: ")); _WS(GBS::PLLAD_KS::read());

            oldHPLLState = rto->HPLLState; // do this first, else it can miss events
            if (currentPllRate != 0) {
                if (currentPllRate < 1030) {
                    rto->HPLLState = 1;
                } else if (currentPllRate < 2300) {
                    rto->HPLLState = 2;
                } else if (currentPllRate < 3200) {
                    rto->HPLLState = 3;
                } else if (currentPllRate < 3800) {
                    rto->HPLLState = 4;
                } else {
                    rto->HPLLState = 5;
                }
            }

            if (rto->videoStandardInput == 15) {
                if (oldHPLLState != rto->HPLLState) {
                    if (rto->HPLLState == 1) {
                        GBS::PLLAD_KS::write(2); // KS = 2 okay
                        GBS::PLLAD_FS::write(0);
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 2) {
                        GBS::PLLAD_KS::write(1);
                        GBS::PLLAD_FS::write(0);
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 3) { // KS = 1 okay
                        GBS::PLLAD_KS::write(1);
                        GBS::PLLAD_FS::write(1);
                        GBS::PLLAD_ICP::write(6); // would need 7 but this is risky
                    } else if (rto->HPLLState == 4) {
                        GBS::PLLAD_KS::write(0); // KS = 0 from here on
                        GBS::PLLAD_FS::write(0);
                        GBS::PLLAD_ICP::write(6);
                    } else if (rto->HPLLState == 5) {
                        GBS::PLLAD_KS::write(0); // KS = 0
                        GBS::PLLAD_FS::write(1);
                        GBS::PLLAD_ICP::write(6);
                    }

                    latchPLLAD();
                    delay(2);
                    setOverSampleRatio(4, false); // false = do apply // will auto decrease to max possible factor
                    _WSF(PSTR("(H-PLL) rate: %ld state: %d\n"), currentPllRate, rto->HPLLState);
                    delay(100);
                }
            } else if (rto->videoStandardInput == 14) {
                if (oldHPLLState != rto->HPLLState) {
                    _WSF(PSTR("(H-PLL) rate: %ld  state (no change): %d\n"), currentPllRate, rto->HPLLState);
                    // need to manage HPLL state change somehow
                    // FrameSync::reset(uopt->frameTimeLockMethod);
                }
            }

            if (rto->videoStandardInput == 14) {
                // scanlines
                if (uopt->wantScanlines) {
                    if (!rto->scanlinesEnabled && !rto->motionAdaptiveDeinterlaceActive) {
                        if (GBS::IF_LD_RAM_BYPS::read() == 0) { // line doubler on?
                            enableScanlines();
                        }
                    } else if (!uopt->wantScanlines && rto->scanlinesEnabled) {
                        disableScanlines();
                    }
                }
            }

            rto->clampPositionIsSet = false; // RGBHV should regularly check clamp position
            lastTimeSogAndPllRateCheck = millis();
        }
    }

    if (rto->noSyncCounter >= 0x07fe) {
        // couldn't recover, source is lost
        // restore initial conditions and move to input detect
        GBS::DAC_RGBS_PWDNZ::write(0); // 0 = disable DAC
        rto->noSyncCounter = 0;
        _WSN();
        yield();
        goLowPowerWithInputDetection(); // does not further nest, so it can be called here // sets reset parameters
    }
}

/**
 * @brief Get the Moving Average object
 *      feed the current measurement, get back the moving average
 * @param item
 * @return uint8_t
 */
uint8_t getMovingAverage(uint8_t item)
{
    static const uint8_t sz = 16;
    static uint8_t arr[sz] = {0};
    static uint8_t pos = 0;

    arr[pos] = item;
    if (pos < (sz - 1)) {
        pos++;
    } else {
        pos = 0;
    }

    uint16_t sum = 0;
    for (uint8_t i = 0; i < sz; i++) {
        sum += arr[i];
    }
    return sum >> 4; // for array size 16
}

/**
 * @brief GBS boards have 2 potential sync sources:
 *          - RCA connectors
 *          - VGA input / 5 pin RGBS header / 8 pin VGA header (all 3 are shared electrically)
 *      This routine looks for sync on the currently active input. If it finds it,
 *      the input is returned. If it doesn't find sync, it switches the input and
 *      returns 0, so that an active input will be found eventually.
 *
 * @return uint8_t
 */
uint8_t detectAndSwitchToActiveInput()
{
    uint8_t currentInput = GBS::ADC_INPUT_SEL::read();
    unsigned long timeout = millis();
    while (millis() - timeout < 450) {
        delay(10);
        // wifiLoop(0);

        bool stable = getStatus16SpHsStable();
        if (stable) {
            currentInput = GBS::ADC_INPUT_SEL::read();
            _WS(F("Activity detected, input: "));
            if (currentInput == 1)
                _WSN(F("RGB"));
            else
                _WSN(F("Component"));

            if (currentInput == 1) { // RGBS or RGBHV
                bool vsyncActive = 0;
                rto->inputIsYPbPr = false; // declare for MD
                rto->currentLevelSOG = 13; // test startup with MD and MS separately!
                setAndUpdateSogLevel(rto->currentLevelSOG);

                unsigned long timeOutStart = millis();
                // vsync test
                // 360ms good up to 5_34 SP_V_TIMER_VAL = 0x0b
                while (!vsyncActive && ((millis() - timeOutStart) < 360)) {
                    vsyncActive = GBS::STATUS_SYNC_PROC_VSACT::read();
                    // wifiLoop(0); // wifi stack
                    delay(1);
                }

                // if VSync is active, it's RGBHV or RGBHV with CSync on HS pin
                if (vsyncActive) {
                    _WSN(F("VSync: present"));
                    GBS::MD_SEL_VGA60::write(1); // VGA 640x480 more likely than EDTV
                    bool hsyncActive = 0;

                    timeOutStart = millis();
                    while (!hsyncActive && millis() - timeOutStart < 400) {
                        hsyncActive = GBS::STATUS_SYNC_PROC_HSACT::read();
                        // wifiLoop(0); // wifi stack
                        delay(1);
                    }

                    if (hsyncActive) {
                        _WS(F("HSync: present"));
                        // The HSync and SOG pins are setup to detect CSync, if present
                        // (SOG mode on, coasting setup, debug bus setup, etc)
                        // SP_H_PROTECT is needed for CSync with a VS source present as well
                        GBS::SP_H_PROTECT::write(1);
                        delay(120);

                        short decodeSuccess = 0;
                        for (int i = 0; i < 3; i++) {
                            // no success if: no signal at all (returns 0.0f), no embedded VSync (returns ~18.5f)
                            // todo: this takes a while with no csync present
                            rto->syncTypeCsync = 1; // temporary for test
                            float sfr = getSourceFieldRate(1);
                            rto->syncTypeCsync = 0; // undo
                            if (sfr > 40.0f)
                                decodeSuccess++; // properly decoded vsync from 40 to xx Hz
                        }

                        if (decodeSuccess >= 2) {
                            _WSN(F(" (with CSync)"));
                            GBS::SP_PRE_COAST::write(0x10); // increase from 9 to 16 (EGA 364)
                            delay(40);
                            rto->syncTypeCsync = true;
                        } else {
                            _WSN();
                            rto->syncTypeCsync = false;
                        }

                        // check for 25khz, all regular SOG modes first // update: only check for mode 8
                        // MD reg for medium res starts at 0x2C and needs 16 loops to ramp to max of 0x3C (vt 360 .. 496)
                        // if source is HS+VS, can't detect via MD unit, need to set 5_11=0x92 and look at vt: counter
                        for (uint8_t i = 0; i < 16; i++) {
                            // printInfo();
                            uint8_t innerVideoMode = getVideoMode();
                            if (innerVideoMode == 8) {
                                setAndUpdateSogLevel(rto->currentLevelSOG);
                                rto->medResLineCount = GBS::MD_HD1250P_CNTRL::read();
                                _WSN(F("med res"));

                                return 1;
                            }
                            // update 25khz detection
                            GBS::MD_HD1250P_CNTRL::write(GBS::MD_HD1250P_CNTRL::read() + 1);
                            // _WSF("0x%04X", GBS::MD_HD1250P_CNTRL::read());
                            delay(30);
                        }

                        rto->videoStandardInput = 15;
                        // exception: apply preset here, not later in syncwatcher
                        applyPresets(rto->videoStandardInput);
                        delay(100);

                        return 3;
                    } else {
                        // need to continue looking
                        _WSN(F("but no HSync!"));
                    }
                }

                if (!vsyncActive) { // then do RGBS check
                    rto->syncTypeCsync = true;
                    GBS::MD_SEL_VGA60::write(0); // EDTV60 more likely than VGA60
                    uint16_t testCycle = 0;
                    timeOutStart = millis();
                    while ((millis() - timeOutStart) < 6000) {
                        delay(2);
                        if (getVideoMode() > 0) {
                            if (getVideoMode() != 8) { // if it's mode 8, need to set stuff first
                                return 1;
                            }
                        }
                        testCycle++;
                        // post coast 18 can mislead occasionally (SNES 239 mode)
                        // but even then it still detects the video mode pretty well
                        if ((testCycle % 150) == 0) {
                            if (rto->currentLevelSOG == 1) {
                                rto->currentLevelSOG = 2;
                            } else {
                                rto->currentLevelSOG += 2;
                            }
                            if (rto->currentLevelSOG >= 15) {
                                rto->currentLevelSOG = 1;
                            }
                            setAndUpdateSogLevel(rto->currentLevelSOG);
                        }

                        // new: check for 25khz, use regular scaling route for those
                        if (getVideoMode() == 8) {
                            rto->currentLevelSOG = rto->thisSourceMaxLevelSOG = 13;
                            setAndUpdateSogLevel(rto->currentLevelSOG);
                            rto->medResLineCount = GBS::MD_HD1250P_CNTRL::read();
                            _WSN(F("med res"));
                            return 1;
                        }

                        uint8_t currentMedResLineCount = GBS::MD_HD1250P_CNTRL::read();
                        if (currentMedResLineCount < 0x3c) {
                            GBS::MD_HD1250P_CNTRL::write(currentMedResLineCount + 1);
                        } else {
                            GBS::MD_HD1250P_CNTRL::write(0x33);
                        }
                        // _WSF("0x%04X", GBS::MD_HD1250P_CNTRL::read());
                    }

                    // rto->currentLevelSOG = rto->thisSourceMaxLevelSOG = 13;
                    // setAndUpdateSogLevel(rto->currentLevelSOG);

                    return 1; // anyway, let later stage deal with it
                }

                GBS::SP_SOG_MODE::write(1);
                resetSyncProcessor();
                resetModeDetect(); // there was some signal but we lost it. MD is stuck anyway, so reset
                delay(40);
            } else if (currentInput == 0) { // YUV
                uint16_t testCycle = 0;
                rto->inputIsYPbPr = true;    // declare for MD
                GBS::MD_SEL_VGA60::write(0); // EDTV more likely than VGA 640x480

                unsigned long timeOutStart = millis();
                while ((millis() - timeOutStart) < 6000) {
                    delay(2);
                    if (getVideoMode() > 0) {
                        return 2;
                    }

                    testCycle++;
                    if ((testCycle % 180) == 0) {
                        if (rto->currentLevelSOG == 1) {
                            rto->currentLevelSOG = 2;
                        } else {
                            rto->currentLevelSOG += 2;
                        }
                        if (rto->currentLevelSOG >= 16) {
                            rto->currentLevelSOG = 1;
                        }
                        setAndUpdateSogLevel(rto->currentLevelSOG);
                        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG;
                    }
                }

                rto->currentLevelSOG = rto->thisSourceMaxLevelSOG = 14;
                setAndUpdateSogLevel(rto->currentLevelSOG);

                return 2; // anyway, let later stage deal with it
            }

            _WSN(F(" lost.."));
            rto->currentLevelSOG = 2;
            setAndUpdateSogLevel(rto->currentLevelSOG);
        }

        GBS::ADC_INPUT_SEL::write(!currentInput); // can only be 1 or 0
        delay(200);

        return 0; // don't do the check on the new input here, wait till next run
    }

    return 0;
}

/**
 * @brief
 *
 * @return uint8_t
 */
uint8_t inputAndSyncDetect()
{
    uint8_t syncFound = detectAndSwitchToActiveInput();

    if (syncFound == 0) {
        if (!getSyncPresent()) {
            if (rto->isInLowPowerMode == false) {
                rto->sourceDisconnected = true;
                rto->videoStandardInput = 0;
                // reset to base settings, then go to low power
                GBS::SP_SOG_MODE::write(1);
                goLowPowerWithInputDetection();
                delay(10);
                rto->isInLowPowerMode = true;
            }
        }
        return 0;
    } else if (syncFound == 1) { // input is RGBS
        rto->inputIsYPbPr = 0;
        rto->sourceDisconnected = false;
        rto->isInLowPowerMode = false;
        resetDebugPort();
        applyRGBPatches();
        LEDON;
        return 1;
    } else if (syncFound == 2) {
        rto->inputIsYPbPr = 1;
        rto->sourceDisconnected = false;
        rto->isInLowPowerMode = false;
        resetDebugPort();
        applyYuvPatches();
        LEDON;
        return 2;
    } else if (syncFound == 3) { // input is RGBHV
        // already applied
        rto->isInLowPowerMode = false;
        rto->inputIsYPbPr = 0;
        rto->sourceDisconnected = false;
        rto->videoStandardInput = 15;
        resetDebugPort();
        LEDON;
        return 3;
    }

    return 0;
}