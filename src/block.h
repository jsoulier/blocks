#pragma once

#include <SDL3/SDL.h>

#include "direction.h"

typedef Uint8 block_t;
enum // block_t
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
    BLOCK_GARDENIA,
    BLOCK_ROSE,
    BLOCK_LAVENDER,
    BLOCK_WATER,
    BLOCK_RED_TORCH,
    BLOCK_GREEN_TORCH,
    BLOCK_BLUE_TORCH,
    BLOCK_YELLOW_TORCH,
    BLOCK_CYAN_TORCH,
    BLOCK_MAGENTA_TORCH,
    BLOCK_WHITE_TORCH,
    BLOCK_PLANKS,

    BLOCK_COUNT,
};

typedef struct light
{
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 radius;
    Sint32 x;
    Sint32 y;
    Sint32 z;
}
light_t;

typedef struct block_gpu
{
    Uint32 is_sprite;
    Uint32 has_occlusion;
    Uint32 has_shadow;
    Uint32 is_fullbright;
    Uint32 indices[DIRECTION_COUNT];
}
block_gpu_t;

bool block_init(SDL_GPUDevice* device);
void block_free();
SDL_GPUBuffer* block_get_buffer();

bool block_is_opaque(block_t block);
bool block_is_sprite(block_t block);
bool block_is_solid(block_t block);
bool block_has_occlusion(block_t block);
int block_get_index(block_t block, direction_t direction);
bool block_is_light(block_t block);
light_t block_get_light(block_t block);
