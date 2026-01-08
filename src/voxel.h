#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "direction.h"

typedef Uint32 voxel_t;

voxel_t voxel_pack_sprite(block_t block, int x, int y, int z, direction_t direction, int i);
voxel_t voxel_pack_cube(block_t block, int x, int y, int z, direction_t direction, int i);