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
void physics_collide(
    float* x1,
    float* y1,
    float* z1,
    const float x2,
    const float y2,
    const float z2);