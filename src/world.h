#pragma once

#include <SDL3/SDL.h>

#include "block.h"

#define CHUNK_WIDTH 30
#define CHUNK_HEIGHT 240
#define WORLD_WIDTH 20

typedef struct Camera Camera;

typedef enum WorldFlags
{
    WORLD_FLAGS_OPAQUE = 0x01,
    WORLD_FLAGS_TRANSPARENT = 0x02,
    WORLD_FLAGS_LIGHT = 0x04,
} WorldFlags;

typedef struct WorldQuery
{
    Block block;
    int current[3];
    int previous[3];
} WorldQuery;

void World_Init(SDL_GPUDevice* device);
void World_Free();
void World_Update(const Camera* camera);
void World_Render(
    const Camera* camera,
    SDL_GPUCommandBuffer* cbuf,
    SDL_GPURenderPass* pass,
    WorldFlags flags);
Block World_GetBlock(const int position[3]);
void World_SetBlock(const int position[3], Block block);
WorldQuery World_Raycast(const Camera* camera, float length);
