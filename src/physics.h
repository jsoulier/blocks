#pragma once

#include <stdbool.h>
#include "helpers.h"

bool physics_raycast(
    float* x,
    float* y,
    float* z,
    const float dx,
    const float dy,
    const float dz,
    const float length,
    const bool previous);