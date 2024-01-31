#ifndef TV5725_H_
#define TV5725_H_

#include "tw.h"

#define GBS_ADDR 0x17 // 7 bit GBS I2C Address

namespace detail
{
    struct TVAttrs
    {
        // Segment register at 0xf0, no bit offset, 8 bits, initial value assumed invalid
        static const uint8_t SegByteOffset = 0xf0;
        static const uint8_t SegBitOffset = 0;
        static const uint8_t SegBitWidth = 8;
        static const uint8_t SegInitial = 0xff;
    };
} // namespace detail

template <uint8_t Addr>
class TV5725 : public tw::SegmentedSlave<Addr, detail::TVAttrs>
{
private:
    // Stupid template boilerplate
    typedef tw::SegmentedSlave<Addr, detail::TVAttrs> Base;
    using typename Base::SegValue;
    template <SegValue Seg, uint8_t ByteOffset, uint8_t BitOffset, uint8_t BitWidth, tw::Signage Signed>
    using Register = typename Base::template Register<Seg, ByteOffset, BitOffset, BitWidth, Signed>;
    template <SegValue Seg, uint8_t ByteOffset, uint8_t BitOffset, uint8_t BitWidth>
    using UReg = Register<Seg, ByteOffset, BitOffset, BitWidth, tw::Signage::UNSIGNED>;

public:
    // STATUS Registers
    // Arbitary names for STATUS_IF register
    typedef UReg<0x00, 0x00, 0, 8> STATUS_00; // whole register for convenience
    typedef UReg<0x00, 0x00, 0, 1> STATUS_IF_VT_OK;
    typedef UReg<0x00, 0x00, 1, 1> STATUS_IF_HT_OK;
    typedef UReg<0x00, 0x00, 2, 1> STATUS_IF_HVT_OK;
    typedef UReg<0x00, 0x00, 3, 1> STATUS_IF_INP_NTSC_INT;
    typedef UReg<0x00, 0x00, 4, 1> STATUS_IF_INP_NTSC_PRG;
    typedef UReg<0x00, 0x00, 5, 1> STATUS_IF_INP_PAL_INT;
    typedef UReg<0x00, 0x00, 6, 1> STATUS_IF_INP_PAL_PRG;
    typedef UReg<0x00, 0x00, 7, 1> STATUS_IF_INP_SD;
    typedef UReg<0x00, 0x01, 0, 1> STATUS_IF_INP_VGA60;
    typedef UReg<0x00, 0x01, 1, 1> STATUS_IF_INP_VGA75;
    typedef UReg<0x00, 0x01, 2, 1> STATUS_IF_INP_VGA86;
    typedef UReg<0x00, 0x01, 3, 1> STATUS_IF_INP_VGA;
    typedef UReg<0x00, 0x01, 4, 1> STATUS_IF_INP_SVGA60;
    typedef UReg<0x00, 0x01, 5, 1> STATUS_IF_INP_SVGA75;
    typedef UReg<0x00, 0x01, 6, 1> STATUS_IF_INP_SVGA85;
    typedef UReg<0x00, 0x01, 7, 1> STATUS_IF_INP_SVGA;
    typedef UReg<0x00, 0x02, 0, 1> STATUS_IF_INP_XGA60;
    typedef UReg<0x00, 0x02, 1, 1> STATUS_IF_INP_XGA70;
    typedef UReg<0x00, 0x02, 2, 1> STATUS_IF_INP_XGA75;
    typedef UReg<0x00, 0x02, 3, 1> STATUS_IF_INP_XGA85;
    typedef UReg<0x00, 0x02, 4, 1> STATUS_IF_INP_XGA;
    typedef UReg<0x00, 0x02, 5, 1> STATUS_IF_INP_SXGA60;
    typedef UReg<0x00, 0x02, 6, 1> STATUS_IF_INP_SXGA75;
    typedef UReg<0x00, 0x02, 7, 1> STATUS_IF_INP_SXGA85;
    typedef UReg<0x00, 0x03, 0, 8> STATUS_03; // whole register for convenience
    typedef UReg<0x00, 0x03, 0, 1> STATUS_IF_INP_SXGA;
    typedef UReg<0x00, 0x03, 1, 1> STATUS_IF_INP_PC;
    typedef UReg<0x00, 0x03, 2, 1> STATUS_IF_INP_720P50;
    typedef UReg<0x00, 0x03, 3, 1> STATUS_IF_INP_720P60;
    typedef UReg<0x00, 0x03, 4, 1> STATUS_IF_INP_720;
    typedef UReg<0x00, 0x03, 5, 1> STATUS_IF_INP_2200_1125I;
    typedef UReg<0x00, 0x03, 6, 1> STATUS_IF_INP_2376_1250I;
    typedef UReg<0x00, 0x03, 7, 1> STATUS_IF_INP_2640_1125I;
    typedef UReg<0x00, 0x04, 0, 8> STATUS_04; // whole register for convenience
    typedef UReg<0x00, 0x04, 0, 1> STATUS_IF_INP_1080I;
    typedef UReg<0x00, 0x04, 1, 1> STATUS_IF_INP_2200_1125P;
    typedef UReg<0x00, 0x04, 2, 1> STATUS_IF_INP_2376_1250P;
    typedef UReg<0x00, 0x04, 3, 1> STATUS_IF_INP_2640_1125P;
    typedef UReg<0x00, 0x04, 4, 1> STATUS_IF_INP_1808P;
    typedef UReg<0x00, 0x04, 5, 1> STATUS_IF_INP_HD;
    typedef UReg<0x00, 0x04, 6, 1> STATUS_IF_INP_INT;
    typedef UReg<0x00, 0x04, 7, 1> STATUS_IF_INP_PRG;
    typedef UReg<0x00, 0x05, 0, 8> STATUS_05; // whole register for convenience
    typedef UReg<0x00, 0x05, 0, 1> STATUS_IF_INP_USER;
    typedef UReg<0x00, 0x05, 1, 1> STATUS_IF_NO_SYNC;
    typedef UReg<0x00, 0x05, 2, 1> STATUS_IF_HT_BAD;
    typedef UReg<0x00, 0x05, 3, 1> STATUS_IF_VT_BAD;
    typedef UReg<0x00, 0x05, 4, 1> STATUS_IF_INP_SW;

    typedef UReg<0x00, 0x06, 0, 9> HPERIOD_IF;
    typedef UReg<0x00, 0x07, 1, 11> VPERIOD_IF;

    typedef UReg<0x00, 0x09, 6, 1> STATUS_MISC_PLL648_LOCK;
    typedef UReg<0x00, 0x09, 7, 1> STATUS_MISC_PLLAD_LOCK;
    typedef UReg<0x00, 0x0A, 0, 1> STATUS_MISC_PIP_EN_V;
    typedef UReg<0x00, 0x0A, 1, 1> STATUS_MISC_PIP_EN_H;
    typedef UReg<0x00, 0x0A, 4, 1> STATUS_MISC_VBLK;
    typedef UReg<0x00, 0x0A, 5, 1> STATUS_MISC_HBLK;
    typedef UReg<0x00, 0x0A, 6, 1> STATUS_MISC_VSYNC;
    typedef UReg<0x00, 0x0A, 7, 1> STATUS_MISC_HSYNC;

    typedef UReg<0x00, 0x0B, 0, 8> CHIP_ID_FOUNDRY;
    typedef UReg<0x00, 0x0C, 0, 8> CHIP_ID_PRODUCT;
    typedef UReg<0x00, 0x0D, 0, 8> CHIP_ID_REVISION;

    typedef UReg<0x00, 0x0E, 0, 1> STATUS_GPIO_GPIO;
    typedef UReg<0x00, 0x0E, 1, 1> STATUS_GPIO_HALF;
    typedef UReg<0x00, 0x0E, 2, 1> STATUS_GPIO_SCLSA;
    typedef UReg<0x00, 0x0E, 3, 1> STATUS_GPIO_MBA;
    typedef UReg<0x00, 0x0E, 4, 1> STATUS_GPIO_MCS1;
    typedef UReg<0x00, 0x0E, 5, 1> STATUS_GPIO_HBOUT;
    typedef UReg<0x00, 0x0E, 6, 1> STATUS_GPIO_VBOUT;
    typedef UReg<0x00, 0x0E, 7, 1> STATUS_GPIO_CLKOUT;

