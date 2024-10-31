#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include "containers.h"
#include "helpers.h"

bool voxmesh_vbo(
    chunk_t* chunk,
    chunk_t* neighbors[DIRECTION_3],
    const int height,
    SDL_GPUDevice* device,
    SDL_GPUTransferBuffer** opaque_tbo,
    SDL_GPUTransferBuffer** transparent_tbo,
    uint32_t* opaque_capacity,
    uint32_t* transparent_capacity);
bool voxmesh_ibo(
    SDL_GPUDevice* device,
    SDL_GPUBuffer** ibo,
    const uint32_t size);