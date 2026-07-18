#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "direction.h"

typedef Uint32 Voxel;

Voxel Voxel_PackSprite(Block block, int x, int y, int z, Direction direction, int vertex);
Voxel Voxel_PackCube(Block block, int x, int y, int z, Direction direction, int vertex, int ambient_occlusion);
void Voxel_GetCubePosition(Direction direction, int vertex, int position[3]);