    typedef UReg<0x00, 0x0F, 0, 8> STATUS_0F; // whole register for convenience
    typedef UReg<0x00, 0x0F, 0, 1> STATUS_INT_SOG_BAD;
    typedef UReg<0x00, 0x0F, 1, 1> STATUS_INT_SOG_SW;
    typedef UReg<0x00, 0x0F, 2, 1> STATUS_INT_SOG_OK;
    typedef UReg<0x00, 0x0F, 3, 1> STATUS_INT_INP_SW;
    typedef UReg<0x00, 0x0F, 4, 1> STATUS_INT_INP_NO_SYNC;
    typedef UReg<0x00, 0x0F, 5, 1> STATUS_INT_INP_HSYNC;
    typedef UReg<0x00, 0x0F, 6, 1> STATUS_INT_INP_VSYNC;
    typedef UReg<0x00, 0x0F, 7, 1> STATUS_INT_INP_CSYNC; //Needs confirmation

    typedef UReg<0x00, 0x10, 0, 4> STATUS_VDS_FR_NUM;
    typedef UReg<0x00, 0x10, 4, 1> STATUS_VDS_OUT_VSYNC;
    typedef UReg<0x00, 0x10, 5, 1> STATUS_VDS_OUT_HSYNC;
    typedef UReg<0x00, 0x11, 0, 1> STATUS_VDS_FIELD;
    typedef UReg<0x00, 0x11, 1, 1> STATUS_VDS_OUT_BLANK;
    typedef UReg<0x00, 0x11, 4, 11> STATUS_VDS_VERT_COUNT;

    typedef UReg<0x00, 0x13, 0, 1> STATUS_MEM_FF_WFF_FIFO_FULL;
    typedef UReg<0x00, 0x13, 1, 1> STATUS_MEM_FF_WFF_FIFO_EMPTY;
    typedef UReg<0x00, 0x13, 2, 1> STATUS_MEM_FF_RFF_FIFO_FULL;
    typedef UReg<0x00, 0x13, 3, 1> STATUS_MEM_FF_RFF_FIFO_EMPTY;
    typedef UReg<0x00, 0x13, 4, 1> STATUS_MEM_FF_CAP_FIFO_FULL;
    typedef UReg<0x00, 0x13, 5, 1> STATUS_MEM_FF_CAP_FIFO_EMPTY;
    typedef UReg<0x00, 0x13, 6, 1> STATUS_MEM_FF_PLY_FIFO_FULL;
    typedef UReg<0x00, 0x13, 7, 1> STATUS_MEM_FF_PLY_FIFO_EMPTY;
    typedef UReg<0x00, 0x14, 0, 1> STATUS_MEM_FF_EXT_FIN;

    typedef UReg<0x00, 0x15, 7, 1> STATUS_DEINT_PULLDN;

    typedef UReg<0x00, 0x16, 0, 8> STATUS_16; // whole register for convenience
    typedef UReg<0x00, 0x16, 0, 1> STATUS_SYNC_PROC_HSPOL;
    typedef UReg<0x00, 0x16, 1, 1> STATUS_SYNC_PROC_HSACT;
    typedef UReg<0x00, 0x16, 2, 1> STATUS_SYNC_PROC_VSPOL;
    typedef UReg<0x00, 0x16, 3, 1> STATUS_SYNC_PROC_VSACT;
    typedef UReg<0x00, 0x17, 0, 12> STATUS_SYNC_PROC_HTOTAL;
    typedef UReg<0x00, 0x19, 0, 12> STATUS_SYNC_PROC_HLOW_LEN;
    typedef UReg<0x00, 0x1B, 0, 11> STATUS_SYNC_PROC_VTOTAL;
    typedef UReg<0x00, 0x1F, 0, 8> TEST_BUS_1F;
    typedef UReg<0x00, 0x20, 0, 16> TEST_FF_STATUS;
    typedef UReg<0x00, 0x23, 0, 8> TEST_BUS_23;
    typedef UReg<0x00, 0x2E, 0, 16> TEST_BUS;
    typedef UReg<0x00, 0x2E, 0, 8> TEST_BUS_2E;
    typedef UReg<0x00, 0x2F, 0, 8> TEST_BUS_2F;

    // Miscellaneous Registers
    typedef UReg<0x00, 0x40, 0, 1> PLL_CKIS;
    typedef UReg<0x00, 0x40, 1, 1> PLL_DIVBY2Z;
    typedef UReg<0x00, 0x40, 2, 1> PLL_IS;
    typedef UReg<0x00, 0x40, 3, 1> PLL_ADS;
    typedef UReg<0x00, 0x40, 4, 3> PLL_MS;
    typedef UReg<0x00, 0x41, 0, 8> PLL648_CONTROL_01;
    typedef UReg<0x00, 0x43, 0, 8> PLL648_CONTROL_03;
    typedef UReg<0x00, 0x43, 0, 2> PLL_R;
    typedef UReg<0x00, 0x43, 2, 2> PLL_S;
    typedef UReg<0x00, 0x43, 4, 1> PLL_LEN;
    typedef UReg<0x00, 0x43, 5, 1> PLL_VCORST;
    typedef UReg<0x00, 0x44, 0, 1> DAC_RGBS_PWDNZ;
    typedef UReg<0x00, 0x44, 2, 1> DAC_RGBS_R0ENZ;
    typedef UReg<0x00, 0x44, 5, 1> DAC_RGBS_G0ENZ;
    typedef UReg<0x00, 0x45, 0, 1> DAC_RGBS_B0ENZ;
    typedef UReg<0x00, 0x45, 2, 1> DAC_RGBS_SPD;
    typedef UReg<0x00, 0x45, 3, 1> DAC_RGBS_S0ENZ;
    typedef UReg<0x00, 0x45, 4, 1> DAC_RGBS_S1EN;
    typedef UReg<0x00, 0x46, 0, 8> RESET_CONTROL_0x46; // fake name
    typedef UReg<0x00, 0x46, 0, 1> SFTRST_IF_RSTZ;
    typedef UReg<0x00, 0x46, 1, 1> SFTRST_DEINT_RSTZ;
    typedef UReg<0x00, 0x46, 2, 1> SFTRST_MEM_FF_RSTZ;
    typedef UReg<0x00, 0x46, 4, 1> SFTRST_FIFO_RSTZ;
    typedef UReg<0x00, 0x46, 6, 1> SFTRST_VDS_RSTZ;
    typedef UReg<0x00, 0x47, 0, 8> RESET_CONTROL_0x47; // fake name
    typedef UReg<0x00, 0x47, 0, 1> SFTRST_DEC_RSTZ;
    typedef UReg<0x00, 0x47, 1, 1> SFTRST_MODE_RSTZ;
    typedef UReg<0x00, 0x47, 2, 1> SFTRST_SYNC_RSTZ;
    typedef UReg<0x00, 0x47, 3, 1> SFTRST_HDBYPS_RSTZ;
    typedef UReg<0x00, 0x47, 4, 1> SFTRST_INT_RSTZ;
    typedef UReg<0x00, 0x48, 0, 8> PAD_CONTROL_00_0x48;
    typedef UReg<0x00, 0x48, 0, 1> PAD_BOUT_EN; // aka debug pin
    typedef UReg<0x00, 0x48, 6, 1> PAD_SYNC1_IN_ENZ;
    typedef UReg<0x00, 0x48, 7, 1> PAD_SYNC2_IN_ENZ;
    typedef UReg<0x00, 0x49, 0, 8> PAD_CONTROL_01_0x49; // fake name
    typedef UReg<0x00, 0x49, 0, 1> PAD_CKIN_ENZ;
    typedef UReg<0x00, 0x49, 1, 1> PAD_CKOUT_ENZ;
    typedef UReg<0x00, 0x49, 2, 1> PAD_SYNC_OUT_ENZ;
    typedef UReg<0x00, 0x49, 4, 1> PAD_TRI_ENZ;
    typedef UReg<0x00, 0x4A, 0, 3> PAD_OSC_CNTRL;
    typedef UReg<0x00, 0x4B, 1, 1> DAC_RGBS_BYPS2DAC;
    typedef UReg<0x00, 0x4B, 2, 1> DAC_RGBS_ADC2DAC;
    typedef UReg<0x00, 0x4D, 0, 5> TEST_BUS_SEL;
    typedef UReg<0x00, 0x4D, 5, 1> TEST_BUS_EN;
    typedef UReg<0x00, 0x4F, 5, 1> OUT_SYNC_CNTRL;
    typedef UReg<0x00, 0x4F, 6, 2> OUT_SYNC_SEL;

    typedef UReg<0x00, 0x52, 0, 8> GPIO_CONTROL_00;
    typedef UReg<0x00, 0x53, 0, 8> GPIO_CONTROL_01;
    typedef UReg<0x00, 0x58, 0, 8> INTERRUPT_CONTROL_00;
    typedef UReg<0x00, 0x58, 0, 1> INT_CONTROL_RST_SOGBAD;
    typedef UReg<0x00, 0x58, 1, 1> INT_CONTROL_RST_SOGSWITCH;
    typedef UReg<0x00, 0x58, 4, 1> INT_CONTROL_RST_NOHSYNC;
    typedef UReg<0x00, 0x59, 0, 8> INTERRUPT_CONTROL_01;

