const uint8_t presetDeinterlacerSectionNew[] PROGMEM = {
0x18, // s2_0
0x3, // s2_1
0xEC, // s2_2
0x1, // s2_3
0xB, // s2_4 // threshold NOUT Nout will be high if pre-value greater than madpt_noise_threshold_nout
0xB, // s2_5 // threshold NOUT VDS
0x0, // s2_6
0x0, // s2_7
0x0, // s2_8
0xC0, // s2_9 //high noise gain (still_noise)
0x20, // s2_A //noise det on (still_noise)
0x4, // s2_B
0xC, // s2_C
0x18, // s2_D
0x7F, // s2_E
0x19, // s2_F
0x19, // s2_10
0x0, // s2_11
0x8E, // s2_12
0x0, // s2_13
0x0, // s2_14
0x0, // s2_15
0x40, // s2_16 //Madpt_vt_filter_cntrl Reg_S2_16 [6] 1 Do VT filter every line supposedly necessary for NR
0x0, // s2_17
0xD4, // s2_18 // was D6 (was motion index divide 2, "D4" is div 4)
0xA1, // s2_19
0x5, // s2_1A
0x24, // s2_1B
0x0, // s2_1C
0x0, // s2_1D
0x0, // s2_1E
0x10, // s2_1F
0x30, // s2_20
0x32, // s2_21 //Enable NOUT for still detection + less still detection
0x4, // s2_22
0xF, // s2_23
0x4, // s2_24
0x0, // s2_25
0x4C, // s2_26
0xC, // s2_27
0x0, // s2_28
0x0, // s2_29
0x0, // s2_2A
0x0, // s2_2B
0x0, // s2_2C
0x0, // s2_2D
0x0, // s2_2E
0x0, // s2_2F
0x0, // s2_30
0x0, // s2_31
0x7F, // s2_32 //
0x7F, // s2_33 //
0x11, // s2_34 // once NRD works, reduce to 0x44
0x50, // s2_35 // MADPT_DD0_SEL (bit3)
0x3, // s2_36
0xB, // s2_37
0x4, // s2_38
0x44, // s2_39
0x29, // s2_3A
0x4, // s2_3B
0x1E, // s2_3C
0x0, // s2_3D
0x0, // s2_3E
0x0, // s2_3F
};