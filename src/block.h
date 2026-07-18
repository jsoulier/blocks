#pragma once

#include <SDL3/SDL.h>

#include "direction.h"

typedef Uint8 Block;
enum // Block
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

typedef struct Light
{
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 radius;
    Sint32 x;
    Sint32 y;
    Sint32 z;
}
Light;

typedef struct BlockGPU
{
    Uint32 is_sprite;
    Uint32 has_occlusion;
    Uint32 is_fullbright;
    Uint32 indices[DIRECTION_COUNT];
}
BlockGPU;

SDL_GPUBuffer* Block_GetBuffer(SDL_GPUDevice* device);

bool Block_IsOpaque(Block block);
bool Block_IsSprite(Block block);
bool Block_IsSolid(Block block);
bool Block_HasOcclusion(Block block);
int Block_GetIndex(Block block, Direction direction);
bool Block_IsLight(Block block);
Light Block_GetLight(Block block);