    // IF Registers
    typedef UReg<0x01, 0x00, 0, 1> IF_IN_DREG_BYPS;
    typedef UReg<0x01, 0x00, 1, 1> IF_MATRIX_BYPS;
    typedef UReg<0x01, 0x00, 2, 1> IF_UV_REVERT;
    typedef UReg<0x01, 0x00, 3, 1> IF_SEL_656;
    typedef UReg<0x01, 0x00, 4, 1> IF_SEL16BIT;
    typedef UReg<0x01, 0x00, 5, 1> IF_VS_SEL;
    typedef UReg<0x01, 0x00, 6, 1> IF_PRGRSV_CNTRL;
    typedef UReg<0x01, 0x00, 7, 1> IF_HS_FLIP;
    typedef UReg<0x01, 0x01, 0, 1> IF_VS_FLIP;
    typedef UReg<0x01, 0x01, 7, 1> IF_SEL24BIT;
    typedef UReg<0x01, 0x02, 0, 8> INPUT_FORMATTER_02; // convenience
    typedef UReg<0x01, 0x02, 0, 1> IF_SEL_WEN;
    typedef UReg<0x01, 0x02, 1, 1> IF_HS_SEL_LPF;
    typedef UReg<0x01, 0x02, 2, 1> IF_HS_INT_LPF_BYPS;
    typedef UReg<0x01, 0x02, 3, 1> IF_HS_PSHIFT_BYPS;
    typedef UReg<0x01, 0x02, 4, 1> IF_HS_TAP11_BYPS;
    typedef UReg<0x01, 0x02, 5, 2> IF_HS_Y_PDELAY;
    typedef UReg<0x01, 0x0b, 4, 2> IF_HS_DEC_FACTOR;
    typedef UReg<0x01, 0x0b, 6, 1> IF_SEL_HSCALE;
    typedef UReg<0x01, 0x0b, 7, 1> IF_LD_SEL_PROV;
    typedef UReg<0x01, 0x0c, 0, 1> IF_LD_RAM_BYPS;
    typedef UReg<0x01, 0x0c, 1, 4> IF_LD_ST;
    typedef UReg<0x01, 0x0c, 5, 11> IF_INI_ST;
    typedef UReg<0x01, 0x0e, 0, 11> IF_HSYNC_RST;
    typedef UReg<0x01, 0x10, 0, 11> IF_HB_ST;
    typedef UReg<0x01, 0x12, 0, 11> IF_HB_SP;
    typedef UReg<0x01, 0x16, 0, 11> IF_HB_SP1;
    typedef UReg<0x01, 0x18, 0, 11> IF_HB_ST2;
    typedef UReg<0x01, 0x1a, 0, 11> IF_HB_SP2;
    typedef UReg<0x01, 0x1c, 0, 11> IF_VB_ST;
    typedef UReg<0x01, 0x1e, 0, 11> IF_VB_SP;
    typedef UReg<0x01, 0x20, 0, 12> IF_LINE_ST;
    typedef UReg<0x01, 0x22, 0, 12> IF_LINE_SP;
    typedef UReg<0x01, 0x24, 0, 12> IF_HBIN_ST;
    typedef UReg<0x01, 0x26, 0, 12> IF_HBIN_SP;
    typedef UReg<0x01, 0x28, 1, 1> IF_LD_WRST_SEL;
    typedef UReg<0x01, 0x28, 2, 1> IF_SEL_ADC_SYNC;
    typedef UReg<0x01, 0x28, 3, 1> IF_TEST_EN;
    typedef UReg<0x01, 0x28, 4, 4> IF_TEST_SEL;
    typedef UReg<0x01, 0x29, 0, 1> IF_AUTO_OFST_EN;
    typedef UReg<0x01, 0x29, 1, 1> IF_AUTO_OFST_PRD;
    typedef UReg<0x01, 0x2A, 0, 4> IF_AUTO_OFST_U_RANGE;
    typedef UReg<0x01, 0x2A, 4, 4> IF_AUTO_OFST_V_RANGE;
    typedef UReg<0x01, 0x2B, 0, 7> GBS_PRESET_ID;
    typedef UReg<0x01, 0x2B, 7, 1> GBS_PRESET_CUSTOM;
    typedef UReg<0x01, 0x2C, 0, 1> GBS_OPTION_SCANLINES_ENABLED;
    typedef UReg<0x01, 0x2C, 1, 1> GBS_OPTION_SCALING_RGBHV;
    typedef UReg<0x01, 0x2C, 2, 1> GBS_OPTION_PALFORCED60_ENABLED;
    typedef UReg<0x01, 0x2C, 3, 1> GBS_RUNTIME_UNUSED_BIT;
    typedef UReg<0x01, 0x2C, 4, 1> GBS_RUNTIME_FTL_ADJUSTED;
    typedef UReg<0x01, 0x2D, 0, 8> GBS_PRESET_DISPLAY_CLOCK;

    // HDBypass
    typedef UReg<0x01, 0x30, 1, 1> HD_MATRIX_BYPS;
    typedef UReg<0x01, 0x30, 2, 1> HD_DYN_BYPS;
    typedef UReg<0x01, 0x30, 3, 1> HD_SEL_BLK_IN;
    typedef UReg<0x01, 0x32, 0, 8> HD_Y_OFFSET;
    typedef UReg<0x01, 0x34, 0, 8> HD_U_OFFSET;
    typedef UReg<0x01, 0x36, 0, 8> HD_V_OFFSET;
    typedef UReg<0x01, 0x37, 0, 11> HD_HSYNC_RST;
    typedef UReg<0x01, 0x39, 0, 11> HD_INI_ST;
    typedef UReg<0x01, 0x3B, 0, 12> HD_HB_ST;
    typedef UReg<0x01, 0x3D, 0, 12> HD_HB_SP;
    typedef UReg<0x01, 0x3F, 0, 12> HD_HS_ST;
    typedef UReg<0x01, 0x41, 0, 12> HD_HS_SP;
    typedef UReg<0x01, 0x43, 0, 12> HD_VB_ST;
    typedef UReg<0x01, 0x45, 0, 12> HD_VB_SP;
    typedef UReg<0x01, 0x47, 0, 12> HD_VS_ST;
    typedef UReg<0x01, 0x49, 0, 12> HD_VS_SP;
    typedef UReg<0x01, 0x53, 0, 8> HD_BLK_GY_DATA;
    typedef UReg<0x01, 0x54, 0, 8> HD_BLK_BU_DATA;
    typedef UReg<0x01, 0x55, 0, 8> HD_BLK_RV_DATA;

    // Mode Detect
    typedef UReg<0x01, 0x60, 0, 5> MD_HPERIOD_LOCK_VALUE;
    typedef UReg<0x01, 0x60, 5, 3> MD_HPERIOD_UNLOCK_VALUE;
    typedef UReg<0x01, 0x61, 0, 5> MD_VPERIOD_LOCK_VALUE;
    typedef UReg<0x01, 0x61, 5, 3> MD_VPERIOD_UNLOCK_VALUE;
    typedef UReg<0x01, 0x63, 6, 1> MD_HS_FLIP;
    typedef UReg<0x01, 0x63, 7, 1> MD_VS_FLIP;
    typedef UReg<0x01, 0x65, 0, 7> MD_VGA_CNTRL;
    typedef UReg<0x01, 0x65, 7, 1> MD_SEL_VGA60;
    typedef UReg<0x01, 0x66, 0, 8> MD_VGA_75HZ_CNTRL;
    typedef UReg<0x01, 0x67, 0, 8> MD_VGA_85HZ_CNTRL;
    typedef UReg<0x01, 0x6A, 0, 8> MD_SVGA_60HZ_CNTRL;
    typedef UReg<0x01, 0x6B, 0, 8> MD_SVGA_75HZ_CNTRL;
    typedef UReg<0x01, 0x6C, 0, 8> MD_SVGA_85HZ_CNTRL;
    typedef UReg<0x01, 0x6E, 0, 8> MD_XGA_60HZ_CNTRL;
    typedef UReg<0x01, 0x6F, 0, 7> MD_XGA_70HZ_CNTRL;
    typedef UReg<0x01, 0x70, 0, 7> MD_XGA_75HZ_CNTRL;
    typedef UReg<0x01, 0x71, 0, 7> MD_XGA_85HZ_CNTRL;
    typedef UReg<0x01, 0x73, 0, 7> MD_SXGA_60HZ_CNTRL;
    typedef UReg<0x01, 0x74, 0, 7> MD_SXGA_75HZ_CNTRL;
    typedef UReg<0x01, 0x75, 0, 7> MD_SXGA_85HZ_CNTRL;
    typedef UReg<0x01, 0x7F, 0, 7> MD_HD1250P_CNTRL; // changed to meaning 24khz

