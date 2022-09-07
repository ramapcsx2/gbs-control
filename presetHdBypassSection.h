const uint8_t presetHdBypassSection[] PROGMEM = {
    0x00, // s1_30
    0x80, // s1_31 // Y 84
    0x00, // s1_32 // Y offset
    0x80, // s1_33 // B/U instead of 0x1c? 78
    0x00, // s1_34 // B offset 1
    0x80, // s1_35 // R/V instead of 0x29? 80
    0x00, // s1_36 // R offset ff
    0xff, // s1_37 // HD_HSYNC_RST
    0x03, // s1_38
    0x16, // s1_39 // HD_INI_ST
    0x04, // s1_3A //
    0x88, // s1_3B // HD_HB_ST
    0x0f, // s1_3C
    0xd0, // s1_3D // HD_HB_SP
    0x00, // s1_3E
    0,    // s1_3F // HD_HS_ST
    0,    // s1_40
    0x7C, // s1_41 // HD_HS_SP
    0,    // s1_42
    0x00, // s1_43 HD_VB_ST
    0,    // s1_44
    0x14, // s1_45
    0,    // s1_46
    0x02, // s1_47 // vsync st
    0,    // s1_48
    0x07, // s1_49 // vsync sp
    0,    // s1_4A
    0,    // s1_4B
    0,    // s1_4C
    0x06, // s1_4D
    0,    // s1_4E
    0,    // s1_4F
    0,    // s1_50
    0x06, // s1_51
    0,    // s1_52
    0,    // s1_53
    0,    // s1_54
    0,    // s1_55
    0,    // s1_56
    0,    // s1_57
    0,    // s1_58
    0,    // s1_59
    0,    // s1_5A
    0,    // s1_5B
    0,    // s1_5C
    0,    // s1_5D
    0,    // s1_5E
    0,    // s1_5F
};