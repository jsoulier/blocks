#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include "helpers.h"
#include "world.h"

bool voxmesh_vbo(
    chunk_t* chunk,
    chunk_t* neighbors[DIRECTION_3],
    const int height,
    SDL_GPUDevice* device,
    SDL_GPUTransferBuffer** tbo,
    uint32_t* capacity);
bool voxmesh_ibo(
    SDL_GPUDevice* device,
    SDL_GPUBuffer** ibo,
    const uint32_t size);