    // Deinterlacer / Scaledown registers
    typedef UReg<0x02, 0x00, 0, 8> DEINT_00;
    typedef UReg<0x02, 0x00, 7, 1> DIAG_BOB_PLDY_RAM_BYPS;
    typedef UReg<0x02, 0x02, 6, 1> MADPT_Y_VSCALE_BYPS;
    typedef UReg<0x02, 0x02, 7, 1> MADPT_UV_VSCALE_BYPS;
    typedef UReg<0x02, 0x0A, 5, 1> MADPT_STILL_NOISE_EST_EN;
    typedef UReg<0x02, 0x0A, 7, 1> MADPT_Y_MI_DET_BYPS;
    typedef UReg<0x02, 0x0B, 0, 7> MADPT_Y_MI_OFFSET;
    typedef UReg<0x02, 0x0C, 4, 1> MADPT_MI_1BIT_BYPS;
    typedef UReg<0x02, 0x0C, 5, 1> MADPT_MI_1BIT_FRAME2_EN;
    typedef UReg<0x02, 0x16, 5, 1> MADPT_MO_ADP_UV_EN;
    typedef UReg<0x02, 0x16, 6, 1> MADPT_VT_FILTER_CNTRL;
    typedef UReg<0x02, 0x16, 7, 1> MAPDT_VT_SEL_PRGV;
    typedef UReg<0x02, 0x17, 0, 8> MADPT_Y_DELAY_UV_DELAY; // convenience
    typedef UReg<0x02, 0x17, 0, 4> MADPT_Y_DELAY;
    typedef UReg<0x02, 0x17, 4, 4> MADPT_UV_DELAY;
    typedef UReg<0x02, 0x18, 3, 1> MADPT_HTAP_BYPS;
    typedef UReg<0x02, 0x19, 0, 1> MADPT_BIT_STILL_EN;
    typedef UReg<0x02, 0x19, 2, 1> MADPT_VTAP2_BYPS;
    typedef UReg<0x02, 0x19, 3, 1> MADPT_VTAP2_ROUND_SEL;
    typedef UReg<0x02, 0x19, 4, 4> MADPT_VTAP2_COEFF;
    typedef UReg<0x02, 0x21, 4, 1> MADPT_EN_NOUT_FOR_STILL;
    typedef UReg<0x02, 0x21, 5, 1> MADPT_EN_NOUT_FOR_LESS_STILL;
    typedef UReg<0x02, 0x24, 2, 1> MADPT_PD_RAM_BYPS;
    typedef UReg<0x02, 0x26, 6, 1> MADPT_VIIR_BYPS;
    typedef UReg<0x02, 0x27, 0, 7> MADPT_VIIR_COEF;
    typedef UReg<0x02, 0x31, 0, 2> MADPT_VSCALE_DEC_FACTOR;
    typedef UReg<0x02, 0x31, 2, 1> MADPT_SEL_PHASE_INI;
    typedef UReg<0x02, 0x35, 3, 1> MADPT_DD0_SEL;
    typedef UReg<0x02, 0x35, 4, 1> MADPT_NRD_VIIR_PD_BYPS;
    typedef UReg<0x02, 0x35, 5, 1> MADPT_UVDLY_PD_BYPS;
    typedef UReg<0x02, 0x35, 6, 1> MADPT_CMP_EN;
    typedef UReg<0x02, 0x39, 0, 4> MADPT_UVDLY_PD_SP;
    typedef UReg<0x02, 0x39, 4, 4> MADPT_UVDLY_PD_ST;
    typedef UReg<0x02, 0x3a, 0, 1> MADPT_EN_UV_DEINT;
    typedef UReg<0x02, 0x3a, 3, 1> MADPT_EN_STILL_FOR_NRD;
    typedef UReg<0x02, 0x3a, 5, 2> MADPT_MI_1BIT_DLY;
    typedef UReg<0x02, 0x3a, 7, 1> MADPT_UV_MI_DET_BYPS;
    typedef UReg<0x02, 0x3b, 0, 7> MADPT_UV_MI_OFFSET;

    // VDS Registers
    typedef UReg<0x03, 0x00, 0, 1> VDS_SYNC_EN;
    typedef UReg<0x03, 0x00, 1, 1> VDS_FIELDAB_EN;
    typedef UReg<0x03, 0x00, 2, 1> VDS_DFIELD_EN;
    typedef UReg<0x03, 0x00, 3, 1> VDS_FIELD_FLIP;
    typedef UReg<0x03, 0x00, 4, 1> VDS_HSCALE_BYPS;
    typedef UReg<0x03, 0x00, 5, 1> VDS_VSCALE_BYPS;
    typedef UReg<0x03, 0x00, 6, 1> VDS_HALF_EN;
    typedef UReg<0x03, 0x00, 7, 1> VDS_SRESET;
    typedef UReg<0x03, 0x01, 0, 12> VDS_HSYNC_RST;
    typedef UReg<0x03, 0x02, 4, 11> VDS_VSYNC_RST;
    typedef UReg<0x03, 0x04, 0, 12> VDS_HB_ST;
    typedef UReg<0x03, 0x05, 4, 12> VDS_HB_SP;
    typedef UReg<0x03, 0x07, 0, 11> VDS_VB_ST;
    typedef UReg<0x03, 0x08, 4, 11> VDS_VB_SP;
    typedef UReg<0x03, 0x0A, 0, 12> VDS_HS_ST;
    typedef UReg<0x03, 0x0B, 4, 12> VDS_HS_SP;
    typedef UReg<0x03, 0x0D, 0, 11> VDS_VS_ST;
    typedef UReg<0x03, 0x0E, 4, 11> VDS_VS_SP;
    typedef UReg<0x03, 0x10, 0, 12> VDS_DIS_HB_ST;
    typedef UReg<0x03, 0x11, 4, 12> VDS_DIS_HB_SP;
    typedef UReg<0x03, 0x13, 0, 11> VDS_DIS_VB_ST;
    typedef UReg<0x03, 0x14, 4, 11> VDS_DIS_VB_SP;
    typedef UReg<0x03, 0x16, 0, 10> VDS_HSCALE;
    typedef UReg<0x03, 0x17, 4, 10> VDS_VSCALE;
    typedef UReg<0x03, 0x19, 0, 10> VDS_FRAME_RST;
    typedef UReg<0x03, 0x1A, 4, 1> VDS_FLOCK_EN;
    typedef UReg<0x03, 0x1A, 5, 1> VDS_FREERUN_FID;
    typedef UReg<0x03, 0x1A, 6, 1> VDS_FID_AA_DLY;
    typedef UReg<0x03, 0x1A, 7, 1> VDS_FID_RST;

    typedef UReg<0x03, 0x1B, 0, 32> VDS_FR_SELECT;
    typedef UReg<0x03, 0x1F, 0, 4> VDS_FRAME_NO;
    typedef UReg<0x03, 0x1F, 4, 1> VDS_DIF_FR_SEL_EN;
    typedef UReg<0x03, 0x1F, 5, 1> VDS_EN_FR_NUM_RST;
    typedef UReg<0x03, 0x20, 0, 11> VDS_VSYN_SIZE1;
    typedef UReg<0x03, 0x22, 0, 11> VDS_VSYN_SIZE2;
    typedef UReg<0x03, 0x24, 0, 8> VDS_3_24; // convenience
    typedef UReg<0x03, 0x24, 0, 1> VDS_UV_FLIP;
    typedef UReg<0x03, 0x24, 1, 1> VDS_U_DELAY;
    typedef UReg<0x03, 0x24, 2, 1> VDS_V_DELAY;
    typedef UReg<0x03, 0x24, 3, 1> VDS_TAP6_BYPS;
    typedef UReg<0x03, 0x24, 4, 2> VDS_Y_DELAY;
    typedef UReg<0x03, 0x24, 6, 2> VDS_WEN_DELAY;
    typedef UReg<0x03, 0x25, 0, 10> VDS_D_SP;
    typedef UReg<0x03, 0x26, 6, 1> VDS_D_RAM_BYPS;
    typedef UReg<0x03, 0x26, 7, 1> VDS_BLEV_AUTO_EN;
    typedef UReg<0x03, 0x27, 0, 4> VDS_USER_MIN;
    typedef UReg<0x03, 0x27, 4, 4> VDS_USER_MAX;
    typedef UReg<0x03, 0x28, 0, 8> VDS_BLEV_LEVEL;
    typedef UReg<0x03, 0x29, 0, 8> VDS_BLEV_GAIN;
    typedef UReg<0x03, 0x2A, 0, 1> VDS_BLEV_BYPS;
    typedef UReg<0x03, 0x2A, 4, 2> VDS_STEP_DLY_CNTRL;
    typedef UReg<0x03, 0x2A, 6, 2> VDS_0X2A_RESERVED_2BITS;
    typedef UReg<0x03, 0x2B, 0, 4> VDS_STEP_GAIN;
    typedef UReg<0x03, 0x2B, 4, 3> VDS_STEP_CLIP;
    typedef UReg<0x03, 0x2B, 7, 1> VDS_UV_STEP_BYPS;

