#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "chunk.h"
#include "worker.h"

#define WORLD_WIDTH 5

typedef struct camera camera_t;

typedef struct world_query
{
    block_t block;
    int current[3];
    int previous[3];
}
world_query_t;

typedef struct world_render_data
{
    camera_t* camera;
    chunk_mesh_type_t type;
    SDL_GPUCommandBuffer* command_buffer;
    SDL_GPURenderPass* render_pass;
    bool use_lights;
}
world_render_data_t;

void world_init(SDL_GPUDevice* device);
void world_free();
void world_update(const camera_t* camera);
void world_render(const world_render_data_t* data);
chunk_t* world_get_chunk(int x, int z);
void world_get_chunks(int x, int z, chunk_t* chunks[3][3]);
block_t world_get_block(int index[3]);
void world_set_block(int index[3], block_t block);
world_query_t world_query(float x, float y, float z, float dx, float dy, float dz, float length);
void world_create_indices(Uint32 size);
