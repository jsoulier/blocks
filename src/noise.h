#pragma once

#include <stdint.h>
#include "world.h"

typedef enum noise
{
    NOISE_CUBE,
    NOISE_FLAT,
    NOISE_COUNT,
} noise_t;

void noise_init(const noise_t noise);
void noise_generate(
    group_t* group,
    const int32_t x,
    const int32_t z);