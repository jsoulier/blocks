#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include "helpers.h"
#include "world.h"

/// @brief Create a procedural voxel mesh for a chunk
/// @param chunk The chunk to create the voxel mesh for
/// @param neighbors The neighboring chunks
/// @param device The device handle
/// @param transfer The transfer buffer (wIll recreate if not large enough)
/// @param capacity The capacity of the transfer buffer (in faces)
/// @return If a voxel mesh was created (false does not mean error)
bool voxmesh_vbo(
    chunk_t* chunk,
    chunk_t* neighbors[DIRECTION_3],
    SDL_GPUDevice* device,
    SDL_GPUTransferBuffer** tbo,
    uint32_t* capacity);

/// @brief Create clockwise wound indices for a voxel mesh
/// @param device The device handle
/// @param buffer The index buffer handle to set
/// @param size The required size of the index buffer (in faces)
/// @return If the indices were created (false does mean error)
bool voxmesh_ibo(
    SDL_GPUDevice* device,
    SDL_GPUBuffer** ibo,
    const uint32_t size);