#pragma once

#include <SDL3/SDL.h>

#include "direction.h"

#define BLOCK_WIDTH 16

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
} Light;

SDL_GPUBuffer* Block_GetBuffer(SDL_GPUDevice* device);
bool Block_IsOpaque(Block block);
bool Block_IsSolid(Block block);
bool Block_IsSprite(Block block);
bool Block_IsOccluded(Block block);
int Block_GetIndex(Block block, Direction direction);
bool Block_IsLight(Block block);
Light Block_GetLight(Block block);
