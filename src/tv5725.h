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
        typedef UReg<0x00, 0x43, 5, 1> PLL_VCORST;                          // VCO control voltage reset bit (When =1, reset VCO control voltage)
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
        typedef UReg<0x00, 0x46, 0, 1> SFTRST_IF_RSTZ;                      // Input formatter reset control (When = 0, input formatter is in reset status When = 1, input formatter work normally)
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
    typedef UReg<0x03, 0x24, 0, 8> VDS_3_24;                            // convenience
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

    //
    // PIP Registers
    //
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

    //
    // Memory Controller Registers
    //
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

    //
    // Playback / Capture / Memory Registers
    //
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

    //
    // OSD Registers    (all registers R/W)
    //
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

    //
    // ADC, SP Registers (all registers R/W)
    //
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

    //
    // Sync Proc (all registers R/W)
    //
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

typedef TV5725<GBS_ADDR> GBS;

#endif
