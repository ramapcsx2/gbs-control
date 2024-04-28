#ifndef _SLOT_H_
#define _SLOT_H_
// SLOTS
#define SLOTS_TOTAL 72          // max number of slots
const char SLOTS_FILE[] PROGMEM = "/slots.bin"; // the file where to store slots metadata
const char EMPTY_SLOT_NAME[] PROGMEM = "Empty                   ";
const char EMPTY_SLOT_LINE[] PROGMEM = "                        ";

typedef struct
{
    char name[25];
    uint8_t presetID;
    uint8_t scanlines;
    uint8_t scanlinesStrength;
    uint8_t slot;
    uint8_t wantVdsLineFilter;
    uint8_t wantStepResponse;
    uint8_t wantPeaking;
} SlotMeta;

typedef struct
{
    SlotMeta slot[SLOTS_TOTAL]; // the max avaliable slots that can be encoded in a the charset[A-Za-z0-9-._~()!*:,;]
} SlotMetaArray;
#endif