#pragma once
#include <stdlib.h>
#include <stdint.h>

enum WrParseResult
{
    WR_PARSE_OK = 0,
    WR_PARSE_INVALID,
    WR_PARSE_RANGE
};

// Parse string to unsigned long with validation.
// Base 0 = auto-detect (decimal, 0x hex, 0 octal).
inline WrParseResult parseUint(const char *str, unsigned long *out, unsigned long maxVal)
{
    if (!str || *str == '\0' || *str == '-') return WR_PARSE_INVALID;
    char *end;
    unsigned long val = strtoul(str, &end, 0);
    if (end == str || *end != '\0') return WR_PARSE_INVALID;
    if (val > maxVal) return WR_PARSE_RANGE;
    *out = val;
    return WR_PARSE_OK;
}
