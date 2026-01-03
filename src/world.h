#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "chunk.h"
#include "noise.h"

#define WORLD_WIDTH 20

typedef struct camera camera_t;
typedef struct save save_t;

typedef struct world
{
    SDL_GPUDevice* device;
    int x;
    int y;
    int z;
    cpu_buffer_t cpu_index_buffer;
    gpu_buffer_t gpu_index_buffer;
    cpu_buffer_t cpu_voxel_buffer;
    cpu_buffer_t cpu_light_buffer;
    noise_t noise;
    chunk_t chunks[WORLD_WIDTH][WORLD_WIDTH];
    int sorted_chunks[WORLD_WIDTH][WORLD_WIDTH][2];
}
world_t;

void world_init(world_t* world, SDL_GPUDevice* device, const noise_t* noise);
void world_free(world_t* world);
void world_tick(world_t* world, const camera_t* camera, save_t* save);
void world_draw(world_t* world, const camera_t* camera);
