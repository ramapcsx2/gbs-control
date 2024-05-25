/*
#####################################################################################
# File: preset.cpp                                                                  #
# File Created: Thursday, 2nd May 2024 6:38:23 pm                                   #
# Author:                                                                           #
# Last Modified: Friday, 24th May 2024 11:59:56 pm                        #
# Modified By: Sergey Ko                                                            #
#####################################################################################
# CHANGELOG:                                                                        #
#####################################################################################
*/

#include "presets.h"

/**
 * @brief
 *
 */
void loadDefaultUserOptions()
{
    // uopt->presetPreference = Output960P; // #1
    // rto->presetID = Output960p;
    rto->resolutionID = Output960p;
    uopt->enableFrameTimeLock = 0;       // permanently adjust frame timing to avoid glitch vertical bar. does not work on all displays!
    uopt->presetSlot = 'A';              //
    uopt->frameTimeLockMethod = 0;       // compatibility with more displays
    uopt->enableAutoGain = 0;
    uopt->wantScanlines = 0;
    uopt->wantOutputComponent = 0;
    uopt->deintMode = 0;
    uopt->wantVdsLineFilter = 0;
    uopt->wantPeaking = 1;
    uopt->preferScalingRgbhv = 1;
    uopt->wantTap6 = 1;
    uopt->PalForce60 = 0;
    uopt->matchPresetSource = 1;             // #14
    uopt->wantStepResponse = 1;              // #15
    uopt->wantFullHeight = 1;                // #16
    uopt->enableCalibrationADC = 1;          // #17
    uopt->scanlineStrength = 0x30;           // #18
    uopt->disableExternalClockGenerator = 0; // #19
}

/**
 * @brief
 *
 */
void loadPresetMdSection()
{
    uint16_t index = 0;
    uint8_t bank[16];
    writeOneByte(0xF0, 1);
    for (int j = 6; j <= 7; j++) { // start at 0x60
        copyBank(bank, presetMdSection, &index);
        writeBytes(j * 16, bank, 16);
    }
    bank[0] = pgm_read_byte(presetMdSection + index);
    bank[1] = pgm_read_byte(presetMdSection + index + 1);
    bank[2] = pgm_read_byte(presetMdSection + index + 2);
    bank[3] = pgm_read_byte(presetMdSection + index + 3);
    writeBytes(8 * 16, bank, 4); // MD section ends at 0x83, not 0x90
}

/**
 * @brief Set the Reset Parameters object
 *
 */
void setResetParameters()
{
    _WSN(F("<reset>"));
    rto->videoStandardInput = 0;
    rto->videoIsFrozen = false;
    rto->applyPresetDoneStage = 0;
    rto->presetVlineShift = 0;
    rto->sourceDisconnected = true;
    rto->outModeHdBypass = 0;
    rto->clampPositionIsSet = 0;
    rto->coastPositionIsSet = 0;
    rto->phaseIsSet = 0;
    rto->continousStableCounter = 0;
    rto->noSyncCounter = 0;
    rto->isInLowPowerMode = false;
    rto->currentLevelSOG = 5;
    rto->thisSourceMaxLevelSOG = 31; // 31 = auto sog has not (yet) run
    rto->failRetryAttempts = 0;
    rto->HPLLState = 0;
    rto->motionAdaptiveDeinterlaceActive = false;
    rto->scanlinesEnabled = false;
    rto->syncTypeCsync = false;
    rto->isValidForScalingRGBHV = false;
    rto->medResLineCount = 0x33; // 51*8=408
    rto->osr = 0;
    rto->useHdmiSyncFix = 0;
    rto->notRecognizedCounter = 0;

    // DEV
    rto->invertSync = false;

    adco->r_gain = 0;
    adco->g_gain = 0;
    adco->b_gain = 0;

    // clear temp storage
    GBS::ADC_UNUSED_64::write(0);
    GBS::ADC_UNUSED_65::write(0);
    GBS::ADC_UNUSED_66::write(0);
    GBS::ADC_UNUSED_67::write(0);
    GBS::GBS_PRESET_CUSTOM::write(0);
    GBS::GBS_PRESET_ID::write(0);
    GBS::GBS_OPTION_SCALING_RGBHV::write(0);
    GBS::GBS_OPTION_PALFORCED60_ENABLED::write(0);

    // set minimum IF parameters
    GBS::IF_VS_SEL::write(1);
    GBS::IF_VS_FLIP::write(1);
    GBS::IF_HSYNC_RST::write(0x3FF);
    GBS::IF_VB_ST::write(0);
    GBS::IF_VB_SP::write(2);

    // could stop ext clock gen output here?
    FrameSync::cleanup();

    GBS::OUT_SYNC_CNTRL::write(0);    // no H / V sync out to PAD
    GBS::DAC_RGBS_PWDNZ::write(0);    // disable DAC
    GBS::ADC_TA_05_CTRL::write(0x02); // 5_05 1 // minor SOG clamp effect
    GBS::ADC_TEST_04::write(0x02);    // 5_04
    GBS::ADC_TEST_0C::write(0x12);    // 5_0c 1 4
    GBS::ADC_CLK_PA::write(0);        // 5_00 0/1 PA_ADC input clock = PLLAD CLKO2
    GBS::ADC_SOGEN::write(1);
    GBS::SP_SOG_MODE::write(1);
    GBS::ADC_INPUT_SEL::write(1); // 1 = RGBS / RGBHV adc data input
    GBS::ADC_POWDZ::write(1);     // ADC on
    setAndUpdateSogLevel(rto->currentLevelSOG);
    GBS::RESET_CONTROL_0x46::write(0x00); // all units off
    GBS::RESET_CONTROL_0x47::write(0x00);
    GBS::GPIO_CONTROL_00::write(0x67);     // most GPIO pins regular GPIO
    GBS::GPIO_CONTROL_01::write(0x00);     // all GPIO outputs disabled
    GBS::DAC_RGBS_PWDNZ::write(0);         // disable DAC (output)
    GBS::PLL648_CONTROL_01::write(0x00);   // VCLK(1/2/4) display clock // needs valid for debug bus
    GBS::PAD_CKIN_ENZ::write(0);           // 0 = clock input enable (pin40)
    GBS::PAD_CKOUT_ENZ::write(1);          // clock output disable
    GBS::IF_SEL_ADC_SYNC::write(1);        // ! 1_28 2
    GBS::PLLAD_VCORST::write(1);           // reset = 1
    GBS::PLL_ADS::write(1);                // When = 1, input clock is from ADC ( otherwise, from unconnected clock at pin 40 )
    GBS::PLL_CKIS::write(0);               // PLL use OSC clock
    GBS::PLL_MS::write(2);                 // fb memory clock can go lower power
    GBS::PAD_CONTROL_00_0x48::write(0x2b); // disable digital inputs, enable debug out pin
    GBS::PAD_CONTROL_01_0x49::write(0x1f); // pad control pull down/up transistors on
    loadHdBypassSection();                 // 1_30 to 1_55
    loadPresetMdSection();                 // 1_60 to 1_83
    setAdcParametersGainAndOffset();
    GBS::SP_PRE_COAST::write(9);    // was 0x07 // need pre / poast to allow all sources to detect
    GBS::SP_POST_COAST::write(18);  // was 0x10 // ps2 1080p 18
    GBS::SP_NO_COAST_REG::write(0); // can be 1 in some soft reset situations, will prevent sog vsync decoding << really?
    GBS::SP_CS_CLP_ST::write(32);   // define it to something at start
    GBS::SP_CS_CLP_SP::write(48);
    GBS::SP_SOG_SRC_SEL::write(0);  // SOG source = ADC
    GBS::SP_EXT_SYNC_SEL::write(0); // connect HV input ( 5_20 bit 3 )
    GBS::SP_NO_CLAMP_REG::write(1);
    GBS::PLLAD_ICP::write(0); // lowest charge pump current
    GBS::PLLAD_FS::write(0);  // low gain (have to deal with cold and warm startups)
    GBS::PLLAD_5_16::write(0x1f);
    GBS::PLLAD_MD::write(0x700);
    resetPLL(); // cycles PLL648
    delay(2);
    resetPLLAD();                            // same for PLLAD
    GBS::PLL_VCORST::write(1);               // reset on
    GBS::PLLAD_CONTROL_00_5x11::write(0x01); // reset on
    resetDebugPort();

    // GBS::RESET_CONTROL_0x47::write(0x16);
    GBS::RESET_CONTROL_0x46::write(0x41);   // new 23.07.19
    GBS::RESET_CONTROL_0x47::write(0x17);   // new 23.07.19 (was 0x16)
    GBS::INTERRUPT_CONTROL_01::write(0xff); // enable interrupts
    GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
    GBS::INTERRUPT_CONTROL_00::write(0x00);
    GBS::PAD_SYNC_OUT_ENZ::write(0); // sync output enabled, will be low (HC125 fix)
    rto->clampPositionIsSet = 0;     // some functions override these, so make sure
    rto->coastPositionIsSet = 0;
    rto->phaseIsSet = 0;
    rto->continousStableCounter = 0;
    serialCommand = '@';
    userCommand = '@';
}

/**
 * @brief
 *
 */