    typedef UReg<0x03, 0x2C, 0, 8> VDS_SK_U_CENTER;
    typedef UReg<0x03, 0x2D, 0, 8> VDS_SK_V_CENTER;
    typedef UReg<0x03, 0x2E, 0, 8> VDS_SK_Y_LOW_TH;
    typedef UReg<0x03, 0x2F, 0, 8> VDS_SK_Y_HIGH_TH;
    typedef UReg<0x03, 0x30, 0, 8> VDS_SK_RANGE;
    typedef UReg<0x03, 0x31, 0, 4> VDS_SK_GAIN;
    typedef UReg<0x03, 0x31, 4, 1> VDS_SK_Y_EN;
    typedef UReg<0x03, 0x31, 5, 1> VDS_SK_BYPS;

    typedef UReg<0x03, 0x32, 0, 2> VDS_SVM_BPF_CNTRL;
    typedef UReg<0x03, 0x32, 2, 1> VDS_SVM_POL_FLIP;
    typedef UReg<0x03, 0x32, 3, 1> VDS_SVM_2ND_BYPS;
    typedef UReg<0x03, 0x32, 4, 3> VDS_SVM_VCLK_DELAY;
    typedef UReg<0x03, 0x32, 7, 1> VDS_SVM_SIGMOID_BYPS;
    typedef UReg<0x03, 0x33, 0, 8> VDS_SVM_GAIN;
    typedef UReg<0x03, 0x34, 0, 8> VDS_SVM_OFFSET;

    typedef UReg<0x03, 0x35, 0, 8> VDS_Y_GAIN;
    typedef UReg<0x03, 0x36, 0, 8> VDS_UCOS_GAIN;
    typedef UReg<0x03, 0x37, 0, 8> VDS_VCOS_GAIN;
    typedef UReg<0x03, 0x38, 0, 8> VDS_USIN_GAIN;
    typedef UReg<0x03, 0x39, 0, 8> VDS_VSIN_GAIN;
    typedef UReg<0x03, 0x3A, 0, 8> VDS_Y_OFST;
    typedef UReg<0x03, 0x3B, 0, 8> VDS_U_OFST;
    typedef UReg<0x03, 0x3C, 0, 8> VDS_V_OFST;

    typedef UReg<0x03, 0x3D, 0, 9> VDS_SYNC_LEV;
    typedef UReg<0x03, 0x3E, 3, 1> VDS_CONVT_BYPS;
    typedef UReg<0x03, 0x3E, 4, 1> VDS_DYN_BYPS;
    typedef UReg<0x03, 0x3E, 7, 1> VDS_BLK_BF_EN;
    typedef UReg<0x03, 0x3F, 0, 8> VDS_UV_BLK_VAL;
    typedef UReg<0x03, 0x40, 0, 1> VDS_1ST_INT_BYPS;
    typedef UReg<0x03, 0x40, 1, 1> VDS_2ND_INT_BYPS;
    typedef UReg<0x03, 0x40, 2, 1> VDS_IN_DREG_BYPS;
    typedef UReg<0x03, 0x40, 4, 2> VDS_SVM_V4CLK_DELAY;

    typedef UReg<0x03, 0x41, 0, 10> VDS_PK_LINE_BUF_SP;
    typedef UReg<0x03, 0x42, 6, 1> VDS_PK_RAM_BYPS;
    typedef UReg<0x03, 0x43, 0, 1> VDS_PK_VL_HL_SEL;
    typedef UReg<0x03, 0x43, 1, 1> VDS_PK_VL_HH_SEL;
    typedef UReg<0x03, 0x43, 2, 1> VDS_PK_VH_HL_SEL;
    typedef UReg<0x03, 0x43, 3, 1> VDS_PK_VH_HH_SEL;
    typedef UReg<0x03, 0x44, 0, 3> VDS_PK_LB_CORE;
    typedef UReg<0x03, 0x44, 3, 5> VDS_PK_LB_CMP;
    typedef UReg<0x03, 0x45, 0, 6> VDS_PK_LB_GAIN;
    typedef UReg<0x03, 0x46, 0, 3> VDS_PK_LH_CORE;
    typedef UReg<0x03, 0x46, 3, 5> VDS_PK_LH_CMP;
    typedef UReg<0x03, 0x47, 0, 6> VDS_PK_LH_GAIN;
    typedef UReg<0x03, 0x48, 0, 3> VDS_PK_HL_CORE;
    typedef UReg<0x03, 0x48, 3, 5> VDS_PK_HL_CMP;
    typedef UReg<0x03, 0x49, 0, 6> VDS_PK_HL_GAIN;
    typedef UReg<0x03, 0x4A, 0, 3> VDS_PK_HB_CORE;
    typedef UReg<0x03, 0x4A, 3, 5> VDS_PK_HB_CMP;
    typedef UReg<0x03, 0x4B, 0, 6> VDS_PK_HB_GAIN;
    typedef UReg<0x03, 0x4C, 0, 3> VDS_PK_HH_CORE;
    typedef UReg<0x03, 0x4C, 3, 5> VDS_PK_HH_CMP;
    typedef UReg<0x03, 0x4D, 0, 6> VDS_PK_HH_GAIN;
    typedef UReg<0x03, 0x4E, 0, 1> VDS_PK_Y_H_BYPS;
    typedef UReg<0x03, 0x4E, 1, 1> VDS_PK_Y_V_BYPS;
    typedef UReg<0x03, 0x4E, 3, 1> VDS_C_VPK_BYPS;
    typedef UReg<0x03, 0x4E, 4, 3> VDS_C_VPK_CORE;
    typedef UReg<0x03, 0x4F, 0, 6> VDS_C_VPK_GAIN;

    typedef UReg<0x03, 0x50, 0, 4> VDS_TEST_BUS_SEL;
    typedef UReg<0x03, 0x50, 4, 1> VDS_TEST_EN;
    typedef UReg<0x03, 0x50, 5, 1> VDS_DO_UV_DEV_BYPS;
    typedef UReg<0x03, 0x50, 6, 1> VDS_DO_UVSEL_FLIP;
    typedef UReg<0x03, 0x50, 7, 1> VDS_DO_16B_EN;

    typedef UReg<0x03, 0x51, 7, 11> VDS_GLB_NOISE;
    typedef UReg<0x03, 0x52, 4, 1> VDS_NR_Y_BYPS;
    typedef UReg<0x03, 0x52, 5, 1> VDS_NR_C_BYPS;
    typedef UReg<0x03, 0x52, 6, 1> VDS_NR_DIF_LPF5_BYPS;
    typedef UReg<0x03, 0x52, 7, 1> VDS_NR_MI_TH_EN;
    typedef UReg<0x03, 0x53, 0, 7> VDS_NR_MI_OFFSET;
    typedef UReg<0x03, 0x53, 7, 1> VDS_NR_MIG_USER_EN;
    typedef UReg<0x03, 0x54, 0, 4> VDS_NR_MI_GAIN;
    typedef UReg<0x03, 0x54, 4, 4> VDS_NR_STILL_GAIN;
    typedef UReg<0x03, 0x55, 0, 4> VDS_NR_MI_THRES;
    typedef UReg<0x03, 0x55, 4, 1> VDS_NR_EN_H_NOISY;
    typedef UReg<0x03, 0x55, 6, 1> VDS_NR_EN_GLB_STILL;
    typedef UReg<0x03, 0x55, 7, 1> VDS_NR_GLB_STILL_MENU;
    typedef UReg<0x03, 0x56, 0, 7> VDS_NR_NOISY_OFFSET;
    typedef UReg<0x03, 0x56, 7, 1> VDS_W_LEV_BYPS;
    typedef UReg<0x03, 0x57, 0, 8> VDS_W_LEV;
    typedef UReg<0x03, 0x58, 0, 8> VDS_WLEV_GAIN;
    typedef UReg<0x03, 0x59, 0, 8> VDS_NS_U_CENTER;
    typedef UReg<0x03, 0x5A, 0, 8> VDS_NS_V_CENTER;
    typedef UReg<0x03, 0x5B, 0, 7> VDS_NS_U_GAIN;
    typedef UReg<0x03, 0x5B, 7, 15> VDS_NS_SQUARE_RAD;
    typedef UReg<0x03, 0x5D, 6, 8> VDS_NS_Y_HIGH_TH;
    typedef UReg<0x03, 0x5E, 6, 7> VDS_NS_V_GAIN;
    typedef UReg<0x03, 0x5F, 5, 5> VDS_NS_Y_LOW_TH;
    typedef UReg<0x03, 0x60, 2, 1> VDS_NS_BYPS;
    typedef UReg<0x03, 0x60, 3, 1> VDS_NS_Y_ACTIVE_EN;

