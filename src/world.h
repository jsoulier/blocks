#pragma once

#include <SDL3/SDL.h>

#include "block.h"

#define CHUNK_WIDTH 30
#define CHUNK_HEIGHT 240
#define WORLD_WIDTH 20

typedef struct camera camera_t;

typedef enum world_flag
{
    WORLD_FLAG_OPAQUE      = 0x01,
    WORLD_FLAG_TRANSPARENT = 0x02,
    WORLD_FLAG_LIGHT       = 0x04,
}
world_flag_t;

typedef struct world_raycast
{
    block_t block;
    int current[3];
    int previous[3];
}
world_raycast_t;

void world_init(SDL_GPUDevice* device);
void world_free();
void world_update(const camera_t* camera);
void world_render(const camera_t* camera, SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass, world_flag_t flags);
block_t world_get_block(const int position[3]);
void world_set_block(const int position[3], block_t block);
world_raycast_t world_raycast(const camera_t* camera, float length);