void doPostPresetLoadSteps()
{
    // unsigned long postLoadTimer = millis();

    // adco->r_gain gets applied if uopt->enableAutoGain is set.
    if (uopt->enableAutoGain) {
        // if (uopt->presetPreference == OutputCustomized) {
        // if (rto->resolutionID == OutputCustom) {
            // Loaded custom preset, we want to keep newly loaded gain. Save
            // gain written by loadPresetFromLFS -> writeProgramArrayNew.
            adco->r_gain = GBS::ADC_RGCTRL::read();
            adco->g_gain = GBS::ADC_GGCTRL::read();
            adco->b_gain = GBS::ADC_BGCTRL::read();
        // } else {
            // Loaded fixed preset. Keep adco->r_gain from before overwriting
            // registers.
        // }
    }

    // GBS::PAD_SYNC_OUT_ENZ::write(1);  // no sync out
    // GBS::DAC_RGBS_PWDNZ::write(0);    // no DAC
    // GBS::SFTRST_MEM_FF_RSTZ::write(0);  // mem fifos keep in reset

    if (rto->videoStandardInput == 0) {
        uint8_t videoMode = getVideoMode();
        _WSF(PSTR("post preset: rto->videoStandardInput 0 > %d\n"), videoMode);
        if (videoMode > 0) {
            rto->videoStandardInput = videoMode;
        }
    }
    // set output resolution
    // rto->presetID = static_cast<OutputResolution>(GBS::GBS_PRESET_ID::read());

    // @sk: override resolution via user preset
    if (rto->resolutionID == OutputBypass
            || rto->resolutionID == PresetHdBypass
                || rto->resolutionID == PresetBypassRGBHV) {
        rto->resolutionID = static_cast<OutputResolution>(GBS::GBS_PRESET_ID::read());
    }
    rto->isCustomPreset = GBS::GBS_PRESET_CUSTOM::read();

    GBS::ADC_UNUSED_64::write(0);
    GBS::ADC_UNUSED_65::write(0); // clear temp storage
    GBS::ADC_UNUSED_66::write(0);
    GBS::ADC_UNUSED_67::write(0); // clear temp storage
    GBS::PAD_CKIN_ENZ::write(0);  // 0 = clock input enable (pin40)

    if (!rto->isCustomPreset) {
        prepareSyncProcessor(); // todo: handle modes 14 and 15 better, now that they support scaling
    }
    if (rto->videoStandardInput == 14) {
        // copy of code in bypassModeSwitch_RGBHV
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
    }

    GBS::SP_H_PROTECT::write(0);
    GBS::SP_COAST_INV_REG::write(0); // just in case
    if (!rto->outModeHdBypass && GBS::GBS_OPTION_SCALING_RGBHV::read() == 0) {
        // setOutModeHdBypass has it's own and needs to update later
        updateSpDynamic(0); // remember: rto->videoStandardInput for RGB(C/HV) in scaling is 1, 2 or 3 here
    }

    GBS::SP_NO_CLAMP_REG::write(1); // (keep) clamp disabled, to be enabled when position determined
    GBS::OUT_SYNC_CNTRL::write(1);  // prepare sync out to PAD

    // auto offset adc prep
    GBS::ADC_AUTO_OFST_PRD::write(1);   // by line (0 = by frame)
    GBS::ADC_AUTO_OFST_DELAY::write(0); // sample delay 0 (1 to 4 pipes)
    GBS::ADC_AUTO_OFST_STEP::write(0);  // 0 = abs diff, then 1 to 3 steps
    GBS::ADC_AUTO_OFST_TEST::write(1);
    GBS::ADC_AUTO_OFST_RANGE_REG::write(0x00); // 5_0f U/V ranges = 0 (full range, 1 to 15)

    if (rto->inputIsYPbPr == true) {
        applyYuvPatches();
    } else {
        applyRGBPatches();
    }

    if (rto->outModeHdBypass) {
        GBS::OUT_SYNC_SEL::write(1); // 0_4f 1=sync from HDBypass, 2=sync from SP
        rto->autoBestHtotalEnabled = false;
    } else {
        rto->autoBestHtotalEnabled = true;
    }

    rto->phaseADC = GBS::PA_ADC_S::read(); // we can't know which is right, get from preset
    rto->phaseSP = 8;                      // get phase into global variables early: before latching

    if (rto->inputIsYPbPr) {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 14;
    } else {
        rto->thisSourceMaxLevelSOG = rto->currentLevelSOG = 13; // similar to yuv, allow variations
    }

    setAndUpdateSogLevel(rto->currentLevelSOG);

    if (!rto->isCustomPreset) {
        // Writes ADC_RGCTRL. If auto gain is enabled, ADC_RGCTRL will be
        // overwritten further down at `uopt->enableAutoGain == 1`.
        setAdcParametersGainAndOffset();
    }

    GBS::GPIO_CONTROL_00::write(0x67); // most GPIO pins regular GPIO
    GBS::GPIO_CONTROL_01::write(0x00); // all GPIO outputs disabled
    rto->clampPositionIsSet = 0;
    rto->coastPositionIsSet = 0;
    rto->phaseIsSet = 0;
    rto->continousStableCounter = 0;
    rto->noSyncCounter = 0;
    rto->motionAdaptiveDeinterlaceActive = false;
    rto->scanlinesEnabled = false;
    rto->failRetryAttempts = 0;
    rto->videoIsFrozen = true;       // ensures unfreeze
    rto->sourceDisconnected = false; // this must be true if we reached here (no syncwatcher operation)
    rto->boardHasPower = true;       // same

    // if (rto->presetID == 0x06 || rto->presetID == 0x16) {
    if (rto->resolutionID == Output15kHz || rto->resolutionID == Output15kHz50) {
        rto->isCustomPreset = 0; // override so it applies section 2 deinterlacer settings
    }

    if (!rto->isCustomPreset) {
        if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4 ||
            rto->videoStandardInput == 8 || rto->videoStandardInput == 9) {
            GBS::IF_LD_RAM_BYPS::write(1); // 1_0c 0 no LD, do this before setIfHblankParameters
        }

        // setIfHblankParameters();              // 1_0e, 1_18, 1_1a
        GBS::IF_INI_ST::write(0); // 16.08.19: don't calculate, use fixed to 0
        // the following sets a field offset that eliminates 240p content forced to 480i flicker
        // GBS::IF_INI_ST::write(GBS::PLLAD_MD::read() * 0.4261f);  // upper: * 0.4282f  lower: 0.424f

        GBS::IF_HS_INT_LPF_BYPS::write(0); // 1_02 2
        // 0 allows int/lpf for smoother scrolling with non-ideal scaling, also reduces jailbars and even noise
        // interpolation or lpf available, lpf looks better
        GBS::IF_HS_SEL_LPF::write(1);     // 1_02 1
        GBS::IF_HS_PSHIFT_BYPS::write(1); // 1_02 3 nonlinear scale phase shift bypass
        // 1_28 1 1:hbin generated write reset 0:line generated write reset
        GBS::IF_LD_WRST_SEL::write(1); // at 1 fixes output position regardless of 1_24
        // GBS::MADPT_Y_DELAY_UV_DELAY::write(0); // 2_17 default: 0 // don't overwrite

        GBS::SP_RT_HS_ST::write(0); // 5_49 // retiming hs ST, SP
        GBS::SP_RT_HS_SP::write(GBS::PLLAD_MD::read() * 0.93f);

        GBS::VDS_PK_LB_CORE::write(0); // 3_44 0-3 // 1 for anti source jailbars
        GBS::VDS_PK_LH_CORE::write(0); // 3_46 0-3 // 1 for anti source jailbars
        // if (rto->presetID == 0x05 || rto->presetID == 0x15) {
        if (rto->resolutionID == Output1080p || rto->resolutionID == Output1080p50) {
            GBS::VDS_PK_LB_GAIN::write(0x16); // 3_45 // peaking HF
            GBS::VDS_PK_LH_GAIN::write(0x0A); // 3_47
        } else {
            GBS::VDS_PK_LB_GAIN::write(0x16); // 3_45
            GBS::VDS_PK_LH_GAIN::write(0x18); // 3_47
        }
        GBS::VDS_PK_VL_HL_SEL::write(0); // 3_43 0 if 1 then 3_45 HF almost no effect (coring 0xf9)
        GBS::VDS_PK_VL_HH_SEL::write(0); // 3_43 1

        GBS::VDS_STEP_GAIN::write(1); // step response, max 15 (VDS_STEP_DLY_CNTRL set in presets)

        // DAC filters / keep in presets for now
        // GBS::VDS_1ST_INT_BYPS::write(1); // disable RGB stage interpolator
        // GBS::VDS_2ND_INT_BYPS::write(1); // disable YUV stage interpolator

        // most cases will use osr 2
        setOverSampleRatio(2, true); // prepare only = true

        // full height option
        if (uopt->wantFullHeight) {
            if (rto->videoStandardInput == 1 || rto->videoStandardInput == 3) {
                // if (rto->presetID == 0x5)
                //{ // out 1080p 60
                //   GBS::VDS_DIS_VB_ST::write(GBS::VDS_VSYNC_RST::read() - 1);
                //   GBS::VDS_DIS_VB_SP::write(42);
                //   GBS::VDS_VB_SP::write(42 - 10); // is VDS_DIS_VB_SP - 10 = 32 // watch for vblank overflow (ps3)
                //   GBS::VDS_VSCALE::write(455);
                //   GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + 4);
                //   GBS::IF_VB_ST::write(GBS::IF_VB_ST::read() + 4);
                //   _WSN(F("full height"));
                // }
            } else if (rto->videoStandardInput == 2 || rto->videoStandardInput == 4) {
                // if (rto->presetID == 0x15) { // out 1080p 50
                if (rto->resolutionID == Output1080p50) { // out 1080p 50
                    GBS::VDS_VSCALE::write(455);
                    GBS::VDS_DIS_VB_ST::write(GBS::VDS_VSYNC_RST::read()); // full = 1125 of 1125
                    GBS::VDS_DIS_VB_SP::write(42);
                    GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + 22);
                    GBS::IF_VB_ST::write(GBS::IF_VB_ST::read() + 22);
                    _WSN(F("full height"));
                }
            }
        }

        if (rto->videoStandardInput == 1 || rto->videoStandardInput == 2) {
            // GBS::PLLAD_ICP::write(5);         // 5 rather than 6 to work well with CVBS sync as well as CSync

            GBS::ADC_FLTR::write(3);             // 5_03 4/5 ADC filter 3=40, 2=70, 1=110, 0=150 Mhz
            GBS::PLLAD_KS::write(2);             // 5_16
            setOverSampleRatio(4, true);         // prepare only = true
            GBS::IF_SEL_WEN::write(0);           // 1_02 0; 0 for SD, 1 for EDTV
            if (rto->inputIsYPbPr) {             // todo: check other videoStandardInput in component vs rgb
                GBS::IF_HS_TAP11_BYPS::write(0); // 1_02 4 Tap11 LPF bypass in YUV444to422
                GBS::IF_HS_Y_PDELAY::write(2);   // 1_02 5+6 delays
                GBS::VDS_V_DELAY::write(0);      // 3_24 2
                GBS::VDS_Y_DELAY::write(3);      // 3_24 4/5 delays
            }

            // downscale preset: source is SD
            // if (rto->presetID == 0x06 || rto->presetID == 0x16) {
            if (rto->resolutionID == Output15kHz || rto->resolutionID == Output15kHz50) {
                setCsVsStart(2);                        // or 3, 0
                setCsVsStop(0);                         // fixes field position
                GBS::IF_VS_SEL::write(1);               // 1_00 5 // turn off VHS sync feature
                GBS::IF_VS_FLIP::write(0);              // 1_01 0
                GBS::IF_LD_RAM_BYPS::write(0);          // 1_0c 0
                GBS::IF_HS_DEC_FACTOR::write(1);        // 1_0b 4
                GBS::IF_LD_SEL_PROV::write(0);          // 1_0b 7
                GBS::IF_HB_ST::write(2);                // 1_10 deinterlace offset
                GBS::MADPT_Y_VSCALE_BYPS::write(0);     // 2_02 6
                GBS::MADPT_UV_VSCALE_BYPS::write(0);    // 2_02 7
                GBS::MADPT_PD_RAM_BYPS::write(0);       // 2_24 2 one line fifo for line phase adjust
                GBS::MADPT_VSCALE_DEC_FACTOR::write(1); // 2_31 0..1
                GBS::MADPT_SEL_PHASE_INI::write(0);     // 2_31 2 disable for SD (check 240p content)
                if (rto->videoStandardInput == 1) {
                    GBS::IF_HB_ST2::write(0x490); // 1_18
                    GBS::IF_HB_SP2::write(0x80);  // 1_1a
                    GBS::IF_HB_SP::write(0x4A);   // 1_12 deinterlace offset, green bar
                    GBS::IF_HBIN_SP::write(0xD0); // 1_26
                } else if (rto->videoStandardInput == 2) {
                    GBS::IF_HB_SP2::write(0x74);  // 1_1a
                    GBS::IF_HB_SP::write(0x50);   // 1_12 deinterlace offset, green bar
                    GBS::IF_HBIN_SP::write(0xD0); // 1_26
                }
            }
        }
        if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4 ||
            rto->videoStandardInput == 8 || rto->videoStandardInput == 9) {
            // EDTV p-scan, need to either double adc data rate and halve vds scaling
            // or disable line doubler (better) (50 / 60Hz shared)

            GBS::ADC_FLTR::write(3); // 5_03 4/5
            GBS::PLLAD_KS::write(1); // 5_16

            // if (rto->presetID != 0x06 && rto->presetID != 0x16) {
            if (rto->resolutionID != Output15kHz && rto->resolutionID != Output15kHz50) {
                setCsVsStart(14);        // pal // hm
                setCsVsStop(11);         // probably setting these for modes 8,9
                GBS::IF_HB_SP::write(0); // 1_12 deinterlace offset, fixes colors (downscale needs diff)
            }
            setOverSampleRatio(2, true);           // with KS = 1 for modes 3, 4, 8
            GBS::IF_HS_DEC_FACTOR::write(0);       // 1_0b 4+5
            GBS::IF_LD_SEL_PROV::write(1);         // 1_0b 7
            GBS::IF_PRGRSV_CNTRL::write(1);        // 1_00 6
            GBS::IF_SEL_WEN::write(1);             // 1_02 0
            GBS::IF_HS_SEL_LPF::write(0);          // 1_02 1   0 = use interpolator not lpf for EDTV
            GBS::IF_HS_TAP11_BYPS::write(0);       // 1_02 4 filter
            GBS::IF_HS_Y_PDELAY::write(3);         // 1_02 5+6 delays (ps2 test on one board clearly says 3, not 2)
            GBS::VDS_V_DELAY::write(1);            // 3_24 2 // new 24.07.2019 : 1, also set 2_17 to 1
            GBS::MADPT_Y_DELAY_UV_DELAY::write(1); // 2_17 : 1
            GBS::VDS_Y_DELAY::write(3);            // 3_24 4/5 delays (ps2 test saying 3 for 1_02 goes with 3 here)
            if (rto->videoStandardInput == 9) {
                if (GBS::STATUS_SYNC_PROC_VTOTAL::read() > 650) {
                    delay(20);
                    if (GBS::STATUS_SYNC_PROC_VTOTAL::read() > 650) {
                        GBS::PLLAD_KS::write(0);        // 5_16
                        GBS::VDS_VSCALE_BYPS::write(1); // 3_00 5 no vscale
                    }
                }
            }

            // downscale preset: source is EDTV
            // if (rto->presetID == 0x06 || rto->presetID == 0x16) {
            if (rto->resolutionID == Output15kHz || rto->resolutionID == Output15kHz50) {
                GBS::MADPT_Y_VSCALE_BYPS::write(0);     // 2_02 6
                GBS::MADPT_UV_VSCALE_BYPS::write(0);    // 2_02 7
                GBS::MADPT_PD_RAM_BYPS::write(0);       // 2_24 2 one line fifo for line phase adjust
                GBS::MADPT_VSCALE_DEC_FACTOR::write(1); // 2_31 0..1
                GBS::MADPT_SEL_PHASE_INI::write(1);     // 2_31 2 enable
                GBS::MADPT_SEL_PHASE_INI::write(0);     // 2_31 2 disable
            }
        }
        // if (rto->videoStandardInput == 3 && rto->presetID != 0x06) { // ED YUV 60
        if (rto->videoStandardInput == 3 && rto->resolutionID != Output15kHz50) { // ED YUV 60
            setCsVsStart(16);                                        // ntsc
            setCsVsStop(13);                                         //
            GBS::IF_HB_ST::write(30);                                // 1_10; magic number
            GBS::IF_HBIN_ST::write(0x20);                            // 1_24
            GBS::IF_HBIN_SP::write(0x60);                            // 1_26
            // if (rto->presetID == 0x5) {                              // out 1080p
            if (rto->resolutionID == Output1080p) {                              // out 1080p
                GBS::IF_HB_SP2::write(0xB0);                         // 1_1a
                GBS::IF_HB_ST2::write(0x4BC);                        // 1_18
            // } else if (rto->presetID == 0x3) {                       // out 720p
            } else if (rto->resolutionID == Output720p) {                       // out 720p
                GBS::VDS_VSCALE::write(683);                         // same as base preset
                GBS::IF_HB_ST2::write(0x478);                        // 1_18
                GBS::IF_HB_SP2::write(0x84);                         // 1_1a
            // } else if (rto->presetID == 0x2) {                       // out x1024
            } else if (rto->resolutionID == Output1024p) {                       // out x1024
                GBS::IF_HB_SP2::write(0x84);                         // 1_1a
                GBS::IF_HB_ST2::write(0x478);                        // 1_18
            // } else if (rto->presetID == 0x1) {                       // out x960
            } else if (rto->resolutionID == Output960p) {                       // out x960
                GBS::IF_HB_SP2::write(0x84);                         // 1_1a
                GBS::IF_HB_ST2::write(0x478);                        // 1_18
            // } else if (rto->presetID == 0x3) {                       // out x480
            } else if (rto->resolutionID == Output720p) {                       // out x480
                GBS::IF_HB_ST2::write(0x478);                        // 1_18
                GBS::IF_HB_SP2::write(0x90);                         // 1_1a
            }
        // } else if (rto->videoStandardInput == 4 && rto->presetID != 0x16) { // ED YUV 50
        } else if (rto->videoStandardInput == 4 && rto->resolutionID != Output15kHz50) { // ED YUV 50
            GBS::IF_HBIN_SP::write(0x40);                                   // 1_26 was 0x80 test: ps2 videomodetester 576p mode
            GBS::IF_HBIN_ST::write(0x20);                                   // 1_24, odd but need to set this here (blue bar)
            GBS::IF_HB_ST::write(0x30);                                     // 1_10
            // if (rto->presetID == 0x15) {                                    // out 1080p
            if (rto->resolutionID == Output1080p50) {                                    // out 1080p
                GBS::IF_HB_ST2::write(0x4C0);                               // 1_18
                GBS::IF_HB_SP2::write(0xC8);                                // 1_1a
            // } else if (rto->presetID == 0x13) {                             // out 720p
            } else if (rto->resolutionID == Output720p50) {                             // out 720p
                GBS::IF_HB_ST2::write(0x478);                               // 1_18
                GBS::IF_HB_SP2::write(0x88);                                // 1_1a
            // } else if (rto->presetID == 0x12) {                             // out x1024
            } else if (rto->resolutionID == Output1024p50) {                             // out x1024
                // VDS_VB_SP -= 12 used to shift pic up, but seems not necessary anymore
                // GBS::VDS_VB_SP::write(GBS::VDS_VB_SP::read() - 12);
                GBS::IF_HB_ST2::write(0x454);   // 1_18
                GBS::IF_HB_SP2::write(0x88);    // 1_1a
            // } else if (rto->presetID == 0x11) { // out x960
            } else if (rto->resolutionID == Output960p50) { // out x960
                GBS::IF_HB_ST2::write(0x454);   // 1_18
                GBS::IF_HB_SP2::write(0x88);    // 1_1a
            // } else if (rto->presetID == 0x14) { // out x576
            } else if (rto->resolutionID == Output576p50) { // out x576
                GBS::IF_HB_ST2::write(0x478);   // 1_18
                GBS::IF_HB_SP2::write(0x90);    // 1_1a
            }
        } else if (rto->videoStandardInput == 5) { // 720p
            GBS::ADC_FLTR::write(1);               // 5_03
            GBS::IF_PRGRSV_CNTRL::write(1);        // progressive
            GBS::IF_HS_DEC_FACTOR::write(0);
            GBS::INPUT_FORMATTER_02::write(0x74);
            GBS::VDS_Y_DELAY::write(3);
        } else if (rto->videoStandardInput == 6 || rto->videoStandardInput == 7) { // 1080i/p
            GBS::ADC_FLTR::write(1);                                               // 5_03
            GBS::PLLAD_KS::write(0);                                               // 5_16
            GBS::IF_PRGRSV_CNTRL::write(1);
            GBS::IF_HS_DEC_FACTOR::write(0);
            GBS::INPUT_FORMATTER_02::write(0x74);
            GBS::VDS_Y_DELAY::write(3);
        } else if (rto->videoStandardInput == 8) { // 25khz
            // todo: this mode for HV sync
            uint32_t pllRate = 0;
            for (int i = 0; i < 8; i++) {
                pllRate += getPllRate();
            }
            pllRate /= 8;
            _WSF(PSTR("(H-PLL) rate: %l\n"), pllRate);
            if (pllRate > 200) {             // is PLL even working?
                if (pllRate < 1800) {        // rate very low?
                    GBS::PLLAD_FS::write(0); // then low gain
                }
            }
            GBS::PLLAD_ICP::write(6); // all 25khz submodes have more lines than NTSC
            GBS::ADC_FLTR::write(1);  // 5_03
            GBS::IF_HB_ST::write(30); // 1_10; magic number
            // GBS::IF_HB_ST2::write(0x60);  // 1_18
            // GBS::IF_HB_SP2::write(0x88);  // 1_1a
            GBS::IF_HBIN_SP::write(0x60); // 1_26 works for all output presets
            // if (rto->presetID == 0x1) {   // out x960
            if (rto->resolutionID == Output960p) {   // out x960
                GBS::VDS_VSCALE::write(410);
            // } else if (rto->presetID == 0x2) { // out x1024
            } else if (rto->resolutionID == Output1024p) { // out x1024
                GBS::VDS_VSCALE::write(402);
            // } else if (rto->presetID == 0x3) { // out 720p
            } else if (rto->resolutionID == Output720p) { // out 720p
                GBS::VDS_VSCALE::write(546);
            // } else if (rto->presetID == 0x5) { // out 1080p
            } else if (rto->resolutionID == Output1080p) { // out 1080p
                GBS::VDS_VSCALE::write(400);
            }
        }
    }

    // if (rto->presetID == 0x06 || rto->presetID == 0x16) {
    if (rto->resolutionID == Output15kHz || rto->resolutionID == Output15kHz50) {
        rto->isCustomPreset = GBS::GBS_PRESET_CUSTOM::read(); // override back
    }

    resetDebugPort();

    bool avoidAutoBest = 0;
    if (rto->syncTypeCsync) {
        if (GBS::TEST_BUS_2F::read() == 0) {
            delay(4);
            if (GBS::TEST_BUS_2F::read() == 0) {
                optimizeSogLevel();
                avoidAutoBest = 1;
                delay(4);
            }
        }
    }

    latchPLLAD(); // besthtotal reliable with this (EDTV modes, possibly others)

    if (rto->isCustomPreset) {
        // patch in segments not covered in custom preset files (currently seg 2)
        if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4 || rto->videoStandardInput == 8) {
            GBS::MADPT_Y_DELAY_UV_DELAY::write(1); // 2_17 : 1
        }

        // get OSR
        if (GBS::DEC1_BYPS::read() && GBS::DEC2_BYPS::read()) {
            rto->osr = 1;
        } else if (GBS::DEC1_BYPS::read() && !GBS::DEC2_BYPS::read()) {
            rto->osr = 2;
        } else {
            rto->osr = 4;
        }

        // always start with internal clock active first
        if (GBS::PLL648_CONTROL_01::read() == 0x75 && GBS::GBS_PRESET_DISPLAY_CLOCK::read() != 0) {
            GBS::PLL648_CONTROL_01::write(GBS::GBS_PRESET_DISPLAY_CLOCK::read());
        } else if (GBS::GBS_PRESET_DISPLAY_CLOCK::read() == 0) {
            _WSN(F("no stored display clock to use!"));
        }
    }

    if (rto->presetIsPalForce60) {
        if (GBS::GBS_OPTION_PALFORCED60_ENABLED::read() != 1) {
            _WSN(F("pal forced 60hz: apply vshift"));
            uint16_t vshift = 56; // default shift
            // if (rto->presetID == 0x5) {
            if (rto->resolutionID == Output1080p) {
                GBS::IF_VB_SP::write(4);
            } // out 1080p
            else {
                GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + vshift);
            }
            GBS::IF_VB_ST::write(GBS::IF_VB_SP::read() - 2);
            GBS::GBS_OPTION_PALFORCED60_ENABLED::write(1);
        }
    }

    // freezeVideo();

    GBS::ADC_TEST_04::write(0x02);    // 5_04
    GBS::ADC_TEST_0C::write(0x12);    // 5_0c 1 4
    GBS::ADC_TA_05_CTRL::write(0x02); // 5_05

    // auto ADC gain
    if (uopt->enableAutoGain == 1) {
        if (adco->r_gain == 0) {
            // SerialM.printf("ADC gain: reset %x := %x\n", GBS::ADC_RGCTRL::read(), AUTO_GAIN_INIT);
            setAdcGain(AUTO_GAIN_INIT);
            GBS::DEC_TEST_ENABLE::write(1);
        } else {
            // SerialM.printf("ADC gain: transferred %x := %x\n", GBS::ADC_RGCTRL::read(), adco->r_gain);
            GBS::ADC_RGCTRL::write(adco->r_gain);
            GBS::ADC_GGCTRL::write(adco->g_gain);
            GBS::ADC_BGCTRL::write(adco->b_gain);
            GBS::DEC_TEST_ENABLE::write(1);
        }
    } else {
        GBS::DEC_TEST_ENABLE::write(0); // no need for decimation test to be enabled
    }

    // ADC offset if measured
    if (adco->r_off != 0 && adco->g_off != 0 && adco->b_off != 0) {
        GBS::ADC_ROFCTRL::write(adco->r_off);
        GBS::ADC_GOFCTRL::write(adco->g_off);
        GBS::ADC_BOFCTRL::write(adco->b_off);
    }

    _WSF(PSTR("ADC offset: R: 0x%04X G: 0x%04X B: 0x%04X\n"),
        GBS::ADC_ROFCTRL::read(),
        GBS::ADC_GOFCTRL::read(),
        GBS::ADC_BOFCTRL::read());

    GBS::IF_AUTO_OFST_U_RANGE::write(0); // 0 seems to be full range, else 1 to 15
    GBS::IF_AUTO_OFST_V_RANGE::write(0); // 0 seems to be full range, else 1 to 15
    GBS::IF_AUTO_OFST_PRD::write(0);     // 0 = by line, 1 = by frame ; by line is easier to spot
    GBS::IF_AUTO_OFST_EN::write(0);      // not reliable yet
    // to get it working with RGB:
    // leave RGB to YUV at the ADC DEC stage (s5_1f 2 = 0)
    // s5s07s42, 1_2a to 0, s5_06 + s5_08 to 0x40
    // 5_06 + 5_08 will be the target center value, 5_07 sets general offset
    // s3s3as00 s3s3bs00 s3s3cs00

    if (uopt->wantVdsLineFilter) {
        GBS::VDS_D_RAM_BYPS::write(0);
    } else {
        GBS::VDS_D_RAM_BYPS::write(1);
    }

    if (uopt->wantPeaking) {
        GBS::VDS_PK_Y_H_BYPS::write(0);
    } else {
        GBS::VDS_PK_Y_H_BYPS::write(1);
    }

    // unused now
    GBS::VDS_TAP6_BYPS::write(0);
    /*if (uopt->wantTap6) { GBS::VDS_TAP6_BYPS::write(0); }
  else {
    GBS::VDS_TAP6_BYPS::write(1);
    if (!isCustomPreset) {
      GBS::MADPT_Y_DELAY_UV_DELAY::write(GBS::MADPT_Y_DELAY_UV_DELAY::read() + 1);
    }
  }*/

    if (uopt->wantStepResponse) {
        // step response requested, but exclude 1080p presets
        // if (rto->presetID != 0x05 && rto->presetID != 0x15) {
        if (rto->resolutionID != Output1080p && rto->resolutionID != Output1080p50) {
            GBS::VDS_UV_STEP_BYPS::write(0);
        } else {
            GBS::VDS_UV_STEP_BYPS::write(1);
        }
    } else {
        GBS::VDS_UV_STEP_BYPS::write(1);
    }

    // transfer preset's display clock to ext. gen
    externalClockGenResetClock();

    // unfreezeVideo();
    Menu::init();
    FrameSync::cleanup();
    rto->syncLockFailIgnore = 16;

    // undo eventual rto->useHdmiSyncFix (not using this method atm)
    GBS::VDS_SYNC_EN::write(0);
    GBS::VDS_FLOCK_EN::write(0);

    if (!rto->outModeHdBypass && rto->autoBestHtotalEnabled &&
        GBS::GBS_OPTION_SCALING_RGBHV::read() == 0 && !avoidAutoBest &&
        (rto->videoStandardInput >= 1 && rto->videoStandardInput <= 4)) {
        // autobesthtotal
        updateCoastPosition(0);
        delay(1);
        resetInterruptNoHsyncBadBit();
        resetInterruptSogBadBit();
        delay(10);
        // works reliably now on my test HDMI dongle
        if (rto->useHdmiSyncFix && !uopt->wantOutputComponent) {
            GBS::PAD_SYNC_OUT_ENZ::write(0); // sync out
        }
        delay(70); // minimum delay without random failures: TBD

        for (uint8_t i = 0; i < 4; i++) {
            if (GBS::STATUS_INT_SOG_BAD::read() == 1) {
                optimizeSogLevel();
                resetInterruptSogBadBit();
                delay(40);
            } else if (getStatus16SpHsStable() && getStatus16SpHsStable()) {
                delay(1); // wifi
                if (getVideoMode() == rto->videoStandardInput) {
                    bool ok = 0;
                    float sfr = getSourceFieldRate(0);
                    // _WSN(sfr, 3);
                    if (rto->videoStandardInput == 1 || rto->videoStandardInput == 3) {
                        if (sfr > 58.6f && sfr < 61.4f)
                            ok = 1;
                    } else if (rto->videoStandardInput == 2 || rto->videoStandardInput == 4) {
                        if (sfr > 49.1f && sfr < 51.1f)
                            ok = 1;
                    }
                    if (ok) { // else leave it for later
                        runAutoBestHTotal();
                        delay(1); // wifi
                        break;
                    }
                }
            }
            delay(10);
        }
    } else {
        // scaling rgbhv, HD modes, no autobesthtotal
        delay(10);
        // works reliably now on my test HDMI dongle
        if (rto->useHdmiSyncFix && !uopt->wantOutputComponent) {
            GBS::PAD_SYNC_OUT_ENZ::write(0); // sync out
        }
        delay(20);
        updateCoastPosition(0);
        updateClampPosition();
    }

    // _WS(F("pp time: ")); _WSN(millis() - postLoadTimer);

    // make sure
    if (rto->useHdmiSyncFix && !uopt->wantOutputComponent) {
        GBS::PAD_SYNC_OUT_ENZ::write(0); // sync out
    }

    // late adjustments that require some delay time first
    if (!rto->isCustomPreset) {
        if (videoStandardInputIsPalNtscSd() && !rto->outModeHdBypass) {
            // SNES has less total lines and a slight offset (only relevant in 60Hz)
            if (GBS::VPERIOD_IF::read() == 523) {
                GBS::IF_VB_SP::write(GBS::IF_VB_SP::read() + 4);
                GBS::IF_VB_ST::write(GBS::IF_VB_ST::read() + 4);
            }
        }
    }

    // new, might be useful (3_6D - 3_72)
    GBS::VDS_EXT_HB_ST::write(GBS::VDS_DIS_HB_ST::read());
    GBS::VDS_EXT_HB_SP::write(GBS::VDS_DIS_HB_SP::read());
    GBS::VDS_EXT_VB_ST::write(GBS::VDS_DIS_VB_ST::read());
    GBS::VDS_EXT_VB_SP::write(GBS::VDS_DIS_VB_SP::read());
    // VDS_VSYN_SIZE1 + VDS_VSYN_SIZE2 to VDS_VSYNC_RST + 2
    GBS::VDS_VSYN_SIZE1::write(GBS::VDS_VSYNC_RST::read() + 2);
    GBS::VDS_VSYN_SIZE2::write(GBS::VDS_VSYNC_RST::read() + 2);
    GBS::VDS_FRAME_RST::write(4); // 3_19
    // VDS_FRAME_NO, VDS_FR_SELECT
    GBS::VDS_FRAME_NO::write(1);  // 3_1f 0-3
    GBS::VDS_FR_SELECT::write(1); // 3_1b, 3_1c, 3_1d, 3_1e

    // noise starts here!
    resetDigital();

    resetPLLAD();             // also turns on pllad
    GBS::PLLAD_LEN::write(1); // 5_11 1

    if (!rto->isCustomPreset) {
        GBS::VDS_IN_DREG_BYPS::write(0); // 3_40 2 // 0 = input data triggered on falling clock edge, 1 = bypass
        GBS::PLLAD_R::write(3);
        GBS::PLLAD_S::write(3);
        GBS::PLL_R::write(1); // PLL lock detector skew
        GBS::PLL_S::write(2);
        GBS::DEC_IDREG_EN::write(1); // 5_1f 7
        GBS::DEC_WEN_MODE::write(1); // 5_1e 7 // 1 keeps ADC phase consistent. around 4 lock positions vs totally random

        // 4 segment
        GBS::CAP_SAFE_GUARD_EN::write(0); // 4_21_5 // does more harm than good
        // memory timings, anti noise
        GBS::PB_CUT_REFRESH::write(1);   // helps with PLL=ICLK mode artefacting
        GBS::RFF_LREQ_CUT::write(0);     // was in motionadaptive toggle function but on, off seems nicer
        GBS::CAP_REQ_OVER::write(0);     // 4_22 0  1=capture stop at hblank 0=free run
        GBS::CAP_STATUS_SEL::write(1);   // 4_22 1  1=capture request when FIFO 50%, 0= FIFO 100%
        GBS::PB_REQ_SEL::write(3);       // PlayBack 11 High request Low request
                                         // 4_2C, 4_2D should be set by preset
        GBS::RFF_WFF_OFFSET::write(0x0); // scanline fix
        if (rto->videoStandardInput == 3 || rto->videoStandardInput == 4) {
            // this also handles mode 14 csync rgbhv
            GBS::PB_CAP_OFFSET::write(GBS::PB_FETCH_NUM::read() + 4); // 4_37 to 4_39 (green bar)
        }
        // 4_12 should be set by preset
    }

    if (!rto->outModeHdBypass) {
        ResetSDRAM();
    }

    setAndUpdateSogLevel(rto->currentLevelSOG); // use this to cycle SP / ADPLL latches

    // if (rto->presetID != 0x06 && rto->presetID != 0x16) {
    if (rto->resolutionID != Output15kHz && rto->resolutionID != Output15kHz50) {
        // IF_VS_SEL = 1 for SD/HD SP mode in HD mode (5_3e 1)
        GBS::IF_VS_SEL::write(0); // 0 = "VCR" IF sync, requires VS_FLIP to be on, more stable?
        GBS::IF_VS_FLIP::write(1);
    }

    GBS::SP_CLP_SRC_SEL::write(0); // 0: 27Mhz clock; 1: pixel clock
    GBS::SP_CS_CLP_ST::write(32);
    GBS::SP_CS_CLP_SP::write(48); // same as reset parameters

    if (!uopt->wantOutputComponent) {
        GBS::PAD_SYNC_OUT_ENZ::write(0); // enable sync out if needed
    }
    GBS::DAC_RGBS_PWDNZ::write(1); // DAC on if needed
    GBS::DAC_RGBS_SPD::write(0);   // 0_45 2 DAC_SVM power down disable, somehow less jailbars
    GBS::DAC_RGBS_S0ENZ::write(0); //
    GBS::DAC_RGBS_S1EN::write(1);  // these 2 also help

    rto->useHdmiSyncFix = 0; // reset flag

    GBS::SP_H_PROTECT::write(0);
    if (rto->videoStandardInput >= 5) {
        GBS::SP_DIS_SUB_COAST::write(1); // might not disable it at all soon
    }

    if (rto->syncTypeCsync) {
        GBS::SP_EXT_SYNC_SEL::write(1); // 5_20 3 disconnect HV input
                                        // stays disconnected until reset condition
    }

    rto->coastPositionIsSet = false; // re-arm these
    rto->clampPositionIsSet = false;

    if (rto->outModeHdBypass) {
        GBS::INTERRUPT_CONTROL_01::write(0xff); // enable interrupts
        GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
        GBS::INTERRUPT_CONTROL_00::write(0x00);
        unfreezeVideo(); // eventhough not used atm
        // DAC and Sync out will be enabled later
        return; // to setOutModeHdBypass();
    }

    if (GBS::GBS_OPTION_SCALING_RGBHV::read() == 1) {
        rto->videoStandardInput = 14;
    }

    if (GBS::GBS_OPTION_SCALING_RGBHV::read() == 0) {
        unsigned long timeout = millis();
        while ((!getStatus16SpHsStable()) && (millis() - timeout < 2002)) {
            delay(4);
            wifiLoop(0);
            updateSpDynamic(0);
        }
        while ((getVideoMode() == 0) && (millis() - timeout < 1505)) {
            delay(4);
            wifiLoop(0);
            updateSpDynamic(0);
        }

        timeout = millis() - timeout;
        if (timeout > 1000) {
            _WSF(PSTR("to1 is: %l\n"), timeout);
        }
        if (timeout >= 1500) {
            if (rto->currentLevelSOG >= 7) {
                optimizeSogLevel();
                delay(300);
            }
        }
    }

    // early attempt
    updateClampPosition();
    if (rto->clampPositionIsSet) {
        if (GBS::SP_NO_CLAMP_REG::read() == 1) {
            GBS::SP_NO_CLAMP_REG::write(0);
        }
    }

    updateSpDynamic(0);

    if (!rto->syncWatcherEnabled) {
        GBS::SP_NO_CLAMP_REG::write(0);
    }

    // this was used with ADC write enable, producing about (exactly?) 4 lock positions
    // cycling through the phase let it land favorably
    // for (uint8_t i = 0; i < 8; i++) {
    //  advancePhase();
    //}

    setAndUpdateSogLevel(rto->currentLevelSOG); // use this to cycle SP / ADPLL latches
    // optimizePhaseSP();  // do this later in run loop

    GBS::INTERRUPT_CONTROL_01::write(0xff); // enable interrupts
    GBS::INTERRUPT_CONTROL_00::write(0xff); // reset irq status
    GBS::INTERRUPT_CONTROL_00::write(0x00);

    OutputComponentOrVGA();

    // presetPreference 10 means the user prefers bypass mode at startup
    // it's best to run a normal format detect > apply preset loop, then enter bypass mode
    // this can lead to an endless loop, so applyPresetDoneStage = 10 applyPresetDoneStage = 11
    // are introduced to break out of it.
    // also need to check for mode 15
    // also make sure to turn off autoBestHtotal
    // if (uopt->presetPreference == 10 && rto->videoStandardInput != 15) {
    if (rto->resolutionID == OutputBypass && rto->videoStandardInput != 15) {
        rto->autoBestHtotalEnabled = 0;
        if (rto->applyPresetDoneStage == 11) {
            // we were here before, stop the loop
            rto->applyPresetDoneStage = 1;
        } else {
            rto->applyPresetDoneStage = 10;
        }
    } else {
        // normal modes
        rto->applyPresetDoneStage = 1;
    }

    unfreezeVideo();

    if (uopt->enableFrameTimeLock) {
        activeFrameTimeLockInitialSteps();
    }

    _WS(F("\npreset applied: "));
    // if (rto->presetID == 0x1 || rto->presetID == 0x11)
    if (rto->resolutionID == Output960p || rto->resolutionID == Output960p50)
        _WS(F("1280x960"));
    // else if (rto->presetID == 0x2 || rto->presetID == 0x12)
    else if (rto->resolutionID == Output1024p || rto->resolutionID == Output1024p50)
        _WS(F("1280x1024"));
    // else if (rto->presetID == 0x3 || rto->presetID == 0x13)
    else if (rto->resolutionID == Output720p || rto->resolutionID == Output720p50)
        _WS(F("1280x720"));
    // else if (rto->presetID == 0x5 || rto->presetID == 0x15)
    else if (rto->resolutionID == Output1080p || rto->resolutionID == Output1080p50)
        _WS(F("1920x1080"));
    // else if (rto->presetID == 0x6 || rto->presetID == 0x16)
    else if (rto->resolutionID == Output15kHz || rto->resolutionID == Output15kHz50)
        _WS(F("downscale"));
    // else if (rto->presetID == 0x04)
    else if (rto->resolutionID == Output480p)
        _WS(F("720x480"));
    // else if (rto->presetID == 0x14)
    else if (rto->resolutionID == Output576p)
        _WS(F("720x576"));
    // else
    else if (rto->resolutionID == OutputBypass)
        _WS(F("passthrough"));
    else
        _WS(F("n/a"));

    if (rto->outModeHdBypass) {
        _WS(F(" (bypass)"));
    } else if (rto->isCustomPreset) {
        _WS(F(" (custom)"));
    }

    _WS(F(" for "));
    if (rto->videoStandardInput == 1)
        _WS(F("NTSC 60Hz "));
    else if (rto->videoStandardInput == 2)
        _WS(F("PAL 50Hz "));
    else if (rto->videoStandardInput == 3)
        _WS(F("EDTV 60Hz"));
    else if (rto->videoStandardInput == 4)
        _WS(F("EDTV 50Hz"));
    else if (rto->videoStandardInput == 5)
        _WS(F("720p 60Hz HDTV "));
    else if (rto->videoStandardInput == 6)
        _WS(F("1080i 60Hz HDTV "));
    else if (rto->videoStandardInput == 7)
        _WS(F("1080p 60Hz HDTV "));
    else if (rto->videoStandardInput == 8)
        _WS(F("Medium Res "));
    else if (rto->videoStandardInput == 13)
        _WS(F("VGA/SVGA/XGA/SXGA"));
    else if (rto->videoStandardInput == 14) {
        if (rto->syncTypeCsync)
            _WS(F("scaling RGB (CSync)"));
        else
            _WS(F("scaling RGB (HV Sync)"));
    } else if (rto->videoStandardInput == 15) {
        if (rto->syncTypeCsync)
            _WS(F("RGB Bypass (CSync)"));
        else
            _WS(F("RGB Bypass (HV Sync)"));
    } else if (rto->videoStandardInput == 0)
        _WS(F("!should not go here!"));             // TODO ???

    // if (rto->presetID == 0x05 || rto->presetID == 0x15) {
    if (rto->resolutionID == Output1080p || rto->resolutionID == Output1080p50) {
        _WS(F("(set your TV aspect ratio to 16:9!)"));
    }
    if (rto->videoStandardInput == 14) {
        _WS(F("\nNote: scaling RGB is still in development"));
    }
    // presetPreference = OutputCustomized may fail to load (missing) preset file and arrive here with defaults
    _WSN();
}

