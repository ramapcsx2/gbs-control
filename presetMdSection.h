const uint8_t presetMdSection[] PROGMEM = {
0xB6, // s1_60 H: unlock: 5, lock: 22 // 0x9c > 4, 28
0x64, // s1_61 V: unlock: 3, lock: 4 // 0x45 > 2, 5
96, // s1_62
38, // s1_63
65, // s1_64
62, // s1_65
178, // s1_66
154, // s1_67
78, // s1_68
214, // s1_69
177, // s1_6A
142, // s1_6B
124, // s1_6C
99, // s1_6D
139, // s1_6E
118, // s1_6F
112, // s1_70
98, // s1_71
133, // s1_72
105, // s1_73
83, // s1_74
72, // s1_75
93, // s1_76
148, // s1_77
178, // s1_78
70, // s1_79
198, // s1_7A
238, // s1_7B
140, // s1_7C
98, // s1_7D
118, // s1_7E
156, // s1_7F
0xff, // s1_80 // custom mode h // was 32(d)
0xff, // s1_81 // custom mode v // was 26(d)
0x01, // s1_82 // was 0x05 // result in 0_16 // 0x31 = SP timer detect used for something
0x0c, // s1_83 MD_UNSTABLE_LOCK_VALUE = 3 // was 0x10 (lock val 4)
};