    typedef UReg<0x03, 0x60, 4, 10> VDS_C1_TAG_LOW_SLOPE;
    typedef UReg<0x03, 0x61, 6, 10> VDS_C1_TAG_HIGH_SLOPE;
    typedef UReg<0x03, 0x63, 0, 4> VDS_C1_GAIN;
    typedef UReg<0x03, 0x63, 4, 8> VDS_C1_U_LOW;
    typedef UReg<0x03, 0x64, 4, 8> VDS_C1_U_HIGH;
    typedef UReg<0x03, 0x65, 4, 1> VDS_C1_BYPS;
    typedef UReg<0x03, 0x65, 5, 8> VDS_C1_Y_THRESH;

    typedef UReg<0x03, 0x66, 5, 10> VDS_C2_TAG_LOW_SLOPE;
    typedef UReg<0x03, 0x67, 7, 10> VDS_C2_TAG_HIGH_SLOPE;
    typedef UReg<0x03, 0x69, 1, 4> VDS_C2_GAIN;
    typedef UReg<0x03, 0x69, 5, 8> VDS_C2_U_LOW;
    typedef UReg<0x03, 0x6A, 5, 8> VDS_C2_U_HIGH;
    typedef UReg<0x03, 0x6B, 5, 1> VDS_C2_BYPS;
    typedef UReg<0x03, 0x6B, 6, 8> VDS_C2_Y_THRESH;

    typedef UReg<0x03, 0x6D, 0, 12> VDS_EXT_HB_ST;
    typedef UReg<0x03, 0x6E, 4, 12> VDS_EXT_HB_SP;
    typedef UReg<0x03, 0x70, 0, 11> VDS_EXT_VB_ST;
    typedef UReg<0x03, 0x71, 4, 11> VDS_EXT_VB_SP;
    typedef UReg<0x03, 0x72, 7, 1> VDS_SYNC_IN_SEL;

    typedef UReg<0x03, 0x73, 0, 3> VDS_BLUE_RANGE;
    typedef UReg<0x03, 0x73, 3, 1> VDS_BLUE_BYPS;
    typedef UReg<0x03, 0x73, 4, 4> VDS_BLUE_UGAIN;
    typedef UReg<0x03, 0x74, 0, 4> VDS_BLUE_VGAIN;
    typedef UReg<0x03, 0x74, 4, 4> VDS_BLUE_Y_LEV;

    // PIP Registers
    typedef UReg<0x03, 0x80, 0, 1> PIP_UV_FLIP;
    typedef UReg<0x03, 0x80, 1, 1> PIP_U_DELAY;
    typedef UReg<0x03, 0x80, 2, 1> PIP_V_DELAY;
    typedef UReg<0x03, 0x80, 3, 1> PIP_TAP3_BYPS;
    typedef UReg<0x03, 0x80, 4, 2> PIP_Y_DELAY;
    typedef UReg<0x03, 0x80, 6, 1> PIP_SUB_16B_SEL;
    typedef UReg<0x03, 0x80, 7, 1> PIP_DYN_BYPS;
    typedef UReg<0x03, 0x81, 0, 1> PIP_CONVT_BYPS;
    typedef UReg<0x03, 0x81, 3, 1> PIP_DREG_BYPS;
    typedef UReg<0x03, 0x81, 7, 1> PIP_EN;
    typedef UReg<0x03, 0x82, 0, 8> PIP_Y_GAIN;
    typedef UReg<0x03, 0x83, 0, 8> PIP_U_GAIN;
    typedef UReg<0x03, 0x84, 0, 8> PIP_V_GAIN;
    typedef UReg<0x03, 0x85, 0, 8> PIP_Y_OFST;
    typedef UReg<0x03, 0x86, 0, 8> PIP_U_OFST;
    typedef UReg<0x03, 0x87, 0, 8> PIP_V_OFST;
    typedef UReg<0x03, 0x88, 0, 12> PIP_H_ST;
    typedef UReg<0x03, 0x8A, 0, 12> PIP_H_SP;
    typedef UReg<0x03, 0x8C, 0, 11> PIP_V_ST;
    typedef UReg<0x03, 0x8E, 0, 11> PIP_V_SP;

    // Memory Controller Registers
    typedef UReg<0x04, 0x00, 0, 8> SDRAM_RESET_CONTROL; // fake name
    typedef UReg<0x04, 0x00, 4, 1> SDRAM_RESET_SIGNAL;
    typedef UReg<0x04, 0x00, 7, 1> SDRAM_START_INITIAL_CYCLE;
    typedef UReg<0x04, 0x12, 0, 1> MEM_INTER_DLYCELL_SEL;
    typedef UReg<0x04, 0x12, 1, 1> MEM_CLK_DLYCELL_SEL;
    typedef UReg<0x04, 0x12, 2, 1> MEM_FBK_CLK_DLYCELL_SEL;
    typedef UReg<0x04, 0x13, 0, 1> MEM_PAD_CLK_INVERT;
    typedef UReg<0x04, 0x13, 1, 1> MEM_RD_DATA_CLK_INVERT;
    typedef UReg<0x04, 0x13, 2, 1> MEM_FBK_CLK_INVERT;
    typedef UReg<0x04, 0x15, 0, 1> MEM_REQ_PBH_RFFH;
    typedef UReg<0x04, 0x1b, 0, 3> MEM_ADR_DLY_REG;
    typedef UReg<0x04, 0x1b, 4, 3> MEM_CLK_DLY_REG;

    // Playback / Capture / Memory Registers
    typedef UReg<0x04, 0x21, 0, 1> CAPTURE_ENABLE;
    typedef UReg<0x04, 0x21, 1, 1> CAP_FF_HALF_REQ;
    typedef UReg<0x04, 0x21, 5, 1> CAP_SAFE_GUARD_EN;
    typedef UReg<0x04, 0x22, 0, 1> CAP_REQ_OVER;
    typedef UReg<0x04, 0x22, 1, 1> CAP_STATUS_SEL;
    typedef UReg<0x04, 0x22, 3, 1> CAP_REQ_FREEZ;
    typedef UReg<0x04, 0x24, 0, 21> CAP_SAFE_GUARD_A;
    typedef UReg<0x04, 0x27, 0, 21> CAP_SAFE_GUARD_B;
    typedef UReg<0x04, 0x2b, 0, 1> PB_CUT_REFRESH;
    typedef UReg<0x04, 0x2b, 1, 2> PB_REQ_SEL;
    typedef UReg<0x04, 0x2b, 3, 1> PB_BYPASS;
    typedef UReg<0x04, 0x2b, 5, 1> PB_DB_BUFFER_EN;
    typedef UReg<0x04, 0x2b, 7, 1> PB_ENABLE;
    typedef UReg<0x04, 0x2c, 0, 8> PB_MAST_FLAG_REG;
    typedef UReg<0x04, 0x2d, 0, 8> PB_GENERAL_FLAG_REG;
    typedef UReg<0x04, 0x37, 0, 10> PB_CAP_OFFSET;
    typedef UReg<0x04, 0x39, 0, 10> PB_FETCH_NUM;
    typedef UReg<0x04, 0x42, 0, 1> WFF_ENABLE;
    typedef UReg<0x04, 0x42, 2, 1> WFF_FF_STA_INV;
    typedef UReg<0x04, 0x42, 3, 1> WFF_SAFE_GUARD;
    typedef UReg<0x04, 0x42, 5, 1> WFF_ADR_ADD_2;
    typedef UReg<0x04, 0x42, 7, 1> WFF_FF_STATUS_SEL;
    typedef UReg<0x04, 0x44, 0, 21> WFF_SAFE_GUARD_A;
    typedef UReg<0x04, 0x47, 0, 21> WFF_SAFE_GUARD_B;
    typedef UReg<0x04, 0x4a, 0, 1> WFF_YUV_DEINTERLACE;
    typedef UReg<0x04, 0x4a, 4, 1> WFF_LINE_FLIP;
    typedef UReg<0x04, 0x4b, 0, 3> WFF_HB_DELAY;
    typedef UReg<0x04, 0x4b, 4, 3> WFF_VB_DELAY;
    typedef UReg<0x04, 0x4d, 4, 1> RFF_ADR_ADD_2;
    typedef UReg<0x04, 0x4d, 5, 2> RFF_REQ_SEL;
    typedef UReg<0x04, 0x4d, 7, 1> RFF_ENABLE;
    typedef UReg<0x04, 0x4e, 0, 8> RFF_MASTER_FLAG;
    typedef UReg<0x04, 0x50, 5, 1> RFF_LINE_FLIP;
    typedef UReg<0x04, 0x50, 6, 1> RFF_YUV_DEINTERLACE;
    typedef UReg<0x04, 0x50, 7, 1> RFF_LREQ_CUT;
    typedef UReg<0x04, 0x51, 0, 21> RFF_WFF_STA_ADDR_A;
    typedef UReg<0x04, 0x54, 0, 21> RFF_WFF_STA_ADDR_B;
    typedef UReg<0x04, 0x57, 0, 10> RFF_WFF_OFFSET;
    typedef UReg<0x04, 0x59, 0, 10> RFF_FETCH_NUM;
    typedef UReg<0x04, 0x5B, 7, 1> MEM_FF_TOP_FF_SEL;

