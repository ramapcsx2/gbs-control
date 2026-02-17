#pragma once
#include <stdint.h>
#include <string.h>

static const uint8_t WR_QUEUE_SIZE = 8;

struct WrEntry
{
    char reg[12];    // named reg (empty string = raw mode)
    uint16_t val;
    uint8_t seg;     // raw mode only
    uint8_t addr;    // raw mode only
    bool isRaw;
};

inline bool wrQueueFull(uint8_t head, uint8_t tail)
{
    return ((head + 1) % WR_QUEUE_SIZE) == tail;
}

inline bool wrQueueEmpty(uint8_t head, uint8_t tail)
{
    return head == tail;
}

inline bool isKnownRegister(const char *name)
{
    static const char *const known[] = {
        "ht", "vt", "hsst", "hssp", "hbst", "hbsp", "hbstd", "hbspd",
        "vsst", "vssp", "vbst", "vbsp", "vbstd", "vbspd", "hscale", "vscale",
        "sog", "ifhbst", "ifhbsp", "ifhbinst", "ifhbinsp", "ifvbst", "ifvbsp",
        "exthbst", "exthbsp", "extvbst", "extvbsp", "hdhbst", "hdhbsp",
        "hdvbst", "hdvbsp", "pbfetch", "pboffset", "rfffetch", "rffoffset",
        "ifhsyncrst", "ifhbst2", "ifhbsp2", "iflinest", "iflinesp",
        "ifinist", "ifldst", "vdsdsp", "ygain", "yofst", "uofst", "vofst", "synclev",
        "rgain", "ggain", "bgain", "roff", "goff", "boff",
        "ucgain", "vcgain", "pklb", "pklh",
        "slstr", "slsoft", "slbrt"
    };
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        if (strcmp(name, known[i]) == 0) return true;
    }
    return false;
}