/**
 * @brief
 *
 * @param videoMode
 */
void applyPresets(uint8_t videoMode)
{
// TODO replace videoMode with VideoStandardInput enum
    if (!rto->boardHasPower) {
        _WSN(F("GBS board not responding!"));
        return;
    }

    // if RGBHV scaling and invoked through web ui or custom preset
    // need to know syncTypeCsync
    if (videoMode == 14) {
        if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
            rto->inputIsYPbPr = 0;
            if (GBS::STATUS_SYNC_PROC_VSACT::read() == 0) {
                rto->syncTypeCsync = 1;
            } else {
                rto->syncTypeCsync = 0;
            }
        }
    }

    bool waitExtra = 0;
    if (rto->outModeHdBypass || rto->videoStandardInput == 15 || rto->videoStandardInput == 0) {
        waitExtra = 1;
        if (videoMode <= 4 || videoMode == 14 || videoMode == 8 || videoMode == 9) {
            GBS::SFTRST_IF_RSTZ::write(1); // early init
            GBS::SFTRST_VDS_RSTZ::write(1);
            GBS::SFTRST_DEC_RSTZ::write(1);
        }
    }
    rto->presetIsPalForce60 = 0; // the default
    rto->outModeHdBypass = 0;    // the default at this stage

    // in case it is set; will get set appropriately later in doPostPresetLoadSteps()
    GBS::GBS_PRESET_CUSTOM::write(0);
    rto->isCustomPreset = false;

    // carry over debug view if possible
    if (GBS::ADC_UNUSED_62::read() != 0x00) {
        // only if the preset to load isn't custom
        // (else the command will instantly disable debug view)
        // if (rto->presetID != OutputCustom) {
        // if (rto->resolutionID != OutputCustom) {
            serialCommand = 'D';    // debug view
        // }
    }

    if (videoMode == 0) {
        // Unknown
        _WSN(F("Source format not properly recognized, using active resolution"));
        videoMode = 3;                   // in case of success: override to 480p60
        GBS::ADC_INPUT_SEL::write(1); // RGB
        delay(100);
        if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
            rto->inputIsYPbPr = 0;
            rto->syncWatcherEnabled = 1;
            if (GBS::STATUS_SYNC_PROC_VSACT::read() == 0) {
                rto->syncTypeCsync = 1;
            } else {
                rto->syncTypeCsync = 0;
            }
        } else {
            GBS::ADC_INPUT_SEL::write(0); // YPbPr
            delay(100);
            if (GBS::STATUS_SYNC_PROC_HSACT::read() == 1) {
                rto->inputIsYPbPr = 1;
                rto->syncTypeCsync = 1;
                rto->syncWatcherEnabled = 1;
            } else {
                // found nothing at all, turn off output

                // If we call setResetParameters(), soon afterwards loop() ->
                // inputAndSyncDetect() -> goLowPowerWithInputDetection() will
                // call setResetParameters() again. But if we don't call
                // setResetParameters() here, the second call will never happen.
                setResetParameters();

                // Deselect the output resolution in the web UI. We cannot call
                // doPostPresetLoadSteps() to select the right resolution, since
                // it *enables* the output (showing a green screen) even if
                // previously disabled, and we want to *disable* it.
                // rto->presetID = OutputNone;
                /// @sk: leaving Bypass
                // rto->resolutionID = OutputBypass;
                return;
            }
        }
    }

    if (uopt->PalForce60 == 1) {
        // if (uopt->presetPreference != 2) { // != custom. custom saved as pal preset has ntsc customization
        // if (rto->resolutionID != OutputCustom) { // != custom. custom saved as pal preset has ntsc customization
            if (videoMode == 2 || videoMode == 4) {
                _WSN(F("PAL@50 to 60Hz"));
                rto->presetIsPalForce60 = 1;
            }
            if (videoMode == 2) {
                videoMode = 1;
            }
            if (videoMode == 4) {
                videoMode = 3;
            }
        // }
    }

    /// If uopt->presetPreference == OutputCustomized and we load a custom
    /// preset, check if it's intended to bypass scaling at the current input
    /// resolution. If so, setup bypass and skip the rest of applyPresets().
    auto applySavedBypassPreset = [&videoMode]() -> bool {
        /*
        Types:

        - Registers:
            - Written by applyPresets() -> writeProgramArrayNew(),
              loadHdBypassSection(), etc.
            - GBS_PRESET_ID @ S1_2B[0:6]
            - GBS_PRESET_CUSTOM @ S1_2B[7]
        - uopt is source of truth, rto is derived cached state???
        - uopt->presetPreference
            - Read by applyPresets() to pick an output resolution.
        - rto->presetID
            - Written by applyPresets() -> doPostPresetLoadSteps().
            - = register GBS_PRESET_ID.
        - rto->isCustomPreset
            - Written by applyPresets() -> doPostPresetLoadSteps().
            - = register GBS_PRESET_CUSTOM.
        - rto->isCustomPreset and rto->presetID control which button is
            highlighted in the web UI (updateWebSocketData() ->
            GBSControl.buttonMapping).

        Control flow:

        applyPresets():
        - If uopt->presetPreference == OutputCustomized (yes):
            - loadPresetFromLFS()
                - All custom presets are saved with GBS_PRESET_CUSTOM = 1.
            - writeProgramArrayNew()
                - GBS_PRESET_ID = output resolution ID
                - GBS_PRESET_CUSTOM = 1
            - applySavedBypassPreset():
            - If GBS_PRESET_ID == PresetHdBypass (yes):
                - rto->videoStandardInput = videoMode; (not sure why)
                - setOutModeHdBypass(regsInitialized=true)
                    - rto->outModeHdBypass = 1;
                    - loadHdBypassSection()
                        - Overwrites S1_30..5F.
                    - GBS::GBS_PRESET_ID::write(PresetHdBypass);
                    - if (!regsInitialized) (false)
                        - ~~GBS::GBS_PRESET_CUSTOM::write(0);~~ (skipped)
                    - doPostPresetLoadSteps()
                        - rto->presetID = GBS::GBS_PRESET_ID::read();
                            - PresetHdBypass
                        - rto->isCustomPreset = GBS::GBS_PRESET_CUSTOM::read();
                            - true
                        - Branches based on rto->presetID
                        - if (rto->outModeHdBypass) (yes) return.
                    - ...
                    - rto->outModeHdBypass = 1; (again?!)
        */

        uint8_t rawPresetId = GBS::GBS_PRESET_ID::read();
        if (rawPresetId == PresetHdBypass) {
            // Required for switching from 240p to 480p to work.
            rto->videoStandardInput = videoMode;

            // Setup video mode passthrough.
            setOutModeHdBypass(true);
            _DBGN(F("setting OutputBypass mode"));
            return true;
        }
        if (rawPresetId == PresetBypassRGBHV) {
            // TODO implement bypassModeSwitch_RGBHV (I don't have RGBHV inputs to verify)
        }
        return false;
    };

    if (videoMode == 1 || videoMode == 3 || videoMode == 8 || videoMode == 9 || videoMode == 14) {
        // NTSC input
        // if (uopt->presetPreference == 0) {
        if (rto->resolutionID == OutputNone) {                  // TODO is this OutputBypass ???
            writeProgramArrayNew(ntsc_240p, false);
            _WSN(F("ntsc_240p is now active"));
        // } else if (uopt->presetPreference == 1) {
        } else if (rto->resolutionID == Output480p) {
            writeProgramArrayNew(ntsc_720x480, false);
            _WSN(F("ntsc_720x480 is now active"));
        // } else if (uopt->presetPreference == 3) {
        } else if (rto->resolutionID == Output720p) {
            writeProgramArrayNew(ntsc_1280x720, false);
            _WSN(F("ntsc_1280x720 is now active"));
        }
#if defined(ESP8266)
        // else if (uopt->presetPreference == OutputCustomized) {
        /* else if (rto->resolutionID == OutputCustom) {*/
        if (rto->resolutionID != OutputBypass
            && rto->resolutionID != PresetHdBypass
                && rto->resolutionID != PresetBypassRGBHV) {
            const uint8_t *preset = loadPresetFromLFS(videoMode);
            writeProgramArrayNew(preset, false);
            _WSF(PSTR("Bypass* preset: %d is now active\n"), preset);
            if (applySavedBypassPreset()) {
                return;
            }
        }
        // } else if (uopt->presetPreference == 4) {
        else if (rto->resolutionID == Output1024p) {
            if (uopt->matchPresetSource && (videoMode != 8) && (GBS::GBS_OPTION_SCALING_RGBHV::read() == 0)) {
                writeProgramArrayNew(ntsc_240p, false); // pref = x1024 override to x960
                _WSN(F("ntsc_240p is now active"));
            } else {
                writeProgramArrayNew(ntsc_1280x1024, false);
                _WSN(F("ntsc_1280x1024 is now active"));
            }
        }
#endif
        // else if (uopt->presetPreference == 5) {
        else if (rto->resolutionID == Output1080p) {
            writeProgramArrayNew(ntsc_1920x1080, false);
            _WSN(F("ntsc_1920x1080 is now active"));
        // } else if (uopt->presetPreference == 6) {
        } else if (rto->resolutionID == Output15kHz) {
            writeProgramArrayNew(ntsc_downscale, false);
            _WSN(F("ntsc_downscale is now active"));
        }

    } else if (videoMode == 2 || videoMode == 4) {

        // PAL input
        // if (uopt->presetPreference == 0) {
        if (rto->resolutionID == OutputNone) {
            if (uopt->matchPresetSource) {
                writeProgramArrayNew(pal_1280x1024, false); // pref = x960 override to x1024
                _WSN(F("pal_1280x1024 is now active"));
            } else {
                writeProgramArrayNew(pal_240p, false);
                _WSN(F("pal_240p is now active"));
            }
        // } else if (uopt->presetPreference == 1) {
        } else if (rto->resolutionID == Output576p) {
            writeProgramArrayNew(pal_768x576, false);
            _WSN(F("pal_768x576 is now active"));
        // } else if (uopt->presetPreference == 3) {
        } else if (rto->resolutionID == Output720p) {
            writeProgramArrayNew(pal_1280x720, false);
            _WSN(F("pal_1280x720 is now active"));
        }
#if defined(ESP8266)
        // else if (uopt->presetPreference == OutputCustomized) {
        /* else if (rto->resolutionID == OutputCustom) {*/
        if (rto->resolutionID != OutputBypass
            && rto->resolutionID != PresetHdBypass
                && rto->resolutionID != PresetBypassRGBHV) {
            const uint8_t *preset = loadPresetFromLFS(videoMode);
            writeProgramArrayNew(preset, false);
            _WSF(PSTR("Bypass* preset: %d is now active\n"), preset);
            if (applySavedBypassPreset()) {
                return;
            }
        }
        // } else if (uopt->presetPreference == 4) {
        else if (rto->resolutionID == Output1024p) {
            writeProgramArrayNew(pal_1280x1024, false);
            _WSN(F("pal_1280x1024 is now active"));
        }
#endif
        // else if (uopt->presetPreference == 5) {
        else if (rto->resolutionID == Output1080p) {
            writeProgramArrayNew(pal_1920x1080, false);
            _WSN(F("pal_1920x1080 is now active"));
        // } else if (uopt->presetPreference == 6) {
        } else if (rto->resolutionID == Output15kHz) {
            writeProgramArrayNew(pal_downscale, false);
            _WSN(F("pal_downscale is now active"));
        }
    } else if (videoMode == 5 || videoMode == 6 || videoMode == 7 || videoMode == 13) {
        // use bypass mode for these HD sources
        rto->videoStandardInput = videoMode;
        setOutModeHdBypass(false);
        return;
    } else if (videoMode == 15) {
        _WS(F("RGB/HV "));
        if (rto->syncTypeCsync) {
            _WS(F("(CSync) "));
        }
        // if (uopt->preferScalingRgbhv) {
        //   _WS(F("(prefer scaling mode)");
        // }
        _WSN();
        bypassModeSwitch_RGBHV();
        // don't go through doPostPresetLoadSteps
        return;
    }

    rto->videoStandardInput = videoMode;
    if (waitExtra) {
        // extra time needed for digital resets, so that autobesthtotal works first attempt
        // _WSN("waitExtra 400ms");
        delay(400); // min ~ 300
    }
    doPostPresetLoadSteps();
}

