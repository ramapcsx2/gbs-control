#ifndef _SLOT_H_
#define _SLOT_H_

// SLOTS
const char slotsFile[] PROGMEM = "/slots.bin"; // the file where to store slots metadata
const char emptySlotName[] PROGMEM = "Empty                   ";
const char emptySlotLine[] PROGMEM = "                        ";

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

#endif                          // _SLOT_H_