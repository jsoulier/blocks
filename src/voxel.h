#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "direction.h"

typedef Uint32 Voxel;

Voxel Voxel_PackSprite(Block block, int x, int y, int z, Direction direction, int index);
Voxel Voxel_PackCube(Block block, int x, int y, int z, Direction direction, int index, int ao);
void Voxel_GetAO(const int ao[4], int order[4]);
void Voxel_GetPosition(Direction direction, int index, int position[3]);
