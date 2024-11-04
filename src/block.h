#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "helpers.h"

typedef uint8_t block_t;
enum
{
    BLOCK_EMPTY,
    BLOCK_CLOUD,
    BLOCK_DANDELION,
    BLOCK_DIRT,
    BLOCK_GRASS,
    BLOCK_LEAVES,
    BLOCK_LOG,
    BLOCK_ROSE,
    BLOCK_SAND,
    BLOCK_SNOW,
    BLOCK_STONE,
    BLOCK_WATER,
    BLOCK_COUNT,
};

bool block_opaque(const block_t block);
bool block_solid(const block_t block);
bool block_sprite(const block_t block);

extern const int blocks[][DIRECTION_3][2];