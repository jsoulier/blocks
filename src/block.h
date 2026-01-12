#pragma once

#include <SDL3/SDL.h>

#include "direction.h"
#include "light.h"

typedef Uint8 block_t;
enum /* Block */
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
    BLOCK_YELLOW_TORCH,

    BLOCK_COUNT,
};

bool block_is_opaque(block_t block);
bool block_is_sprite(block_t block);
bool block_is_solid(block_t block);
bool block_is_occluded(block_t block);
bool block_has_shadow(block_t block);
int block_get_index(block_t block, direction_t direction);
bool block_is_light(block_t block);
light_t block_get_light(block_t block);