/**
 * @brief
 *
 */
void loadPresetDeinterlacerSection()
{
    uint16_t index = 0;
    uint8_t bank[16];
    writeOneByte(0xF0, 2);
    for (int j = 0; j <= 3; j++) { // start at 0x00
        copyBank(bank, presetDeinterlacerSection, &index);
        writeBytes(j * 16, bank, 16);
    }
}




#if defined(ESP8266)

/**
 * @brief Load preset preferences from "preset.slot" file
 *
 * @param forVideoMode
 * @return const uint8_t*
 */
const uint8_t * loadPresetFromLFS(byte forVideoMode)
{
    static uint8_t preset[432];
    String s = "";
    char slot = '0';
    char buffer[32] = "";
    fs::File f;

    f = LittleFS.open(FPSTR(preferencesFile), "r");
    if (f) {
        // uint8_t result[3];
        // todo: move file cursor manually
        // result[0] = f.read();   // slotID
        // result[1] = f.read();
        // result[2] = f.read();

        slot = static_cast<char>(f.read());
        f.close();
        _WSF(PSTR("current slot: %c\n"), &slot);
    } else {
        // file not found, we don't know what preset to load
        _WSN(F("please select a preset slot first!")); // say "slot" here to make people save usersettings
        if (forVideoMode == 2 || forVideoMode == 4)
            return pal_240p;
        else
            return ntsc_240p;
    }

    _WSF(PSTR("loading from preset slot %c: "), slot);

    if (forVideoMode == 1) {
        // strcpy(buffer, PSTR("/preset_ntsc."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_ntsc.%02x"), static_cast<uint8_t>(slot));
    } else if (forVideoMode == 2) {
        // strcpy(buffer, PSTR("/preset_pal."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_pal.%02x"), static_cast<uint8_t>(slot));
    } else if (forVideoMode == 3) {
        // strcpy(buffer, PSTR("/preset_ntsc_480p."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_ntsc_480p.%02x"), static_cast<uint8_t>(slot));
    } else if (forVideoMode == 4) {
        // strcpy(buffer, PSTR("/preset_pal_576p."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_pal_576p.%02x"), static_cast<uint8_t>(slot));
    } else if (forVideoMode == 5) {
        // strcpy(buffer, PSTR("/preset_ntsc_720p."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_ntsc_720p.%02x"), static_cast<uint8_t>(slot));
    } else if (forVideoMode == 6) {
        // strcpy(buffer, PSTR("/preset_ntsc_1080p."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_ntsc_1080p.%02x"), static_cast<uint8_t>(slot));
    } else if (forVideoMode == 8) {
        // strcpy(buffer, PSTR("/preset_medium_res."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_medium_res.%02x"), static_cast<uint8_t>(slot));
    } else if (forVideoMode == 14) {
        // strcpy(buffer, PSTR("/preset_vga_upscale."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_vga_upscale.%02x"), static_cast<uint8_t>(slot));
    } else if (forVideoMode == 0) {
        // strcpy(buffer, PSTR("/preset_unknown."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_unknown.%02x"), static_cast<uint8_t>(slot));
    }

    // reading preset data
    f = LittleFS.open(buffer, "r");
    if (!f) {
        _WSN(F("no preset file for this slot and source"));
        if (forVideoMode == 2 || forVideoMode == 4)
            return pal_240p;
        else
            return ntsc_240p;
    } else {
        _WSF(PSTR("reading preset data from: %s\n"), f.name());
        s = f.readStringUntil('}');
        f.close();
    }

    char *tmp;
    uint16_t i = 0;
    tmp = strtok(&s[0], ",");
    while (tmp) {
        preset[i++] = (uint8_t)atoi(tmp);
        tmp = strtok(NULL, ",");
        yield(); // wifi stack
    }

    return preset;
}

