#pragma once

#include <stdint.h>

#include "block.h"

typedef uint32_t voxel_t;

voxel_t voxel_pack_sprite(block_t block, int x, int y, int z, direction_t direction, int i);
voxel_t voxel_pack_cube(block_t block, int x, int y, int z, direction_t direction, int occlusion, int i);