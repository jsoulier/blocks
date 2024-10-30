#pragma once

#include <stdint.h>
#include "containers.h"

typedef enum {
    NOISE_TYPE_CUBE,
    NOISE_TYPE_FLAT,
    NOISE_TYPE_V1,
    NOISE_TYPE_LATEST = NOISE_TYPE_V1,
} noise_type_t;

void noise_init(
    const noise_type_t type,
    const int seed);
noise_type_t noise_type();
int noise_seed();
void noise_generate(
    group_t* group,
    const int32_t x,
    const int32_t z);