#pragma once

#include <stdint.h>

#include "direction.h"

typedef uint8_t block_t;
enum /* block_t */
{
    BLOCK_EMPTY,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_SAND,
    BLOCK_SNOW,
    BLOCK_STONE,
    BLOCK_LOG,
    BLOCK_LEAVES,
    BLOCK_CLOUD,
    BLOCK_BUSH,
    BLOCK_BLUEBELL,
    BLOCK_DANDELION,
    BLOCK_ROSE,
    BLOCK_LAVENDER,
    BLOCK_WATER,
    BLOCK_COUNT,
};

extern const int blocks[][DIRECTION_3];