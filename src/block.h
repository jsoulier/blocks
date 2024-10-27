#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "helpers.h"

typedef uint8_t block_t;
enum
{
    BLOCK_EMPTY,
    BLOCK_CLOUD,
    BLOCK_DIRT,
    BLOCK_GLASS,
    BLOCK_GRASS,
    BLOCK_LAVA,
    BLOCK_SAND,
    BLOCK_SNOW,
    BLOCK_STONE,
    BLOCK_WATER,
    BLOCK_COUNT,
};

extern const int blocks[][DIRECTION_3][2];

bool block_visible(const block_t a, const block_t b);