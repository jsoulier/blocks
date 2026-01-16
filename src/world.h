#pragma once

#include <SDL3/SDL.h>

#include "block.h"

#define CHUNK_WIDTH 30
#define CHUNK_HEIGHT 240
#define WORLD_WIDTH 20

typedef struct camera camera_t;

typedef enum world_flags
{
    WORLD_FLAGS_OPAQUE      = 0x01,
    WORLD_FLAGS_TRANSPARENT = 0x02,
    WORLD_FLAGS_LIGHT       = 0x04,
}
world_flags_t;

typedef struct world_query
{
    block_t block;
    int current[3];
    int previous[3];
}
world_query_t;

void world_init(SDL_GPUDevice* device);
void world_free();
void world_update(const camera_t* camera);
void world_render(const camera_t* camera, SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, world_flags_t flags);
block_t world_get_block(const int position[3]);
void world_set_block(const int position[3], block_t block);
world_query_t world_raycast(const camera_t* camera, float length);
