#pragma once

#include <stdint.h>
#include "helpers.h"
#include "world.h"

/// @brief Create a procedural voxel mesh from a chunk
/// @param chunk The chunk to crate the voxel mesh for
/// @param neighbors The neighboring chunks
/// @param data The mapped transfer buffer
/// @param capacity The capacity (in faces) of the mapped transfer buffer
/// @return The required capacity (in faces)
uint32_t voxmesh_make(
    const chunk_t* chunk,
    const chunk_t* neighbors[DIRECTION_3],
    uint32_t* data,
    const uint32_t capacity);

/// @brief Create clockwise wound imdices for a voxel mesh
/// @param data The mapped transfer buffer
/// @param size The required number of faces
void voxmesh_indices(uint32_t* data, const uint32_t size);