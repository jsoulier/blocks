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
    BLOCK_GLASS,
    BLOCK_GRASS,
    BLOCK_ICE,
    BLOCK_LAVA,
    BLOCK_ROSE,
    BLOCK_SAND,
    BLOCK_SNOW,
    BLOCK_STONE,
    BLOCK_TUMBLEWEED,
    BLOCK_WATER,
    BLOCK_COUNT,
};

bool block_liquid(const block_t block);
bool block_opaque(const block_t block);
bool block_solid(const block_t block);
bool block_sprite(const block_t block);

extern const int blocks[][DIRECTION_3][2];