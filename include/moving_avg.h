#pragma once
#include <stdint.h>
#include <string.h>

// Parameterized moving-average filter.
// The firmware uses a 16-sample window; the struct allows testing without static state.

#define MOVING_AVG_SIZE 16
#define MOVING_AVG_SHIFT 4 // log2(MOVING_AVG_SIZE)

struct MovingAvg
{
    uint8_t arr[MOVING_AVG_SIZE];
    uint8_t pos;
};

// Zero-initialize a MovingAvg context.
inline void movingAvgReset(MovingAvg *ctx)
{
    memset(ctx->arr, 0, MOVING_AVG_SIZE);
    ctx->pos = 0;
}

// Feed one sample, return the current average.
inline uint8_t movingAvgFeed(MovingAvg *ctx, uint8_t item)
{
    ctx->arr[ctx->pos] = item;
    if (ctx->pos < (MOVING_AVG_SIZE - 1)) {
        ctx->pos++;
    } else {
        ctx->pos = 0;
    }

    uint16_t sum = 0;
    for (uint8_t i = 0; i < MOVING_AVG_SIZE; i++) {
        sum += ctx->arr[i];
    }
    return sum >> MOVING_AVG_SHIFT;
}

// Pure function: returns true when videoStandardInput represents PAL or NTSC SD.
// videoStandardInput == 1 → NTSC, == 2 → PAL
inline bool isPalNtscSd(uint8_t videoStandardInput)
{
    return videoStandardInput == 1 || videoStandardInput == 2;
}