/**
 * @brief Save "preset.slot" data. If preference file exists it will be overriden
 *
 */
void savePresetToFS()
{
    uint8_t readout = 0;
    fs::File f;
    char slot = '0';
    char buffer[32] = "";

    // first figure out if the user has set a preferenced slot
    f = LittleFS.open(FPSTR(preferencesFile), "r");
    if (f) {
        // uint8_t result[3];
        // todo: move file cursor manually
        // result[0] = f.read();       // slotID
        // result[1] = f.read();
        // result[2] = f.read();

        slot = static_cast<char>(f.read()); // got the slot to save to now
        f.close();
        _WSF(PSTR("current slot: %c\n"), &slot);
    } else {
        // file not found, we don't know where to save this preset
        _WSN(F("please select a preset slot first!"));
        return;
    }

    _WSF(PSTR("saving to preset slot %c: "), slot);

    if (rto->videoStandardInput == 1) {
        // strcpy(buffer, PSTR("/preset_ntsc."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_ntsc.%02x"), static_cast<uint8_t>(slot));
    } else if (rto->videoStandardInput == 2) {
        // strcpy(buffer, PSTR("/preset_pal."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_pal.%02x"), static_cast<uint8_t>(slot));
    } else if (rto->videoStandardInput == 3) {
        // strcpy(buffer, PSTR("/preset_ntsc_480p."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_ntsc_480p.%02x"), static_cast<uint8_t>(slot));
    } else if (rto->videoStandardInput == 4) {
        // strcpy(buffer, PSTR("/preset_pal_576p."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_pal_576p.%02x"), static_cast<uint8_t>(slot));
    } else if (rto->videoStandardInput == 5) {
        // strcpy(buffer, PSTR("/preset_ntsc_720p."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_ntsc_720p.%02x"), static_cast<uint8_t>(slot));
    } else if (rto->videoStandardInput == 6) {
        // strcpy(buffer, PSTR("/preset_ntsc_1080p."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_ntsc_1080p.%02x"), static_cast<uint8_t>(slot));
    } else if (rto->videoStandardInput == 8) {
        // strcpy(buffer, PSTR("/preset_medium_res."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_medium_res.%02x"), static_cast<uint8_t>(slot));
    } else if (rto->videoStandardInput == 14) {
        // strcpy(buffer, PSTR("/preset_vga_upscale."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_vga_upscale.%02x"), static_cast<uint8_t>(slot));
    } else if (rto->videoStandardInput == 0) {
        // strcpy(buffer, PSTR("/preset_unknown."));
        // strcat(buffer, &slot);
        sprintf(buffer, PSTR("/preset_unknown.%02x"), static_cast<uint8_t>(slot));
    }

    // open preset file
    f = LittleFS.open(buffer, "w");
    if (!f) {
        _WSF(PSTR("failed to open preset file: %s\n"), buffer);
    } else {
        _WSF(PSTR("reading preset file: %s\n"), buffer);

        GBS::GBS_PRESET_CUSTOM::write(1); // use one reserved bit to mark this as a custom preset
        // don't store scanlines
        if (GBS::GBS_OPTION_SCANLINES_ENABLED::read() == 1) {
            disableScanlines();
        }

        if (!rto->extClockGenDetected) {
            if (uopt->enableFrameTimeLock && FrameSync::getSyncLastCorrection() != 0) {
                FrameSync::reset(uopt->frameTimeLockMethod);
            }
        }

        for (int i = 0; i <= 5; i++) {
            writeOneByte(0xF0, i);
            switch (i) {
                case 0:
                    for (int x = 0x40; x <= 0x5F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    for (int x = 0x90; x <= 0x9F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 1:
                    for (int x = 0x0; x <= 0x2F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 2:
                    // not needed anymore
                    break;
                case 3:
                    for (int x = 0x0; x <= 0x7F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 4:
                    for (int x = 0x0; x <= 0x5F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
                case 5:
                    for (int x = 0x0; x <= 0x6F; x++) {
                        readFromRegister(x, 1, &readout);
                        f.print(readout);
                        f.println(",");
                    }
                    break;
            }
        }
        // EOF "byte"
        f.println("};");
        _WSF(PSTR("preset saved as: %s\n"), f.name());
        f.close();
    }
}

/**
 * @brief Save/Update user preferences into preferencesFile
 *
 */
void saveUserPrefs()
{
    fs::File f = LittleFS.open(FPSTR(preferencesFile), "w");
    if (!f) {
        _WSF(PSTR("saveUserPrefs: open %s file failed\n"), FPSTR(preferencesFile));
        return;
    }
    // !###############################
    // ! preferencesv2.txt structure
    // !###############################
    // f.write(uopt->presetPreference + '0'); // #1
    f.write(uopt->presetSlot);
    f.write(uopt->enableFrameTimeLock + '0');
    f.write(uopt->frameTimeLockMethod + '0');
    f.write(uopt->enableAutoGain + '0');
    f.write(uopt->wantScanlines + '0');
    f.write(uopt->wantOutputComponent + '0');
    f.write(uopt->deintMode + '0');
    f.write(uopt->wantVdsLineFilter + '0');
    f.write(uopt->wantPeaking + '0');
    f.write(uopt->preferScalingRgbhv + '0');
    f.write(uopt->wantTap6 + '0');
    f.write(uopt->PalForce60 + '0');
    f.write(uopt->matchPresetSource + '0');             // #14
    f.write(uopt->wantStepResponse + '0');              // #15
    f.write(uopt->wantFullHeight + '0');                // #16
    f.write(uopt->enableCalibrationADC + '0');          // #17
    f.write(uopt->scanlineStrength + '0');              // #18
    f.write(uopt->disableExternalClockGenerator + '0'); // #19


    f.close();
}

#endif                                  // defined(ESP8266)