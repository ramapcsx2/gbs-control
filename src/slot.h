#ifndef _SLOT_H_
#define _SLOT_H_

#include <LittleFS.h>
#include "options.h"
#include "wserial.h"

// SLOTS
const char slotsFile[] PROGMEM = "/slots.bin"; // the file where to store slots metadata
const char emptySlotName[] PROGMEM = "Empty                   ";

typedef struct
{
    char name[25];
    uint8_t slot;
    OutputResolution resolutionID;
    uint8_t scanlines;
    uint8_t scanlinesStrength;
    uint8_t wantVdsLineFilter;
    uint8_t wantStepResponse;
    uint8_t wantPeaking;
    uint8_t enableAutoGain;
    uint8_t enableFrameTimeLock;
    uint8_t frameTimeLockMethod;
    uint8_t deintMode;
    uint8_t wantTap6;
    uint8_t wantFullHeight;
    uint8_t matchPresetSource;
    uint8_t PalForce60;
} SlotMeta;

typedef struct
{
    SlotMeta slot[SLOTS_TOTAL];
} SlotMetaArray;

bool slotLoad(const uint8_t & slotIndex);
void slotUpdate(SlotMetaArray & slotsObject, const uint8_t & slotIndex, String * slotName = nullptr);
int8_t slotGetData(SlotMetaArray & slotsObject);
bool slotSetData(SlotMetaArray & slotsObject);
bool slotFlush();
bool slotResetFlush(const uint8_t & slotIndex);
void slotResetDefaults(SlotMetaArray & slotsObject, const uint8_t & slotIndex);

#endif                          // _SLOT_H_