    // OSD Registers
    typedef UReg<0x00, 0x90, 0, 1> OSD_SW_RESET;
    typedef UReg<0x00, 0x90, 1, 3> OSD_HORIZONTAL_ZOOM;
    typedef UReg<0x00, 0x90, 4, 2> OSD_VERTICAL_ZOOM;
    typedef UReg<0x00, 0x90, 6, 1> OSD_DISP_EN;
    typedef UReg<0x00, 0x90, 7, 1> OSD_MENU_EN;
    typedef UReg<0x00, 0x91, 0, 4> OSD_MENU_ICON_SEL;
    typedef UReg<0x00, 0x91, 4, 4> OSD_MENU_MOD_SEL;
    typedef UReg<0x00, 0x92, 0, 3> OSD_MENU_BAR_FONT_FORCOR;
    typedef UReg<0x00, 0x92, 3, 3> OSD_MENU_BAR_FONT_BGCOR;
    typedef UReg<0x00, 0x92, 6, 3> OSD_MENU_BAR_BORD_COR;
    typedef UReg<0x00, 0x93, 1, 3> OSD_MENU_SEL_FORCOR;
    typedef UReg<0x00, 0x93, 4, 3> OSD_MENU_SEL_BGCOR;
    typedef UReg<0x00, 0x93, 7, 1> OSD_COMMAND_FINISH;
    typedef UReg<0x00, 0x94, 0, 1> OSD_MENU_DISP_STYLE;
    typedef UReg<0x00, 0x94, 2, 1> OSD_YCBCR_RGB_FORMAT;
    typedef UReg<0x00, 0x94, 3, 1> OSD_INT_NG_LAT;
    typedef UReg<0x00, 0x94, 4, 4> OSD_TEST_SEL;
    typedef UReg<0x00, 0x95, 0, 8> OSD_MENU_HORI_START;
    typedef UReg<0x00, 0x96, 0, 8> OSD_MENU_VER_START;
    typedef UReg<0x00, 0x97, 0, 8> OSD_BAR_LENGTH;
    typedef UReg<0x00, 0x98, 0, 8> OSD_BAR_FOREGROUND_VALUE;

