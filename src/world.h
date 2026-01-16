#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"

#define CHUNK_WIDTH 30
#define CHUNK_HEIGHT 240
#define WORLD_WIDTH 20

typedef struct camera camera_t;

typedef enum world_mesh
{
    WORLD_MESH_OPAQUE,
    WORLD_MESH_TRANSPARENT,
    WORLD_MESH_COUNT,
}
world_mesh_t;

typedef struct world_raycast
{
    block_t block;
    int current[3];
    int previous[3];
}
world_raycast_t;

typedef struct world_pass
{
    world_mesh_t mesh;
    SDL_GPUCommandBuffer* command_buffer;
    SDL_GPURenderPass* render_pass;
    camera_t* camera;
    bool lights;
}
world_pass_t;

void world_init(SDL_GPUDevice* device);
void world_free();
void world_update(const camera_t* camera);
void world_render(const world_pass_t* data);
block_t world_get_block(int index[3]);
void world_set_block(int index[3], block_t block);
world_raycast_t world_raycast(const camera_t* camera, float length);
