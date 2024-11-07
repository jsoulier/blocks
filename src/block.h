#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "helpers.h"

typedef uint8_t block_t;
enum
{
    BLOCK_EMPTY,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_SAND,
    BLOCK_SNOW,
    BLOCK_STONE,
    BLOCK_LOG,
    BLOCK_LEAVES,
    BLOCK_BLUEBELL,
    BLOCK_DANDELION,
    BLOCK_ROSE,
    BLOCK_LAVENDER,
    BLOCK_WATER,
    BLOCK_CLOUD,
    BLOCK_COUNT,
};

bool block_opaque(const block_t block);
bool block_solid(const block_t block);
bool block_sprite(const block_t block);

extern const int blocks[][DIRECTION_3][2];