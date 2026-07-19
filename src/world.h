#pragma once

#include <SDL3/SDL.h>

#include "block.h"

#define CHUNK_WIDTH 30
#define CHUNK_HEIGHT 240
#define WORLD_WIDTH 20

typedef struct Camera Camera;

typedef enum WorldMeshType
{
    WORLD_MESH_TYPE_OPAQUE,
    WORLD_MESH_TYPE_TRANSPARENT,
    WORLD_MESH_TYPE_COUNT,
} WorldMeshType;

typedef struct WorldQuery
{
    Block block;
    int current[3];
    int previous[3];
} WorldQuery;

void World_Init(SDL_GPUDevice* device);
void World_Free();
void World_Update(const Camera* camera);
void World_Render(const Camera* camera, WorldMeshType type, SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass);
void World_SetBlock(const int position[3], Block block);
Block World_GetBlock(const int position[3]);
WorldQuery World_Raycast(const Camera* camera, float max_distance);
