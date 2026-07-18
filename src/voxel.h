#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "direction.h"

typedef Uint32 Voxel;

Voxel Voxel_PackSprite(Block block, int x, int y, int z, Direction direction, int i);
Voxel Voxel_PackCube(Block block, int x, int y, int z, Direction direction, int i, int ao);
void Voxel_GetCubePosition(Direction direction, int i, int position[3]);
