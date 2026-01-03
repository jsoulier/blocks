#pragma once

#include <SDL3/SDL.h>

#include "block.h"

typedef Uint32 Voxel;

Voxel VoxelPackSprite(Block block, int x, int y, int z, Direction direction, int i);
Voxel VoxelPackCube(Block block, int x, int y, int z, Direction direction, int occlusion, int i);