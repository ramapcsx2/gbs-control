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
    //
    // STATUS Registers (all registers RO)
    //
    typedef UReg<0x00, 0x00, 0, 8> STATUS_00;                           // INPUT MODE STATUS 00
        typedef UReg<0x00, 0x00, 0, 1> STATUS_IF_VT_OK;                     // Vertical stable indicator (When =1, means input vertical timing is stable)
        typedef UReg<0x00, 0x00, 1, 1> STATUS_IF_HT_OK;                     // Horizontal stable indicator (When =1, means input horizontal timing is table)
        typedef UReg<0x00, 0x00, 2, 1> STATUS_IF_HVT_OK;                    // H & V stable indicator (When =1, means input H/V timing are both stable)
        typedef UReg<0x00, 0x00, 3, 1> STATUS_IF_INP_NTSC_INT;              // NTSC interlace indicator (When =1, means input is NTSC interlace (480i) source)
        typedef UReg<0x00, 0x00, 4, 1> STATUS_IF_INP_NTSC_PRG;              // NTSC progressive indicator (When =1, means input is NTSC progressive (480P) source)
        typedef UReg<0x00, 0x00, 5, 1> STATUS_IF_INP_PAL_INT;               // PAL interlace indicator (When =1, means input is PAL interlace (576i) source)
        typedef UReg<0x00, 0x00, 6, 1> STATUS_IF_INP_PAL_PRG;               // PAL progressive indicator (When =1, means input is PAL progressive (576P) source)
        typedef UReg<0x00, 0x00, 7, 1> STATUS_IF_INP_SD;                    // SD mode indicator (When =1, means input is SD mode (480i, 480P, 576i, 576P))
    typedef UReg<0x00, 0x01, 0, 1> STATUS_IF_INP_VGA60;                 // VGA 60Hz mode (When =1, means input is VGA (640x480) 60Hz mode)
    typedef UReg<0x00, 0x01, 1, 1> STATUS_IF_INP_VGA75;                 // VGA 75Hz mode (When =1, means input is VGA (640x480) 75Hz mode)
    typedef UReg<0x00, 0x01, 2, 1> STATUS_IF_INP_VGA86;                 // VGA 85 Hz mode (When =1, means input is VGA (640x480) 85Hz mode)
    typedef UReg<0x00, 0x01, 3, 1> STATUS_IF_INP_VGA;                   // VGA mode indicator (When =1, means input is VGA (640x480) source, include 60Hz/75Hz/85Hz)
    typedef UReg<0x00, 0x01, 4, 1> STATUS_IF_INP_SVGA60;                // SVGA 60Hz mode (When =1, means input is SVGA (800x600) 60Hz mode)
    typedef UReg<0x00, 0x01, 5, 1> STATUS_IF_INP_SVGA75;                // SVGA 75Hz mode (When =1, means input is SVGA (800x600) 75Hz mode)
    typedef UReg<0x00, 0x01, 6, 1> STATUS_IF_INP_SVGA85;                // SVGA 85Hz mode (When =1, means input is SVGA (800x600) 85Hz mode)
    typedef UReg<0x00, 0x01, 7, 1> STATUS_IF_INP_SVGA;                  // SVGA mode indicator (When =1, means input is SVGA (800x600) source, include 60Hz/75Hz/85Hz)
    typedef UReg<0x00, 0x02, 0, 1> STATUS_IF_INP_XGA60;                 // XGA 60Hz mode (When =1, means input is XGA (1024x768) 60Hz mode)
    typedef UReg<0x00, 0x02, 1, 1> STATUS_IF_INP_XGA70;                 // XGA 70Hz mode (When =1, means input is XGA (1024x768) 70Hz mode)
    typedef UReg<0x00, 0x02, 2, 1> STATUS_IF_INP_XGA75;                 // XGA 75Hz mode (When =1, means input is XGA (1024x768) 75Hz mode)
    typedef UReg<0x00, 0x02, 3, 1> STATUS_IF_INP_XGA85;                 // XGA 85Hz mode (When =1, means input is XGA (1024x768) 85Hz mode)
    typedef UReg<0x00, 0x02, 4, 1> STATUS_IF_INP_XGA;                   // XGA mode indicator (When =1, means input is XGA (1024x768) source, include 60/70/75/85Hz)
    typedef UReg<0x00, 0x02, 5, 1> STATUS_IF_INP_SXGA60;                // SXGA 60Hz mode (When =1, means input is SXGA (1280x1024) 60Hz mode)
    typedef UReg<0x00, 0x02, 6, 1> STATUS_IF_INP_SXGA75;                // SXGA 75Hz mode (When =1, means input is SXGA (1280x1024) 75Hz mode)
    typedef UReg<0x00, 0x02, 7, 1> STATUS_IF_INP_SXGA85;                // SXGA 85Hz mode (When =1, means input is SXGA (1280x1024) 85Hz mode)
    typedef UReg<0x00, 0x03, 0, 8> STATUS_03;                           // INPUT MODE STATUS 03
        typedef UReg<0x00, 0x03, 0, 1> STATUS_IF_INP_SXGA;                  // SXGA mode indicator (When =1, means input is SXGA (1280x1024) mode, include 60/75/85Hz)
        typedef UReg<0x00, 0x03, 1, 1> STATUS_IF_INP_PC;                    // Graphic mode indicator (When =1, means input is graphic mode input, include VGA/SVGA/XGA/SXGA)
        typedef UReg<0x00, 0x03, 2, 1> STATUS_IF_INP_720P50;                // HD720P 50Hz mode (When =1, means input is HD720P (1280x720) 50Hz mode)
        typedef UReg<0x00, 0x03, 3, 1> STATUS_IF_INP_720P60;                // HD720P 60Hz mode (When =1, means input is HD720P (1280x720) 60Hz mode)
        typedef UReg<0x00, 0x03, 4, 1> STATUS_IF_INP_720;                   // HD720P mode indicator (When =1, means input is HD720P source, include 50Hz/60Hz)
        typedef UReg<0x00, 0x03, 5, 1> STATUS_IF_INP_2200_1125I;            // HD2200_1125 interlace (When =1, means input is 2200x1125i mode)
        typedef UReg<0x00, 0x03, 6, 1> STATUS_IF_INP_2376_1250I;            // HD2376_1250 interlace (When =1, means input is 2376x1250i mode)
        typedef UReg<0x00, 0x03, 7, 1> STATUS_IF_INP_2640_1125I;            // HD2640_1125 interlace (When =1, means input is 2640x1125i mode)
    typedef UReg<0x00, 0x04, 0, 8> STATUS_04;                           // INPUT MODE STATUS 04
        typedef UReg<0x00, 0x04, 0, 1> STATUS_IF_INP_1080I;                 // HD1808i indicator (When =1, means input is HD1080i source, include 2200x1125i, 2376x1250i, 2640x1125i modes)
        typedef UReg<0x00, 0x04, 1, 1> STATUS_IF_INP_2200_1125P;            // HD2200_1125P (When =1, means input is HD 2200x1125P mode)
        typedef UReg<0x00, 0x04, 2, 1> STATUS_IF_INP_2376_1250P;            // HD2376_1250P (When =1, means input is HD 2376x1250P mode)
        typedef UReg<0x00, 0x04, 3, 1> STATUS_IF_INP_2640_1125P;            // HD2640_1125P (When =1, means input is HD 2640x1125P mode)
        typedef UReg<0x00, 0x04, 4, 1> STATUS_IF_INP_1808P;                 // HD 1080P indicator (When =1, means input is 1080P source, include 2200x1250P, 2376x1125P)
        typedef UReg<0x00, 0x04, 5, 1> STATUS_IF_INP_HD;                    // HD mode indicator (When =1, means input is HD source, include 720P, 1080i, 1080P)
        typedef UReg<0x00, 0x04, 6, 1> STATUS_IF_INP_INT;                   // Interlace video indicator (When =1, means input is interlace video source, include 480i, 576i, 1080i)
        typedef UReg<0x00, 0x04, 7, 1> STATUS_IF_INP_PRG;                   // Progressive video indicator (When =1, means input is progressive video source, include 480P , 1080P modes)
    typedef UReg<0x00, 0x05, 0, 8> STATUS_05;                           // INPUT MODE STATUS 05
        typedef UReg<0x00, 0x05, 0, 1> STATUS_IF_INP_USER;                  // User define mode (When =1, means input is the mode which match user define resolution)
        typedef UReg<0x00, 0x05, 1, 1> STATUS_IF_NO_SYNC;                   // No sync indicator (When =1, means input is not sync timing)
        typedef UReg<0x00, 0x05, 2, 1> STATUS_IF_HT_BAD;                    // Horizontal unstable indicator (When =1, means input H sync is not stable)
        typedef UReg<0x00, 0x05, 3, 1> STATUS_IF_VT_BAD;                    // Vertical unstable indicator (When =1, means input V sync is not stable)
        typedef UReg<0x00, 0x05, 4, 1> STATUS_IF_INP_SW;                    // Mode switch indicator (When =1, means input source switch the mode)
        // 06_6:2   -   reserved

    typedef UReg<0x00, 0x06, 0, 9> HPERIOD_IF;                          // Input source H total measurement result (The value = input source H total pixels / 4)
    typedef UReg<0x00, 0x07, 1, 11> VPERIOD_IF;                         // Input source V total measurement result (The value = input source V total pixels / 4)
    // 08_4:4   -   reserved
    // 09_0:6   -   reserved
    typedef UReg<0x00, 0x09, 6, 1> STATUS_MISC_PLL648_LOCK;             // LOCK indicator from PLL648
    typedef UReg<0x00, 0x09, 7, 1> STATUS_MISC_PLLAD_LOCK;              // LOCK indicator from PLLAD
    typedef UReg<0x00, 0x0A, 0, 1> STATUS_MISC_PIP_EN_V;                // PIP enable signal in Vertical (When =1, means sub picture’s vertical period in PIP mode)
    typedef UReg<0x00, 0x0A, 1, 1> STATUS_MISC_PIP_EN_H;                // PIP enable signal in Horizontal (When =1, means sub picture’s horizontal period in PIP mode)
    // 0A_2:2   -   reserved
    typedef UReg<0x00, 0x0A, 4, 1> STATUS_MISC_VBLK;                    // Display output Vertical Blank (When =1, means in display vertical blanking)
    typedef UReg<0x00, 0x0A, 5, 1> STATUS_MISC_HBLK;                    // Display output Horizontal Blank (When =1, means in display horizontal blanking)
    typedef UReg<0x00, 0x0A, 6, 1> STATUS_MISC_VSYNC;                   // Display output Vertical Sync (When =1, means in display vertical sync (the output sync is high active))
    typedef UReg<0x00, 0x0A, 7, 1> STATUS_MISC_HSYNC;                   // Display output Horizontal Sync (When =1, means in display horizontal sync (the output sync is high active))

    typedef UReg<0x00, 0x0B, 0, 8> CHIP_ID_FOUNDRY;
    typedef UReg<0x00, 0x0C, 0, 8> CHIP_ID_PRODUCT;
    typedef UReg<0x00, 0x0D, 0, 8> CHIP_ID_REVISION;

    typedef UReg<0x00, 0x0E, 0, 1> STATUS_GPIO_GPIO;                    // GPIO bit0 (GPIO pin76) status
    typedef UReg<0x00, 0x0E, 1, 1> STATUS_GPIO_HALF;                    // GPIO bit1 (HALF pin77) status
    typedef UReg<0x00, 0x0E, 2, 1> STATUS_GPIO_SCLSA;                   // GPIO bit2 (SCLSA pin43) status
    typedef UReg<0x00, 0x0E, 3, 1> STATUS_GPIO_MBA;                     // GPIO bit3 (MBA pin107) status
    typedef UReg<0x00, 0x0E, 4, 1> STATUS_GPIO_MCS1;                    // GPIO bit4 (MCS1 pin109) status
    typedef UReg<0x00, 0x0E, 5, 1> STATUS_GPIO_HBOUT;                   // GPIO bit5 (HBOUT pin6) status
    typedef UReg<0x00, 0x0E, 6, 1> STATUS_GPIO_VBOUT;                   // GPIO bit6 (VBOUT pin7) status
    typedef UReg<0x00, 0x0E, 7, 1> STATUS_GPIO_CLKOUT;                  // GPIO bit7 (CLKOUT pin4) status

    typedef UReg<0x00, 0x0F, 0, 8> STATUS_0F;                           // INTERRUPT STATUS 00
        typedef UReg<0x00, 0x0F, 0, 1> STATUS_INT_SOG_BAD;                  // Interrupt status bit0, SOG unstable (When =1, means input SOG source is unstable)
        typedef UReg<0x00, 0x0F, 1, 1> STATUS_INT_SOG_SW;                   // Interrupt status bit1, SOG switch (When =1, means input SOG source switch the mode)
        typedef UReg<0x00, 0x0F, 2, 1> STATUS_INT_SOG_OK;                   // Interrupt status bit2, SOG stable (When =1, means input SOG source is stable)
        typedef UReg<0x00, 0x0F, 3, 1> STATUS_INT_INP_SW;                   // Interrupt status bit3, mode switch (When =1, means input source switch the mode)
        typedef UReg<0x00, 0x0F, 4, 1> STATUS_INT_INP_NO_SYNC;              // Interrupt status bit4, no sync (When =1, means input source is not H-sync input.)
        typedef UReg<0x00, 0x0F, 5, 1> STATUS_INT_INP_HSYNC;                // Interrupt status bit5, H-sync status (When =1, means input H-sync status is changed between stable and unstable)
        typedef UReg<0x00, 0x0F, 6, 1> STATUS_INT_INP_VSYNC;                // Interrupt status bi6, V-sync status (When =1, means input V-sync status is changed between stable and unstable)
        typedef UReg<0x00, 0x0F, 7, 1> STATUS_INT_INP_CSYNC;                // Interrupt status bit7, H-sync status (When =1, means input H-sync status is changed between stable and unstable)

    typedef UReg<0x00, 0x10, 0, 4> STATUS_VDS_FR_NUM;                   // Frame number
    typedef UReg<0x00, 0x10, 4, 1> STATUS_VDS_OUT_VSYNC;                // Output Vertical Sync
    typedef UReg<0x00, 0x10, 5, 1> STATUS_VDS_OUT_HSYNC;                // Output Horizontal Sync
    // 10_6:2   -    reserved
    typedef UReg<0x00, 0x11, 0, 1> STATUS_VDS_FIELD;                    // Field Index (When =0, in display top field; When =1, in display bottom field)
    typedef UReg<0x00, 0x11, 1, 1> STATUS_VDS_OUT_BLANK;                // Composite Blanking (When =0, in display active period; When =1, in display blanking period)
    // 11_2:2   -   reserved
    typedef UReg<0x00, 0x11, 4, 11> STATUS_VDS_VERT_COUNT;              // Vertical counter bit [3:0] (Vertical counter value, indicate the line number in display)
    // 12_7:1   -   reserved
    typedef UReg<0x00, 0x13, 0, 1> STATUS_MEM_FF_WFF_FIFO_FULL;         // WFF FIFO full indicator (When =1, means WFF FIFO is full)
    typedef UReg<0x00, 0x13, 1, 1> STATUS_MEM_FF_WFF_FIFO_EMPTY;        // WFF FIFO empty indicator (When =1, means WFF FIFO is empty)
    typedef UReg<0x00, 0x13, 2, 1> STATUS_MEM_FF_RFF_FIFO_FULL;         // RFF FIFO full indicator (When =1, means RFF FIFO is full)
    typedef UReg<0x00, 0x13, 3, 1> STATUS_MEM_FF_RFF_FIFO_EMPTY;        // RFF FIFO empty indicator (When =1, means RFF FIFO is empty)
    typedef UReg<0x00, 0x13, 4, 1> STATUS_MEM_FF_CAP_FIFO_FULL;         // Capture FIFO full indicator (When =1, means capture FIFO is full)
    typedef UReg<0x00, 0x13, 5, 1> STATUS_MEM_FF_CAP_FIFO_EMPTY;        // Capture FIFO empty indicator (When =1, means capture FIFO is empty)
    typedef UReg<0x00, 0x13, 6, 1> STATUS_MEM_FF_PLY_FIFO_FULL;         // Playback FIFO full indicator (When =1, means playback FIFO is full)
    typedef UReg<0x00, 0x13, 7, 1> STATUS_MEM_FF_PLY_FIFO_EMPTY;        // Playback FIFO empty indicator (When =1, means playback FIFO is empty)
    typedef UReg<0x00, 0x14, 0, 1> STATUS_MEM_FF_EXT_FIN;               // Memory control initial indicator (When =1, means external memory chip initial is finished)
    // 14_1:6   -   reserved
    // 15_0:6   -   reserved
    typedef UReg<0x00, 0x15, 7, 1> STATUS_DEINT_PULLDN;                 // 3:2 pull-down indicator (When =1, means de-interlace is in 3:2 pull-down mode)

    typedef UReg<0x00, 0x16, 0, 8> STATUS_16;                           // SYNC PROC STATUS 00
        typedef UReg<0x00, 0x16, 0, 1> STATUS_SYNC_PROC_HSPOL;              // HS polarity (When =0, means input H-sync is low active When =1, means input H-sync is high active)
        typedef UReg<0x00, 0x16, 1, 1> STATUS_SYNC_PROC_HSACT;              // HS active
        typedef UReg<0x00, 0x16, 2, 1> STATUS_SYNC_PROC_VSPOL;              // VS polarity (When =0, means input V-sync is low active When =1, means input V-sync is high active)
        typedef UReg<0x00, 0x16, 3, 1> STATUS_SYNC_PROC_VSACT;              // VS active
    // 16_4:3   -   reserved
    typedef UReg<0x00, 0x17, 0, 12> STATUS_SYNC_PROC_HTOTAL;            // H total value (Input source H-total value)
    // 18_4:4   -   reserved
    typedef UReg<0x00, 0x19, 0, 12> STATUS_SYNC_PROC_HLOW_LEN;          // H low pulse length value (Input H-sync low active pulse length (for H-sync polarity detection))
    // 1A_4:4   -   reserved
    typedef UReg<0x00, 0x1B, 0, 11> STATUS_SYNC_PROC_VTOTAL;            // V total value (Input source V-total lines value)
    // 1C_3:5   -   reserved
    // 1D_0:8   -   reserved
    // 1E_0:8   -   reserved
    typedef UReg<0x00, 0x1F, 0, 8> TEST_BUS_1F;                         // reserved (RO)
    typedef UReg<0x00, 0x20, 0, 16> TEST_FF_STATUS;                     // reserved (RO)
    typedef UReg<0x00, 0x22, 0, 8> CRC_REGOUT_RFF;                      // reserved (RO)
    typedef UReg<0x00, 0x23, 0, 8> CRC_REGOUT_PB;                       // reserved (RO)
    typedef UReg<0x00, 0x24, 0, 80> CRC_STATUS;                         // reserved (RO)
    typedef UReg<0x00, 0x2E, 0, 16> TEST_BUS;                           // reserved (RO)
        typedef UReg<0x00, 0x2E, 0, 8> TEST_BUS_2E;                         // reserved (RO)
        typedef UReg<0x00, 0x2F, 0, 8> TEST_BUS_2F;                         // reserved (RO)

    //
    // Miscellaneous Registers (all registers R/W)
    //
    typedef UReg<0x00, 0x40, 0, 1> PLL_CKIS;                            // CKIS, PLL source clock selection (When = 0, PLL use OSC clock When = 1, PLL use input clock)
    typedef UReg<0x00, 0x40, 1, 1> PLL_DIVBY2Z;                         // DIVBY2Z, PLL source clock divide bypass (When = 0, PLL source clock divide by two When = 1, PLL source clock bypass divide)
    typedef UReg<0x00, 0x40, 2, 1> PLL_IS;                              // IS, ICLK source selection (When = 0, ICLK use PLL clock When = 1, ICLK use input clock)
    typedef UReg<0x00, 0x40, 3, 1> PLL_ADS;                             // ADS, input clock selection (When = 0, input clock is from PCLKIN(pin40) When = 1, input clock is from ADC)
    typedef UReg<0x00, 0x40, 4, 3> PLL_MS;                              // MS[2:0], memory clock control
                                                                        //      When = 000, memory clock =108MHz
                                                                        //      When = 001, memory clock = 81MHz
                                                                        //      When = 010, memory clock from FBCLK (pin110) When = 011, memory clock = 162MHz
                                                                        //      When = 100, memory clock = 144MHz
                                                                        //      When = 101, memory clock = 185MHz
                                                                        //      When = 110, memory clock = 216MHz
                                                                        //      When = 111, memory clock = 129.6Mhz
    typedef UReg<0x00, 0x41, 0, 8> PLL648_CONTROL_01;                   //  See DEVELOPER_NOTES.md -> S0_41
        typedef UReg<0x00, 0x41, 0, 2> PLL_VS;                               // Display clock tuning register
        typedef UReg<0x00, 0x41, 2, 2> PLL_VS2;                              // Display clock tuning register
        typedef UReg<0x00, 0x41, 4, 2> PLL_VS4;                              // Display clock tuning register
        typedef UReg<0x00, 0x41, 6, 1> PLL_2XV;                              // Display clock tuning register
        typedef UReg<0x00, 0x41, 7, 1> PLL_4XV;                              // Display clock tuning register
    // S0_42    -    reserved
    typedef UReg<0x00, 0x43, 0, 8> PLL648_CONTROL_03;                   //
        typedef UReg<0x00, 0x43, 0, 2> PLL_R;                               // Skew control for testing
        typedef UReg<0x00, 0x43, 2, 2> PLL_S;                               // Skew control for testing
        typedef UReg<0x00, 0x43, 4, 1> PLL_LEN;                             // Lock Enable
        typedef UReg<0x00, 0x43, 5, 1> PLL_VCORST;                          // VCO control voltage reset bit (When = 1, reset VCO control voltage)
    typedef UReg<0x00, 0x44, 0, 1> DAC_RGBS_PWDNZ;                      // DAC enable (When = 0, DAC (R,G,B,S) in power down mode When = 1, DAC (R,G,B,S) is enable)
    typedef UReg<0x00, 0x44, 1, 1> DAC_RGBS_RPD;                        // RPD, RDAC power down control (When = 0, RDAC work normally; When = 1, RDAC is in power down mode)
    typedef UReg<0x00, 0x44, 2, 1> DAC_RGBS_R0ENZ;                      // R0ENZ, DAC min output bypass (When = 0, RDAC output Min voltage; When = 1, RDAC output follow input R data)
    typedef UReg<0x00, 0x44, 3, 1> DAC_RGBS_R1EN;                       // R1EN, RDAC max output control (When = 0, RDAC output follow input R data When = 1, RDAC output Max voltage)
    typedef UReg<0x00, 0x44, 4, 1> DAC_RGBS_GPD;                        // GPD, GDAC power down control (When = 0, GDAC work normally; When = 1, GDAC is in power down mode)
    typedef UReg<0x00, 0x44, 5, 1> DAC_RGBS_G0ENZ;                      // G0ENZ, GDAC min output bypass (When = 0, GDAC output Min voltage; When = 1, GDAC output follow input G data)
    typedef UReg<0x00, 0x44, 6, 1> DAC_RGBS_G1EN;                       // R1EN, RDAC max output control (When = 0, RDAC output follow input R data When = 1, RDAC output Max voltage)
    typedef UReg<0x00, 0x44, 7, 1> DAC_RGBS_BPD;                        // BPD, BDAC power down control (When = 0, BDAC work normally; When = 1, BDAC is in power down mode)
    typedef UReg<0x00, 0x45, 0, 1> DAC_RGBS_B0ENZ;                      // B0ENZ, BDAC min output bypass (When = 0, BDAC output Min voltage; When = 1, BDAC output follow input B data)
    typedef UReg<0x00, 0x45, 1, 1> DAC_RGBS_B1EN;                       // B1EN, BDAC max output control (When = 0, BDAC output follow input B data; When = 1, BDAC output Max voltage)
    typedef UReg<0x00, 0x45, 2, 1> DAC_RGBS_SPD;                        // SPD, SDAC power down control (When = 0, GDAC work normally; When = 1, GDAC is in power down mode)
    typedef UReg<0x00, 0x45, 3, 1> DAC_RGBS_S0ENZ;                      // S0ENZ, SDAC min output bypass (When = 0, SDAC output Min voltage; When = 1, SDAC output follow input S data)
    typedef UReg<0x00, 0x45, 4, 1> DAC_RGBS_S1EN;                       // S1EN, SDAC max output control (When = 0, SDAC output follow input S data; When = 1, SDAC output Max voltage)
    // S0_45_5:1    -   reserverd
    typedef UReg<0x00, 0x45, 6, 2> CKT_FF_CNTRL;                        // CKT used to control FIFO
    typedef UReg<0x00, 0x46, 0, 8> RESET_CONTROL_0x46;                  // CONTROL 00
        typedef UReg<0x00, 0x46, 0, 1> SFTRST_IF_RSTZ;                      // Input formatter reset control (When = 0, input formatter is in reset status; When = 1, input formatter work normally)
        typedef UReg<0x00, 0x46, 1, 1> SFTRST_DEINT_RSTZ;                   // Deint_madpt3 reset control (When = 0, deint_madpt3 is in reset status; When = 1, deint_madpt3 work normally)
        typedef UReg<0x00, 0x46, 2, 1> SFTRST_MEM_FF_RSTZ;                  // Mem_ff (wff/rff/playback/capture) reset control (When = 0, mem_ff is in reset status; When = 1, mem_ff work normally)
        typedef UReg<0x00, 0x46, 3, 1> SFTRST_MEM_RSTZ;                     // Mem controller reset control (When = 0, mem controller is in reset status; When = 1, mem controller work normally)
        typedef UReg<0x00, 0x46, 4, 1> SFTRST_FIFO_RSTZ;                    // FIFO reset control (When = 0, all FIFO (FF64/FF512) is in reset status; When = 1, all FIFO work normally)
        typedef UReg<0x00, 0x46, 5, 1> SFTRST_OSD_RSTZ;                     // OSD reset control (When = 0, OSD generator is in reset status; When = 1, OSD generator work normally)
        typedef UReg<0x00, 0x46, 6, 1> SFTRST_VDS_RSTZ;                     // Vds_proc reset control (When = 0, vds_proc is in reset status; When = 1, vds_proc work normally)
        // 46_7:1   -   reserved
    typedef UReg<0x00, 0x47, 0, 8> RESET_CONTROL_0x47;                  // CONTROL 01
        typedef UReg<0x00, 0x47, 0, 1> SFTRST_DEC_RSTZ;                     // Decimation reset control (When = 0, decimation is in reset status; When = 1, decimation work normally)
        typedef UReg<0x00, 0x47, 1, 1> SFTRST_MODE_RSTZ;                    // Mode detection reset control (When = 0, mode detection is in reset status When = 1, mode detection work normally)
        typedef UReg<0x00, 0x47, 2, 1> SFTRST_SYNC_RSTZ;                    // Sync procesor reset control (When = 0, sync processor is in reset status When = 1, sync processor work normally)
        typedef UReg<0x00, 0x47, 3, 1> SFTRST_HDBYPS_RSTZ;                  // HD bypass channel reset control (When = 0, HD bypass is in reset status; When = 1, HD bypasswork normally)
        typedef UReg<0x00, 0x47, 4, 1> SFTRST_INT_RSTZ;                     // Interrupt generator reset control (When = 0, interrupt generator is in reset status; When = 1, interrupt generator work normally)
        // 47_5:3   -    reserved
    typedef UReg<0x00, 0x48, 0, 8> PAD_CONTROL_00_0x48;                 // CONTROL 00
        typedef UReg<0x00, 0x48, 0, 1> PAD_BOUT_EN;                         // VB_[7:0] output control aka debug pin (When = 0, disable VB_[7:0] (test_out_[7:0]) output; When = 1, enable VB_[7:0] (test_out_[7:0]) output)
        typedef UReg<0x00, 0x48, 1, 1> PAD_BIN_ENZ;                         // VB_[7:0] input control (When = 0, enable VB_[7:0] input; When = 1, disable VB_[7:0] input)
        typedef UReg<0x00, 0x48, 2, 1> PAD_ROUT_EN;                         // VR_[7:0] output control (When = 0, disable VR_[7:0] (test_out_[15:8]) output; When = 1, enable VR_[7:0] (test_out_[15:8]) output)
        typedef UReg<0x00, 0x48, 3, 1> PAD_RIN_ENZ;                         // VR_[7:0] input control (When = 0, enable VR_[7:0] input; When = 1, disable VR_[7:0] input)
        typedef UReg<0x00, 0x48, 4, 1> PAD_GOUT_EN;                         // VG_[7:0] output control (When = 0, disable VG_[7:0] (test_out_[23:16]) output; When = 1, enable VG_[7:0] (test_out_[23:16]) output)
        typedef UReg<0x00, 0x48, 5, 1> PAD_GIN_ENZ;                         // VG_[7:0] input control (When = 0, enable VG_[7:0] input; When = 1, disable VG_[7:0] input)
        typedef UReg<0x00, 0x48, 6, 1> PAD_SYNC1_IN_ENZ;                    // H/V sync1 input control (When = 0, enable H/V sync1 input filter; When = 1, disable H/V sync1 input filter)
        typedef UReg<0x00, 0x48, 7, 1> PAD_SYNC2_IN_ENZ;                    // H/V sync2 input control (When = 0, enable H/V sync2 input filter; When = 1, disable H/V sync2 input filter)
    typedef UReg<0x00, 0x49, 0, 8> PAD_CONTROL_01_0x49;                 // CONTROL 01
        typedef UReg<0x00, 0x49, 0, 1> PAD_CKIN_ENZ;                        // PCLKIN control (When = 0, PCLKIN input enable When = 1, PCLKIN input disable)
        typedef UReg<0x00, 0x49, 1, 1> PAD_CKOUT_ENZ;                       // CLKOUT control (When = 0, CLKOUT output enable When = 1, CLKOUT output disable)
        typedef UReg<0x00, 0x49, 2, 1> PAD_SYNC_OUT_ENZ;                    // HSOUT/VSOUT control (When = 0, HSOUT/VSOUT output enable When = 1, HSOUT/VSOUT output disable)
        typedef UReg<0x00, 0x49, 3, 1> PAD_BLK_OUT_ENZ;                     // HBOUT/VBOUT control (When = 0, HBOUT/VBOUT output enable When = 1, HBOUT/VBOUT output disable)
        typedef UReg<0x00, 0x49, 4, 1> PAD_TRI_ENZ;                         // Tri-state gate control (When = 0, enable output pad’s tri-state gate When = 1, disable output pad’s tri-state gate)
        typedef UReg<0x00, 0x49, 5, 1> PAD_PLDN_ENZ;                        // Pull-down control (When = 0, enable pad’s pull-down transistor; When = 1, disable pad’s pull-down transistor)
        typedef UReg<0x00, 0x49, 6, 1> PAD_PLUP_ENZ;                        // Pull-up control (When = 0, enable pad’s pull-up transistor; When = 1, disable pad’s pull-up transistor)
        // 49_7:1   -   reserved
    typedef UReg<0x00, 0x4A, 0, 3> PAD_OSC_CNTRL;                       // OSC pad C2/C1/C0 control
    typedef UReg<0x00, 0x4A, 3, 1> PAD_XTOUT_TTL;                       // OSC pad output control (When = 0, enable OSC pad output by schmitt When = 1, enable OSC pad output by TTL)
    typedef UReg<0x00, 0x4B, 0, 1> DAC_RGBS_BYPS_IREG;                  // DAC input DFF control (When = 0, DAC input DFF is falling edge D-flipflop When = 1, bypass falling edge D-flipflop)
    typedef UReg<0x00, 0x4B, 1, 1> DAC_RGBS_BYPS2DAC;                   // HD bypass to DAC control (When = 0, disable HD bypass channel to DAC; When = 1, enable HD bypass channel to DAC directly)
    typedef UReg<0x00, 0x4B, 2, 1> DAC_RGBS_ADC2DAC;                    // ADC to DAC control (When = 0, disable ADC (with decimation) to DAC; When = 1, enable ADC (with decimation) to DAC directly)
    typedef UReg<0x00, 0x4D, 0, 5> TEST_BUS_SEL;                        // Test bus selection
    typedef UReg<0x00, 0x4D, 5, 1> TEST_BUS_EN;                         // Test bus enable (When = 0, disable test bus output; When = 1, test bus output to VR_[7:0], VB_[7:0] (test_out_[15:0]))
    // 4D_6:2   -   reserved
    typedef UReg<0x00, 0x4E, 0, 1> DIGOUT_BYPS2PAD;                     // HD bypass channel to digital output control (When = 0, disable HD bypass to digital output; When = 1, enable HD bypass to digital output (VG_[7:0], VR_[7:0], VB_[7:0]))
    typedef UReg<0x00, 0x4E, 1, 1> DIGOUT_ADC2PAD;                      // ADC to digital output control (When = 0, disable ADC to digital output; When = 1, enable ADC (with decimation) to digital output (VG, VR, VB))
    // 4E_2:6   -   reserved
    typedef UReg<0x00, 0x4F, 0, 1> DAC_RGBS_V4CLK_INVT;                 // V4CLK invert control ()
    typedef UReg<0x00, 0x4F, 1, 1> OUT_CLK_PHASE_CNTRL;                 // CLKOUT invert control ()
    typedef UReg<0x00, 0x4F, 2, 1> OUT_CLK_EN;                          // CLKOUT selection control ()
    typedef UReg<0x00, 0x4F, 3, 2> CLKOUT_EN;                           // CLKOUT enable control ()
    typedef UReg<0x00, 0x4F, 5, 1> OUT_SYNC_CNTRL;                      // H/V sync output enable ()
    typedef UReg<0x00, 0x4F, 6, 2> OUT_SYNC_SEL;                        // H/V sync output selection control
                                                                        //      When = 00, H/V sync output are from vds_proc
                                                                        //      When = 01, H/V sync output are from HD bypass
                                                                        //      When = 10, H/V sync output are from sync processor
                                                                        //      When = 11, reserved
    typedef UReg<0x00, 0x50, 0, 1> OUT_BLANK_SEL_0;                     // HBOUT/VBUT selection control (When = 0, VBOUT output Vertical Blank; When = 1, VBOUT output composite Display Enable)
    typedef UReg<0x00, 0x50, 1, 1> OUT_BLANK_SEL_1;                     // HBOUT/VBOUT selection control (When = 0, HBOUT/VBOUT is from vds_proc When = 1, HBOUT/VBOUT is from HD bypass)
    // 50_2:2    -  reserved
    typedef UReg<0x00, 0x50, 4, 1> IN_BLANK_SEL;                        // Input blank selection (When = 0, disable input composite Display Enable When = 1, enable input composite Display Enable)
    typedef UReg<0x00, 0x50, 5, 1> IN_BLANK_IREG_BYPS;                  // Input blank IREG bypas (When = 0, input composite Display Enable latched by falling edge DFF When = 1, bypass falling edge DFF)
    // 50_6:2   -  reserved
    // 51_0:8   - not described

    typedef UReg<0x00, 0x52, 0, 8> GPIO_CONTROL_00;                     // CONTROL 00
        typedef UReg<0x00, 0x52, 0, 1> GPIO_SEL_0;                          // GPIO bit0 selection (When = 0, GPIO (pin76) is used as INTZ output When = 1, GPIO (pin76) is used as GPIO bit0)
        typedef UReg<0x00, 0x52, 1, 1> GPIO_SEL_1;                          // GPIO bit1 selection (When = 0, HALF (pin77) is used as half tone input When = 1, HALF (pin77) is used as GPIO bit1)
        typedef UReg<0x00, 0x52, 2, 1> GPIO_SEL_2;                          // GPIO bit2 selection (When = 0, SCLSA (pin43) is used as two wire serial bus slave address selection When = 1, SCLSA (pin43) is used as GPIO bit2)
        typedef UReg<0x00, 0x52, 3, 1> GPIO_SEL_3;                          // GPIO bit3 selection (When = 0, MBA (pin107) is used as external memory BA When = 1, MBA (pin107) is used as GPIO bit3)
        typedef UReg<0x00, 0x52, 4, 1> GPIO_SEL_4;                          // GPIO bit4 selection (When = 0, MCS1 (pin109) is used as external memory CS1 When = 1, MCS1 (pin109) is used as GPIO bit4)
        typedef UReg<0x00, 0x52, 5, 1> GPIO_SEL_5;                          // GPIO bit5 selection (When = 0, HBOUT (pin6) is used as H-blank output When = 1, HBOUT (pin6) is used as GPIO bit5)
        typedef UReg<0x00, 0x52, 6, 1> GPIO_SEL_6;                          // GPIO bit6 selection (When = 0, VBOUT (pin7) is used as V-blank output When = 1, VBOUT (pin7) is used as GPIO bit6)
        typedef UReg<0x00, 0x52, 7, 1> GPIO_SEL_7;                          // GPIO bit7 selection (When = 0, CLKOUT (pin4) is used as clock output When = 1, CLKOUT (pin4) is used as GPIO bit7)
    typedef UReg<0x00, 0x53, 0, 8> GPIO_CONTROL_01;                     // CONTROL 01
        typedef UReg<0x00, 0x53, 0, 1> GPIO_EN_0;                           // GPIO bit0 output enable (When = 0, GPIO bit0 output disable When = 1, GPIO bit0 output enable)
        typedef UReg<0x00, 0x53, 1, 1> GPIO_EN_1;                           // GPIO bit1 output enable (When = 0, GPIO bit1 output disable When = 1, GPIO bit1 output enable)
        typedef UReg<0x00, 0x53, 2, 1> GPIO_EN_2;                           // GPIO bit2 output enable (When = 0, GPIO bit2 output disable When = 1, GPIO bit2 output enable)
        typedef UReg<0x00, 0x53, 3, 1> GPIO_EN_3;                           // GPIO bit3 output enable (When = 0, GPIO bit3 output disable When = 1, GPIO bit3 output enable)
        typedef UReg<0x00, 0x53, 4, 1> GPIO_EN_4;                           // GPIO bit4 output enable (When = 0, GPIO bit4 output disable When = 1, GPIO bit4 output enable)
        typedef UReg<0x00, 0x53, 5, 1> GPIO_EN_5;                           // GPIO bit5 output enable (When = 0, GPIO bit5 output disable When = 1, GPIO bit5 output enable)
        typedef UReg<0x00, 0x53, 6, 1> GPIO_EN_6;                           // GPIO bit6 output enable (When = 0, GPIO bit6 output disable When = 1, GPIO bit6 output enable)
        typedef UReg<0x00, 0x53, 7, 1> GPIO_EN_7;                           // GPIO bit7 output enable (When = 0, GPIO bit7 output disable When = 1, GPIO bit7 output enable)
    typedef UReg<0x00, 0x54, 0, 8> GPIO_CONTROL_02;                     // CONTROL 02
        typedef UReg<0x00, 0x54, 0, 1> GPIO_VAL_0;                          // GPIO bit0 output value
        typedef UReg<0x00, 0x54, 1, 1> GPIO_VAL_1;                          // GPIO bit1 output value
        typedef UReg<0x00, 0x54, 2, 1> GPIO_VAL_2;                          // GPIO bit2 output value
        typedef UReg<0x00, 0x54, 3, 1> GPIO_VAL_3;                          // GPIO bit3 output value
        typedef UReg<0x00, 0x54, 4, 1> GPIO_VAL_4;                          // GPIO bit4 output value
        typedef UReg<0x00, 0x54, 5, 1> GPIO_VAL_5;                          // GPIO bit5 output value
        typedef UReg<0x00, 0x54, 6, 1> GPIO_VAL_6;                          // GPIO bit6 output value
        typedef UReg<0x00, 0x54, 7, 1> GPIO_VAL_7;                          // GPIO bit7 output value
    // 55   -   not described
    typedef UReg<0x00, 0x57, 7, 1> INVT_RING_EN;                        // Enable invert ring (When = 0, disable invert ring; When = 1, enable invert ring for processing test)
    typedef UReg<0x00, 0x58, 0, 8> INTERRUPT_CONTROL_00;                // CONTROL 00
        typedef UReg<0x00, 0x58, 0, 1> INT_CONTROL_RST_SOGBAD;              // Interrupt bit0 reset control (When = 1, interrupt bit status will be reset to zero)
        typedef UReg<0x00, 0x58, 1, 1> INT_CONTROL_RST_SOGSWITCH;           // Interrupt bit1 reset control (When = 1, interrupt bit status will be reset to zero)
        typedef UReg<0x00, 0x58, 2, 1> INT_RST_2;                           // Interrupt bit2 reset control (When = 1, interrupt bit status will be reset to zero)
        typedef UReg<0x00, 0x58, 3, 1> INT_RST_3;                           // Interrupt bit3 reset control (When = 1, interrupt bit status will be reset to zero)
        typedef UReg<0x00, 0x58, 4, 1> INT_CONTROL_RST_NOHSYNC;             // Interrupt bit4 reset control (When = 1, interrupt bit status will be reset to zero)
        typedef UReg<0x00, 0x58, 5, 1> INT_RST_5;                           // Interrupt bit5 reset control (When = 1, interrupt bit status will be reset to zero)
        typedef UReg<0x00, 0x58, 6, 1> INT_RST_6;                           // Interrupt bit6 reset control (When = 1, interrupt bit status will be reset to zero)
        typedef UReg<0x00, 0x58, 7, 1> INT_RST_7;                           // Interrupt bit7 reset control (When = 1, interrupt bit status will be reset to zero)
    typedef UReg<0x00, 0x59, 0, 8> INTERRUPT_CONTROL_01;                // CONTROL 01
        typedef UReg<0x00, 0x59, 0, 1> INT_ENABLE0;                         // Interrupt bit0 enable (When = 1, enable interrupt bit generator)
        typedef UReg<0x00, 0x59, 1, 1> INT_ENABLE1;                         // Interrupt bit1 enable (When = 1, enable interrupt bit generator)
        typedef UReg<0x00, 0x59, 2, 1> INT_ENABLE2;                         // Interrupt bit2 enable (When = 1, enable interrupt bit generator)
        typedef UReg<0x00, 0x59, 3, 1> INT_ENABLE3;                         // Interrupt bit3 enable (When = 1, enable interrupt bit generator)
        typedef UReg<0x00, 0x59, 4, 1> INT_ENABLE4;                         // Interrupt bit4 enable (When = 1, enable interrupt bit generator)
        typedef UReg<0x00, 0x59, 5, 1> INT_ENABLE5;                         // Interrupt bit5 enable (When = 1, enable interrupt bit generator)
        typedef UReg<0x00, 0x59, 6, 1> INT_ENABLE6;                         // Interrupt bit6 enable (When = 1, enable interrupt bit generator)
        typedef UReg<0x00, 0x59, 7, 1> INT_ENABLE7;                         // Interrupt bit7 enable (When = 1, enable interrupt bit generator)

    //
    // Input Formatter Registers (all registers R/W)
    //
    typedef UReg<0x01, 0x00, 0, 1> IF_IN_DREG_BYPS;                     // Input pipe by pass (Use the falling or rising edge of clock to get the input data. 0: Clock input data on the falling edge of ICLK.; 1: Clock input date on the rising edge of ICLK.)
    typedef UReg<0x01, 0x00, 1, 1> IF_MATRIX_BYPS;                      // Rgb2yuv matrix bypass (If source is yuv24bit, bypass the rgb2yuv matrix. 0:source is 24bit RGB. Do rgb2yuv; 1: data bypass)
    typedef UReg<0x01, 0x00, 2, 1> IF_UV_REVERT;                        // 8bit to 16bit convert Y/UV flip control (If input is 8bit data, when it convert to 16bit, this bit control Y and UV order: 0: Keep the designed order; 1: Flip the Y and UV order)
    typedef UReg<0x01, 0x00, 3, 1> IF_SEL_656;                          // Select CCIR656 data (If input data is 8bit CCIR656 mode, choose the 656 data path. 0: input is CCIR 601 mode. Choose the CCIR601mode timing. 1: input is CCIR 656 mode. Choose the CCIR656 mode timing)
    typedef UReg<0x01, 0x00, 4, 1> IF_SEL16BIT;                         // Select 16bit data (If source data is 16bit. Choose the 16bits data path. Use in conjunction with register sel_24bit to choose the input data format.)
    typedef UReg<0x01, 0x00, 5, 1> IF_VS_SEL;                           // Vertical sync select (Choose the periodical or virtual vertical timing. 0: choose the VCR mode timing generation. 1: choose the normal mode timing generation) (see: DEVELOPER_NOTES.md -> S1_00)
    typedef UReg<0x01, 0x00, 6, 1> IF_PRGRSV_CNTRL;                     // Select progressive data (Progressive mode. Choose the progressive data. 0: source is interlaced; 1: source is progressive)
    typedef UReg<0x01, 0x00, 7, 1> IF_HS_FLIP;                          // Horizontal sync flip control (Control the horizontal sync output from CCIR process 0: keep the original horizontal sync; 1: flip horizontal sync.)
    typedef UReg<0x01, 0x01, 0, 1> IF_VS_FLIP;                          // Vertical sync flip control (Control the vertical sync output from CCIR process 0: keep original vertical sync; 1: flip vertical sync.)
    typedef UReg<0x01, 0x01, 1, 1> IF_UV_FLIP;                          // YUV 422to444 UV flip control (Control the U and V order in yuv422to444 conversion. 0: keep original U and V order; 1: exchange the U and V order)
    typedef UReg<0x01, 0x01, 2, 1> IF_U_DELAY;                          // U data select in YUV 422to444 conversion (Select original U data or 1-clock delayed U data, so that it can align with V data. 0: select original U data after dmux; 1: select 1-clock delayed U data after dmux)
    typedef UReg<0x01, 0x01, 3, 1> IF_V_DELAY;                          // V data select in YUV 422to444 conversion (Select original V data or 1-clock delayed V data, so that it can align with U data. 0: select original V data after dmux; 1: select 1-clock delayed V data after dmux)
    typedef UReg<0x01, 0x01, 4, 1> IF_TAP6_BYPS;                        // Tap6 interpolator bypass control in YUV 422to444 conversion (Select the data if pass the tap6 interpolator or not. 0: the data will pass the tap6 interpolator; 1: the data will not pass the tap6 interpolator)
    typedef UReg<0x01, 0x01, 5, 2> IF_Y_DELAY;                          // Y data pipes control in YUV422to444 conversion (Control the Y data pipe delay, so that it can align with U and V. see: DEVELOPER_NOTES.md -> S01_01)
    typedef UReg<0x01, 0x01, 7, 1> IF_SEL24BIT;                         // Select 24bit data (If input source is 24bit data, choose the 24bit data path.)
    typedef UReg<0x01, 0x02, 0, 8> INPUT_FORMATTER_02;                  // INPUT_FORMATTER 02
        typedef UReg<0x01, 0x02, 0, 1> IF_SEL_WEN;                          // Select the write enable for line double (If the input is HD source, this bit will be set to 1. 0: if the source is SD data; 1: if the source is HD data)
        typedef UReg<0x01, 0x02, 1, 1> IF_HS_SEL_LPF;                       // Low pass filter or interpolator selection (The low pass filter and interpolator data path is combined together. 0: select interpolator data path; 1: select low pass filter data path)
        typedef UReg<0x01, 0x02, 2, 1> IF_HS_INT_LPF_BYPS;                  // Combined INT and LPF data path bypass control (If the data can’t do horizontal scaling-down, bypass the INT/LPF data path. 0: select the INT/LPF data path; 1: bypass the INT/LPF data path)
        typedef UReg<0x01, 0x02, 3, 1> IF_HS_PSHIFT_BYPS;                   // Phase adjustment bypass control (If the data can’t do phase adjustment, this bit should be set to 1. 0: select phase adjustment data path; 1: bypass phase adjustment)
        typedef UReg<0x01, 0x02, 4, 1> IF_HS_TAP11_BYPS;                    // Tap11 LPF bypass control in YUV444to422 conversion (Select the data if pass the tap11 LPF or not. 0: the data will pass the tap11 low pass filter; 1: the data will not pass the tap11 low pass filter)
        typedef UReg<0x01, 0x02, 5, 2> IF_HS_Y_PDELAY;                      // Y data pipes control in YUV444to422 conversion - Control the Y data pipe delay, so that it can align with UV. (see: DEVELOPER_NOTE.md -> S1_02)
        typedef UReg<0x01, 0x02, 7, 1> IF_HS_UV_SIGN2UNSIGN;                // UV data select (If UV is signed, select the unsigned UV data 0: select the original UV; 1: select the UV after sign processing)
    typedef UReg<0x01, 0x03, 0, 8> IF_HS_RATE_SEG0;                     // Horizontal non-linear scaling-down 1st segment DDA increment [11:4] (total 12 bits)
                                                                        // The entire segment share the lowest 4bit, that is to say, the whole scale ration is
                                                                        // hscale = {hscale0, hscale_low}. Assume the scaling ratio is n/m, then the value should be 4095x(m-n)/n
    typedef UReg<0x01, 0x04, 0, 8> IF_HS_RATE_SEG1;                     // Horizontal non-linear scaling-down 2nd segment DDA increment [11:4] (total 12 bits)
    typedef UReg<0x01, 0x05, 0, 8> IF_HS_RATE_SEG2;                     // Horizontal non-linear scaling-down 3rd segment DDA increment [11:4] (total 12 bits)
    typedef UReg<0x01, 0x06, 0, 8> IF_HS_RATE_SEG3;                     // Horizontal non-linear scaling-down 4th segment DDA increment [11:4] (total 12 bits)
    typedef UReg<0x01, 0x07, 0, 8> IF_HS_RATE_SEG4;                     // Horizontal non-linear scaling-down 5th segment DDA increment [11:4] (total 12 bits)
    typedef UReg<0x01, 0x08, 0, 8> IF_HS_RATE_SEG5;                     // Horizontal non-linear scaling-down 6th segment DDA increment [11:4] (total 12 bits)
    typedef UReg<0x01, 0x09, 0, 8> IF_HS_RATE_SEG6;                     // Horizontal non-linear scaling-down 7th segment DDA increment [11:4] (total 12 bits)
    typedef UReg<0x01, 0x0A, 0, 8> IF_HS_RATE_SEG7;                     // Horizontal non-linear scaling-down 8th segment DDA increment [11:4] (total 12 bits)
    typedef UReg<0x01, 0x0B, 0, 4> IF_HS_RATE_LOW;                      // Horizontal non-linear scaling-down DDA increment shared lowest 4 bits [3:0] (total 12 bits)
    typedef UReg<0x01, 0x0B, 4, 2> IF_HS_DEC_FACTOR;                    // Horizontal non-linear scaling-down factor select
                                                                        //    If the scaling ratio is less than 1⁄2, use it and DDA to generate the we and phase
                                                                        //      00: scaling-ratio is more than 1⁄2
                                                                        //      01: scaling-ratio is less than 1⁄2
                                                                        //      10: scaling-ratio is less than 1⁄4
    typedef UReg<0x01, 0x0B, 6, 1> IF_SEL_HSCALE;                       // Select the data path after horizontal scaling-down
                                                                        //  If the data have do scaling-down, this bit should be open.
                                                                        //     0: select the data and write enable from CCIR to line double.
                                                                        //     1: select the scaling-down data and write enable to line double)
    typedef UReg<0x01, 0x0B, 7, 1> IF_LD_SEL_PROV;                      // Line double read reset select
                                                                        //  If source is progressive data, choose the related progressive timing as read reset timing.
                                                                        //      0: select read reset timing of interlace data.
                                                                        //      1: select read reset timing of progressive data
    typedef UReg<0x01, 0x0c, 0, 1> IF_LD_RAM_BYPS;                      // Line double bypass control
                                                                        //  If the interlace data can’t do line double, if the progressive data can’t do scaling- down, line double FIFO should be bypass.
                                                                        //     0: select interlace line double data from FIFO.
                                                                        //     1: bypass line double FIFO
    typedef UReg<0x01, 0x0c, 1, 4> IF_LD_ST;                            // Line double write reset generation start position (If the internal counter equals the defined value the write reset will be high pulse.)
    typedef UReg<0x01, 0x0c, 5, 11> IF_INI_ST;                          // Initial position (Start position indicator of vertical blanking. For the internal line_counter, the detail pixel’s shift that the line_counter count compare to the horizontal sync)
    typedef UReg<0x01, 0x0e, 0, 11> IF_HSYNC_RST;                       // Total pixel number per line (Use to generate progressive timing if input is interlace data [10:0])

    typedef UReg<0x01, 0x10, 0, 11> IF_HB_ST;                           // Horizontal blanking (set 0) start position [10:0].
    typedef UReg<0x01, 0x12, 0, 11> IF_HB_SP;                           // Horizontal blanking (set 0) stop position [10:0]
    typedef UReg<0x01, 0x14, 0, 11> IF_HB_ST1;                          // Horizontal blanking (set 1) start position [10:0]
    typedef UReg<0x01, 0x16, 0, 11> IF_HB_SP1;                          // Horizontal blanking (set 1) stop position [10:0]
    typedef UReg<0x01, 0x18, 0, 11> IF_HB_ST2;                          // Horizontal blanking (set 2) start position [10:0].
    typedef UReg<0x01, 0x1a, 0, 11> IF_HB_SP2;                          // Horizontal blanking (set 2) stop position [10:0]
    typedef UReg<0x01, 0x1c, 0, 11> IF_VB_ST;                           // Vertical blanking start position [10:0].
    typedef UReg<0x01, 0x1e, 0, 11> IF_VB_SP;                           // Vertical blanking stop position [10:0].
    typedef UReg<0x01, 0x20, 0, 12> IF_LINE_ST;                         // Progressive line start position.
    typedef UReg<0x01, 0x22, 0, 12> IF_LINE_SP;                         // Progressive line stop position.
    typedef UReg<0x01, 0x24, 0, 12> IF_HBIN_ST;                         // Horizontal blank for scale down line reset start position [11:0]
    typedef UReg<0x01, 0x26, 0, 12> IF_HBIN_SP;                         // Horizontal blank for scale down line reset stop position [11:0]
    typedef UReg<0x01, 0x28, 1, 1> IF_LD_WRST_SEL;                      // Select hbin/line write reset (0 - select line generated write reset, 1 - select hbin generated write reset)
    typedef UReg<0x01, 0x28, 2, 1> IF_SEL_ADC_SYNC;                     // Select ADC sync to data path
    typedef UReg<0x01, 0x28, 3, 1> IF_TEST_EN;                          // IF test bus control enable - Enable test signal.
    typedef UReg<0x01, 0x28, 4, 4> IF_TEST_SEL;                         // Test signals select bits - Select which signal to the test bus.
    typedef UReg<0x01, 0x29, 0, 1> IF_AUTO_OFST_EN;                     // Auto offset adjustment enable (1 - enable, 0 - disable)
    typedef UReg<0x01, 0x29, 1, 1> IF_AUTO_OFST_PRD;                    // Auto offset adjustment period control (1 - by frame, 0 - by line)
    typedef UReg<0x01, 0x2A, 0, 4> IF_AUTO_OFST_U_RANGE;                // U channel offset detection range
    typedef UReg<0x01, 0x2A, 4, 4> IF_AUTO_OFST_V_RANGE;                // V channel offset detection range
    // 01_2B:2F    -   not described registers
    typedef UReg<0x01, 0x2B, 0, 7> GBS_PRESET_ID;

    // ! @sk: supressed for now
    // typedef UReg<0x01, 0x2B, 7, 1> GBS_PRESET_CUSTOM;
    // typedef UReg<0x01, 0x2C, 0, 1> GBS_OPTION_SCANLINES_ENABLED;
    // typedef UReg<0x01, 0x2C, 1, 1> GBS_OPTION_SCALING_RGBHV;
    // typedef UReg<0x01, 0x2C, 2, 1> GBS_OPTION_PALFORCED60_ENABLED;
    // typedef UReg<0x01, 0x2C, 3, 1> GBS_RUNTIME_UNUSED_BIT;
    // typedef UReg<0x01, 0x2C, 4, 1> GBS_RUNTIME_FTL_ADJUSTED;
    // typedef UReg<0x01, 0x2D, 0, 8> GBS_PRESET_DISPLAY_CLOCK;

    //
    // HDBypass (all registers R/W)
    //
    typedef UReg<0x01, 0x30, 1, 1> HD_MATRIX_BYPS;                      // Available only when input source is YUV source
    typedef UReg<0x01, 0x30, 2, 1> HD_DYN_BYPS;                         // If the input is YUV data, it must do dynamic range.
    typedef UReg<0x01, 0x30, 3, 1> HD_SEL_BLK_IN;                       // Choose the input blank or generated blank use sync.
    typedef UReg<0x01, 0x31, 0, 8> HD_Y_GAIN;                           // The gain value of Y dynamic range.
    typedef UReg<0x01, 0x32, 0, 8> HD_Y_OFFSET;                         // The offset value of Y dynamic range.
    typedef UReg<0x01, 0x33, 0, 8> HD_U_GAIN;                           // The gain value of U dynamic range.
    typedef UReg<0x01, 0x34, 0, 8> HD_U_OFFSET;                         // The offset value of U dynamic range.
    typedef UReg<0x01, 0x35, 0, 8> HD_V_GAIN;                           // The gain value of V dynamic range.
    typedef UReg<0x01, 0x36, 0, 8> HD_V_OFFSET;                         // The offset value of V dynamic range.
    typedef UReg<0x01, 0x37, 0, 11> HD_HSYNC_RST;                       // Horizontal counter reset value [10:0].
    typedef UReg<0x01, 0x39, 0, 11> HD_INI_ST;                          // Vertical counter write enable, adjust the distance between hblank and vblank [10:0].
    typedef UReg<0x01, 0x3B, 0, 12> HD_HB_ST;                           // Generate horizontal blank to select programmed data [11:0]
    typedef UReg<0x01, 0x3D, 0, 12> HD_HB_SP;                           // Generate horizontal blank to select programmed data [11:0].
    typedef UReg<0x01, 0x3F, 0, 12> HD_HS_ST;                           // Output sync to DAC start position [11:0]
    typedef UReg<0x01, 0x41, 0, 12> HD_HS_SP;                           // Output sync to DAC stop position [11:0]
    typedef UReg<0x01, 0x43, 0, 12> HD_VB_ST;                           // Generate blank to select program data in blank [11:0]
    typedef UReg<0x01, 0x45, 0, 12> HD_VB_SP;                           // Generate blank to select program data in blank [11:0]
    typedef UReg<0x01, 0x47, 0, 12> HD_VS_ST;                           // Output vertical sync to DAC start position [11:0]
    typedef UReg<0x01, 0x49, 0, 12> HD_VS_SP;                           // Output vertical sync to DAC stop position [11:0]
    typedef UReg<0x01, 0x4B, 0, 12> HD_EXT_VB_ST;                       // Output vertical blank to DAC for DIV mode start position [11:0]
    typedef UReg<0x01, 0x4D, 0, 12> HD_EXT_VB_SP;                       // Output vertical blank to DAC for DIV mode stop position [11:0]
    typedef UReg<0x01, 0x4F, 0, 12> HD_EXT_HB_ST;                       // Output horizontal blank to DAC for DIV mode start position [11:0]
    typedef UReg<0x01, 0x51, 0, 12> HD_EXT_HB_SP;                       // Output horizontal blank to DAC for DIV mode stop position [11:0]
    typedef UReg<0x01, 0x53, 0, 8> HD_BLK_GY_DATA;                      // Force the blank of GY data to the defined programmed data
    typedef UReg<0x01, 0x54, 0, 8> HD_BLK_BU_DATA;                      // Force the blank of BU data to the defined programmed data
    typedef UReg<0x01, 0x55, 0, 8> HD_BLK_RV_DATA;                      // Force the blank of BU data to the defined programmed data

    //
    // Mode Detect (all registers R/W)
    //
    typedef UReg<0x01, 0x60, 0, 5> MD_HPERIOD_LOCK_VALUE;               // Mode Detect Horizontal Period Lock Value (If the continuous stabled line number is equal to the defined value, the horizontal stable indicator will be high)
    typedef UReg<0x01, 0x60, 5, 3> MD_HPERIOD_UNLOCK_VALUE;             // Mode Detect Horizontal Period Unlock Value (If the continuous unstable line number is equal to the defined value, the horizontal stable indicator will be low)
    typedef UReg<0x01, 0x61, 0, 5> MD_VPERIOD_LOCK_VALUE;               // Mode Detect Vertical Period Lock Value (If the continuous stabled frame number is equal to the defined value, the vertical stable indicator will be high)
    typedef UReg<0x01, 0x61, 5, 3> MD_VPERIOD_UNLOCK_VALUE;             // Mode Detect Vertical Period Unlock Value (If the continuous unstable frame number is equal to the defined value, the vertical stable indicator will be low)
    typedef UReg<0x01, 0x62, 0, 6> MD_NTSC_INT_CNTRL;                   // NTSC Interlace Mode Detect Value (If the vertical period number is equal to the defined value, This mode is NTSC Interlace mode)
    typedef UReg<0x01, 0x62, 6, 2> MD_WEN_CNTRL;                        // Horizontal Stable Estimation Error Range Control (The continuous line is stable in the defined error range. see: DEVELOPER_NOTES.md -> S1_62)
    typedef UReg<0x01, 0x63, 0, 5> MD_PAL_INT_CNTRL;                    // PAL Interlace Mode Detect Value (If the vertical period number is equal to the defined value, This mode is PAL interlace mode)
    typedef UReg<0x01, 0x63, 6, 1> MD_HS_FLIP;                          // Input Horizontal sync polarity Control (When set it to 1, the input horizontal sync will be inverted.)
    typedef UReg<0x01, 0x63, 7, 1> MD_VS_FLIP;                          // Input Vertical sync polarity Control (When set it to 1, the input vertical sync will be inverted)
    typedef UReg<0x01, 0x64, 0, 7> MD_NTSC_PRG_CNTRL;                   // NTSC Progressive Mode Detect Value (If the vertical period number is equal to the defined value, This mode is NTSC progressive mode or VGA 60HZ mode)
    // 64_7:1   -   reserved
    typedef UReg<0x01, 0x65, 0, 7> MD_VGA_CNTRL;                        // VGA Mode Vertical Detect Value (If the vertical period number is equal to the defined value, this mode is VGA mode, except VGA 60HZ mode)
    typedef UReg<0x01, 0x65, 7, 1> MD_SEL_VGA60;                        // Select VGA 60HZ mode: Program this bit to distinguish between VGA 60Hz mode and NTSC progressive mode;
                                                                        //      When set to 1, select VGA 60Hz mode
                                                                        //      When set to 0, select NTSC progressive mode
    typedef UReg<0x01, 0x66, 0, 8> MD_VGA_75HZ_CNTRL;                   // VGA 75Hz Horizontal Detect Value (If the horizontal period number is equal to the defined value, in VGA mode, this mode Is VGA 75Hz mode)
    typedef UReg<0x01, 0x67, 0, 8> MD_VGA_85HZ_CNTRL;                   // VGA 85Hz Horizontal Detect Value (If the horizontal period number is equal to the defined value, in VGA mode, this mode Is VGA 85Hz mode)
    typedef UReg<0x01, 0x68, 0, 7> MD_V1250_VCNTRL;                     // Vertical 1250 Line Mode Vertical Detect Value
    // 68_7:1   -   reserved
    typedef UReg<0x01, 0x69, 0, 8> MD_V1250_HCNTRL;                     // Vertical 1250 Line Mode Horizontal Detect Value, horizontal 866 pixels mode detect value
    typedef UReg<0x01, 0x6A, 0, 8> MD_SVGA_60HZ_CNTRL;                  // SVGA 60HZ Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, in SVGA mode, it’s SVGA 60Hz mode.)
    typedef UReg<0x01, 0x6B, 0, 8> MD_SVGA_75HZ_CNTRL;                  // SVGA 75HZ Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, in SVGA mode, it’s SVGA 75Hz mode)
    typedef UReg<0x01, 0x6C, 0, 8> MD_SVGA_85HZ_CNTRL;                  // SVGA 85HZ Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, in SVGA mode, it’s SVGA 85Hz mode)
    typedef UReg<0x01, 0x6D, 0, 7> MD_XGA_CNTRL;                        // XGA Mode Vertical Detect Value (If the vertical period number is equal to the defined value, it’s XGA mode.)
    // 6D_7:1   -   reserved
    typedef UReg<0x01, 0x6E, 0, 8> MD_XGA_60HZ_CNTRL;                   // XGA 60Hz Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, in XGA modes, It’s XGA 60Hz mode)
    typedef UReg<0x01, 0x6F, 0, 7> MD_XGA_70HZ_CNTRL;                   // XGA 70Hz Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, in XGA modes, It’s XGA 70Hz mode)
    // 6F_7:1   -   reserved
    typedef UReg<0x01, 0x70, 0, 7> MD_XGA_75HZ_CNTRL;                   // XGA 75Hz Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, in XGA modes, It’s XGA 75Hz mode)
    // 70_7:1   -   reserved
    typedef UReg<0x01, 0x71, 0, 7> MD_XGA_85HZ_CNTRL;                   // XGA 85Hz Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, in XGA modes, It’s XGA 85Hz mode)
    // 71_7:1   -   reserved
    typedef UReg<0x01, 0x72, 0, 8> MD_SXGA_CNTRL;                       // SXGA Mode Vertical Detect Value (If the vertical period number is equal to the defined value, It’s SXGA mode)
    typedef UReg<0x01, 0x73, 0, 7> MD_SXGA_60HZ_CNTRL;                  // SXGA 60Hz Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, in SXGA modes, It’s SXGA 60Hz mode)
    // 73_7:1   -   reserved
    typedef UReg<0x01, 0x74, 0, 7> MD_SXGA_75HZ_CNTRL;                  // SXGA 75Hz Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, in SXGA modes, It’s SXGA 75Hz mode)
    // 74_7:1   -   reserved
    typedef UReg<0x01, 0x75, 0, 7> MD_SXGA_85HZ_CNTRL;                  // SXGA 85Hz Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, in SXGA modes, It’s SXGA 85Hz mode)
    // 75_7:1   -   reserved
    typedef UReg<0x01, 0x76, 0, 7> MD_HD720P_CNTRL;                     // HD720P Vertical Detect Value (If the vertical period number is equal to the defined value, It’s HD720P mode)
    // 76_7:1   -   reserved
    typedef UReg<0x01, 0x77, 0, 8> MD_HD720P_60HZ_CNTRL;                // HD720P 60Hz Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, in HD720P mode. It is HD720P 60Hz mode)
    typedef UReg<0x01, 0x78, 0, 8> MD_HD720P_50HZ_CNTRL;                // HD720P 50Hz Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, in HD720P mode. It is HD720P 50Hz mode)
    typedef UReg<0x01, 0x79, 0, 7> MD_HD1125I_CNTRL;                    // 1080I Mode 1125 Line Vertical Detect Value (If the vertical period number is equal to the defined value, It’s 1125I mode)
    // 79_7:1   -   reserved
    typedef UReg<0x01, 0x7A, 0, 8> MD_HD2200_1125I_CNTRL;               // 1080I Mode 2200x1125I Horizontal Detect Value (If the horizontal period number is equal to the defined value, in 1080I mode. It is HD2200x1125I mode)
    typedef UReg<0x01, 0x7B, 0, 8> MD_HD2640_1125I_CNTRL;               // 1080I Mode 2640x1125I Horizontal Detect Value (If the horizontal period number is equal to the defined value, in 1080I mode. It is HD2640x1125I mode)
    typedef UReg<0x01, 0x7C, 0, 8> MD_HD1125P_CNTRL;                    // 1080P Mode 1125 Line Vertical Detect Value (If the vertical period number is equal to the defined value, It is HD1125P mode)
    typedef UReg<0x01, 0x7D, 0, 7> MD_HD2200_1125P_CNTRL;               // 1080P Mode 2200x1125P Horizontal Detect Value (If the horizontal period number is equal to the defined value, in 1080P mode, It is HD2200x1125P mode)
    // 7D_7:1   -   reserved
    typedef UReg<0x01, 0x7E, 0, 7> MD_HD2640_1125P_CNTRL;               // 1080P Mode 2640x1125P Horizontal Detect Value (If the horizontal period number is equal to the defined value, in 1080P mode, It is HD2640x1125P mode)
    // 7E_7:1   -   reserved
    typedef UReg<0x01, 0x7F, 0, 8> MD_HD1250P_CNTRL;                    // [changed to meaning 24khz] 1080P Mode 2640x1125P Horizontal Detect Value (If the horizontal period number is equal to the defined value, in 1080P mode, It is HD2640x1125P mode)
    typedef UReg<0x01, 0x80, 0, 8> MD_USER_DEF_VCNTRL;                  // User Defined Mode Vertical Detect Value (If the vertical period number is equal to the defined value, It is user-defined mode)
    typedef UReg<0x01, 0x81, 0, 8> MD_USER_DEF_HCNTRL;                  // User Defined Mode Horizontal Detect Value (If the horizontal period number is equal to the defined value, It is user-defined mode)
    typedef UReg<0x01, 0x82, 0, 1> MD_NOSYNC_DET_EN;                    // Sync Connection Detect Enable (Detect the horizontal sync signal if connect or not. 0: user mode; 1: auto detect)
    typedef UReg<0x01, 0x82, 1, 1> MD_NOSYNC_USER_ID;                   // Sync Connection Detect User Defined ID (User defined indicator in user mode. 0: sync connected; 1: no sync connected)
    typedef UReg<0x01, 0x82, 2, 1> MD_SW_DET_EN;                        // Mode Switch Detect Enable (Enable bit of auto detect if the mode changed or not. 0: user mode; 1: auto detect)
    typedef UReg<0x01, 0x82, 3, 1> MD_SW_USER_ID;                       // Mode Switch Detect User Defined ID (User defined indicator in user mode. 0->1: mode changed; 1->0: mode changed)
    typedef UReg<0x01, 0x82, 4, 1> MD_TIMER_DET_EN_H;                   // Horizontal Unstable Estimation Timer Detect Enable (Enable the timer detect result in horizontal unstable estimation. 0: use the hstable indicator in hperiod detect; 1: use the timer detected unstable indicator)
    typedef UReg<0x01, 0x82, 5, 1> MD_TIMER_DET_EN_V;                   // Vertical Unstable Estimation Timer Detect Enable (Enable the timer detect result in vertical unstable estimation. 0: use the vstable indicator in vperiod detect.; 1: use the timer detected unstable indicator)
    typedef UReg<0x01, 0x82, 6, 1> MD_DET_BYPS_H;                       // Horizontal Unstable Estimation Bypass Control (Bypass the horizontal unstable estimation 0: auto mode; 1: user mode)
    typedef UReg<0x01, 0x82, 7, 1> MD_H_USER_ID;                        // Horizontal Unstable Estimation User Defined ID (User defined indicator in user mode. 0: stable; 1: unstable)
    typedef UReg<0x01, 0x83, 0, 1> MD_DET_BYPS_V;                       // Vertical Unstable Estimation Bypass Control (Bypass the vertical unstable estimation auto detect 0: auto mode; 1: user mode)
    typedef UReg<0x01, 0x83, 1, 1> MD_V_USER_ID;                        // Vertical Unstable Estimation User Defined ID (User defined indicator in user mode. 0: stable; 1: unstable)
    typedef UReg<0x01, 0x83, 2, 4> MD_UNSTABLE_LOCK_VALUE;              // Unstable Estimation Lock Value (If the internal counter equals the defined value, the unstable indicator will be high. Horizontal and vertical estimation shared this value)
    // 83_6:2   -   reserved

    //
    // Deinterlacer / Scaledown registers
    //
    typedef UReg<0x02, 0x00, 0, 8> DEINT_00;                            // DEINTERLACER 00
        typedef UReg<0x02, 0x00, 0, 1> DIAG_BOB_MIN_BYPS;                  // Diagonal Function Bypass Control (When set to 1, bypass diagonal min selection for Y. No diagonal detection, just vertically two pixels average.)
        typedef UReg<0x02, 0x00, 1, 1> DIAG_BOB_COEF_SEL;                  // Diagonal Bob Low pass Filter Coefficient Selection (see DEVELOPER_NOTES.md -> Diagonal Bob Low pass Filter Coefficient Selection)
        typedef UReg<0x02, 0x00, 2, 1> DIAG_BOB_WEAVE_BYPS;                // Weave Function Bypass Control (When set to 1, weave function will bypass. Just repeat original data.)
        typedef UReg<0x02, 0x00, 3, 2> DIAG_BOB_DET_BYPS;                  // Diagonal Bob Deinterlacer Angle Detect Bypass Control (When set to 1, bypass the detection of angle arctan (1/4):(1/6).)
        typedef UReg<0x02, 0x00, 5, 1> DIAG_BOB_YTAP3_BYPS;                // Diagonal Bob Deinterlacer Y Tap3 Filter Bypass control (When set to 1, bypass the tap3 filter for Y.)
        typedef UReg<0x02, 0x00, 6, 1> DIAG_BOB_MIN_CBYPS;                 // Diagonal Bob Min Control For UV (When set to 1, bypass diagonal min select for UV. No diagonal detection, just vertically two pixels average.)
        typedef UReg<0x02, 0x00, 7, 1> DIAG_BOB_PLDY_RAM_BYPS;             // Bypass Control For Pdelayer FIFO (When set to 1, bypass FIFO for pdelayer.)
    typedef UReg<0x02, 0x01, 0, 9> DIAG_BOB_PLDY_SP;                    // The Distance Control of Pdelayer Reset [7:0] (In pdelayer, adjust the delay between read reset and write reset.)
    // 02_1:4    -  reserved
    typedef UReg<0x02, 0x02, 5, 1> MADPT_SEL_22;                        // 2:2 pull-down selection (When set to 1, enable 2:2 pull-down detection; When set to 0, enable 3:2 pull-down detection.)
    typedef UReg<0x02, 0x02, 6, 1> MADPT_Y_VSCALE_BYPS;                 // Bypass Y phase adjustment in vertical scaling down (When set to 1, Y phase adjustment in vertical scaling down will be bypass)
    typedef UReg<0x02, 0x02, 7, 1> MADPT_UV_VSCALE_BYPS;                // Bypass UV phase adjustment in vertical scaling down (When set to 1, UV phase adjustment in vertical scaling down will be bypass)
    typedef UReg<0x02, 0x03, 0, 1> MADPT_NOISE_DET_SEL;                 // Noise detection selection (When set to 1, noise detection is in video active period. When set to 0, noise detection is in video blanking period.)
    typedef UReg<0x02, 0x03, 1, 2> MADPT_NOISE_DET_SHIFT;               // Noise detection shift
                                                                        //      When set to 3, noise detection drop 15bits
                                                                        //      When set to 2, noise detection drop 16bits
                                                                        //      When set to 1, noise detection drop 17bits
                                                                        //      When set to 0, noise detection drop 18bits
    typedef UReg<0x02, 0x03, 3, 2> MADPT_NOISE_DET_RST;                 // Noise detection time reset value
                                                                        //      When set to 3, time counter reset at 1023.
                                                                        //      When set to 2, time counter reset at 511.
                                                                        //      When set to 1, time counter reset at 255.
                                                                        //      When set to 0, time counter reset at 127.
    // 03_5:3   -   reserved
    typedef UReg<0x02, 0x04, 0, 6> MADPT_NOISE_THRESHOLD_NOUT;          // Auto noise detect threshold for NOUT - Threshold for NOUT signal.
    typedef UReg<0x02, 0x05, 0, 6> MADPT_NOISE_THRESHOLD_VDS;           // Auto noise detect threshold for nout_vds_proc - Threshold for nout_vds_proc signal.
    typedef UReg<0x02, 0x06, 0, 8> MADPT_GM_NOISE_VALUE;                // Global noise low/global noise auto detect offset low
                                                                        //      In global motion noise manual mode, global motion detection noise bit [3:0]
                                                                        //      In global motion noise auto-detect mode, global motion noise’s offset bit [3:0]
                                                                        //      In global motion noise manual mode, global motion detection noise bit [7:4]
                                                                        //      In global motion noise auto-detect mode, global motion noise’s offset bit [7:4]
    typedef UReg<0x02, 0x07, 0, 8> MADPT_STILL_NOISE_VALUE;             // Global still control value (In manual mode, still-noise value bit; In auto-detect mode, still-noise’s offset bit.)
    typedef UReg<0x02, 0x08, 0, 8> MADPT_LESS_NOISE_VALUE;              // Less-still noise value (User defined less still noise value.)
    typedef UReg<0x02, 0x09, 0, 4> MADPT_NOISE_EST_GAIN;                // Global motion noise gain (in auto-detect mode)
    typedef UReg<0x02, 0x09, 4, 8> MADPT_STILL_NOISE_EST_GAIN;          // Still-noise gain (in auto-detect mode)
    // 0A_0:4   -   reserved
    typedef UReg<0x02, 0x0A, 4, 1> MADPT_NOISE_EST_EN;                  // Global noise auto detection enable (When set to 1,global noise detection is in auto mode.; When set to 0,global noise detection is in manual mode.)
    typedef UReg<0x02, 0x0A, 5, 1> MADPT_STILL_NOISE_EST_EN;            // Still-noise auto detection enable (When set to 1, still-noise is in auto detection; When set to 0, still-noise is in manual mode.)
    // 0A_6:1   -    reserved
    typedef UReg<0x02, 0x0A, 7, 1> MADPT_Y_MI_DET_BYPS;                 // MADPT_Y_MI_DET_BYPS (When set to 1, Y motion index generation is in manual mode)
    typedef UReg<0x02, 0x0B, 0, 7> MADPT_Y_MI_OFFSET;                   // Y motion index offset (In auto mode, Y motion index’s offset.; In manual mode, Y motion index’s user value.)
    // 0B_7:1   -   reserved
    typedef UReg<0x02, 0x0C, 0, 4> MADPT_Y_MI_GAIN;                     // Y motion index gain
    typedef UReg<0x02, 0x0C, 4, 1> MADPT_MI_1BIT_BYPS;                  // Motion index feedback-bit bypass (When set to 1, motion index feedback-bit function will be bypass)
    typedef UReg<0x02, 0x0C, 5, 1> MADPT_MI_1BIT_FRAME2_EN;             // Enable Frame-two feedback-bit (When set to 1, enable frame-two feedback-bit.)
    // 0C_7:1   -   reserved
    typedef UReg<0x02, 0x0D, 0, 7> MADPT_MI_THRESHOLD_D;                // Motion index feedback-bit generation’s threshold bit
    typedef UReg<0x02, 0x0E, 0, 7> MADPT_MI_THRESHOLD_E;                // Motion index fixed value
    typedef UReg<0x02, 0x0F, 0, 1> MADPT_STILL_DET_EN;                  // MStill detection enable (When set to 1, still detection is in auto mode. When set to 0, still detection is in manual mode.)
    typedef UReg<0x02, 0x0F, 1, 1> MADPT_STILL_ID;                      // Still indicator defined by user (in manual mode only) (Still indicator defined by user, only useful in STILL_DET_EN =0.)
    typedef UReg<0x02, 0x0F, 2, 2> MADPT_STILL_UNLOCK;                  // Still detection’s auto unlock value (When unlock counter equals unlock value, “still” will go inactive.)
    typedef UReg<0x02, 0x0F, 4, 4> MADPT_STILL_LOCK;                    // Still detection’s auto lock value (When lock counter equals lock value, “still” will go active.)
    typedef UReg<0x02, 0x10, 0, 1> MADPT_LESS_STILL_DET_EN;             // Less still detection enable (When set to 1, less-still detection is in auto mode. When set to 0, less-still detection is in manual mode.)
    typedef UReg<0x02, 0x10, 1, 1> MADPT_LESS_STILL_ID;                 // Less still indicator defined by user (in manual mode only) (Less-still indicator defined by user, only useful in LESS_STILL_DET_EN =0)
    typedef UReg<0x02, 0x10, 2, 2> MADPT_LESS_STILL_UNLOCK;             // Less still detection’s auto unlock value (When unlock counter equals unlock value, “less-still” will go inactive.)
    typedef UReg<0x02, 0x10, 4, 4> MADPT_LESS_STILL_LOCK;               // Less still detection’s auto lock value (When lock counter equals lock value, “less-still” will go active.)
    // 11_0:2   -   reserved
    typedef UReg<0x02, 0x11, 3, 1> MADPT_EN_PULLDOWN32;                 // 3:2 pull-down detection enable (When set to 1, 3:2 pull-down detection is in auto mode. When set to 0, 3:2 pull-down detection is in manual mode.)
    typedef UReg<0x02, 0x11, 4, 1> MADPT_PULLDOWN32_ID;                 // 3:2 pull-down indicator defined by user (in manual mode) (3:2 pull-down indicator by user, only useful in 32PULLDOWN_EN =0)
    typedef UReg<0x02, 0x11, 5, 3> MADPT_PULLDOWN32_OFFSET;             // 3:2 pull-down sequence offset
    typedef UReg<0x02, 0x12, 0, 7> MADPT_PULLDOWN32_LOCK_RST;           // 3:2 pull-down auto lock value bit (When lock counter equals lock value, 3:2 pull-down is in active.)
    // 12_7:1   -   reserved
    typedef UReg<0x02, 0x13, 0, 1> EN_22PULLDOWN;                       // 2:2 pull-down detection enable (When set to 1, 2:2 pull-down detection is in auto mode. When set to 0, 2:2 pull-down detection is in manual mode.)
    typedef UReg<0x02, 0x13, 1, 1> ID_22PULLDOWN;                       // 2:2 pull-down indicator defined by user (in manual mode) (2:2 pull-down indicator by user, only useful in 22PULLDOWN_EN =0)
    typedef UReg<0x02, 0x13, 2, 1> MADPT_PULLDOWN22_OFFSET;             // 2:2 pull-down sequence offset
    // 13_3:1   -   reserved
    typedef UReg<0x02, 0x13, 4, 3> MADPT_PULLDOWN22_DET_CNTRL;          // 2:2 pull-down detection control bit
    // 13_7:1   -   reserved
    typedef UReg<0x02, 0x14, 0, 18> MADPT_PULLDOWN22_THRESHOLD;          // 2:2 pull-down detection threshold bit [7:0]
    // 16_2:2   -   reserved
    typedef UReg<0x02, 0x16, 4, 1> MADPT_MO_ADP_Y_EN;                   // Enable pull-down in Y motion adaptive (When set to 1, enable pull-down for Y data motion adaptive.)
    typedef UReg<0x02, 0x16, 5, 1> MADPT_MO_ADP_UV_EN;                  // Enable pull-down in UV motion adaptive (When set to 1, enable pull-down for UV data motion adaptive.)
    typedef UReg<0x02, 0x16, 6, 1> MADPT_VT_FILTER_CNTRL;               // Vertical Temporal Filter Control (When set to 1, do motion adaptive in interpolated line only; When set to 0, do motion adaptive in every line.)
    typedef UReg<0x02, 0x16, 7, 1> MAPDT_VT_SEL_PRGV;                   // Select original data in progressive mode in VT filter (If the input is progressive mode or graphic mode, this bit must be set to 1)
    typedef UReg<0x02, 0x17, 0, 8> MADPT_Y_DELAY_UV_DELAY;              //
        typedef UReg<0x02, 0x17, 0, 4> MADPT_Y_DELAY;                       // Y delay pipe control (see: DEVELOPER_NOTES.md -> Y delay pipe control)
        typedef UReg<0x02, 0x17, 4, 4> MADPT_UV_DELAY;                      // UV delay pipe control (see: DEVELOPER_NOTES.md -> UV delay pipe control)
    typedef UReg<0x02, 0x18, 0, 1> MADPT_DIVID_BYPS;                    // Motion index divide bypass (When = 1, motion index no divide.; When = 0, motion index will by divided by 2 or 4)
    typedef UReg<0x02, 0x18, 1, 1> MADPT_DIVID_SEL;                     // Motion index divide selection (When = 1, motion index will be divided by 2 in still. When = 0, motion index will be divided by 4 in still.)
    // 18_2:1   -   reserved
    typedef UReg<0x02, 0x18, 3, 1> MADPT_HTAP_BYPS;                     // Motion index horizontal filter bypass (When =1, motion index horizontal filter will be bypass)
    typedef UReg<0x02, 0x18, 4, 4> MADPT_HTAP_COEFF;                    // Motion index horizontal filter coefficient
    typedef UReg<0x02, 0x19, 0, 1> MADPT_BIT_STILL_EN;                  // Enable pixel base still (When set to 1, pixel base still function will enable.)
    // 19_1:1   -   reserved
    typedef UReg<0x02, 0x19, 2, 1> MADPT_VTAP2_BYPS;                    // Motion index vertical filter bypass (When = 1, motion index’s vertical filter will be bypass.)
    typedef UReg<0x02, 0x19, 3, 1> MADPT_VTAP2_ROUND_SEL;               // Motion index vertical filter round selection (When set to 1, the input data will be divided by 2.)
    typedef UReg<0x02, 0x19, 4, 4> MADPT_VTAP2_COEFF;                   // Motion index vertical filter coefficient
    typedef UReg<0x02, 0x1A, 0, 8> MADPT_PIXEL_STILL_THRESHOLD_1;       // Pixel base still threshold level 1
    typedef UReg<0x02, 0x1B, 0, 8> MADPT_PIXEL_STILL_THRESHOLD_2;       // Pixel base still threshold level 2
    // 1C_0:8   -   reserved
    // 1D_0:8   -   reserved
    // 1E_0:8   -   reserved
    typedef UReg<0x02, 0x1F, 0, 8> MADPT_HFREQ_NOISE;                   // High-frequency detection noise value - The noise value for high-frequency detection.
    typedef UReg<0x02, 0x20, 0, 1> MADPT_HFREQ_DET_EN;                  // High-frequency detection enable (When set to 1, high-frequency detection is in auto mode. When set to 0, high-frequency detection is in manual mode.)
    typedef UReg<0x02, 0x20, 1, 1> MADPT_HFREQ_ID;                      // High-frequency indicator by user (in manual mode) (High-frequency indicator by user, only useful in HFREQ_DET_EN =0)
    // 20_2:2   -   reserved
    typedef UReg<0x02, 0x20, 4, 4> MADPT_HFREQ_LOCK;                    // High-frequency auto lock value (When high-frequency lock counter equals lock value, high-frequency will be active)
    typedef UReg<0x02, 0x21, 0, 3> MADPT_HFREQ_UNLOCK;                  // High-frequency auto unlock value (When high-frequency unlock counter equals unlock value, high-frequency will be inactive)
    // 21_3:1  -    reserved
    typedef UReg<0x02, 0x21, 4, 1> MADPT_EN_NOUT_FOR_STILL;             // Enable NOUT for still detection
    typedef UReg<0x02, 0x21, 5, 1> MADPT_EN_NOUT_FOR_LESS_STILL;        // Enable NOUT for less-still detection
    // 21_6:2   -   reserved
    typedef UReg<0x02, 0x22, 0, 5> MADPT_PD_SP;                         // Scaling down line buffer WRSTZ position adjustment bits (Adjust the position of write reset in vertical IIR filter line buffer, and phase adjustment line buffer.)
    // 22_5:3   -   reserved
    typedef UReg<0x02, 0x23, 0, 5> MADPT_PD_ST;                         // Scaling down line buffer WRSTZ position adjustment bits (Adjust the position of write reset in vertical IIR filter line buffer, and phase adjustment line buffer.)
    // 23_5:3   -   reserved
    typedef UReg<0x02, 0x24, 2, 1> MADPT_PD_RAM_BYPS;                   // Bypass scaling down’s line buffer (When set to 1, scaling down’s line buffer will be bypass.)
    // 25_0:8   -   reserved
    typedef UReg<0x02, 0x26, 6, 1> MADPT_VIIR_BYPS;                     // Bypass V-IIR filter in vertical scaling down (When set to 1, V-IIR filter in vertical scaling down will be bypass.)
    typedef UReg<0x02, 0x26, 7, 1> MADPT_VIIR_ROUND_SEL;                // V-IIR filter in vertical scaling down round selection (When set to 1, the input data will be divided by 2)
    typedef UReg<0x02, 0x27, 0, 7> MADPT_VIIR_COEF;                     // V-IIR filter coefficient
    // 27_7:1   -   reserved
    // 28_0:4   -   reserved
    typedef UReg<0x02, 0x28, 4, 4> MADPT_VSCALE_RATE_LOW;               // Vertical non-linear scale down DDA increment shared low 4-bit - All the segment DDA increment share low 4bit
    typedef UReg<0x02, 0x29, 0, 8> MADPT_VSCALE_RATE_SEG0;              // Vertical non-linear scale down 1st segment DDA increment value (The actual DDA increment is vscale={vscale0, vscale_low}. Assume the scale factor is n/m, then vscale= 4095x(m-n)/n)
    typedef UReg<0x02, 0x2A, 0, 8> MADPT_VSCALE_RATE_SEG1;              // Vertical non-linear scale down 2nd segment DDA increment value (The actual DDA increment is vscale={vscale1, vscale_low})
    typedef UReg<0x02, 0x2B, 0, 8> MADPT_VSCALE_RATE_SEG2;              // Vertical non-linear scale down 3rd segment DDA increment value (The actual DDA increment is vscale={vscale2, vscale_low}.)
    typedef UReg<0x02, 0x2C, 0, 8> MADPT_VSCALE_RATE_SEG3;              // Vertical non-linear scale down 4th segment DDA increment value (The actual DDA increment is vscale={vscale3, vscale_low}.)
    typedef UReg<0x02, 0x2D, 0, 8> MADPT_VSCALE_RATE_SEG4;              // Vertical non-linear scale down 5th segment DDA increment value (The actual DDA increment is vscale={vscale4, vscale_low}.)
    typedef UReg<0x02, 0x2E, 0, 8> MADPT_VSCALE_RATE_SEG5;              // Vertical non-linear scale down 6th segment DDA increment value (The actual DDA increment is vscale={vscale5, vscale_low}.)
    typedef UReg<0x02, 0x2F, 0, 8> MADPT_VSCALE_RATE_SEG6;              // Vertical non-linear scale down 7th segment DDA increment value (The actual DDA increment is vscale={vscale6, vscale_low}.)
    typedef UReg<0x02, 0x30, 0, 8> MADPT_VSCALE_RATE_SEG7;              // Vertical non-linear scale down 8th segment DDA increment value (The actual DDA increment is vscale={vscale7, vscale_l
    typedef UReg<0x02, 0x31, 0, 2> MADPT_VSCALE_DEC_FACTOR;             // Vertical non-linear scaling-down factor select
                                                                        //      If the scaling ratio is less than 1⁄2, use it and DDA to generate the we and phase 00: scaling-ratio is more than 1⁄2.
                                                                        //      01: scaling-ratio is less than 1⁄2.
                                                                        //      10: scaling-ratio is less than 1⁄4.
    typedef UReg<0x02, 0x31, 2, 1> MADPT_SEL_PHASE_INI;                 // Vertical scaling down initial phase selection
    typedef UReg<0x02, 0x31, 3, 1> MADPT_TET_EN;                        // Test bus output enable (Internal hardware debugging use only.)
    typedef UReg<0x02, 0x31, 4, 4> MADPT_TEST_SEL;                      // Test bus select (Internal hardware debugging use only.)
    typedef UReg<0x02, 0x32, 0, 4> MADPT_Y_HTAP_CNTRL;                  // Y horizontal filter control for background reduction (Y_HTAP_CNTRL[3:0] could bypass four tap3 FIR filter.)
    typedef UReg<0x02, 0x32, 4, 3> MADPT_Y_VTAP_CNTRL;                  // Y vertical filter control for background reduction
                                                                        //      Y_VTAP_CNTRL[0]: when set to1, bypass vertical filter
                                                                        //      Y_VTAP_CNTRL[1]: when set to 1, enable FIR filter
                                                                        //      Y_VTAP_CNTRL[2]: when set to 1, bypass IIR filter
    typedef UReg<0x02, 0x32, 7, 1> MADPT_NRD_SEL;                       // Background reduction selection control (Only set it to 1 in huge noise condition)
    typedef UReg<0x02, 0x33, 0, 4> MADPT_M_HTAP_CNTRL;                  // Background noise reduction H filter control in huge noise condition (M_HTAP_CNTRL[3:0] could bypass four tap3 FIR filter.)
    typedef UReg<0x02, 0x33, 4, 3> MADPT_M_VTAP_CNTRL;                  // Background noise reduction V filter control in huge noise condition
                                                                        //      M_VTAP_CNTRL[0]: when set to1, bypass vertical filter
                                                                        //      M_VTAP_CNTRL[1]: when set to 1, enable FIR filter
                                                                        //      M_VTAP_CNTRL[2]: when set to 1, bypass IIR filter
    // 33_7:1   -   reserved
    typedef UReg<0x02, 0x34, 0, 1> MADPT_Y_WOUT_BYPS;                   // Bypass Y WOUT
    typedef UReg<0x02, 0x34, 1, 3> MADPT_Y_WOUT;                        // Coefficient for Y noise reduction
    typedef UReg<0x02, 0x34, 4, 1> MADPT_UV_WOUT_BYPS;                  // Bypass UV WOUT
    typedef UReg<0x02, 0x34, 5, 3> MADPT_UV_WOUT;                       // Coefficient for UV noise reduction
    typedef UReg<0x02, 0x35, 0, 1> MADPT_Y_NRD_ENABLE;                  // Enable background noise reduction in Y domain (When set to 1, enable background noise reduction in Y domain.)
    typedef UReg<0x02, 0x35, 1, 1> MADPT_UV_NRD_ENABLE;                 // Enable background noise reduction in UV domain (When set to 1, enable background noise reduction in UV domain.)
    typedef UReg<0x02, 0x35, 2, 1> MADPT_NRD_OUT_SEL;                   // NRD output selection (Only set it to 1 in huge noise condition)
    typedef UReg<0x02, 0x35, 3, 1> MADPT_DD0_SEL;                       // DD0 select control (Set it to 1 when background noise reduction enable; Set it to 0 when background noise reduction disable)
    typedef UReg<0x02, 0x35, 4, 1> MADPT_NRD_VIIR_PD_BYPS;              // Bypass NRD VIIR line buffer
    typedef UReg<0x02, 0x35, 5, 1> MADPT_UVDLY_PD_BYPS;                 // Bypass UV delay line buffer
    typedef UReg<0x02, 0x35, 6, 1> MADPT_CMP_EN;                        // Motion compare enable (When set to 1, enable motion compare; When set to 0, motion compare is in manual mode)
    typedef UReg<0x02, 0x35, 7, 1> MADPT_CMP_USER_ID;                   // Motion compare result defined by user (in manual mode) (Motion compare result defined by user when CMP_EN = 0)
    typedef UReg<0x02, 0x36, 0, 8> MADPT_CMP_LOW_THRESHOLD;             // Motion compare low level threshold
    typedef UReg<0x02, 0x37, 0, 8> MADPT_CMP_HIGH_THRESHOLD;            // Motion compare high level threshold
    typedef UReg<0x02, 0x38, 0, 4> MADPT_NRD_VIIR_PD_SP;                // NRD line buffer WRSTZ position adjustment
    typedef UReg<0x02, 0x38, 4, 4> MADPT_NRD_VIIR_PD_ST;                // NRD line buffer RRSTZ position adjustment
    typedef UReg<0x02, 0x39, 0, 4> MADPT_UVDLY_PD_SP;                   // UV delay line buffer WRSTZ position adjustment
    typedef UReg<0x02, 0x39, 4, 4> MADPT_UVDLY_PD_ST;                   // UV delay line buffer RRSTZ position adjustment
    typedef UReg<0x02, 0x3a, 0, 1> MADPT_EN_UV_DEINT;                   // Enable UV deinterlacer (When set to 1, enable UV deinterlacer)
    typedef UReg<0x02, 0x3a, 1, 1> MADPT_EN_PULLDWN_FOR_NRD;            // Enable pull-down to block STILL for NRD (Set it to 1, background noise reduction will in low noise level when in 32/22 pull- down source)
    typedef UReg<0x02, 0x3a, 2, 1> MADPT_EN_NOUT_FOR_NRD;               // Enable NOUT for background noise reduction
    typedef UReg<0x02, 0x3a, 3, 1> MADPT_EN_STILL_FOR_NRD;              // Enable still for background noise reduction
    typedef UReg<0x02, 0x3a, 4, 1> MADPT_EN_STILL_FOR_PULLDWN;          // Enable STILL to reset pull-down detection (When set to 1, still will be used to reset 3:2/2:2 pull-down detection)
    typedef UReg<0x02, 0x3a, 5, 2> MADPT_MI_1BIT_DLY;                   // Delay pipe control for motion index feedback-bit (see: DEVELOPER_NOTES.md -> Delay pipe control for motion index feedback-bit)
    typedef UReg<0x02, 0x3a, 7, 1> MADPT_UV_MI_DET_BYPS;                // UV motion index generation bypass (When set to 1, UV motion index generation is in manual mode.)
    typedef UReg<0x02, 0x3b, 0, 7> MADPT_UV_MI_OFFSET;                  // UV motion index offset (In auto mode, UV motion index offset; In manual mode, UV motion index user defined value)
    // 3B_7:1   -   reserved
    typedef UReg<0x02, 0x3C, 0, 4> MADPT_UV_MI_GAIN;                    // UV motion index gain (UV motion index gain.)
    typedef UReg<0x02, 0x3C, 4, 3> MADPT_MI_DELAY;                      // Motion index delay control (Control motion index (both Y and UV’s) delay pipes, so that the motion index can align with corresponding data.)
                                                                        // see: DEVELOPER_NOTES.md -> Motion index delay control
    // 3C_7:1   -   reserved

    //
    // Video Processor Registers (all registers R/W)
    //
    typedef UReg<0x03, 0x00, 0, 1> VDS_SYNC_EN;                         // External sync enable, active high - This bit enable sync lock mode (see: DEVELOPER_NOTES.md -> S3_00)
    typedef UReg<0x03, 0x00, 1, 1> VDS_FIELDAB_EN;                      // ABAB double field mode enable (In field double mode, when this bit is 1, VDS works in ABAB mode, otherwise it works in AABB mode.)
    typedef UReg<0x03, 0x00, 2, 1> VDS_DFIELD_EN;                       // Double field mode enable active high (This bit enable field double mode, ex, frame rate from 50Hz to 100Hz, or from 60Hz to 120Hz. When this bit is 1, the output timing is interlaced)
    typedef UReg<0x03, 0x00, 3, 1> VDS_FIELD_FLIP;                      // Flip field control (This bit is field flip control bit, it only used in interlace mode. When it is 1, it inverts the output field)
    typedef UReg<0x03, 0x00, 4, 1> VDS_HSCALE_BYPS;                     // Horizontal scale up bypass control, active high (When this bit is 1, data will bypass horizontal scale up process)
    typedef UReg<0x03, 0x00, 5, 1> VDS_VSCALE_BYPS;                     // Vertical scale up bypass control, active high (When this bit is 1, data will bypass vertical scale up process)
    typedef UReg<0x03, 0x00, 6, 1> VDS_HALF_EN;                         // Horizontal scale up bypass control, active high
    typedef UReg<0x03, 0x00, 7, 1> VDS_SRESET;                          // Horizontal scale up bypass control, active high (When this bit is 1, it reset the VDS_PROC internal module ds_video_enhance)
    typedef UReg<0x03, 0x01, 0, 12> VDS_HSYNC_RST;                      // Internal Horizontal period control bit[7:0], Half of total pixels in field double mode.
                                                                        // This field contains horizontal total value minus 1.
                                                                        // EX: Horizontal pixels is A, then HSYNC_RST[9:0] = A-1, in field double mode, HSYNC_RST[9:0] = (A/2 –1))
    typedef UReg<0x03, 0x02, 4, 11> VDS_VSYNC_RST;                      // Internal Vertical period control bit (This field contains vertical total value minus 1)
    // 02_7:1   -   reserved
    typedef UReg<0x03, 0x04, 0, 12> VDS_HB_ST;                          // Horizontal blanking start position control bit (This field is used to program horizontal blanking start position, this blanking is used to get data from memory)
    typedef UReg<0x03, 0x05, 4, 12> VDS_HB_SP;                          // Horizontal blanking stop position control bit (This field is used to program horizontal blanking stop position, this blanking is used to get data from memory)
    typedef UReg<0x03, 0x07, 0, 11> VDS_VB_ST;                          // Vertical blanking start position control bit (This field is used to program vertical blanking start position)
    // 08_3:1   -   reserved
    typedef UReg<0x03, 0x08, 4, 11> VDS_VB_SP;                          // Vertical blanking stop position control bit (This field is used to program vertical blanking stop position)
    // 09_7:1   -   reserved
    typedef UReg<0x03, 0x0A, 0, 12> VDS_HS_ST;                          // Horizontal sync start position control bit (This field is used to program horizontal sync start position)
    typedef UReg<0x03, 0x0B, 4, 12> VDS_HS_SP;                          // Horizontal sync stop position control bit (This field is used to program horizontal sync stop position)
    typedef UReg<0x03, 0x0D, 0, 11> VDS_VS_ST;                          // Vertical sync start position control bit (This field is used to program vertical sync start position)
    // 0E_3:1   -   reserved
    typedef UReg<0x03, 0x0E, 4, 11> VDS_VS_SP;                          // Vertical sync stop position control bit (This field is used to program vertical sync stop position)
    typedef UReg<0x03, 0x10, 0, 12> VDS_DIS_HB_ST;                      // Final display horizontal blanking start position control bit (This field contains final display horizontal blanking start position control, this blanking is used to clean the output data in blanking)
    typedef UReg<0x03, 0x11, 4, 12> VDS_DIS_HB_SP;                      // Final display horizontal blanking stop position control bit (This field contains final display horizontal blanking stop position control, this blanking is used to clean the output data in blanking)
    typedef UReg<0x03, 0x13, 0, 11> VDS_DIS_VB_ST;                      // Final display vertical blanking start position control bit (This field contains final display vertical blanking start position control, this blanking is used to clean the output data in blanking)
    // 14_3:1   -   reserved
    typedef UReg<0x03, 0x14, 4, 11> VDS_DIS_VB_SP;                      // Final display vertical blanking stop position control bit (This field contains final display vertical blanking stop position control, this blanking is used to clean the output data in blanking)
    typedef UReg<0x03, 0x16, 0, 10> VDS_HSCALE;                         // Horizontal scaling coefficient bit.
                                                                        // This field indicates the ratio of scaling up.
                                                                        // HSCALE = 1024 * (resolution of input) / (resolution of output)
                                                                        // EX: 720 * 480 -> 800 * 480, HSCALE = 1024 * 720 / 800
    // 17_2:2   -   reserved
    typedef UReg<0x03, 0x17, 4, 10> VDS_VSCALE;                         // Vertical scaling up coefficient bit.
                                                                        // This field indicates the ratio of vertical scaling up.
                                                                        // VSCALE = 1024 * (resolution of input / resolution of output)
                                                                        // EX: 720*480 -> 720*576, VSCALE = 1024 * 480 /576
    // 18_6:2   -   reserved
    typedef UReg<0x03, 0x19, 0, 10> VDS_FRAME_RST;                      // Frame reset period control bit
                                                                        // This field indicates how many frames VSD_PROC locked at each time, it based on the input vertical sync.
                                                                        // EX: FRAME_RST=4, this means VDS_PROC will lock every 5 frames,
                                                                        // (This frame number is counts at every input vertical sync, the frame number of VDS_PROC output maybe different)
    // 1A_2:2   -   reserved
    typedef UReg<0x03, 0x1A, 4, 1> VDS_FLOCK_EN;                        // Frame lock enable, active high
                                                                        // This bit enables the frame lock mode, when this bit is 1, VDS_PROC output
                                                                        // timing will lock with its input timing (from INPUT_FORMATTER) at every 2 or more frames.
    typedef UReg<0x03, 0x1A, 5, 1> VDS_FREERUN_FID;                     // Enable internal free run field index generation, active high
                                                                        // When this bit is 1, the output field index is internal free run field,
                                                                        // otherwise the output field index is based on input field index.
    typedef UReg<0x03, 0x1A, 6, 1> VDS_FID_AA_DLY;                      // Enable internal free run AABB field delay 1 frame, active high (When this bit is 1, the internal free run AABB field will delay 1 frame)
    typedef UReg<0x03, 0x1A, 7, 1> VDS_FID_RST;                         // Enable internal free run field index reset, active high (When this bit is 1, internal free run field index will reset at every frame number is 0)

    typedef UReg<0x03, 0x1B, 0, 32> VDS_FR_SELECT;                      // Frame size select control bit (FR_SELECT[2n+1:2n] is for frame n selection. 0 select VSYNC_RST; 1 select VSYNC_SIZE1; 2 select VSYNC_SIZE2)
    typedef UReg<0x03, 0x1F, 0, 4> VDS_FRAME_NO;                        // Programmable repeat frame number control bit (This field defines the repeated frame number, EX: if frame_no = 2, then the frame will repeat every 3 frame) see: DEVELOPER_NOTES.md -> S3_1F
    typedef UReg<0x03, 0x1F, 4, 1> VDS_DIF_FR_SEL_EN;                   // Enable the different frame size, active high (When this bit is 1, VDS_PROC can generate a sequence of different frame size)
    typedef UReg<0x03, 0x1F, 5, 1> VDS_EN_FR_NUM_RST;                   // Enable frame number reset, active high (When this bit is 1, frame number will be reset to 1 when frame lock is occur)
    // 1F_6:2   -   reserved
    typedef UReg<0x03, 0x20, 0, 11> VDS_VSYN_SIZE1;                     // Programmable vertical total size 1 control bit
                                                                        // This field contains the vertical total line number minus 1. It can be the same
                                                                        // as vsync_rst and vsync_size2, it also can different with them, and it
                                                                        // can be used to define different frame size.
    // 20_3:5   -   reserved
    typedef UReg<0x03, 0x22, 0, 11> VDS_VSYN_SIZE2;                     // Programmable vertical total size 2 control bit
                                                                        // This field contains the vertical total line number minus 1.
                                                                        // It can be the same as vsync_rst and vsync_size1, it also can different
                                                                        // with them, and it can be used to define different frame size.
    // 23_3:5   -   reserved
    typedef UReg<0x03, 0x24, 0, 8> VDS_3_24;                            // VDS_PROC 36
        typedef UReg<0x03, 0x24, 0, 1> VDS_UV_FLIP;                         // 422 to 444 conversion UV flip control (This bit is used to flip UV, when this bit is 1, UV position will be flipped)
        typedef UReg<0x03, 0x24, 1, 1> VDS_U_DELAY;                         // UV 422 to 444 conversion U delay (When this bit is 1, U will delay 1 clock, otherwise, no delay for internal pipe)
        typedef UReg<0x03, 0x24, 2, 1> VDS_V_DELAY;                         // UV 422 to 444 conversion V delay (When this bit is 1, V will delay 1 clock, otherwise, no delay for internal pipe)
        typedef UReg<0x03, 0x24, 3, 1> VDS_TAP6_BYPS;                       // Tap6 filter in 422 to 444 conversion bypass control, active high (This bit is the UV interpolation filter enable control; when this bit is 1, UV bypass the filter)
        typedef UReg<0x03, 0x24, 4, 2> VDS_Y_DELAY;                         // Y compensation delay control bit [1:0] in 422 to 444 conversion (To compensation the pipe of UV, program this field can delay Y from 1 to 4 clocks) (see: DEVELOPER_NOTES.md -> S3_24_4)
        typedef UReg<0x03, 0x24, 6, 2> VDS_WEN_DELAY;                       // Compensation delay control bit [1:0] for horizontal write enable (This two-bit register defines the compensation delay of horizontal scale up write enable and phase)  (see: DEVELOPER_NOTES.md -> S3_24_6)
    typedef UReg<0x03, 0x25, 0, 10> VDS_D_SP;                           // Line buffer write reset position control bit (This field contains the write reset position of the line buffer, this position is also the write start position of the buffer)
    // 26_2:4   -   reserved
    typedef UReg<0x03, 0x26, 6, 1> VDS_D_RAM_BYPS;                      // Line buffer one line delay data bypass, active high (When this bit is 1, data will bypass the line buffer)
    typedef UReg<0x03, 0x26, 7, 1> VDS_BLEV_AUTO_EN;                    // Y minimum and maximum level auto detection enable, active high
                                                                        // This bit is the Y min and max auto detection enable bit for black/white level expansion,
                                                                        // when this bit is 1, the min and max value of Y in every frame will be detected,
                                                                        // otherwise, the min and max value are defined by register
    typedef UReg<0x03, 0x27, 0, 4> VDS_USER_MIN;                        // Programmable minimum value control bit (This field is the user defined min value for black level expansion, the actual min value in use is 2*blev_det_min+1)
    typedef UReg<0x03, 0x27, 4, 4> VDS_USER_MAX;                        // Programmable maximum value control bit (This field is the user defined max value for black level expansion, the actual min value in use is 16*blev_det_max+15)
    typedef UReg<0x03, 0x28, 0, 8> VDS_BLEV_LEVEL;                      // Black level expansion level control bit (This field defines the black level expansion threshold level value, data larger than this level will have no black level expansion process)
    typedef UReg<0x03, 0x29, 0, 8> VDS_BLEV_GAIN;                       // Black level expansion gain control bit (This field contains the gain control of black level expansion, its range is (0~16)*16)
    typedef UReg<0x03, 0x2A, 0, 1> VDS_BLEV_BYPS;                       // Black level expansion bypass control, active high (This bit is the bypass control bit of black level expansion, when it is 1, data will bypass black level expansion process)
    // 2A_1:3   -   reserved
    typedef UReg<0x03, 0x2A, 4, 2> VDS_STEP_DLY_CNTRL;                  // UV step response data select control bit (see: DEVELOPER_NOTES.md -> S3_2A)
    // 2A_6:2   -   reserved
    // typedef UReg<0x03, 0x2A, 6, 2> VDS_0X2A_RESERVED_2BITS;
    typedef UReg<0x03, 0x2B, 0, 4> VDS_STEP_GAIN;                       // UV Step response gain control bit (This field register can adjust the UV edge improvement, the larger value of this register, the sharper edge will appear, the range of this gain is (0~4)*4)
    typedef UReg<0x03, 0x2B, 4, 3> VDS_STEP_CLIP;                       // UV step response clip control bit (This filed contains the clip control value of UV step response)
    typedef UReg<0x03, 0x2B, 7, 1> VDS_UV_STEP_BYPS;                    // UV step response bypass control, active high (When this bit is 1, UV data will don’t do step response)

    typedef UReg<0x03, 0x2C, 0, 8> VDS_SK_U_CENTER;                     // Skin color correction U center position control bit (This field contains the skin color center position U value, the value is 2’s)
    typedef UReg<0x03, 0x2D, 0, 8> VDS_SK_V_CENTER;                     // Skin color correction V center position control bit (This field contains the skin color center position U value, the value is 2’s)
    typedef UReg<0x03, 0x2E, 0, 8> VDS_SK_Y_LOW_TH;                     // Skin color correction Y low threshold control bit (Y low threshold value for skin color correction, if y less than this threshold, no skin color correction done)
    typedef UReg<0x03, 0x2F, 0, 8> VDS_SK_Y_HIGH_TH;                    // Skin color correction Y high threshold control bit (Y high threshold value for skin color correction, if y larger than this threshold, no skin color correction done)
    typedef UReg<0x03, 0x30, 0, 8> VDS_SK_RANGE;                        // Skin color correction range control bit (The skin color correction will done just when the value abs(u-u_center)+abs(v- v_enter) less than this programmable range)
    typedef UReg<0x03, 0x31, 0, 4> VDS_SK_GAIN;                         // Skin color correction gain control bit (This register defines the degree of the skin color correction, the higher the value, the more skin color correction done. Its range is (0~1)*16)
    typedef UReg<0x03, 0x31, 4, 1> VDS_SK_Y_EN;                         // Skin color Y detect enable, active high (When this bit is 1, take the Y value as the condition of skin color correction, just when the Y value larger than y_low_th and less the y_high_th, the correction can be done)
    typedef UReg<0x03, 0x31, 5, 1> VDS_SK_BYPS;                         // Skin color correction bypass control, active high (When this bit is 1, the skin color correction will be bypassed)
    // 31_6:2   -   reserved
    typedef UReg<0x03, 0x32, 0, 2> VDS_SVM_BPF_CNTRL;                   // SVM data generation select control (see: DEVELOPER_NOTES.md -> S3_32)
    typedef UReg<0x03, 0x32, 2, 1> VDS_SVM_POL_FLIP;                    // SVM polarity flip control bit (When this bit is 1, the SVM signal’s polarity will be flipped, otherwise, SVM remains the original phase)
    typedef UReg<0x03, 0x32, 3, 1> VDS_SVM_2ND_BYPS;                    // 2nd order SVM signal generation bypass, active high (When this bit is 1, SVM signal is 1st order, otherwise, it is 2nd order derivative signal)
    typedef UReg<0x03, 0x32, 4, 3> VDS_SVM_VCLK_DELAY;                  // To match YUV pipe, SVM data delay by VCLK control bit (This field define the SVM compensation delay from 1 to 8 VCLKs)
    typedef UReg<0x03, 0x32, 7, 1> VDS_SVM_SIGMOID_BYPS;                // SVM bypass the sigmoid function, active high (When this bit is 1, SVM signal bypass a sigmoid function. This function can make the SVM signal sharper)
    typedef UReg<0x03, 0x33, 0, 8> VDS_SVM_GAIN;                        // SVM gain control bit (This field contains the gain value of SVM data., its range is (0~16)*16)
    typedef UReg<0x03, 0x34, 0, 8> VDS_SVM_OFFSET;                      // SVM offset control bit (This field contains the offset value of SVM data, its range is 0~255)

    typedef UReg<0x03, 0x35, 0, 8> VDS_Y_GAIN;                          // Y dynamic range expansion gain control bit (This field contains the Y gain value in dynamic range expansion process, its range is (0 ~ 2)*128)
    typedef UReg<0x03, 0x36, 0, 8> VDS_UCOS_GAIN;                       // U dynamic range expansion cos gain control bit (This field contains the U gain value in dynamic range expansion process, its range is (-4 ~ 4)*32)
    typedef UReg<0x03, 0x37, 0, 8> VDS_VCOS_GAIN;                       // V dynamic range expansion gain control bit (This field contains the V gain value in dynamic range expansion process, its range is (-4 ~ 4)*32)
    typedef UReg<0x03, 0x38, 0, 8> VDS_USIN_GAIN;                       // U dynamic range expansion sin gain control bit (This field contains the U sin gain value in dynamic range expansion process, its range is (-4 ~ 4)*32)
    typedef UReg<0x03, 0x39, 0, 8> VDS_VSIN_GAIN;                       // V dynamic range expansion sin gain control bit (This field contains the V sin gain value in dynamic range expansion process, its range is (-4 ~ 4)*32)
    typedef UReg<0x03, 0x3A, 0, 8> VDS_Y_OFST;                          // Y dynamic range expansion offset control bit (This field contains the Y offset value in dynamic range expansion process, its range is –128 ~ 127)
    typedef UReg<0x03, 0x3B, 0, 8> VDS_U_OFST;                          // U dynamic range expansion offset control bit (This field contains the U offset value in dynamic range expansion process, its range is –128 ~ 127)
    typedef UReg<0x03, 0x3C, 0, 8> VDS_V_OFST;                          // V dynamic range expansion offset control bit (This field contains the V offset value in dynamic range expansion process., its range is –128 ~ 127)

    typedef UReg<0x03, 0x3D, 0, 9> VDS_SYNC_LEV;                        // Sync level bit
                                                                        // This field contains the composite sync level value, this value will add on Y,
                                                                        // outside the composite sync interval. If the Y out is 1V, sync is 0.3V,
                                                                        // then this value is (0.3/1)*1024=307, and the output sync’s max voltage is 0.5V
    // 3E_1:2   -   reserved
    typedef UReg<0x03, 0x3E, 3, 1> VDS_CONVT_BYPS;                      // YUV to RGB color space conversion bypass control, active high
                                                                        // When this bit is 1, YUV data will bypass the YUV to RGB conversion, the output will still be YUV data.
                                                                        // When this bit is 0, YUV data will do YUV to RGB conversion, the output will be RGB data.
    typedef UReg<0x03, 0x3E, 4, 1> VDS_DYN_BYPS;                        // Dynamic range expansion bypass control, active high (When this bit is 1, data will bypass the dynamic range expansion process)
    // 3E_5:2   -   reserved
    typedef UReg<0x03, 0x3E, 7, 1> VDS_BLK_BF_EN;                       // Blanking set up enable, active high (When this bit is 1, final composite blank (dis_hb|dis_vb) will cut the garbage data in blanking interval)
    typedef UReg<0x03, 0x3F, 0, 8> VDS_UV_BLK_VAL;                      // UV blanking amplitude value control bit (This filed indicates the amplitude value of UV in blanking interval, the highest bit of this programmable register is sign bit)
    typedef UReg<0x03, 0x40, 0, 1> VDS_1ST_INT_BYPS;                    // The 1st stage interpolation bypass control, active high (When this bit is 1, the 1st stage interpolation (in YUV domain) will be bypassed, Y use tap19, and UV use tap7)
    typedef UReg<0x03, 0x40, 1, 1> VDS_2ND_INT_BYPS;                    // The 2nd stage interpolation bypass control, active high (When this bit is 1, the 2nd stage interpolation (in RGB domain) will be bypassed, all RGB use tap11)
    typedef UReg<0x03, 0x40, 2, 1> VDS_IN_DREG_BYPS;                    // Input data bypass the negedge trigger control, active high:
                                                                        // When this bit is 0, input data will triggered by falling edge clock,
                                                                        // When this bit is 1, the input data will bypass this falling edge clock delay.)
    // 40_3:1   -   reserved
    typedef UReg<0x03, 0x40, 4, 2> VDS_SVM_V4CLK_DELAY;                 // SVM delay be V2CLK control bit (see: DEVELOPER_NOTES.md -> S3_40)
    // 40_6:2   -   reserved
    typedef UReg<0x03, 0x41, 0, 10> VDS_PK_LINE_BUF_SP;                 // Line buffer for 2D peaking write reset position control bit (This field contains the write reset position of the line buffer, this position is also the write start position of the buffer.)
    // 42_2:4   -   reserved
    typedef UReg<0x03, 0x42, 6, 1> VDS_PK_RAM_BYPS;                     // Line buffer for 2D peaking one line delay data bypass, active high (When this bit is 1, data will bypass the line buffer)
    // 42_7:1   -   reserved
    typedef UReg<0x03, 0x43, 0, 1> VDS_PK_VL_HL_SEL;                    // 2D peaking vertical low-pass signal select the horizontal split filter control (low-pass filter select, 1 for tap3 and 0 for tap5)
    typedef UReg<0x03, 0x43, 1, 1> VDS_PK_VL_HH_SEL;                    // 2D peaking vertical low-pass signal select the horizontal split filter control (for high-pass filter select, 1 for tap3 and 0 for tap5)
    typedef UReg<0x03, 0x43, 2, 1> VDS_PK_VH_HL_SEL;                    // 2D peaking vertical high-pass signal select the horizontal split filter control (high-pass filter select, 1 for tap3 and 0 for tap5)
    typedef UReg<0x03, 0x43, 3, 1> VDS_PK_VH_HH_SEL;                    // 2D peaking vertical high-pass signal select the horizontal split filter control (low-pass filter select, 1 for tap3 and 0 for tap5)
    // 43_4:4   -   reserved
    typedef UReg<0x03, 0x44, 0, 3> VDS_PK_LB_CORE;                      // 2D peaking vertical low-pass horizontal band-pass signal coring level (Vertical low-pass and horizontal band-pass signal larger than this coring level will remain unchanged, otherwise it will be cut to 0)
    typedef UReg<0x03, 0x44, 3, 5> VDS_PK_LB_CMP;                       // 2D peaking vertical low-pass horizontal band-pass signal threshold level (Vertical low-pass and horizontal band-pass signal larger than this coring level will remain unchanged, otherwise the gain will added on it)
    typedef UReg<0x03, 0x45, 0, 6> VDS_PK_LB_GAIN;                      // 2D peaking vertical low-pass horizontal band-pass signal gain control (Vertical low-pass horizontal band-pass signal gain, its range is (0~4)*16)
    // 45_6:2   -   reserved
    typedef UReg<0x03, 0x46, 0, 3> VDS_PK_LH_CORE;                      // 2D peaking vertical low-pass horizontal high-pass signal coring level (Vertical low-pass and horizontal high-pass signal larger than this coring level will remain unchanged, otherwise it will be cut to 0)
    typedef UReg<0x03, 0x46, 3, 5> VDS_PK_LH_CMP;                       // 2D peaking vertical low-pass horizontal high-pass signal threshold level (Vertical low-pass and horizontal high-pass signal larger than this coring level will remain unchanged, otherwise the gain will added on it)
    typedef UReg<0x03, 0x47, 0, 6> VDS_PK_LH_GAIN;                      // 2D peaking vertical low-pass horizontal high-pass signal gain control (Vertical low-pass horizontal high-pass signal gain, its range is (0~4)*16)
    // 47_6:2   -   reserved
    typedef UReg<0x03, 0x48, 0, 3> VDS_PK_HL_CORE;                      // 2D peaking vertical high-pass horizontal low-pass signal coring level (Vertical high-pass and horizontal low-pass signal larger than this coring level will remain unchanged, otherwise it will be cut to 0)
    typedef UReg<0x03, 0x48, 3, 5> VDS_PK_HL_CMP;                       // 2D peaking vertical high-pass horizontal low-pass signal threshold level (Vertical high-pass and horizontal low-pass signal larger than this coring level will remain unchanged, otherwise the gain will added on it)
    typedef UReg<0x03, 0x49, 0, 6> VDS_PK_HL_GAIN;                      // 2D peaking vertical high-pass horizontal low-pass signal gain control (Vertical high-pass horizontal low-pass signal gain, its range is (0~4)*16)
    // 49_6:2   -   reserved
    typedef UReg<0x03, 0x4A, 0, 3> VDS_PK_HB_CORE;                      // 2D peaking vertical high-pass horizontal band-pass signal coring level (Vertical high-pass and horizontal band-pass signal larger than this coring level will remain unchanged, otherwise it will be cut to 0)
    typedef UReg<0x03, 0x4A, 3, 5> VDS_PK_HB_CMP;                       // 2D peaking vertical high-pass horizontal band-pass signal threshold level (Vertical high-pass and horizontal band-pass signal larger than this coring level will remain unchanged, otherwise the gain will added on it)
    typedef UReg<0x03, 0x4B, 0, 6> VDS_PK_HB_GAIN;                      // 2D peaking vertical high-pass horizontal band-pass signal gain control (Vertical high-pass horizontal band-pass signal gain, its range is (0~4)*16)
    typedef UReg<0x03, 0x4C, 0, 3> VDS_PK_HH_CORE;                      // 2D peaking vertical high-pass horizontal high-pass signal coring level (Vertical high-pass and horizontal high-pass signal larger than this coring level will remain unchanged, otherwise it will be cut to 0)
    typedef UReg<0x03, 0x4C, 3, 5> VDS_PK_HH_CMP;                       // 2D peaking vertical high-pass horizontal high-pass signal threshold level (Vertical high-pass and horizontal high-pass signal larger than this coring level will remain unchanged, otherwise the gain will added on it)
    typedef UReg<0x03, 0x4D, 0, 6> VDS_PK_HH_GAIN;                      // 2D peaking vertical high-pass horizontal high-pass signal gain control (Vertical high-pass horizontal high-pass signal gain, its range is (0~4)*16)
    // 4D_6:2   -   reserved
    typedef UReg<0x03, 0x4E, 0, 1> VDS_PK_Y_H_BYPS;                     // Y horizontal peaking bypass control, active high (When this bit is 1, Y horizontal peaking will be bypassed)
    typedef UReg<0x03, 0x4E, 1, 1> VDS_PK_Y_V_BYPS;                     // Y vertical peaking bypass control, active high (When this bit is 1, Y vertical peaking will be bypassed)
    // 4E_2:1   -   reserved
    typedef UReg<0x03, 0x4E, 3, 1> VDS_C_VPK_BYPS;                      // UV vertical peaking bypass control, active high (When this bit is 1, UV vertical peaking will be bypassed)
    typedef UReg<0x03, 0x4E, 4, 3> VDS_C_VPK_CORE;                      // UV vertical peaking coring level (UV vertical high-pass signal larger than this coring level will remain unchanged, otherwise it will be cut to 0)
    // 4E_7:1   -   reserved
    typedef UReg<0x03, 0x4F, 0, 6> VDS_C_VPK_GAIN;                      // UV vertical peaking gain control bit (UV vertical high-pass signal gain control, its range is (0~4)*16)
    // 4F_6:2   -   reserved
    typedef UReg<0x03, 0x50, 0, 4> VDS_TEST_BUS_SEL;                    // Test out select control bit (This register is used to select internal status bus to test bus)
    typedef UReg<0x03, 0x50, 4, 1> VDS_TEST_EN;                         // Test enable, active high (This bit is the test bus out enable bit, when this bit is 1, the test bus can output the internal status, and otherwise, the test bus is 0Xaaaa)
    typedef UReg<0x03, 0x50, 5, 1> VDS_DO_UV_DEV_BYPS;                  // 16-bit digital out UV decimation filter bypass control, active high (When this bit is 1, 16-bit 422 YUV digital out UV decimation will be bypassed)
    typedef UReg<0x03, 0x50, 6, 1> VDS_DO_UVSEL_FLIP;                   // 16-bit digital out UV flip control (When this bit is 1, 16-bit 422 YUV digital out UV position will be flipped)
    typedef UReg<0x03, 0x50, 7, 1> VDS_DO_16B_EN;                       // 16-bit digital out (422 format yuv) enable (When this bit is 1, digital out is 16-bit 422 YUV format; When it is 0, digital out is 24-bit)

    typedef UReg<0x03, 0x51, 7, 11> VDS_GLB_NOISE;                      // Global still detection threshold value control bit:
                                                                        // This field contains the global noise threshold value. If the total difference of two
                                                                        // frame less than this programmable value, the picture is taken as still,
                                                                        // otherwise, the picture is taken as moving picture.
    // 52_3:1   -   reserved
    typedef UReg<0x03, 0x52, 4, 1> VDS_NR_Y_BYPS;                       // Y bypass the noise reduction process control (When this bit is 1, Y data will bypass the noise reduction process)
    typedef UReg<0x03, 0x52, 5, 1> VDS_NR_C_BYPS;                       // UV bypass the noise reduction process control (When this bit is 1, UV data will bypass the noise reduction process)
    typedef UReg<0x03, 0x52, 6, 1> VDS_NR_DIF_LPF5_BYPS;                // Bypass control of the tap5 low-pass filter used for Y difference between two frames (When this bit is 1, Y difference data will bypass the tap5 low-pass filter)
    typedef UReg<0x03, 0x52, 7, 1> VDS_NR_MI_TH_EN;                     // Noise reduction threshold control enable (This bit will enable the threshold control, active high)
    typedef UReg<0x03, 0x53, 0, 7> VDS_NR_MI_OFFSET;                    // Motion index offset control bit (The offset control for motion index generation; When ds_mig_en is 1, ds_mig_offset[3:0] is user-defined motion index)
    typedef UReg<0x03, 0x53, 7, 1> VDS_NR_MIG_USER_EN;                  // Motion index generation user mode enable (When this bit is 1, the motion index generation will use nr_mig_offt[3:0] as Motion index)
    typedef UReg<0x03, 0x54, 0, 4> VDS_NR_MI_GAIN;                      // Motion index generation gain control bit (Motion index generation gain control, its range is (0~8)*2)
    typedef UReg<0x03, 0x54, 4, 4> VDS_NR_STILL_GAIN;                   // Motion index generation gain control bit for still picture (When picture is still, this field contains the motion index generation gain, its range is (0~8)*2)
    typedef UReg<0x03, 0x55, 0, 4> VDS_NR_MI_THRES;                     // Noise reduction threshold value bit (Noise-reduction threshold value. When MI is smaller than the threshold value, the noise reduction is enabled. Otherwise it is not)
    typedef UReg<0x03, 0x55, 4, 1> VDS_NR_EN_H_NOISY;                   // High noisy picture index enable, active high (Enable high noisy index from de-interlacer, it means the picture’s noise is very large)
    // 55_5:1   -   reserved
    typedef UReg<0x03, 0x55, 6, 1> VDS_NR_EN_GLB_STILL;                 // Global still index enable, active high (This bit enables the global still signal)
    typedef UReg<0x03, 0x55, 7, 1> VDS_NR_GLB_STILL_MENU;               // Menu mode control for global still index (used for debug) (see: DEVELOPER_NOTES.md -> S3_55)
    typedef UReg<0x03, 0x56, 0, 7> VDS_NR_NOISY_OFFSET;                 // Motion index generation offset control bit for high noisy picture (When the picture is high noisy picture, this field contains the offset control for motion index generation)
    typedef UReg<0x03, 0x56, 7, 1> VDS_W_LEV_BYPS;                      // White level expansion bypass control, active high (When this bit is 1, Y don’t do white level expansion)
    typedef UReg<0x03, 0x57, 0, 8> VDS_W_LEV;                           // White level expansion level control bit (This field defines the white level expansion threshold level value; data less than this level will have no white level expansion process)
    typedef UReg<0x03, 0x58, 0, 8> VDS_WLEV_GAIN;                       // White level expansion gain control bit (This field defines the white level expansion threshold level value; data less than this level will have no white level expansion process)
    typedef UReg<0x03, 0x59, 0, 8> VDS_NS_U_CENTER;                     // Non-linear saturation center position U value control bit (This field contains the non-linear saturation center position U value, the value is 2’s)
    typedef UReg<0x03, 0x5A, 0, 8> VDS_NS_V_CENTER;                     // Non-linear saturation center position V value control bit (This field contains the non-linear saturation center position V value, the value is 2’s)
    typedef UReg<0x03, 0x5B, 0, 7> VDS_NS_U_GAIN;                       // Non-linear saturation U gain control bit (This field contains the U gain control for U component in the area which should do non-linear saturation, its range is (0~1)*128)
    typedef UReg<0x03, 0x5B, 7, 15> VDS_NS_SQUARE_RAD;                  // Non-linear saturation range control bit (Non-linear saturation only did When (u-u_center)^2 + (v-v_center)^2 less than this programmable range value)
    typedef UReg<0x03, 0x5D, 6, 8> VDS_NS_Y_HIGH_TH;                    // Non-linear saturation Y high threshold control bit (This filed defines the Y high threshold value for non-linear saturation, when y detect enable (60[3]=1), if y larger than this programmable value, no non-linear did)
    typedef UReg<0x03, 0x5E, 6, 7> VDS_NS_V_GAIN;                       // Non-linear saturation V gain control bit (This field contains the V gain control for V component in the area which should do non-linear saturation, its range is (0~1)*128)
    typedef UReg<0x03, 0x5F, 5, 5> VDS_NS_Y_LOW_TH;                     // Non-linear saturation Y low threshold control bit (This filed defines the Y low threshold value for non-linear saturation, when y detect enable (60[3]=1), if y less than this programmable value, no non-linear did)
    typedef UReg<0x03, 0x60, 2, 1> VDS_NS_BYPS;                         // Non-linear saturation bypass control, active high (When this bit is 1, the process non-linear saturation will be bypassed)
    typedef UReg<0x03, 0x60, 3, 1> VDS_NS_Y_ACTIVE_EN;                  // Non-linear saturation Y detect enable, active high (When this bit is 1, the process non-linear saturation only done when the Y larger than the value ns_y_low_th and less than the value ns_y_high_th)

    typedef UReg<0x03, 0x60, 4, 10> VDS_C1_TAG_LOW_SLOPE;               // Red enhance angle tan value low threshold value control bit:
                                                                        // This filed contains the low threshold value for red enhance angle tan value,
                                                                        // when the input UV angle tan value less than this programmable value,
                                                                        // no enhancement did
    typedef UReg<0x03, 0x61, 6, 10> VDS_C1_TAG_HIGH_SLOPE;              // Red enhance angle tan value high threshold value control bit:
                                                                        // This filed contains the high threshold value for red enhance angle tan value,
                                                                        // when the input UV angle tan value larger than this programmable value,
                                                                        // no enhancement did.
    typedef UReg<0x03, 0x63, 0, 4> VDS_C1_GAIN;                         // Red enhance gain control bit (This field contains the gain control for red enhance, its range is (0~1)*16)
    typedef UReg<0x03, 0x63, 4, 8> VDS_C1_U_LOW;                        // Red enhance U low threshold value control bit (This field contains the low threshold value for U component, if input U less then this programmable value, no enhancement did)
    typedef UReg<0x03, 0x64, 4, 8> VDS_C1_U_HIGH;                       // Red enhance U high threshold value control bit (This field contains the high threshold value for U component, if input U larger then this programmable value, no enhancement did)
    typedef UReg<0x03, 0x65, 4, 1> VDS_C1_BYPS;                         // Red enhance bypass control, active high (When this bit is 1, red enhancement will be bypassed)
    typedef UReg<0x03, 0x65, 5, 8> VDS_C1_Y_THRESH;                     // Red enhance Y threshold value control bit (This field contains the Y threshold for red enhancement, when input Y larger than this programmable value, no enhancement did)

    typedef UReg<0x03, 0x66, 5, 10> VDS_C2_TAG_LOW_SLOPE;               // Green enhance angle tan value low threshold value control bit:
                                                                        // This filed contains the low threshold value for green enhance angle tan value,
                                                                        // when the input UV angle tan value less than this programmable value, no enhancement did.
    typedef UReg<0x03, 0x67, 7, 10> VDS_C2_TAG_HIGH_SLOPE;              //Green enhance angle tan value high threshold value control bit:
                                                                        // This filed contains the high threshold value for green enhance angle tan value,
                                                                        // when the input UV angle tan value larger than this programmable value,
                                                                        // no enhancement did.
    typedef UReg<0x03, 0x69, 1, 4> VDS_C2_GAIN;                         // Color enhance gain control bit (This field contains the gain control for green enhance, its range is (0~1)*16)
    typedef UReg<0x03, 0x69, 5, 8> VDS_C2_U_LOW;                        // Green enhance U low threshold value control bit (This field contains the low threshold value for U component, if input U less then this programmable value, no enhancement did)
    typedef UReg<0x03, 0x6A, 5, 8> VDS_C2_U_HIGH;                       // Green enhance U high threshold value control bit (This field contains the high threshold value for U component, if input U larger then this programmable value, no enhancement did)
    typedef UReg<0x03, 0x6B, 5, 1> VDS_C2_BYPS;                         // Green enhance bypass control (When this bit is 1, color enhancement will be bypassed)
    typedef UReg<0x03, 0x6B, 6, 8> VDS_C2_Y_THRESH;                     // Green enhance Y threshold value control bit (This field contains the Y threshold for green enhancement, when input Y larger than this programmable value, no enhancement did)
    // 6C_6:2   -   reserved
    typedef UReg<0x03, 0x6D, 0, 12> VDS_EXT_HB_ST;                      // External used horizontal blanking start position control bit (This field is used to program horizontal blanking start position, this blanking is for external used)
    typedef UReg<0x03, 0x6E, 4, 12> VDS_EXT_HB_SP;                      // External used horizontal blanking stop position control bit (This field is used to program horizontal blanking stop position, this blanking is for external used)
    typedef UReg<0x03, 0x70, 0, 11> VDS_EXT_VB_ST;                      // External used vertical blanking start position control bit (This field is used to program vertical blanking start position, this blanking is for external used)
    // 71_3:1   -   reserved
    typedef UReg<0x03, 0x71, 4, 11> VDS_EXT_VB_SP;                      // External used vertical blanking stop position control bit (This field is used to program vertical blanking stop position, this blanking is for external used)
    typedef UReg<0x03, 0x72, 7, 1> VDS_SYNC_IN_SEL;                     // VDS module input sync selection control (When this bit is 1, the sync to VDS module is from external (out of the CHIP); When this bit is 0, the sync to VDS module is from IF module)

    typedef UReg<0x03, 0x73, 0, 3> VDS_BLUE_RANGE;                      // Blue extend range control bit (see: DEVELOPER_NOTES.md -> S3_73)
    typedef UReg<0x03, 0x73, 3, 1> VDS_BLUE_BYPS;                       // Blue extend bypass control, active high (When this bit is 1, the blue extend process will be bypassed)
    typedef UReg<0x03, 0x73, 4, 4> VDS_BLUE_UGAIN;                      // Blue extend U gain control bit (This field defines the U gain for U component in the area which should do blue extend, its range is (0~1)*16)
    typedef UReg<0x03, 0x74, 0, 4> VDS_BLUE_VGAIN;                      // Blue extend V gain control bit (This field defines the V gain for V component in the area which should do blue extend, its range is (0~1)*16)
    typedef UReg<0x03, 0x74, 4, 4> VDS_BLUE_Y_LEV;                      // Blue extend Y level threshold control bit:
                                                                        // This field defines the Y threshold value of blue extend, the real level in the
                                                                        // circuit is 16*blue_y_th + 15, the blue extend process done
                                                                        // only when Y value larger than this level (real level).

    //
    // Video Processor Registers -> PIP Registers
    //
    typedef UReg<0x03, 0x80, 0, 1> PIP_UV_FLIP;                         // 422 to 444 conversion UV flip control (This bit is used to flip UV, when this bit is 1, UV position will be flipped)
    typedef UReg<0x03, 0x80, 1, 1> PIP_U_DELAY;                         // UV 422 to 444 conversion U delay (When this bit is 1, U will delay 1 clock, otherwise, no delay for internal pipe)
    typedef UReg<0x03, 0x80, 2, 1> PIP_V_DELAY;                         // UV 422 to 444 conversion V delay (When this bit is 1, V will delay 1 clock, otherwise, no delay for internal pipe)
    typedef UReg<0x03, 0x80, 3, 1> PIP_TAP3_BYPS;                       // Tap3 filter in 422 to 444 conversion bypass control, active high (This bit is the UV interpolation filter enable control; when this bit is 1, UV bypass the filter)
    typedef UReg<0x03, 0x80, 4, 2> PIP_Y_DELAY;                         // Y compensation delay control bit in 422 to 444 conversion (see: DEVELOPER_NOTES.md -> S3_80)
    typedef UReg<0x03, 0x80, 6, 1> PIP_SUB_16B_SEL;                     // PIP 16-bit sub-picture select, active high (When this bit is 1, select 16-bit sub-picture; When it is 0, select 24-bit sub-picture)
    typedef UReg<0x03, 0x80, 7, 1> PIP_DYN_BYPS;                        // Dynamic range expansion bypass control, active high (When this bit is 1, data will bypass the dynamic range expansion process)
    typedef UReg<0x03, 0x81, 0, 1> PIP_CONVT_BYPS;                      // YUV to RGB color space conversion bypass control, active high:
                                                                        // When this bit is 1, YUV data will bypass the YUV to RGB conversion, the output will still be YUV data.
                                                                        // When this bit is 0, YUV data will do YUV to RGB conversion, the output will be RGB data
    // 81_1:2   -   reserved
    typedef UReg<0x03, 0x81, 3, 1> PIP_DREG_BYPS;                       // Input data bypass the negedge trigger control, active high:
                                                                        // When this bit is 0, input data will triggered by falling edge clock,
                                                                        // When this bit is 1, the input data will bypass this falling edge clock delay
    // 81_4:3   -   reserved
    typedef UReg<0x03, 0x81, 7, 1> PIP_EN;                              // PIP enable, active high (When this bit is 1, PIP insertion is enabled, otherwise, no PIP)
    typedef UReg<0x03, 0x82, 0, 8> PIP_Y_GAIN;                          // Y dynamic range expansion gain control bit (This field contains the Y gain value in dynamic range expansion process, its range is (0 ~ 2)*128)
    typedef UReg<0x03, 0x83, 0, 8> PIP_U_GAIN;                          // U dynamic range expansion gain control bit (This field contains the U gain value in dynamic range expansion process, its range is (0 ~ 4)*64)
    typedef UReg<0x03, 0x84, 0, 8> PIP_V_GAIN;                          // V dynamic range expansion gain control bit (This field contains the V gain value in dynamic range expansion process, its range is (0 ~ 4)*64)
    typedef UReg<0x03, 0x85, 0, 8> PIP_Y_OFST;                          // Y dynamic range expansion offset control bit (This field contains the Y offset value in dynamic range expansion process, its range is –128 ~ 127)
    typedef UReg<0x03, 0x86, 0, 8> PIP_U_OFST;                          // U dynamic range expansion offset control bit (This field contains the U offset value in dynamic range expansion process, its range is –128 ~ 127)
    typedef UReg<0x03, 0x87, 0, 8> PIP_V_OFST;                          // V dynamic range expansion offset control bit (This field contains the V offset value in dynamic range expansion process, its range is –128 ~ 127)
    typedef UReg<0x03, 0x88, 0, 12> PIP_H_ST;                           // PIP window horizontal start position control bit (This field contains the horizontal start position of PIP window.)
    // 89_4:4   -   reserved
    typedef UReg<0x03, 0x8A, 0, 12> PIP_H_SP;                           // PIP window horizontal stop position control bit (This field contains the horizontal stop position of PIP window)
    // 8B_4:4   -   reserved
    typedef UReg<0x03, 0x8C, 0, 11> PIP_V_ST;                           // PIP window vertical start position control bit (This field contains the vertical start position of PIP window)
    // 8D_3:5   -   reserved
    typedef UReg<0x03, 0x8E, 0, 11> PIP_V_SP;                           // PIP window vertical stop position control  (This field contains the vertical stop position of PIP window)
    // 8F_3:5   -   reserved

    //
    // Memory Controller Registers
    //
    typedef UReg<0x04, 0x00, 0, 8> SDRAM_RESET_CONTROL;                 // MEMORY CONTROLLER 00
        typedef UReg<0x04, 0x00, 0, 1> MEM_INI_REG_0;                       // SDRAM Idle Period Control and IDLE Done Select: (default 0) (see: DEVELOPER_NOTES.md -> S4_00)
        typedef UReg<0x04, 0x00, 0, 1> MEM_INI_REG_1;                       // Software Control SDRAM Idle Period (When this bit is 1, software programming will control the idle period to access memory.this bit is useful only when the register r_mslidl[1:0] sets 2’b11)
        // 00_3:1   -   reserved
        typedef UReg<0x04, 0x00, 4, 1> SDRAM_RESET_SIGNAL;                  // SDRAM Reset Signal (When this bit is 1, will generate 5-mmclk pulse, and reset memory controller timing, data pipe and state machine)
        // 00_5:1   -   reserved
        typedef UReg<0x04, 0x00, 6, 1> MEM_INI_REG_6;                       // Initial Cycle Mode Select (When this bit is 1, then during initial period, the mode cycle will go before refresh cycle; otherwise refresh cycle will be before mode cycle)
        typedef UReg<0x04, 0x00, 7, 1> SDRAM_START_INITIAL_CYCLE;           // SDRAM Start Initial Cycle (This register should work with the register 80/[2:0]; When this bit is 1, memory controller initial cycle enable; When this bit is 0, memory controller initial cycle disable)
    // TODO fine tuning memory registers
    typedef UReg<0x04, 0x12, 0, 1> MEM_INTER_DLYCELL_SEL;               // Select SDRAM Delay Cell (This register is control the delay of data/address/command; When it is at 0, select bypass delay cell, when it is at 1, select DLY8LV cell)
    typedef UReg<0x04, 0x12, 1, 1> MEM_CLK_DLYCELL_SEL;                 // Select SDRAM Delay Cell (This register is only control the delay of clock send to PAD; When it is at 0, select bypass delay cell, when it is at 1, select DLY8LV cell)
    typedef UReg<0x04, 0x12, 2, 1> MEM_FBK_CLK_DLYCELL_SEL;             // Select SDRAM Delay Cell (This register is only control the delay of feed back clock; When it is at 0, select bypass delay cell, when it is at 1, select DLY8LV cell)
    // 12_3:5   -   reserved
    typedef UReg<0x04, 0x13, 0, 1> MEM_PAD_CLK_INVERT;                  // Invert Memory Rising Edge Clock to PAD (When this bit is 1, invert memory clock and send to PAD; When this bit is 0, will bypass memory clock and send to PAD)
    typedef UReg<0x04, 0x13, 1, 1> MEM_RD_DATA_CLK_INVERT;              // Read memory data with Memory Clock rising or falling edge (When this bit is 1, with Memory clock falling edge; When this bit is 0, with Memory clock rising edge)
    typedef UReg<0x04, 0x13, 2, 1> MEM_FBK_CLK_INVERT;                  // Control feedback clock register (When this bit is at 1, will invert feedback clock; When it’s at 0, will bypass feedback clock)
    // 13_3:5   -   reserved
    // TODO fine tuning memory registers
    typedef UReg<0x04, 0x15, 0, 1> MEM_REQ_PBH_RFFH;                    // Play back high request priority exchange with read FIFO high request (When this bit is 1, read FIFO high request > play back high request; When this bit is 0, play back high request >read FIFO high request)
    typedef UReg<0x04, 0x1b, 0, 3> MEM_ADR_DLY_REG;                     // Capture request exchange with PlayBack low request and Read FIFO low request (When this bit is 0: play back low req > read FIFO low req > capture req; When this bit is 1: cap req > play back low req > read FIFO low req)
    typedef UReg<0x04, 0x1b, 4, 3> MEM_CLK_DLY_REG;                     // Write FIFO request priority exchange with capture request (When this bit is 1, capture request >write FIFO request, When this bit is 0, write FIFO request > capture request)
    // 15_3:5   -   reserved
    // TODO fine tuning memory registers

    //
    // Playback / Capture / Memory Registers
    //
    typedef UReg<0x04, 0x20, 0, 3> CAP_CNTRL_TST;                       // Capture Test logic control (Bit [2:0]: select capture internal test bus)
    // 20_3:2   -   reserved
    typedef UReg<0x04, 0x20, 5, 3> CAP_NR_STATUS_OFFSET;                // Capture Noise Reduction Frame Status Offset:
                                                                        // For NTSC and PAL, Noise Reduction will save 4 or 6 frame data, for
                                                                        // Play back read which frame at first, set different value,
                                                                        // will read different frame data firstly, default 0.

    typedef UReg<0x04, 0x21, 0, 1> CAPTURE_ENABLE;                      // Enable capture (When it’s set 1, capture will be turn on; When it’s set 0, capture will be turn off)
    typedef UReg<0x04, 0x21, 1, 1> CAP_FF_HALF_REQ;                     // Request generated when capture FIFO half (When set to 1, request generated when capture FIFO half; When set to 0, request generated when capture FIFO write pointer is 1)

    typedef UReg<0x04, 0x21, 2, 1> CAP_BUF_STA_INV;                     // Capture double buffer status invert before output (When set to 1, double buffer status invert; When set to 0, double buffer status doesn’t change)
    typedef UReg<0x04, 0x21, 3, 1> CAP_DOUBLE_BUFFER;                   // Enable double buffer (When set to 1, enable double buffer; When set to 0, disable double buffer)
    // 21_4:1   -   reserved
    typedef UReg<0x04, 0x21, 5, 1> CAP_SAFE_GUARD_EN;                   // Enable safe guard function (When set to 1, turn on safe guard function; When set to 0, turn off safe guard function)
    typedef UReg<0x04, 0x21, 6, 1> CAP_VRST_FFRST_EN;                   // Enable input v-sync reset FIFO (When set to 1, enable feed back v-sync reset FIFO; When set to 0, disable feed back v-sync reset FIFO)
    typedef UReg<0x04, 0x21, 7, 1> CAP_ADR_ADD_2;                       // Enable address add by 2 (When set to 1,address added by 2 per pixel; When set to 0,added by 1 per pixel)
    typedef UReg<0x04, 0x22, 0, 1> CAP_REQ_OVER;                        // Horizontal request end (When this bit set 1, the final capture request of one line is in the horizontal blank rising edge, set 0 capture request will free run)
    typedef UReg<0x04, 0x22, 1, 1> CAP_STATUS_SEL;                      // Capture FIFO half status select (When set to 1, request generated when capture FIFO is half; When set to 0, request generated when capture FIFO is delm’s value)
    typedef UReg<0x04, 0x22, 2, 1> CAP_LAST_POP_CTL;                    // Capture POP data control (When set to 1, horizontal or vertical load start address will check if there is pop; When set to 0, horizontal or vertical load start address will not check)
    typedef UReg<0x04, 0x22, 3, 1> CAP_REQ_FREEZ;                       // Capture Request Freeze (When set to 1, capture FIFO will pause the FIFO write and read; When set to 0, capture FIFO will operate normally)
    // 22_4:4   -   reserved
    typedef UReg<0x04, 0x23, 0, 8> CAP_FF_STATUS;                       // Capture FIFO status (When cap_cntrl_[17] set 1’b1, this register will be valid, this value will less than 64)
    typedef UReg<0x04, 0x24, 0, 21> CAP_SAFE_GUARD_A;                   // Safe Guard Address For Buffer A (Safe guard address A [7:0], Mapping to 32bits width data bus field)
    // 26_5:3   -   reserved
    typedef UReg<0x04, 0x27, 0, 21> CAP_SAFE_GUARD_B;                   // Safe Guard Address For Buffer B (Safe guard address B [7:0]; Mapping to 32bits width data bus field)
    // 29_5:3   -   reserved
    typedef UReg<0x04, 0x2b, 0, 1> PB_CUT_REFRESH;                      // Disable refresh request generation (When set to 1, disable refresh request generation; When set to 0, enable refresh request generation)
    typedef UReg<0x04, 0x2b, 1, 2> PB_REQ_SEL;                          // Enable playback request mode (see: DEVELOPER_NOTES.md -> S4_28)
    typedef UReg<0x04, 0x2b, 3, 1> PB_BYPASS;                           // Enable VDS input to select playback output or de-interlace data out (When this bit is 1, select de-interlace data out to VDS; When this bit is 0, select playback output to VDS)
    typedef UReg<0x04, 0x2b, 4, 1> PB_DB_FIELD_EN;                      // Enable double field display (When set to 1, enable double field display; When set to 0, disable double field display)
    typedef UReg<0x04, 0x2b, 5, 1> PB_DB_BUFFER_EN;                     // Enable double buffer (When set to 1, enable double field display; When set to 0, disable double field display)
    typedef UReg<0x04, 0x2b, 6, 1> PB_2FRAME_EXCHG;                     // Exchange playback two frames output data (When set to 1, exchange playback current frame with past frame and output; When set to 0, don’t exchange)
    typedef UReg<0x04, 0x2b, 7, 1> PB_ENABLE;                           // Enable Playback (When it’s set 1, play back will be on work, or will not work)
    typedef UReg<0x04, 0x2c, 0, 8> PB_MAST_FLAG_REG;                    // Master line flag (Playback FIFO policy master value: This field will define FIFO high request timing)
    // 2C_6:2   -   reserved
    typedef UReg<0x04, 0x2d, 0, 8> PB_GENERAL_FLAG_REG;                 // General line flag (Playback FIFO policy general value: This field will define FIFO low request timing)
    // 2D_6:2   -   reserved
    typedef UReg<0x04, 0x2E, 0, 1> PB_RBUF_INV;                         // When rate convert from up to down, capture FIFO will refer to the play back buffer status, this bit is invert play back buffer status
    typedef UReg<0x04, 0x2E, 1, 1> PB_RBUF_SEL;                         // When rate convert from up to down, capture FIFO will refer to the play back buffer status, this bit will be set to 1. Otherwise, it will be set to 0.
    // 2E_2:5   -   reserved
    typedef UReg<0x04, 0x2E, 7, 1> PB_DOUBLE_REFRESH_EN;                // Refresh Double (When set to 1, refresh request will at the rising and falling edge of hbout. When set to 0, refresh will be only at the rising edge of hbout)
    typedef UReg<0x04, 0x2F, 0, 4> PB_TST_REG;                          // PlayBack Test Logic (To select playback test bus, total 8 groups can be selected.)
    // 2F_4:4   -   reserved
    typedef UReg<0x04, 0x30, 0, 4> PB_CAP_NOISE_CMD;                    // Capture Noise Reduction Command:
                                                                        // 0: disable noise reduce function
                                                                        // 1: turn on PAL mode 2 (50hz to 50hz) and storage in memory 5 frames
                                                                        // 2: turn on PAL mode 3
                                                                        // 5: turn on NTSC mode 2 and storage memory 3 frames
                                                                        // 6: turn on NTSC mode 3
                                                                        // 9: turn on PAL mode 2 (50hz to 50hz, 50hz to 60hz, 50hz to 100hz) and storage memory 6 frames.
                                                                        // D: turn on NTSC mode 2 (60hz to 60hz, 60hz to 120hz) and storage memory 4 frames
                                                                        // Note: in 50 to 100hz and 60 to 120,we must turn on [4] = 1 In playback
    // 30_4:4   -   reserved
    typedef UReg<0x04, 0x31, 0, 21> PB_CAP_BUF_STA_ADDR_A;              // Capture and Play Back Buffer A START ADDRESS (Mapping to 32bits width data bus field)
    // 33_5:3   -   reserved
    typedef UReg<0x04, 0x34, 0, 21> PB_CAP_BUF_STA_ADDR_B;              // Buffer B START address (When in double buffer mode, this is defined as capture and playback buffer B start address. Mapping to 32bits width data bus field)

    typedef UReg<0x04, 0x37, 0, 10> PB_CAP_OFFSET;                      // Capture and Play Back Offset (Offset [7:0] will determine next line start address, Mapping to 64bits width data bus field)
    // 38_2:6   -   reserved
    typedef UReg<0x04, 0x39, 0, 10> PB_FETCH_NUM;                       // Fetch number [7:0] will determine to fetch the number of pixels from memory, Mapping to 64bits width data bus field.
    // 3A_2:6   -   reserved
    typedef UReg<0x04, 0x3B, 0, 21> PB_CAP_BUF_STA_ADDR_C;              // Capture and playback Buffer C Start Address (When in noise reduction mode, this is defined as capture and playback buffer C start address. Mapping to 32 bits width data bus field)
    // 3D_5:3   -   reserved
    typedef UReg<0x04, 0x3E, 0, 21> PB_CAP_BUF_STA_ADDR_D;              // Capture and Play Back Buffer D Start Address (When in noise reduction mode, this is defined as capture and playback buffer D start address. Mapping to 32 bits width data bus field)
    // 40_5:3   -   reserved

    //
    // Video Processor Registers - Write & Read FIFO registers
    //
    // 41 - WRITE FIFO Test logic control
    typedef UReg<0x04, 0x42, 0, 1> WFF_ENABLE;                          // Enable write FIFO (When it’s set 1, write FIFO will be turn on. When it’s set 0, write FIFO will be turn off)
    typedef UReg<0x04, 0x42, 1, 1> WFF_FF_HALF_REQ;                     // Request generated when FIFO half (When set to 1, request generated when FIFO half; When set to 0, request generate when FIFO write pointer is 1)
    typedef UReg<0x04, 0x42, 2, 1> WFF_FF_STA_INV;                      // Write FIFO status invert (When set to 1, write FIFO status invert; When set to 0, write FIFO status don’t change)
    typedef UReg<0x04, 0x42, 3, 1> WFF_SAFE_GUARD;                      // Enable write FIFO safe guard (When set to 1, enable write FIFO safe guard. When set to 0, disable write FIFO safe guard.)
    typedef UReg<0x04, 0x42, 4, 1> WFF_VRST_FF_RST;                     // Enable input V-sync reset FIFO (When set to 1, enable feedback v-sync reset FIFO. When set to 0, disable feedback v-sync reset FIFO)
    typedef UReg<0x04, 0x42, 5, 1> WFF_ADR_ADD_2;                       // WRITE FIFO Address count select: (When it’s set to 1, address added by 2 per pixel. When it’s set to 0, address added by 1 per pixel)
    typedef UReg<0x04, 0x42, 6, 1> WFF_REQ_OVER;                        // WRITE FIFO Horizontal Request End (When this bit set 1, the final write FIFO request of one line is in the horizontal blank rising edge, set 0 write FIFO request will free run)
    typedef UReg<0x04, 0x42, 7, 1> WFF_FF_STATUS_SEL;                   // Write fifo half status select (When set to 1, request generated when FIFO is half; When set to 0, request generated when c FIFO is delm’s value)
    typedef UReg<0x04, 0x43, 0, 8> WFF_FF_STATUS;                       // Write FIFO status (When wff_cntrl_[15] set 1’b1, this register will be valid, this value will less than 64)
    typedef UReg<0x04, 0x44, 0, 21> WFF_SAFE_GUARD_A;                   // Write FIFO Buffer A Safe Guard Address (Safe guard address buffer A [7:0], Mapping to 32bits width data bus field)
    // 46_5:3  -   reserved
    typedef UReg<0x04, 0x47, 0, 21> WFF_SAFE_GUARD_B;                   // Write FIFO Buffer B Safe Guard Address (Safe guard address buffer B [7:0], Mapping to 32bits width data bus field)
    // 49_5:3   -   reserved
    typedef UReg<0x04, 0x4a, 0, 1> WFF_YUV_DEINTERLACE;                 // WRITE FIFO YUV DE-INTERLACE (When set 1, write FIFO will write one field YUV, set 0, will write one frame Y)
    // 4A_1:3   -   reserved
    typedef UReg<0x04, 0x4a, 4, 1> WFF_LINE_FLIP;                       // WRITE FIFO LINE INVERT (When set 1, line id will be inverted; When set 0, line id will be normal)
    // 4A_5:2   -   reserved
    typedef UReg<0x04, 0x4a, 7, 1> WFF_LAST_POP_CTL;                    // When set to 1, horizontal or vertical load start address will check if there is pop When set to 0, horizontal or vertical load start address will not check.

    typedef UReg<0x04, 0x4b, 0, 3> WFF_HB_DELAY;                        // Write FIFO H-Timing Programmable Delay:
    // 4B_3:1   -   reserved
    typedef UReg<0x04, 0x4b, 4, 3> WFF_VB_DELAY;                        // Write FIFO V-Timing Programmable Delay
    // 4B_7:1   -   reserved
    typedef UReg<0x04, 0x4d, 0, 4> RFF_NEW_PAGE;                        // Read buffer page select from 1 to 16 (see: DEVELOPER_NOTES.md -> S4_4D_0)
    typedef UReg<0x04, 0x4d, 4, 1> RFF_ADR_ADD_2;                       // Enable read FIFO address add by 2: Default 0 for added by 1 (When set 1, read FIFO address will count by 2, When set 0, read FIFO address will count by 1)
    typedef UReg<0x04, 0x4d, 5, 2> RFF_REQ_SEL;                         // Enable read FIFO request mode (see: DEVELOPER_NOTES.md -> S4_4D_5)
    typedef UReg<0x04, 0x4d, 7, 1> RFF_ENABLE;                          // Enable Read FIFO (When set 1, read FIFO will be turned on; When set 0, read FIFO will be turned off)
    typedef UReg<0x04, 0x4e, 0, 6> RFF_MASTER_FLAG;                     // Master line flag (Read FIFO policy master value: This field will define FIFO high request timing)
    // 4E_6:2   -   reserved
    typedef UReg<0x04, 0x4F, 0, 6> RFF_GENERAL_FLAG;                    // General line flag (Read FIFO policy master value: This field will define FIFO low request timing)
    // 4F_6:2   -   reserved
    typedef UReg<0x04, 0x50, 0, 3> RFF_TST_REG;                         // General Test Logic (Read FIFO test bus select)
    // 50_4:1   -   reserved
    typedef UReg<0x04, 0x50, 5, 1> RFF_LINE_FLIP;                       // Line ID Invert (When set 1, line ID will be inverted; When set 0, line ID will be normal)
    typedef UReg<0x04, 0x50, 6, 1> RFF_YUV_DEINTERLACE;                 // Read FIFO YUV De-interlace:
                                                                        // When set 1, Read FIFO will read Frame 2 YUV data in line = 1, line =0, read Frame 1 YUV data.
                                                                        // When set 0, Read FIFO will read Frame 2 Y data in line = 1, line =0 , read Frame 1 Y data
    typedef UReg<0x04, 0x50, 7, 1> RFF_LREQ_CUT;                        // Read fifo low request cut enable (Cut the read FIFO low request, only output high request to memory)
    typedef UReg<0x04, 0x51, 0, 21> RFF_WFF_STA_ADDR_A;                 // Read FIFO AND Write FIFO START Address buffer A (Mapping to 32bits width data bus field.)
    // 53_5:3   -   reserved
    typedef UReg<0x04, 0x54, 0, 21> RFF_WFF_STA_ADDR_B;                 // Read FIFO AND Write FIFO START Address Buffer B (Mapping to 32bits width data bus field)
    // 56_5:3   -   reserved
    typedef UReg<0x04, 0x57, 0, 10> RFF_WFF_OFFSET;                     // Read FIFO and Write FIFO offset (Offset will determine next horizontal line start address, Mapping to 64bits width data bus field.)
    // 58_2:6   -   reserved
    typedef UReg<0x04, 0x59, 0, 10> RFF_FETCH_NUM;                      // Fetch number (READ FIFO USE ONLY) (This will determine to fetch the number of pixels from memory each horizontal line. Mapping to 64bits width data bus field.)
    // 5A_2:6   -   reserved
    // 5B_0:7   -   reserved
    typedef UReg<0x04, 0x5B, 7, 1> MEM_FF_TOP_FF_SEL;                   // All FIFO Status Output Enable (When set 1, all FIFO status output, can read FIFO status through test bus; When set 0, not FIFO status output.)

    //
    // OSD Registers    (all registers R/W)
    //
    typedef UReg<0x00, 0x90, 0, 1> OSD_SW_RESET;                        // Software reset for module , active high (When this bit is 1, it reset osd_top module)
    typedef UReg<0x00, 0x90, 1, 3> OSD_HORIZONTAL_ZOOM;                 // Osd horizontal zoom select (see: DEVELOPER_NOTES.md -> S0_90_1)
    typedef UReg<0x00, 0x90, 4, 2> OSD_VERTICAL_ZOOM;                   // Osd vertical zoom select (see: DEVELOPER_NOTES.md -> S0_90_4)
    typedef UReg<0x00, 0x90, 6, 1> OSD_DISP_EN;                         // Osd display enable, active high (When this bit is 1, osd can display on screen)
    typedef UReg<0x00, 0x90, 7, 1> OSD_MENU_EN;                         // Osd menu display enable, active high (When this bit is 1, osd state will jump to menu display state)
    typedef UReg<0x00, 0x91, 0, 4> OSD_MENU_ICON_SEL;                   // Osd menu icons select (see: DEVELOPER_NOTES.md -> S0_91_0)
    typedef UReg<0x00, 0x91, 4, 4> OSD_MENU_MOD_SEL;                    // Osd icons modification select (see: DEVELOPER_NOTES.md -> S0_90_4)
    typedef UReg<0x00, 0x92, 0, 3> OSD_MENU_BAR_FONT_FORCOR;            // Menu font or bar foreground color (For bar and menu will not display on screen at the same time, so they are shared)
    typedef UReg<0x00, 0x92, 3, 3> OSD_MENU_BAR_FONT_BGCOR;             // Menu font or bar background color (For bar and menu will not display on screen at the same time, so they are shared)
    typedef UReg<0x00, 0x92, 6, 3> OSD_MENU_BAR_BORD_COR;               // Menu or bar border color (It is the low 2 bits of menu or bar border color, for bar and menu will not display on screen at the same time, so they are shared)
    typedef UReg<0x00, 0x93, 1, 3> OSD_MENU_SEL_FORCOR;                 // Selected icon or bar’s icon foreground color
    typedef UReg<0x00, 0x93, 4, 3> OSD_MENU_SEL_BGCOR;                  // Selected icon or bar’s icon background color
    typedef UReg<0x00, 0x93, 7, 1> OSD_COMMAND_FINISH;                  // Command finished status:
                                                                        // WHEN THIS BIT IS 1, IT MEANS CPU HAS FINISHED COMMAND AND HARDWARE CAN EXECUTE THE COMMAND,
                                                                        // ELSE HARDWARE WILL DO LAST OPERATION. IN ORDER TO AVOID TEARING, WHEN YOU WANT TO ACCESS OSD,
                                                                        // PULL THIS BIT DOWN FIRST AND PULL UP THIS BIT WHEN YOU FINISH PROGRAMMING OSD RESPONDING REGISTERS.
    typedef UReg<0x00, 0x94, 0, 1> OSD_MENU_DISP_STYLE;                 // Menu display in row or column mode (When 1, osd menu displays in row style, else in column style)
    typedef UReg<0x00, 0x94, 2, 1> OSD_YCBCR_RGB_FORMAT;                // YCbCr or RGB output (Osd display in YCbCr or RGB format, when set to 1, display in YCbCr mode)
    typedef UReg<0x00, 0x94, 3, 1> OSD_INT_NG_LAT;                      // V2clk latch osd data with negative enable (When set to 1, V2CLK clock can latch osd data with negative edge)
    typedef UReg<0x00, 0x94, 4, 4> OSD_TEST_SEL;                        // Test logic output select:
                                                                        // TEST_SEL[0], test logic output enable, when set to 1, test logic can output.
                                                                        // TEST_SEL[3:1] select 8 test logics to test bus.
    typedef UReg<0x00, 0x95, 0, 8> OSD_MENU_HORI_START;                 // Menu or bar horizontal start address (The real address is { MENU_BAR_HORZ_START [7:0], 3’h0})
    typedef UReg<0x00, 0x96, 0, 8> OSD_MENU_VER_START;                  // Menu or bar vertical start address (The real address is { MENU_BAR_VIRT_START [7:0], 3’h0})
    typedef UReg<0x00, 0x97, 0, 8> OSD_BAR_LENGTH;                      // Bar display total length (Bar display on screen’s total length, when horizontal zoom is 0.)
    typedef UReg<0x00, 0x98, 0, 8> OSD_BAR_FOREGROUND_VALUE;            // Bar foreground color value (The value of this register indicates the real value of icon, such as brightness’s value is 8’hf0, then this register is also programmed to 8’hf0)

    //
    // ADC, SP Registers    (all registers R/W)
    //
    typedef UReg<0x05, 0x00, 0, 8> ADC_5_00;                            // ADC CLK CONTROL 00
        typedef UReg<0x05, 0x00, 0, 2> ADC_CLK_PA;                          // Clock selection for PA_ADC:
                                                                            // When = 00, PA_ADC input clock is from PLLAD’s CLKO2
                                                                            // When = 01, PA_ADC input clock is from PCLKIN
                                                                            // When = 10, PA_ADC input clock is from V4CLK
                                                                            // When = 11, reserved
        typedef UReg<0x05, 0x00, 2, 1> ADC_CLK_PLLAD;                       // Clock selection for PLLAD (When = 0, PLLAD input clock is from sync processor When = 1, PLLAD input clock is from OSC)
        typedef UReg<0x05, 0x00, 3, 1> ADC_CLK_ICLK2X;                      // ICLK2X control (When = 0, ICLK2X = ADC output clock When = 1, ICLK2X = ADC output clock / 2)
        typedef UReg<0x05, 0x00, 4, 1> ADC_CLK_ICLK1X;                      // ICLK1X control (When = 0, ICLK1X = ICLK2X When = 1, ICLK1X = ICLK2X /2)
        // 00_5:3   -   reserved
    typedef UReg<0x05, 0x02, 0, 1> ADC_SOGEN;                           // ADC SOG enable (When = 0, ADC disable SOG mode When = 1, ADC enable SOG mode)
    typedef UReg<0x05, 0x02, 1, 5> ADC_SOGCTRL;                         // SOG control signal
    typedef UReg<0x05, 0x02, 6, 2> ADC_INPUT_SEL;                       // ADC input selection:
                                                                        // When = 00, R0/G0/B0/SOG0 as input
                                                                        // When = 01, R1/G1/B1/SOG1 as input
                                                                        // When = 10, R2/G2/B2 as input
                                                                        // When = 11, reserved
    typedef UReg<0x05, 0x03, 0, 8> ADC_5_03;                            // ADC CONTROL 01
        typedef UReg<0x05, 0x03, 0, 1> ADC_POWDZ;                           // ADC power down control (When = 0, ADC in power down mode; When = 1, ADC work normally)
        typedef UReg<0x05, 0x03, 1, 1> ADC_RYSEL_R;                         // Clamp to ground or midscale for R ADC (When = 0, clamp to GND When = 1, clamp to midscale)
        typedef UReg<0x05, 0x03, 2, 1> ADC_RYSEL_G;                         // Clamp to ground or midscale for G ADC (When = 0, clamp to GND When = 1, clamp to midscale)
        typedef UReg<0x05, 0x03, 3, 1> ADC_RYSEL_B;                         // Clamp to ground or midscale for B ADC (When = 0, clamp to GND When = 1, clamp to midscale)
        typedef UReg<0x05, 0x03, 4, 2> ADC_FLTR;                            // ADC internal filter control (When = 00, 150MHz; When = 01, 110MHz; When = 10, 70MHz; When = 11, 40MHz)
        // 03_6:2    -  reserved
    typedef UReg<0x05, 0x04, 0, 8> ADC_TEST_04;                         // ADC CONTROL 02
        typedef UReg<0x05, 0x04, 0, 2> ADC_TR_RSEL;                         // REF test resistor selection
        typedef UReg<0x05, 0x04, 2, 3> ADC_TR_ISEL;                         // REF test currents selection
        // 04_5:3   -   reserved
    typedef UReg<0x05, 0x05, 0, 8> ADC_TA_05_CTRL;                      // ADC CONTROL 03
        typedef UReg<0x05, 0x05, 0, 1> ADC_TA_05_EN;                            // ADC test enable (When = 0, ADC work normally; When = 1, ADC is in test mode)
        typedef UReg<0x05, 0x05, 1, 4> ADC_TA_CTRL;                             // ADC test bus control bit
        // 05_5:3   -   reserved
    typedef UReg<0x05, 0x06, 0, 7> ADC_ROFCTRL;                         // Offset control for R channel of ADC
    // 06_7:1   -   reserved
    typedef UReg<0x05, 0x07, 0, 7> ADC_GOFCTRL;                         // Offset control for G channel of ADC
    // 07_7:1   -   reserved
    typedef UReg<0x05, 0x08, 0, 7> ADC_BOFCTRL;                         // Offset control for B channel of ADC
    // 08_7:1   -   reserved
    typedef UReg<0x05, 0x09, 0, 8> ADC_RGCTRL;                          // Gain control for R channel of ADC
    typedef UReg<0x05, 0x0A, 0, 8> ADC_GGCTRL;                          // Gain control for G channel of ADC
    typedef UReg<0x05, 0x0B, 0, 8> ADC_BGCTRL;                          // Gain control for B channel of ADC
    typedef UReg<0x05, 0x0C, 0, 8> ADC_TEST_0C;                         // ADC CONTROL 10
        typedef UReg<0x05, 0x0C, 0, 1> ADC_CKBS;                            // ADC output clock invert control (When = 0, default; When = 1, ADC output clock will be invert)
        typedef UReg<0x05, 0x0C, 1, 4> ADC_TEST;                            // For ADC test reserved
        // 0C_5:3   -   reserved
    typedef UReg<0x05, 0x0E, 0, 1> ADC_AUTO_OFST_EN;                    // Auto offset adjustment enable (When = 0, auto offset adjustment disable; When = 1, auto offset adjustment enable)
    typedef UReg<0x05, 0x0E, 1, 1> ADC_AUTO_OFST_PRD;                   // Offset adjustment by frame (When = 0, offset adjustment by frame; When = 1, offset adjustment by line)
    typedef UReg<0x05, 0x0E, 2, 2> ADC_AUTO_OFST_DELAY;                 // Horizontal sample delay control:
                                                                        // When = 00, offset adjustment horizontal sample delay 1 pipe
                                                                        // When = 01, offset adjustment horizontal sample delay 2 pipe
                                                                        // When = 10, offset adjustment horizontal sample delay 3 pipe
                                                                        // When = 11, offset adjustment horizontal sample delay 4 pipe
    typedef UReg<0x05, 0x0E, 4, 2> ADC_AUTO_OFST_STEP;                  // Offset adjustment step control:
                                                                        // When = 00, offset adjustment by absolute difference
                                                                        // When = 01, offset adjustment by 1
                                                                        // When = 10, offset adjustment by 2
                                                                        // When = 11, offset adjustment by 3
    // 0E_6:1   -   reserved
    typedef UReg<0x05, 0x0E, 7, 1> ADC_AUTO_OFST_TEST;                  // Auto offset adjustment test control
    typedef UReg<0x05, 0x0F, 0, 4> ADC_AUTO_OFST_U_RANGE;               // U channel offset detection range (Define U channel offset detection range 0~15)
    typedef UReg<0x05, 0x0F, 4, 4> ADC_AUTO_OFST_V_RANGE;               // V channel offset detection range (Define V channel offset detection range 0~15)
    typedef UReg<0x05, 0x11, 0, 8> PLLAD_CONTROL_00_5x11;               // PLLAD CONTROL 00
        typedef UReg<0x05, 0x11, 0, 1> PLLAD_VCORST;                        // Initial VCO control voltage
        typedef UReg<0x05, 0x11, 1, 1> PLLAD_LEN;                           // Enable signal for clock
        typedef UReg<0x05, 0x11, 2, 1> PLLAD_TEST;                          // Test clock selection
        typedef UReg<0x05, 0x11, 3, 1> PLLAD_TS;                            // Test clock selection and HSL clock selection
        typedef UReg<0x05, 0x11, 4, 1> PLLAD_PDZ;                           // PDZ (When = 0, PLLAD is power down mode; When = 1, PLLAD work normally)
        typedef UReg<0x05, 0x11, 5, 1> PLLAD_FS;                            // FS, VCO gain selection (When = 0, default; When = 1, high gain selected)
        typedef UReg<0x05, 0x11, 6, 1> PLLAD_BPS;                           // BPS (When = 0, default; When = 1, bypass input clock to CKO1 and CKO2)
        typedef UReg<0x05, 0x11, 7, 1> PLLAD_LAT;                           // Latch control for PLLAD control - This bit’s rising edge is used to trigger PLLAD control bit: ND, MD, KS, CKOS, ICP
    typedef UReg<0x05, 0x12, 0, 12> PLLAD_MD;                           // PLLAD feedback divider control
    // 13_4:4   -   reserved
    typedef UReg<0x05, 0x14, 0, 12> PLLAD_ND;                           // PLLAD input divider control
    // 15_4:4   -   reserved
    typedef UReg<0x05, 0x16, 0, 8> PLLAD_5_16;                          // PLLAD CONTROL 05
        typedef UReg<0x05, 0x16, 0, 2> PLLAD_R;                             // Skew control for testing
        typedef UReg<0x05, 0x16, 2, 2> PLLAD_S;                             // Skew control for testing
        typedef UReg<0x05, 0x16, 4, 2> PLLAD_KS;                            // VCO post divider control, it is determined by CKO frequency:
                                                                            // When = 00, divide by 1 (162MHz~80MHz)
                                                                            // When = 01, divide by 2 (80MHz~40MHz)
                                                                            // When = 10, divide by 4 (40MHz~20MHz)
                                                                            // When = 11, divide by 8 (20MHz~min MHz)
        typedef UReg<0x05, 0x16, 6, 2> PLLAD_CKOS;                          // PLLAD CKO2 output clock selection (see: DEVELOPER_NOTES.md -> S5_16)
    typedef UReg<0x05, 0x17, 0, 3> PLLAD_ICP;                           // ICP - Charge pump current selection:
                                                                        // When = 000, Icp = 50uA
                                                                        // When = 001, Icp = 100uA
                                                                        // When = 010, Icp = 150uA
                                                                        // When = 011, Icp = 250uA
                                                                        // When = 100, Icp = 350uA
                                                                        // When = 101, Icp = 500uA
                                                                        // When = 110, Icp = 750uA
                                                                        // When = 111, Icp = 1mA
    // 17_3:5   -   reserved
    typedef UReg<0x05, 0x18, 0, 1> PA_ADC_BYPSZ;                        // BYPSZ, PA for ADC bypass control (When = 0, PA_ADC is bypass When = 1, PA_ADC work normally)
    typedef UReg<0x05, 0x18, 1, 5> PA_ADC_S;                            // PA_ADC phase control
    typedef UReg<0x05, 0x18, 6, 1> PA_ADC_LOCKOFF;                      // LOCKOFF (When = 0, default; When = 1, PA_ADC lock circuit disable)
    typedef UReg<0x05, 0x18, 7, 1> PA_ADC_LAT;                          // PA_ADC latch signal (This bit’s rising edge is used to trigger PA_ADC_CNTRL)
    typedef UReg<0x05, 0x19, 0, 1> PA_SP_BYPSZ;                         // BYPSZ, PA for PLLAD bypass control (When = 0, PA_PLLAD is bypass; When = 1, PA_PLLAD work normally)
    typedef UReg<0x05, 0x19, 1, 5> PA_SP_S;                             // PA_PLLAD phase control
    typedef UReg<0x05, 0x19, 6, 1> PA_SP_LOCKOFF;                       // LOCKOFF (When = 0, default; When = 1, PA_PLLAD lock circuit disable)
    typedef UReg<0x05, 0x19, 7, 1> PA_SP_LAT;                           // PA_PLLAD latch signal (This bit’s rising edge is used to trigger PA_PLLAD_CNTRL)
    // 20 - 1D - not described
    // 1E_0:7   -   reserved
    typedef UReg<0x05, 0x1E, 7, 1> DEC_WEN_MODE;                        // Write enable mode enable (When this bit is 1, then decimator will drop data by write enable signal generated by horizontal sync, else write enable is not used.)
    typedef UReg<0x05, 0x1F, 0, 8> DEC_REG_01;                          // DEC_REG_01
        typedef UReg<0x05, 0x1F, 0, 1> DEC1_BYPS;                           // The 4x to 2x decimator bypass enable (When 1, the 4x to 2x decimator bypass.)
        typedef UReg<0x05, 0x1F, 1, 1> DEC2_BYPS;                           // The 2x to 1x decimator bypass enable (When 1, the 2x to 1x decimator hypass)
        typedef UReg<0x05, 0x1F, 2, 1> DEC_MATRIX_BYPS;                     // Color space convert bypass enable (When set to 1, color space convert module bypass.)
        typedef UReg<0x05, 0x1F, 3, 1> DEC_TEST_ENABLE;                 // Test logic output select (test logic output enable, when set to 1, test logic can output.)
        typedef UReg<0x05, 0x1F, 4, 3> DEC_TEST_SEL;                    // Test logic output select (select 8 test logics to test bus)
        typedef UReg<0x05, 0x1F, 7, 1> DEC_IDREG_EN;                    // Negative clock edge latch input hsync and vsync enable (When set to 1, decimator 4x clock will latch HSYNC and VSYNC with falling edge)

    //
    // Sync Proc (all registers R/W)
    //
    typedef UReg<0x05, 0x20, 0, 1> SP_SOG_SRC_SEL;                      // SOG signal source select (0: from ADC; 1: select hs as sog source)
    typedef UReg<0x05, 0x20, 1, 1> SP_SOG_P_ATO;                        // SOG auto correct polarity
    typedef UReg<0x05, 0x20, 2, 1> SP_SOG_P_INV;                        // Invert sog.
    typedef UReg<0x05, 0x20, 3, 1> SP_EXT_SYNC_SEL;                     // Ext 2 set Hs_Hs select
    typedef UReg<0x05, 0x20, 4, 1> SP_JITTER_SYNC;                      // Sync using both rising and falling trigger (Use falling and rising edge to sync input Hsync)
    // 20_5:3    -  reserved
    typedef UReg<0x05, 0x21, 0, 16> SP_SYNC_TGL_THD;                    // h active detect control (Sync toggle times threshold)
    // 23_0:8   -   reserved
    typedef UReg<0x05, 0x24, 0, 12> SP_T_DLT_REG;                       // H active detect control (H total width different threshold)
    // 25:4:4   -   reserved
    typedef UReg<0x05, 0x26, 0, 12> SP_SYNC_PD_THD;                     // H active detect control (H sync pulse width threshold)
    // 27_4:4   -   reserved
    typedef UReg<0x05, 0x2A, 0, 8> SP_PRD_EQ_THD;                       // H active detect control (How many continue legal line as valid)
    // 2B - 2C - not described
    typedef UReg<0x05, 0x2D, 0, 8> SP_VSYNC_TGL_THD;                    // V active detect control (V sync toggle times threshold)
    typedef UReg<0x05, 0x2E, 0, 8> SP_SYNC_WIDTH_DTHD;                  // V active detect control (V sync pulse width threshod)
    typedef UReg<0x05, 0x2F, 0, 8> SP_V_PRD_EQ_THD;                     // V active detect control (How many continue legal v sync as valid)
    typedef UReg<0x05, 0x31, 0, 8> SP_VT_DLT_REG;                       // V active detect control (V total different threshold)
    typedef UReg<0x05, 0x32, 0, 1> SP_VSIN_INV_REG;                     // V active detect control (Input v sync invert to v active detect)
    // 32_1:7   -   reserved
    typedef UReg<0x05, 0x33, 0, 8> SP_H_TIMER_VAL;                      // Timer value control (H timer value for h detect)
    typedef UReg<0x05, 0x34, 0, 8> SP_V_TIMER_VAL;                      // Timer value control (V timer for V detect)
    typedef UReg<0x05, 0x35, 0, 12> SP_DLT_REG;                         // Sync separation control (MSB for sync pulse width difference compare value)
    // 36_4:4   -   reserved
    typedef UReg<0x05, 0x37, 0, 8> SP_H_PULSE_IGNOR;                    // Sync separation control (H pulse less than this value will be ignore this counter is start when sync large different)
    typedef UReg<0x05, 0x38, 0, 8> SP_PRE_COAST;                        // Sync separation control (Set the coast will valid before vertical sync (line number))
    typedef UReg<0x05, 0x39, 0, 8> SP_POST_COAST;                       // Sync separation control (When line cnt reach this value coast goes down)
    typedef UReg<0x05, 0x3A, 0, 8> SP_H_TOTAL_EQ_THD;                   // Sync separation control (How many regular line regard it as legal)
    typedef UReg<0x05, 0x3B, 0, 3> SP_SDCS_VSST_REG_H;                  // Sync separation control (High bit of SD vs. start position)
    // 3B_3:1   -   reserved
    typedef UReg<0x05, 0x3B, 4, 3> SP_SDCS_VSSP_REG_H;                  // Sync separation control (High bit of SD vs. stop position)
    // 3B_7:1   -   reserved
    typedef UReg<0x05, 0x3E, 0, 8> SP_CS_0x3E;                          // SYNC_PROC 23
        typedef UReg<0x05, 0x3E, 0, 1> SP_CS_P_SWAP;                        // Sync separation control (cs_p_swap cs edge reference select default rising edge)
        typedef UReg<0x05, 0x3E, 1, 1> SP_HD_MODE;                          // hd_mode 1: HD mode 0: SD mode
        typedef UReg<0x05, 0x3E, 2, 1> SP_H_COAST;                          // h_coast 1: with sub coast out
        typedef UReg<0x05, 0x3E, 3, 1> SP_CS_INV_REG;                       // cs_inv_reg cs input invert
        typedef UReg<0x05, 0x3E, 4, 1> SP_H_PROTECT;                        // H count overflow protect
        typedef UReg<0x05, 0x3E, 5, 1> SP_DIS_SUB_COAST;                    // Disable sub coast
        // 3E_6:2   -   reserved
    typedef UReg<0x05, 0x3F, 0, 8> SP_SDCS_VSST_REG_L;                  // Sync separation control (SD vs. start position)
    typedef UReg<0x05, 0x40, 0, 8> SP_SDCS_VSSP_REG_L;                  // Sync separation control (SD vs. stop position)
    typedef UReg<0x05, 0x41, 0, 12> SP_CS_CLP_ST;                       // Sync separation control (SOG clamp start position MSB...LSB)
    // 42_4:4   -   reserved
    typedef UReg<0x05, 0x43, 0, 12> SP_CS_CLP_SP;                       // Sync separation control (SOG clamp stop position MSB...LSB)
    // 42_4:4   -   reserved
    typedef UReg<0x05, 0x45, 0, 12> SP_CS_HS_ST;                        // Sync separation control (If the horizontal period number is equal to the defined value, in XGA modes, It’s XGA 75Hz mode)
    // 46_4:4   -   reserved
    typedef UReg<0x05, 0x47, 0, 12> SP_CS_HS_SP;                        // Sync separation control (SOG hs stop position)
    // 48_4:4   -   reserved
    typedef UReg<0x05, 0x49, 0, 12> SP_RT_HS_ST;                        // Retiming control (Retiming hs start position)
    // 4A_4:4   -   reserved
    typedef UReg<0x05, 0x4B, 0, 12> SP_RT_HS_SP;                        // Retiming control (Retiming hs stop postion)
    // 4C_4:4   -   reserved
    typedef UReg<0x05, 0x4D, 0, 12> SP_H_CST_ST;                        // Retiming control (H coast start position (total-this value))
    // 4E_4:4   -   reserved
    typedef UReg<0x05, 0x4F, 0, 12> SP_H_CST_SP;                        // Retiming control (H coast stop position)
    // 50_4:4   -   reserved
    typedef UReg<0x05, 0x51, 0, 12> SP_RT_VS_ST;                        // Retiming control (Retiming vs start position)
    // 52_4:4   -   reserved
    typedef UReg<0x05, 0x53, 0, 12> SP_RT_VS_SP;                        // Retiming control (Retiming vs stop position)
    // 54_4:4   -   reserved
    typedef UReg<0x05, 0x55, 0, 2> SP_HS_EP_DLY_SEL;                    // Retiming control (Hs pulse delay sel for ( sync with vs ))
    typedef UReg<0x05, 0x55, 3, 1> SP_HS_INV_REG;                       // Retiming control (hs_inv_reg inver hs to retimming module
    typedef UReg<0x05, 0x55, 4, 1> SP_HS_POL_ATO;                       // Retiming control (hs auto correct in retiming module)
    typedef UReg<0x05, 0x55, 5, 1> SP_VS_INV_REG;                       // Retiming control (vs inv_reg invert hs to retiming module)
    typedef UReg<0x05, 0x55, 6, 1> SP_VS_POL_ATO;                       // Retiming control (vs auto correct in retiming module)
    typedef UReg<0x05, 0x55, 7, 1> SP_HCST_AUTO_EN;                     // Retiming control (If enable h coast will start at ( V total - hcst_st))
    typedef UReg<0x05, 0x56, 0, 8> SP_5_56;                             // SYNC_PROC 48
        typedef UReg<0x05, 0x56, 0, 1> SP_SOG_MODE;                         // Out control (1: SOG mode; 0: normal mode)
        typedef UReg<0x05, 0x56, 1, 1> SP_HS2PLL_INV_REG;                   // Out control (When =1, HS to PLL invert)
        typedef UReg<0x05, 0x56, 2, 1> SP_CLAMP_MANUAL;                     // Out control (1: clamp turn on off by control by software (default) 0: for test)
        typedef UReg<0x05, 0x56, 3, 1> SP_CLP_SRC_SEL;                      // Out control (Clamp source select - 1: pixel clock generate; 0: 27Mhz clock generate)
        typedef UReg<0x05, 0x56, 4, 1> SP_SYNC_BYPS;                        // Out control (External sync bypass to decimator)
        typedef UReg<0x05, 0x56, 5, 1> SP_HS_PROC_INV_REG;                  // Out control (HS to decimator invert)
        typedef UReg<0x05, 0x56, 6, 1> SP_VS_PROC_INV_REG;                  // Out control (VS to decimator invert)
        typedef UReg<0x05, 0x56, 7, 1> SP_CLAMP_INV_REG;                    // Out control (Clamp to ADC invert)
    typedef UReg<0x05, 0x57, 0, 8> SP_5_57;                             // SYNC_PROC 49
        typedef UReg<0x05, 0x57, 0, 1> SP_NO_CLAMP_REG;                     // Out control (Clamp always be 0)
        typedef UReg<0x05, 0x57, 1, 1> SP_COAST_INV_REG;                    // Out control (Coast invert)
        typedef UReg<0x05, 0x57, 2, 1> SP_NO_COAST_REG;                     // Out control (Coast always be REG S5_57[3])
        typedef UReg<0x05, 0x57, 3, 1> SP_COAST_VALUE_REG;                  // Out control (Coast use 1x clk generate)
        // 57_4:2   -   reserved
        typedef UReg<0x05, 0x57, 6, 1> SP_HS_LOOP_SEL;                      // Out control (Bypass PLL HS to 57 core)
        typedef UReg<0x05, 0x57, 7, 1> SP_HS_REG;                           // Out control (When sub_coast enable will select this value)
    typedef UReg<0x05, 0x58, 0, 12> SP_HT_DIFF_REG;                         // Auto clamp control (H total difference less this value as valid for auto clamp enable control)
    // 59_4:4   -   reserved
    typedef UReg<0x05, 0x5A, 0, 12> SP_VT_DIFF_REG;                         // Auto clamp control (V total difference less this value as valid for auto clamp enable control)
    // 5B_4:4   -   reserved
    typedef UReg<0x05, 0x5C, 0, 8> SP_STBLE_CNT_REG;                         // Auto clamp control (Stable indicate frame threshold for auto clamp enable control)
    // 5D - 62 - not described
    // typedef UReg<0x05, 0x60, 0, 8> ADC_UNUSED_60;
    // typedef UReg<0x05, 0x61, 0, 8> ADC_UNUSED_61;
    // typedef UReg<0x05, 0x62, 0, 8> ADC_UNUSED_62;
    typedef UReg<0x05, 0x63, 0, 8> TEST_BUS_SP_SEL;                         // SYNC_PROC 55
        typedef UReg<0x05, 0x63, 0, 1> SP_TEST_EN;                              // Test bus enable
        typedef UReg<0x05, 0x63, 1, 3> SP_TEST_MODULE;                          // test module select:
                                                                                // # 0 none
                                                                                // # 1 hs_pol_det module
                                                                                // # 2 hs_act_det module
                                                                                // # 3 vs_pol_det module
                                                                                // # 4 vs_act_det module
                                                                                // # 5 cs_sep module
                                                                                // # 6 retiming module
                                                                                // # 7 out proc module
        typedef UReg<0x05, 0x63, 4, 2> SP_TEST_SIGNAL_SEL;                      // Test signal select
        // 63_7:1   -   reserved
    //
    // there is no more registers ahead...
    //
    // typedef UReg<0x05, 0x64, 0, 8> ADC_UNUSED_64;
    // typedef UReg<0x05, 0x65, 0, 8> ADC_UNUSED_65;
    // typedef UReg<0x05, 0x66, 0, 8> ADC_UNUSED_66;
    // typedef UReg<0x05, 0x67, 0, 16> ADC_UNUSED_67; // + ADC_UNUSED_68;
    // typedef UReg<0x05, 0x69, 0, 8> ADC_UNUSED_69;

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

typedef TV5725<GBS_ADDR> GBS;

#endif