    // ADC, SP Registers
    typedef UReg<0x05, 0x00, 0, 8> ADC_5_00; // convenience
    typedef UReg<0x05, 0x00, 0, 2> ADC_CLK_PA;
    typedef UReg<0x05, 0x00, 3, 1> ADC_CLK_ICLK2X;
    typedef UReg<0x05, 0x00, 4, 1> ADC_CLK_ICLK1X;
    typedef UReg<0x05, 0x02, 0, 1> ADC_SOGEN;
    typedef UReg<0x05, 0x02, 1, 5> ADC_SOGCTRL;
    typedef UReg<0x05, 0x02, 6, 2> ADC_INPUT_SEL;
    typedef UReg<0x05, 0x03, 0, 8> ADC_5_03; // convenience
    typedef UReg<0x05, 0x03, 0, 1> ADC_POWDZ;
    typedef UReg<0x05, 0x03, 1, 1> ADC_RYSEL_R;
    typedef UReg<0x05, 0x03, 2, 1> ADC_RYSEL_G;
    typedef UReg<0x05, 0x03, 3, 1> ADC_RYSEL_B;
    typedef UReg<0x05, 0x03, 4, 2> ADC_FLTR;
    typedef UReg<0x05, 0x04, 0, 8> ADC_TEST_04;
    typedef UReg<0x05, 0x04, 0, 2> ADC_TR_RSEL;
    typedef UReg<0x05, 0x04, 1, 1> ADC_TR_RSEL_04_BIT1;
    typedef UReg<0x05, 0x04, 2, 3> ADC_TR_ISEL;
    typedef UReg<0x05, 0x05, 0, 1> ADC_TA_05_EN;
    typedef UReg<0x05, 0x05, 0, 8> ADC_TA_05_CTRL;
    typedef UReg<0x05, 0x06, 0, 8> ADC_ROFCTRL;
    typedef UReg<0x05, 0x07, 0, 8> ADC_GOFCTRL;
    typedef UReg<0x05, 0x08, 0, 8> ADC_BOFCTRL;
    typedef UReg<0x05, 0x09, 0, 8> ADC_RGCTRL;
    typedef UReg<0x05, 0x0A, 0, 8> ADC_GGCTRL;
    typedef UReg<0x05, 0x0B, 0, 8> ADC_BGCTRL;
    typedef UReg<0x05, 0x0C, 0, 8> ADC_TEST_0C;
    typedef UReg<0x05, 0x0C, 1, 1> ADC_TEST_0C_BIT1;
    typedef UReg<0x05, 0x0C, 3, 1> ADC_TEST_0C_BIT3;
    typedef UReg<0x05, 0x0C, 4, 1> ADC_TEST_0C_BIT4;
    typedef UReg<0x05, 0x0E, 0, 1> ADC_AUTO_OFST_EN;
    typedef UReg<0x05, 0x0E, 1, 1> ADC_AUTO_OFST_PRD;
    typedef UReg<0x05, 0x0E, 2, 2> ADC_AUTO_OFST_DELAY;
    typedef UReg<0x05, 0x0E, 4, 2> ADC_AUTO_OFST_STEP;
    typedef UReg<0x05, 0x0E, 7, 1> ADC_AUTO_OFST_TEST;
    typedef UReg<0x05, 0x0F, 0, 8> ADC_AUTO_OFST_RANGE_REG;
    typedef UReg<0x05, 0x11, 0, 8> PLLAD_CONTROL_00_5x11; // fake name
    typedef UReg<0x05, 0x11, 0, 1> PLLAD_VCORST;
    typedef UReg<0x05, 0x11, 1, 1> PLLAD_LEN;
    typedef UReg<0x05, 0x11, 2, 1> PLLAD_TEST;
    typedef UReg<0x05, 0x11, 3, 1> PLLAD_TS;
    typedef UReg<0x05, 0x11, 4, 1> PLLAD_PDZ;
    typedef UReg<0x05, 0x11, 5, 1> PLLAD_FS;
    typedef UReg<0x05, 0x11, 6, 1> PLLAD_BPS;
    typedef UReg<0x05, 0x11, 7, 1> PLLAD_LAT;
    typedef UReg<0x05, 0x12, 0, 12> PLLAD_MD;
    typedef UReg<0x05, 0x16, 0, 8> PLLAD_5_16; // fake name
    typedef UReg<0x05, 0x16, 0, 2> PLLAD_R;
    typedef UReg<0x05, 0x16, 2, 2> PLLAD_S;
    typedef UReg<0x05, 0x16, 4, 2> PLLAD_KS;
    typedef UReg<0x05, 0x16, 6, 2> PLLAD_CKOS;
    typedef UReg<0x05, 0x17, 0, 3> PLLAD_ICP;
    typedef UReg<0x05, 0x18, 0, 1> PA_ADC_BYPSZ;
    typedef UReg<0x05, 0x18, 1, 5> PA_ADC_S;
    typedef UReg<0x05, 0x18, 6, 1> PA_ADC_LOCKOFF;
    typedef UReg<0x05, 0x18, 7, 1> PA_ADC_LAT;
    typedef UReg<0x05, 0x19, 0, 1> PA_SP_BYPSZ;
    typedef UReg<0x05, 0x19, 1, 5> PA_SP_S;
    typedef UReg<0x05, 0x19, 6, 1> PA_SP_LOCKOFF;
    typedef UReg<0x05, 0x19, 7, 1> PA_SP_LAT;
    typedef UReg<0x05, 0x1E, 7, 1> DEC_WEN_MODE;
    typedef UReg<0x05, 0x1F, 0, 8> DEC_5_1F; // convenience
    typedef UReg<0x05, 0x1F, 0, 1> DEC1_BYPS;
    typedef UReg<0x05, 0x1F, 1, 1> DEC2_BYPS;
    typedef UReg<0x05, 0x1F, 2, 1> DEC_MATRIX_BYPS;
    typedef UReg<0x05, 0x1F, 3, 1> DEC_TEST_ENABLE; // fake name
    typedef UReg<0x05, 0x1F, 4, 3> DEC_TEST_SEL;
    typedef UReg<0x05, 0x1F, 7, 1> DEC_IDREG_EN;
    typedef UReg<0x05, 0x20, 0, 1> SP_SOG_SRC_SEL;
    typedef UReg<0x05, 0x20, 1, 1> SP_SOG_P_ATO;
    typedef UReg<0x05, 0x20, 2, 1> SP_SOG_P_INV;
    typedef UReg<0x05, 0x20, 3, 1> SP_EXT_SYNC_SEL;
    typedef UReg<0x05, 0x20, 4, 1> SP_JITTER_SYNC;
    typedef UReg<0x05, 0x26, 0, 12> SP_SYNC_PD_THD;
    typedef UReg<0x05, 0x33, 0, 8> SP_H_TIMER_VAL;
    typedef UReg<0x05, 0x35, 0, 12> SP_DLT_REG;
    typedef UReg<0x05, 0x37, 0, 8> SP_H_PULSE_IGNOR;
    typedef UReg<0x05, 0x38, 0, 8> SP_PRE_COAST;
    typedef UReg<0x05, 0x39, 0, 8> SP_POST_COAST;
    typedef UReg<0x05, 0x3A, 0, 8> SP_H_TOTAL_EQ_THD;
    typedef UReg<0x05, 0x3B, 0, 3> SP_SDCS_VSST_REG_H;
    typedef UReg<0x05, 0x3B, 4, 3> SP_SDCS_VSSP_REG_H;
    typedef UReg<0x05, 0x3E, 0, 8> SP_CS_0x3E; // fake name
    typedef UReg<0x05, 0x3E, 0, 1> SP_CS_P_SWAP;
    typedef UReg<0x05, 0x3E, 1, 1> SP_HD_MODE;
    typedef UReg<0x05, 0x3E, 2, 1> SP_H_COAST;
    typedef UReg<0x05, 0x3E, 4, 1> SP_H_PROTECT;
    typedef UReg<0x05, 0x3E, 5, 1> SP_DIS_SUB_COAST;
    typedef UReg<0x05, 0x3F, 0, 8> SP_SDCS_VSST_REG_L;
    typedef UReg<0x05, 0x40, 0, 8> SP_SDCS_VSSP_REG_L;
    typedef UReg<0x05, 0x41, 0, 12> SP_CS_CLP_ST;
    typedef UReg<0x05, 0x43, 0, 12> SP_CS_CLP_SP;
    typedef UReg<0x05, 0x45, 0, 12> SP_CS_HS_ST;
    typedef UReg<0x05, 0x47, 0, 12> SP_CS_HS_SP;
    typedef UReg<0x05, 0x49, 0, 12> SP_RT_HS_ST;
    typedef UReg<0x05, 0x4B, 0, 12> SP_RT_HS_SP;
    typedef UReg<0x05, 0x4D, 0, 12> SP_H_CST_ST;
    typedef UReg<0x05, 0x4F, 0, 12> SP_H_CST_SP;
    typedef UReg<0x05, 0x55, 4, 1> SP_HS_POL_ATO;
    typedef UReg<0x05, 0x55, 6, 1> SP_VS_POL_ATO;
    typedef UReg<0x05, 0x55, 7, 1> SP_HCST_AUTO_EN;
    typedef UReg<0x05, 0x56, 0, 8> SP_5_56; // convenience
    typedef UReg<0x05, 0x56, 0, 1> SP_SOG_MODE;
    typedef UReg<0x05, 0x56, 1, 1> SP_HS2PLL_INV_REG;
    typedef UReg<0x05, 0x56, 2, 1> SP_CLAMP_MANUAL;
    typedef UReg<0x05, 0x56, 3, 1> SP_CLP_SRC_SEL;
    typedef UReg<0x05, 0x56, 4, 1> SP_SYNC_BYPS;
    typedef UReg<0x05, 0x56, 5, 1> SP_HS_PROC_INV_REG;
    typedef UReg<0x05, 0x56, 6, 1> SP_VS_PROC_INV_REG;
    typedef UReg<0x05, 0x56, 7, 1> SP_CLAMP_INV_REG;
    typedef UReg<0x05, 0x57, 0, 8> SP_5_57; // convenience
    typedef UReg<0x05, 0x57, 0, 1> SP_NO_CLAMP_REG;
    typedef UReg<0x05, 0x57, 1, 1> SP_COAST_INV_REG;
    typedef UReg<0x05, 0x57, 2, 1> SP_NO_COAST_REG;
    typedef UReg<0x05, 0x57, 6, 1> SP_HS_LOOP_SEL;
    typedef UReg<0x05, 0x57, 7, 1> SP_HS_REG;
    typedef UReg<0x05, 0x60, 0, 8> ADC_UNUSED_60;
    typedef UReg<0x05, 0x61, 0, 8> ADC_UNUSED_61;
    typedef UReg<0x05, 0x62, 0, 8> ADC_UNUSED_62;
    typedef UReg<0x05, 0x63, 0, 8> TEST_BUS_SP_SEL;
    typedef UReg<0x05, 0x64, 0, 8> ADC_UNUSED_64;
    typedef UReg<0x05, 0x65, 0, 8> ADC_UNUSED_65;
    typedef UReg<0x05, 0x66, 0, 8> ADC_UNUSED_66;
    typedef UReg<0x05, 0x67, 0, 16> ADC_UNUSED_67; // + ADC_UNUSED_68;
    typedef UReg<0x05, 0x69, 0, 8> ADC_UNUSED_69;

    typedef UReg<0x05, 0xD0, 0, 32> VERYWIDEDUMMYREG;

    static const uint8_t OSD_ZOOM_1X = 0;
    static const uint8_t OSD_ZOOM_2X = 1;
    static const uint8_t OSD_ZOOM_3X = 2;
    static const uint8_t OSD_ZOOM_4X = 3;
    static const uint8_t OSD_ZOOM_5X = 4;
    static const uint8_t OSD_ZOOM_6X = 5;
    static const uint8_t OSD_ZOOM_7X = 6;
    static const uint8_t OSD_ZOOM_8X = 7;

    static const uint8_t OSD_MENU_DISP_STYLE_VERTICAL = 0;
    static const uint8_t OSD_MENU_DISP_STYLE_HORIZONTAL = 1;

    static const uint8_t OSD_ICON_NONE = 0;
    static const uint8_t OSD_ICON_BRIGHTNESS = 1;
    static const uint8_t OSD_ICON_CONTRAST = 2;
    static const uint8_t OSD_ICON_HUE = 3;
    static const uint8_t OSD_ICON_SOUND = 4;
    static const uint8_t OSD_ICON_UP_DOWN = 8;
    static const uint8_t OSD_ICON_LEFT_RIGHT = 9;
    static const uint8_t OSD_ICON_VERTICAL_SIZE = 10;
    static const uint8_t OSD_ICON_HORIZONTAL_SIZE = 11;
    static const uint8_t OSD_ICON_COUNT = 8;

    static inline uint8_t osdIcon(uint8_t index)
    {
        static const uint8_t osdIcons[8] = {
            OSD_ICON_BRIGHTNESS,
            OSD_ICON_CONTRAST,
            OSD_ICON_HUE,
            OSD_ICON_SOUND,
            OSD_ICON_UP_DOWN,
            OSD_ICON_LEFT_RIGHT,
            OSD_ICON_VERTICAL_SIZE,
            OSD_ICON_HORIZONTAL_SIZE,
        };
        return osdIcons[index];
    }

    static const uint8_t OSD_COLOR_BLACK = 0;
    static const uint8_t OSD_COLOR_BLUE = 1;
    static const uint8_t OSD_COLOR_GREEN = 2;
    static const uint8_t OSD_COLOR_CYAN = 3;
    static const uint8_t OSD_COLOR_RED = 4;
    static const uint8_t OSD_COLOR_MAGENTA = 5;
    static const uint8_t OSD_COLOR_YELLOW = 6;
    static const uint8_t OSD_COLOR_WHITE = 7;

    static const uint8_t OSD_FORMAT_YCBCR = 1;
    static const uint8_t OSD_FORMAT_RGB = 0;
};

#endif
